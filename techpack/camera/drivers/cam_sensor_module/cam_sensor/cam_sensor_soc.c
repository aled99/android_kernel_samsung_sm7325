// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <cam_sensor_cmn_header.h>
#include <cam_sensor_util.h>
#include <cam_sensor_io.h>
#include <cam_req_mgr_util.h>
#include "cam_sensor_soc.h"
#include "cam_soc_util.h"

#if defined(CONFIG_CAMERA_SYSFS_V2)
extern char rear_cam_info[150];
extern char front_cam_info[150];
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
extern char front2_cam_info[150];
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
extern char front3_cam_info[150];
#else
extern char front2_cam_info[150];
#endif
#endif

#if defined(CONFIG_SAMSUNG_REAR_DUAL)
extern char rear2_cam_info[150];
#endif
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
extern char rear3_cam_info[150];
#endif
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
extern char rear4_cam_info[150];
#endif

struct caminfo_element {
	char* property_name;
	char* prefix;
	char* values[32];
};

struct caminfo_element caminfos[] = {
	{ "cam,isp",            "ISP",      { "INT", "EXT", "SOC" }     },
	{ "cam,cal_memory",     "CALMEM",   { "N", "Y", "Y", "Y" }      },
	{ "cam,read_version",   "READVER",  { "SYSFS", "CAMON" }        },
	{ "cam,core_voltage",   "COREVOLT", { "N", "Y" }                },
	{ "cam,upgrade",        "UPGRADE",  { "N", "SYSFS", "CAMON" }   },
	{ "cam,fw_write",       "FWWRITE",  { "N", "OIS", "SD", "ALL" } },
	{ "cam,fw_dump",        "FWDUMP",   { "N", "Y" }                },
	{ "cam,companion_chip", "CC",       { "N", "Y" }                },
	{ "cam,ois",            "OIS",      { "N", "Y" }                },
	{ "cam,valid",          "VALID",    { "N", "Y" }                },
	{ "cam,dual_open",      "DUALOPEN", { "N", "Y" }                },
};

int cam_sensor_get_dt_camera_info(
	struct cam_sensor_ctrl_t *s_ctrl,
	struct device_node *of_node)
{
	int rc = 0, i = 0, idx = 0, offset = 0, cnt = 0;
	char* cam_info = NULL;
	bool isValid = false;

	/* camera information */
	if (s_ctrl->id == SEC_WIDE_SENSOR)
		cam_info = rear_cam_info;
	else if (s_ctrl->id == SEC_FRONT_SENSOR)
		cam_info = front_cam_info;
#if defined(CONFIG_SAMSUNG_REAR_DUAL)
	else if (s_ctrl->id == SEC_ULTRA_WIDE_SENSOR)
		cam_info = rear2_cam_info;
#endif
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
	else if (s_ctrl->id == SEC_TELE_SENSOR)
		cam_info = rear3_cam_info;
#if defined (CONFIG_SEC_M52XQ_PROJECT)
	else if (s_ctrl->id == SEC_MACRO_SENSOR)
		cam_info = rear3_cam_info;
#endif
#endif
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
	else if (s_ctrl->id == SEC_TELE2_SENSOR)
		cam_info = rear4_cam_info;
#if defined (CONFIG_SEC_A52SXQ_PROJECT) || defined(CONFIG_SEC_A73XQ_PROJECT)
	else if (s_ctrl->id == SEC_MACRO_SENSOR)
		cam_info = rear4_cam_info;
#endif
#endif
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
	else if (s_ctrl->id == SEC_FRONT_AUX1_SENSOR)
		cam_info = front2_cam_info;
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
	else if (s_ctrl->id == SEC_FRONT_TOP_SENSOR)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
		cam_info = front3_cam_info;
#else
		cam_info = front2_cam_info;
#endif
#endif
	else
		cam_info = NULL;

	if (cam_info == NULL)
		return 0;

	memset(cam_info, 0, sizeof(char) * 150);

	for (i = 0; i < ARRAY_SIZE(caminfos); i++) {
		if (caminfos[i].property_name == NULL)
			continue;

		rc = of_property_read_u32(of_node,
			caminfos[i].property_name, &idx);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "failed");
			goto ERROR1;
		}

		isValid = (idx >= 0) && (idx < ARRAY_SIZE(caminfos[i].values));
		cnt = scnprintf(&cam_info[offset], PAGE_SIZE, "%s=%s;",
			caminfos[i].prefix, (isValid ? caminfos[i].values[idx] : "NULL"));
		offset += cnt;
	}
	cam_info[offset] = '\0';

	return 0;

ERROR1:
	strcpy(cam_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;FWWRITE=NULL;FWDUMP=NULL;FW_CC=NULL;OIS=NULL;DUALOPEN=NULL");
	return rc;
}
#endif

