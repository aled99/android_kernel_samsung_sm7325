/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PM_QOS_H
#define _LINUX_PM_QOS_H
/* interface for the pm_qos_power infrastructure of the linux kernel.
 *
 * Mark Gross <mgross@linux.intel.com>
 */
#include <linux/plist.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>

enum {
	PM_QOS_RESERVED = 0,
	PM_QOS_CPU_DMA_LATENCY,
#if IS_ENABLED(CONFIG_SEC_INPUT_BOOSTER)
	PM_QOS_BIAS_HYST,
#endif
#ifdef CONFIG_ARGOS
	PM_QOS_NETWORK_THROUGHPUT,
#endif
	/* insert new class ID */
	PM_QOS_NUM_CLASSES,
};

enum pm_qos_flags_status {
	PM_QOS_FLAGS_UNDEFINED = -1,
	PM_QOS_FLAGS_NONE,
	PM_QOS_FLAGS_SOME,
	PM_QOS_FLAGS_ALL,
};

#define PM_QOS_DEFAULT_VALUE	(-1)
#define PM_QOS_LATENCY_ANY	S32_MAX
#define PM_QOS_LATENCY_ANY_NS	((s64)PM_QOS_LATENCY_ANY * NSEC_PER_USEC)

#define PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE	(2000 * USEC_PER_SEC)
#define PM_QOS_RESUME_LATENCY_DEFAULT_VALUE	PM_QOS_LATENCY_ANY
#define PM_QOS_RESUME_LATENCY_NO_CONSTRAINT	PM_QOS_LATENCY_ANY
#define PM_QOS_RESUME_LATENCY_NO_CONSTRAINT_NS	PM_QOS_LATENCY_ANY_NS
#define PM_QOS_LATENCY_TOLERANCE_DEFAULT_VALUE	0
#define PM_QOS_MIN_FREQUENCY_DEFAULT_VALUE	0
#if IS_ENABLED(CONFIG_SEC_INPUT_BOOSTER)
#define PM_QOS_BIAS_HYST_DEFAULT_VALUE 0
#endif
#ifdef CONFIG_ARGOS
#define PM_QOS_NETWORK_THROUGHPUT_DEFAULT_VALUE	0
#endif
#define PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE	FREQ_QOS_MAX_DEFAULT_VALUE
#define PM_QOS_LATENCY_TOLERANCE_NO_CONSTRAINT	(-1)

#define PM_QOS_FLAG_NO_POWER_OFF	(1 << 0)

enum pm_qos_req_type {
	PM_QOS_REQ_ALL_CORES = 0,
	PM_QOS_REQ_AFFINE_CORES,
#ifdef CONFIG_SMP
	PM_QOS_REQ_AFFINE_IRQ,
#endif
};

struct pm_qos_request {
	unsigned long cpus_affine;
	enum pm_qos_req_type type;
#ifdef CONFIG_SMP
	uint32_t irq;
	/* Internal structure members */
	struct irq_affinity_notify irq_notify;
#endif
	struct plist_node node;
	int pm_qos_class;
};

struct pm_qos_flags_request {
	struct list_head node;
	s32 flags;	/* Do not change to 64 bit */
};

enum pm_qos_type {
	PM_QOS_UNITIALIZED,
	PM_QOS_MAX,		/* return the largest value */
	PM_QOS_MIN,		/* return the smallest value */
	PM_QOS_SUM		/* return the sum */
};

/*
 * Note: The lockless read path depends on the CPU accessing target_value
 * or effective_flags atomically.  Atomic access is only guaranteed on all CPU
 * types linux supports for 32 bit quantites
 */
struct pm_qos_constraints {
	struct plist_head list;
	s32 target_value;	/* Do not change to 64 bit */
	s32 target_per_cpu[NR_CPUS];
	s32 default_value;
	s32 no_constraint_value;
	enum pm_qos_type type;
	struct blocking_notifier_head *notifiers;
};

struct pm_qos_flags {
	struct list_head list;
	s32 effective_flags;	/* Do not change to 64 bit */
};


#define FREQ_QOS_MIN_DEFAULT_VALUE	0
#define FREQ_QOS_MAX_DEFAULT_VALUE	S32_MAX

enum freq_qos_req_type {
	FREQ_QOS_MIN = 1,
	FREQ_QOS_MAX,
};

struct freq_constraints {
	struct pm_qos_constraints min_freq;
	struct blocking_notifier_head min_freq_notifiers;
	struct pm_qos_constraints max_freq;
	struct blocking_notifier_head max_freq_notifiers;
};

struct freq_qos_request {
	enum freq_qos_req_type type;
	struct plist_node pnode;
	struct freq_constraints *qos;
};


enum dev_pm_qos_req_type {
	DEV_PM_QOS_RESUME_LATENCY = 1,
	DEV_PM_QOS_LATENCY_TOLERANCE,
	DEV_PM_QOS_MIN_FREQUENCY,
	DEV_PM_QOS_MAX_FREQUENCY,
#if IS_ENABLED(CONFIG_SEC_INPUT_BOOSTER)
	DEV_PM_QOS_BIAS_HYST,
#endif
	DEV_PM_QOS_FLAGS,
};

