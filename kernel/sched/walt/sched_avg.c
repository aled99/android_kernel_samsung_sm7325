// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012, 2015-2021, The Linux Foundation. All rights reserved.
 */
/*
 * Scheduler hook for average runqueue determination
 */
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/math64.h>
#if IS_ENABLED(CONFIG_SEC_INPUT_BOOSTER)
#include <linux/pm_qos.h>
#endif

#include "qc_vas.h"
#include <trace/events/sched.h>

static DEFINE_PER_CPU(u64, nr_prod_sum);
static DEFINE_PER_CPU(u64, last_time);
static DEFINE_PER_CPU(u64, nr_big_prod_sum);
static DEFINE_PER_CPU(u64, nr);
static DEFINE_PER_CPU(u64, nr_max);

static DEFINE_PER_CPU(spinlock_t, nr_lock) = __SPIN_LOCK_UNLOCKED(nr_lock);
static s64 last_get_time;

unsigned int sysctl_sched_busy_hyst_enable_cpus;
unsigned int sysctl_sched_busy_hyst;
unsigned int sysctl_sched_coloc_busy_hyst_enable_cpus = 112;
unsigned int sysctl_sched_coloc_busy_hyst_cpu[NR_CPUS] = {
		[0 ... NR_CPUS-1] = 39000000 };
unsigned int sysctl_sched_coloc_busy_hyst_max_ms = 5000;
unsigned int sysctl_sched_coloc_busy_hyst_cpu_busy_pct[NR_CPUS] = {
		[0 ... NR_CPUS-1] = 10 };
static DEFINE_PER_CPU(atomic64_t, busy_hyst_end_time) = ATOMIC64_INIT(0);

static DEFINE_PER_CPU(u64, hyst_time);
static DEFINE_PER_CPU(u64, coloc_hyst_busy);
static DEFINE_PER_CPU(u64, coloc_hyst_time);

#define NR_THRESHOLD_PCT		15
#define MAX_RTGB_TIME (sysctl_sched_coloc_busy_hyst_max_ms * NSEC_PER_MSEC)

/**
 * sched_get_nr_running_avg
 * @return: Average nr_running, iowait and nr_big_tasks value since last poll.
 *	    Returns the avg * 100 to return up to two decimal points
 *	    of accuracy.
 *
 * Obtains the average nr_running value since the last poll.
 * This function may not be called concurrently with itself
 */
void sched_get_nr_running_avg(struct sched_avg_stats *stats)
{
	int cpu;
	u64 curr_time = sched_clock();
	u64 period = curr_time - last_get_time;
	u64 tmp_nr, tmp_misfit;
	bool any_hyst_time = false;

	if (!period)
		return;

	/* read and reset nr_running counts */
	for_each_possible_cpu(cpu) {
		unsigned long flags;
		u64 diff;

		spin_lock_irqsave(&per_cpu(nr_lock, cpu), flags);
		curr_time = sched_clock();
		diff = curr_time - per_cpu(last_time, cpu);
		BUG_ON((s64)diff < 0);

		tmp_nr = per_cpu(nr_prod_sum, cpu);
		tmp_nr += per_cpu(nr, cpu) * diff;
		tmp_nr = div64_u64((tmp_nr * 100), period);

		tmp_misfit = per_cpu(nr_big_prod_sum, cpu);
		tmp_misfit += walt_big_tasks(cpu) * diff;
		tmp_misfit = div64_u64((tmp_misfit * 100), period);

		/*
		 * NR_THRESHOLD_PCT is to make sure that the task ran
		 * at least 85% in the last window to compensate any
		 * over estimating being done.
		 */
		stats[cpu].nr = (int)div64_u64((tmp_nr + NR_THRESHOLD_PCT),
								100);
		stats[cpu].nr_misfit = (int)div64_u64((tmp_misfit +
						NR_THRESHOLD_PCT), 100);
		stats[cpu].nr_max = per_cpu(nr_max, cpu);
		stats[cpu].nr_scaled = tmp_nr;

		trace_sched_get_nr_running_avg(cpu, stats[cpu].nr,
				stats[cpu].nr_misfit, stats[cpu].nr_max,
				stats[cpu].nr_scaled);

		per_cpu(last_time, cpu) = curr_time;
		per_cpu(nr_prod_sum, cpu) = 0;
		per_cpu(nr_big_prod_sum, cpu) = 0;
		per_cpu(nr_max, cpu) = per_cpu(nr, cpu);

		spin_unlock_irqrestore(&per_cpu(nr_lock, cpu), flags);
	}

	for_each_possible_cpu(cpu) {
		if (per_cpu(coloc_hyst_time, cpu)) {
			any_hyst_time = true;
			break;
		}
	}
	if (any_hyst_time && get_rtgb_active_time() >= MAX_RTGB_TIME)
		sched_update_hyst_times();

	last_get_time = curr_time;

}
EXPORT_SYMBOL(sched_get_nr_running_avg);