int32_t cam_sensor_get_sub_module_index(struct device_node *of_node,
	struct cam_sensor_board_info *s_info)
{
	int rc = 0, i = 0;
	uint32_t val = 0;
	struct device_node *src_node = NULL;
	struct cam_sensor_board_info *sensor_info;

	sensor_info = s_info;

	for (i = 0; i < SUB_MODULE_MAX; i++)
		sensor_info->subdev_id[i] = -1;

	src_node = of_parse_phandle(of_node, "actuator-src", 0);
	if (!src_node) {
		CAM_DBG(CAM_SENSOR, "src_node NULL");
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CAM_DBG(CAM_SENSOR, "actuator cell index %d, rc %d", val, rc);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "failed %d", rc);
			of_node_put(src_node);
			return rc;
		}
		sensor_info->subdev_id[SUB_MODULE_ACTUATOR] = val;
		of_node_put(src_node);
	}

	src_node = of_parse_phandle(of_node, "ois-src", 0);
	if (!src_node) {
		CAM_DBG(CAM_SENSOR, "src_node NULL");
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CAM_DBG(CAM_SENSOR, " ois cell index %d, rc %d", val, rc);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "failed %d",  rc);
			of_node_put(src_node);
			return rc;
		}
		sensor_info->subdev_id[SUB_MODULE_OIS] = val;
		of_node_put(src_node);
	}

	src_node = of_parse_phandle(of_node, "eeprom-src", 0);
	if (!src_node) {
		CAM_DBG(CAM_SENSOR, "eeprom src_node NULL");
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CAM_DBG(CAM_SENSOR, "eeprom cell index %d, rc %d", val, rc);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "failed %d", rc);
			of_node_put(src_node);
			return rc;
		}
		sensor_info->subdev_id[SUB_MODULE_EEPROM] = val;
		of_node_put(src_node);
	}

	src_node = of_parse_phandle(of_node, "led-flash-src", 0);
	if (!src_node) {
		CAM_DBG(CAM_SENSOR, " src_node NULL");
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CAM_DBG(CAM_SENSOR, "led flash cell index %d, rc %d", val, rc);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "failed %d", rc);
			of_node_put(src_node);
			return rc;
		}
		sensor_info->subdev_id[SUB_MODULE_LED_FLASH] = val;
		of_node_put(src_node);
	}

	rc = of_property_read_u32(of_node, "csiphy-sd-index", &val);
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "paring the dt node for csiphy rc %d", rc);
	else
		sensor_info->subdev_id[SUB_MODULE_CSIPHY] = val;

	return rc;
}