struct dev_pm_qos_request {
	enum dev_pm_qos_req_type type;
	union {
		struct plist_node pnode;
		struct pm_qos_flags_request flr;
		struct freq_qos_request freq;
	} data;
	struct device *dev;
};

struct dev_pm_qos {
	struct pm_qos_constraints resume_latency;
	struct pm_qos_constraints latency_tolerance;
	struct freq_constraints freq;
	struct pm_qos_flags flags;
	struct dev_pm_qos_request *resume_latency_req;
	struct dev_pm_qos_request *latency_tolerance_req;
	struct dev_pm_qos_request *flags_req;
};

/* Action requested to pm_qos_update_target */
enum pm_qos_req_action {
	PM_QOS_ADD_REQ,		/* Add a new request */
	PM_QOS_UPDATE_REQ,	/* Update an existing request */
	PM_QOS_REMOVE_REQ	/* Remove an existing request */
};

static inline int dev_pm_qos_request_active(struct dev_pm_qos_request *req)
{
	return req->dev != NULL;
}

int pm_qos_update_target(struct pm_qos_constraints *c, struct plist_node *node,
			 enum pm_qos_req_action action, int value);
bool pm_qos_update_flags(struct pm_qos_flags *pqf,
			 struct pm_qos_flags_request *req,
			 enum pm_qos_req_action action, s32 val);
void pm_qos_add_request(struct pm_qos_request *req, int pm_qos_class,
			s32 value);
void pm_qos_update_request(struct pm_qos_request *req,
			   s32 new_value);
void pm_qos_remove_request(struct pm_qos_request *req);

int pm_qos_request(int pm_qos_class);
int pm_qos_request_for_cpu(int pm_qos_class, int cpu);
int pm_qos_request_for_cpumask(int pm_qos_class, struct cpumask *mask);
int pm_qos_add_notifier(int pm_qos_class, struct notifier_block *notifier);
int pm_qos_remove_notifier(int pm_qos_class, struct notifier_block *notifier);
int pm_qos_request_active(struct pm_qos_request *req);
s32 pm_qos_read_value(struct pm_qos_constraints *c);

#ifdef CONFIG_PM
enum pm_qos_flags_status __dev_pm_qos_flags(struct device *dev, s32 mask);
enum pm_qos_flags_status dev_pm_qos_flags(struct device *dev, s32 mask);
s32 __dev_pm_qos_resume_latency(struct device *dev);
s32 dev_pm_qos_read_value(struct device *dev, enum dev_pm_qos_req_type type);
int dev_pm_qos_add_request(struct device *dev, struct dev_pm_qos_request *req,
			   enum dev_pm_qos_req_type type, s32 value);
int dev_pm_qos_update_request(struct dev_pm_qos_request *req, s32 new_value);
int dev_pm_qos_remove_request(struct dev_pm_qos_request *req);
int dev_pm_qos_add_notifier(struct device *dev,
			    struct notifier_block *notifier,
			    enum dev_pm_qos_req_type type);
int dev_pm_qos_remove_notifier(struct device *dev,
			       struct notifier_block *notifier,
			       enum dev_pm_qos_req_type type);
void dev_pm_qos_constraints_init(struct device *dev);
void dev_pm_qos_constraints_destroy(struct device *dev);
int dev_pm_qos_add_ancestor_request(struct device *dev,
				    struct dev_pm_qos_request *req,
				    enum dev_pm_qos_req_type type, s32 value);
int dev_pm_qos_expose_latency_limit(struct device *dev, s32 value);
void dev_pm_qos_hide_latency_limit(struct device *dev);
int dev_pm_qos_expose_flags(struct device *dev, s32 value);
void dev_pm_qos_hide_flags(struct device *dev);
int dev_pm_qos_update_flags(struct device *dev, s32 mask, bool set);
s32 dev_pm_qos_get_user_latency_tolerance(struct device *dev);
int dev_pm_qos_update_user_latency_tolerance(struct device *dev, s32 val);
int dev_pm_qos_expose_latency_tolerance(struct device *dev);
void dev_pm_qos_hide_latency_tolerance(struct device *dev);

static inline s32 dev_pm_qos_requested_resume_latency(struct device *dev)
{
	return dev->power.qos->resume_latency_req->data.pnode.prio;
}

static inline s32 dev_pm_qos_requested_flags(struct device *dev)
{
	return dev->power.qos->flags_req->data.flr.flags;
}

static inline s32 dev_pm_qos_raw_resume_latency(struct device *dev)
{
	return IS_ERR_OR_NULL(dev->power.qos) ?
		PM_QOS_RESUME_LATENCY_NO_CONSTRAINT :
		pm_qos_read_value(&dev->power.qos->resume_latency);
}
#else
static inline enum pm_qos_flags_status __dev_pm_qos_flags(struct device *dev,
							  s32 mask)
			{ return PM_QOS_FLAGS_UNDEFINED; }