void sched_update_hyst_times(void)
{
	bool rtgb_active;
	int cpu;
	unsigned long cpu_cap, coloc_busy_pct;

	rtgb_active = is_rtgb_active() && (sched_boost() != CONSERVATIVE_BOOST)
			&& (get_rtgb_active_time() < MAX_RTGB_TIME);

	for_each_possible_cpu(cpu) {
		cpu_cap = arch_scale_cpu_capacity(cpu);
		coloc_busy_pct = sysctl_sched_coloc_busy_hyst_cpu_busy_pct[cpu];
		per_cpu(hyst_time, cpu) = (BIT(cpu)
			     & sysctl_sched_busy_hyst_enable_cpus) ?
#if IS_ENABLED(CONFIG_SEC_INPUT_BOOSTER)
			     max(sysctl_sched_busy_hyst,
				    (unsigned int)(pm_qos_request(PM_QOS_BIAS_HYST) * NSEC_PER_MSEC)) : 0;
#else
			     sysctl_sched_busy_hyst : 0;
#endif
		per_cpu(coloc_hyst_time, cpu) = ((BIT(cpu)
			     & sysctl_sched_coloc_busy_hyst_enable_cpus)
			     && rtgb_active) ?
			     sysctl_sched_coloc_busy_hyst_cpu[cpu] : 0;
		per_cpu(coloc_hyst_busy, cpu) = mult_frac(cpu_cap,
							coloc_busy_pct, 100);
	}
}

#define BUSY_NR_RUN		3
#define BUSY_LOAD_FACTOR	10
static inline void update_busy_hyst_end_time(int cpu, bool dequeue,
				unsigned long prev_nr_run, u64 curr_time)
{
	bool nr_run_trigger = false;
	bool load_trigger = false, coloc_load_trigger = false;
	u64 agg_hyst_time;

	if (!per_cpu(hyst_time, cpu) && !per_cpu(coloc_hyst_time, cpu))
		return;

	if (prev_nr_run >= BUSY_NR_RUN && per_cpu(nr, cpu) < BUSY_NR_RUN)
		nr_run_trigger = true;

	if (dequeue && (cpu_util(cpu) * BUSY_LOAD_FACTOR) >
			capacity_orig_of(cpu))
		load_trigger = true;

	if (dequeue && cpu_util(cpu) > per_cpu(coloc_hyst_busy, cpu))
		coloc_load_trigger = true;

	agg_hyst_time = max((nr_run_trigger || load_trigger) ?
				per_cpu(hyst_time, cpu) : 0,
				(nr_run_trigger || coloc_load_trigger) ?
				per_cpu(coloc_hyst_time, cpu) : 0);

	if (agg_hyst_time)
		atomic64_set(&per_cpu(busy_hyst_end_time, cpu),
				curr_time + agg_hyst_time);
}

int sched_busy_hyst_handler(struct ctl_table *table, int write,
				void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	if (table->maxlen > (sizeof(unsigned int) * num_possible_cpus()))
		table->maxlen = sizeof(unsigned int) * num_possible_cpus();

	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);

	if (!ret && write)
		sched_update_hyst_times();

	return ret;
}

/**
 * sched_update_nr_prod
 * @cpu: The core id of the nr running driver.
 * @delta: Adjust nr by 'delta' amount
 * @inc: Whether we are increasing or decreasing the count
 * @return: N/A
 *
 * Update average with latest nr_running value for CPU
 */
void sched_update_nr_prod(int cpu, long delta, bool inc)
{
	u64 diff;
	u64 curr_time;
	unsigned long flags, nr_running;

	spin_lock_irqsave(&per_cpu(nr_lock, cpu), flags);
	nr_running = per_cpu(nr, cpu);
	curr_time = sched_clock();
	diff = curr_time - per_cpu(last_time, cpu);
	BUG_ON((s64)diff < 0);
	per_cpu(last_time, cpu) = curr_time;
	per_cpu(nr, cpu) = nr_running + (inc ? delta : -delta);

	BUG_ON((s64)per_cpu(nr, cpu) < 0);

	if (per_cpu(nr, cpu) > per_cpu(nr_max, cpu))
		per_cpu(nr_max, cpu) = per_cpu(nr, cpu);

	update_busy_hyst_end_time(cpu, !inc, nr_running, curr_time);

	per_cpu(nr_prod_sum, cpu) += nr_running * diff;
	per_cpu(nr_big_prod_sum, cpu) += walt_big_tasks(cpu) * diff;
	spin_unlock_irqrestore(&per_cpu(nr_lock, cpu), flags);
}
EXPORT_SYMBOL(sched_update_nr_prod);

/*
 * Returns the CPU utilization % in the last window.
 *
 */
unsigned int sched_get_cpu_util(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	u64 util;
	unsigned long capacity, flags;
	unsigned int busy;

	raw_spin_lock_irqsave(&rq->lock, flags);

	capacity = capacity_orig_of(cpu);

	util = rq->wrq.prev_runnable_sum + rq->wrq.grp_time.prev_runnable_sum;
	util = div64_u64(util, sched_ravg_window >> SCHED_CAPACITY_SHIFT);
	raw_spin_unlock_irqrestore(&rq->lock, flags);

	util = (util >= capacity) ? capacity : util;
	busy = div64_ul((util * 100), capacity);
	return busy;
}

u64 sched_lpm_disallowed_time(int cpu)
{
	u64 now = sched_clock();
	u64 bias_end_time = atomic64_read(&per_cpu(busy_hyst_end_time, cpu));

	if (now < bias_end_time)
		return bias_end_time - now;

	return 0;
}