static int32_t cam_sensor_driver_get_dt_data(struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	int i = 0;
	struct cam_sensor_board_info *sensordata = NULL;
	struct device_node *of_node = s_ctrl->of_node;
	struct device_node *of_parent = NULL;
	struct cam_hw_soc_info *soc_info = &s_ctrl->soc_info;

	s_ctrl->sensordata = kzalloc(sizeof(*sensordata), GFP_KERNEL);
	if (!s_ctrl->sensordata)
		return -ENOMEM;

	sensordata = s_ctrl->sensordata;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Failed to read DT properties rc %d", rc);
		goto FREE_SENSOR_DATA;
	}

	rc =  cam_sensor_util_init_gpio_pin_tbl(soc_info,
			&sensordata->power_info.gpio_num_info);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Failed to read gpios %d", rc);
		goto FREE_SENSOR_DATA;
	}

	s_ctrl->id = soc_info->index;

	/* Validate cell_id */
	if (s_ctrl->id >= MAX_CAMERAS) {
		CAM_ERR(CAM_SENSOR, "Failed invalid cell_id %d", s_ctrl->id);
		rc = -EINVAL;
		goto FREE_SENSOR_DATA;
	}

	/* Store the index of BoB regulator if it is available */
	for (i = 0; i < soc_info->num_rgltr; i++) {
		if (!strcmp(soc_info->rgltr_name[i],
			"cam_bob")) {
			CAM_DBG(CAM_SENSOR,
				"i: %d cam_bob", i);
			s_ctrl->bob_reg_index = i;
			soc_info->rgltr[i] = devm_regulator_get(soc_info->dev,
				soc_info->rgltr_name[i]);
			if (IS_ERR_OR_NULL(soc_info->rgltr[i])) {
				CAM_WARN(CAM_SENSOR,
					"Regulator: %s get failed",
					soc_info->rgltr_name[i]);
				soc_info->rgltr[i] = NULL;
			} else {
				if (!of_property_read_bool(of_node,
					"pwm-switch")) {
					CAM_DBG(CAM_SENSOR,
					"No BoB PWM switch param defined");
					s_ctrl->bob_pwm_switch = false;
				} else {
					s_ctrl->bob_pwm_switch = true;
				}
			}
		}
	}

	/* Read subdev info */
	rc = cam_sensor_get_sub_module_index(of_node, sensordata);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "failed to get sub module index, rc=%d",
			 rc);
		goto FREE_SENSOR_DATA;
	}

	if (s_ctrl->io_master_info.master_type == CCI_MASTER) {
		/* Get CCI master */
		rc = of_property_read_u32(of_node, "cci-master",
			&s_ctrl->cci_i2c_master);
		CAM_DBG(CAM_SENSOR, "cci-master %d, rc %d",
			s_ctrl->cci_i2c_master, rc);
		if (rc < 0) {
			/* Set default master 0 */
			s_ctrl->cci_i2c_master = MASTER_0;
			rc = 0;
		}

		of_parent = of_get_parent(of_node);
		if (of_property_read_u32(of_parent, "cell-index",
				&s_ctrl->cci_num) < 0)
			/* Set default master 0 */
			s_ctrl->cci_num = CCI_DEVICE_0;

		CAM_DBG(CAM_SENSOR, "cci-index %d", s_ctrl->cci_num);
	}

	if (of_property_read_u32(of_node, "sensor-position-pitch",
		&sensordata->pos_pitch) < 0) {
		CAM_DBG(CAM_SENSOR, "Invalid sensor position");
		sensordata->pos_pitch = 360;
	}
	if (of_property_read_u32(of_node, "sensor-position-roll",
		&sensordata->pos_roll) < 0) {
		CAM_DBG(CAM_SENSOR, "Invalid sensor position");
		sensordata->pos_roll = 360;
	}
	if (of_property_read_u32(of_node, "sensor-position-yaw",
		&sensordata->pos_yaw) < 0) {
		CAM_DBG(CAM_SENSOR, "Invalid sensor position");
		sensordata->pos_yaw = 360;
	}

#if defined(CONFIG_CAMERA_SYSFS_V2)
	cam_sensor_get_dt_camera_info(s_ctrl, of_node);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "fail, cell-index %d rc %d",
			s_ctrl->id, rc);
	}
#endif

	return rc;

FREE_SENSOR_DATA:
	kfree(sensordata);
	return rc;
}

int32_t msm_sensor_init_default_params(struct cam_sensor_ctrl_t *s_ctrl)
{
	/* Validate input parameters */
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "failed: invalid params s_ctrl %pK",
			s_ctrl);
		return -EINVAL;
	}

	CAM_DBG(CAM_SENSOR,
		"master_type: %d", s_ctrl->io_master_info.master_type);
	/* Initialize cci_client */
	if (s_ctrl->io_master_info.master_type == CCI_MASTER) {
		s_ctrl->io_master_info.cci_client = kzalloc(sizeof(
			struct cam_sensor_cci_client), GFP_KERNEL);
		if (!(s_ctrl->io_master_info.cci_client))
			return -ENOMEM;

		s_ctrl->io_master_info.cci_client->cci_device
			= s_ctrl->cci_num;
	} else if (s_ctrl->io_master_info.master_type == I2C_MASTER) {
		if (!(s_ctrl->io_master_info.client))
			return -EINVAL;
	} else {
		CAM_ERR(CAM_SENSOR,
			"Invalid master / Master type Not supported");
		return -EINVAL;
	}

	return 0;
}

int32_t cam_sensor_parse_dt(struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t i, rc = 0;
	struct cam_hw_soc_info *soc_info = &s_ctrl->soc_info;

	/* Parse dt information and store in sensor control structure */
	rc = cam_sensor_driver_get_dt_data(s_ctrl);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Failed to get dt data rc %d", rc);
		return rc;
	}

	/* Initialize mutex */
	mutex_init(&(s_ctrl->cam_sensor_mutex));

	/* Initialize default parameters */
	for (i = 0; i < soc_info->num_clk; i++) {
		soc_info->clk[i] = devm_clk_get(soc_info->dev,
					soc_info->clk_name[i]);
		if (!soc_info->clk[i]) {
			CAM_ERR(CAM_SENSOR, "get failed for %s",
				 soc_info->clk_name[i]);
			rc = -ENOENT;
			return rc;
		}
	}

	rc = msm_sensor_init_default_params(s_ctrl);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR,
			"failed: msm_sensor_init_default_params rc %d", rc);
		goto FREE_DT_DATA;
	}

	return rc;

FREE_DT_DATA:
	kfree(s_ctrl->sensordata);
	s_ctrl->sensordata = NULL;

	return rc;
}