static inline enum pm_qos_flags_status dev_pm_qos_flags(struct device *dev,
							s32 mask)
			{ return PM_QOS_FLAGS_UNDEFINED; }
static inline s32 __dev_pm_qos_resume_latency(struct device *dev)
			{ return PM_QOS_RESUME_LATENCY_NO_CONSTRAINT; }
static inline s32 dev_pm_qos_read_value(struct device *dev,
					enum dev_pm_qos_req_type type)
{
	switch (type) {
	case DEV_PM_QOS_RESUME_LATENCY:
		return PM_QOS_RESUME_LATENCY_NO_CONSTRAINT;
	case DEV_PM_QOS_MIN_FREQUENCY:
		return PM_QOS_MIN_FREQUENCY_DEFAULT_VALUE;
	case DEV_PM_QOS_MAX_FREQUENCY:
		return PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE;
#if IS_ENABLED(CONFIG_SEC_INPUT_BOOSTER)
	case DEV_PM_QOS_BIAS_HYST:
		return PM_QOS_BIAS_HYST_DEFAULT_VALUE;
#endif
	default:
		WARN_ON(1);
		return 0;
	}
}

static inline int dev_pm_qos_add_request(struct device *dev,
					 struct dev_pm_qos_request *req,
					 enum dev_pm_qos_req_type type,
					 s32 value)
			{ return 0; }
static inline int dev_pm_qos_update_request(struct dev_pm_qos_request *req,
					    s32 new_value)
			{ return 0; }
static inline int dev_pm_qos_remove_request(struct dev_pm_qos_request *req)
			{ return 0; }
static inline int dev_pm_qos_add_notifier(struct device *dev,
					  struct notifier_block *notifier,
					  enum dev_pm_qos_req_type type)
			{ return 0; }
static inline int dev_pm_qos_remove_notifier(struct device *dev,
					     struct notifier_block *notifier,
					     enum dev_pm_qos_req_type type)
			{ return 0; }
static inline void dev_pm_qos_constraints_init(struct device *dev)
{
	dev->power.power_state = PMSG_ON;
}
static inline void dev_pm_qos_constraints_destroy(struct device *dev)
{
	dev->power.power_state = PMSG_INVALID;
}
static inline int dev_pm_qos_add_ancestor_request(struct device *dev,
						  struct dev_pm_qos_request *req,
						  enum dev_pm_qos_req_type type,
						  s32 value)
			{ return 0; }
static inline int dev_pm_qos_expose_latency_limit(struct device *dev, s32 value)
			{ return 0; }
static inline void dev_pm_qos_hide_latency_limit(struct device *dev) {}
static inline int dev_pm_qos_expose_flags(struct device *dev, s32 value)
			{ return 0; }
static inline void dev_pm_qos_hide_flags(struct device *dev) {}
static inline int dev_pm_qos_update_flags(struct device *dev, s32 m, bool set)
			{ return 0; }
static inline s32 dev_pm_qos_get_user_latency_tolerance(struct device *dev)
			{ return PM_QOS_LATENCY_TOLERANCE_NO_CONSTRAINT; }
static inline int dev_pm_qos_update_user_latency_tolerance(struct device *dev, s32 val)
			{ return 0; }
static inline int dev_pm_qos_expose_latency_tolerance(struct device *dev)
			{ return 0; }
static inline void dev_pm_qos_hide_latency_tolerance(struct device *dev) {}

static inline s32 dev_pm_qos_requested_resume_latency(struct device *dev)
{
	return PM_QOS_RESUME_LATENCY_NO_CONSTRAINT;
}
static inline s32 dev_pm_qos_requested_flags(struct device *dev) { return 0; }
static inline s32 dev_pm_qos_raw_resume_latency(struct device *dev)
{
	return PM_QOS_RESUME_LATENCY_NO_CONSTRAINT;
}
#endif

static inline int freq_qos_request_active(struct freq_qos_request *req)
{
	return !IS_ERR_OR_NULL(req->qos);
}

void freq_constraints_init(struct freq_constraints *qos);

s32 freq_qos_read_value(struct freq_constraints *qos,
			enum freq_qos_req_type type);

int freq_qos_add_request(struct freq_constraints *qos,
			 struct freq_qos_request *req,
			 enum freq_qos_req_type type, s32 value);
int freq_qos_update_request(struct freq_qos_request *req, s32 new_value);
int freq_qos_remove_request(struct freq_qos_request *req);
int freq_qos_apply(struct freq_qos_request *req,
		   enum pm_qos_req_action action, s32 value);

int freq_qos_add_notifier(struct freq_constraints *qos,
			  enum freq_qos_req_type type,
			  struct notifier_block *notifier);
int freq_qos_remove_notifier(struct freq_constraints *qos,
			     enum freq_qos_req_type type,
			     struct notifier_block *notifier);

#endif
