
/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/io.h>
#include <linux/file.h>
#include <mach/dma.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <mach/hardware.h>
#include <mach/board.h>
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <mach/arch_misc.h>

#if defined (CONFIG_ARCH_SC8825)
#include <mach/i2c-sprd.h>
#define SENSOR_I2C_ID 1
#elif defined (CONFIG_ARCH_SC8810)
#include <mach/i2c-sc8810.h>
#elif defined (CONFIG_ARCH_SCX35)
#include <mach/i2c-sprd.h>
#include <mach/adi.h>
#define SENSOR_I2C_ID 0
#endif

#include <video/sensor_drv_k.h>
#include "sensor_drv_sprd.h"
#include <linux/sprd_iommu.h>
#include "csi2/csi_api.h"
#include "../sprd_dcam/flash/flash.h"

/* FIXME: Move to camera device platform data later */
/*#if defined(CONFIG_ARCH_SC8825)*/

#if defined(CONFIG_ARCH_SCX35)
#define REGU_NAME_CAMVIO	"vddcamio"
#define SENSOR_CLK		"clk_sensor"

#if defined(CONFIG_MACH_KANAS_W) || defined(CONFIG_MACH_KANAS_TD)
#define REGU_NAME_SUB_CAMDVDD	"vddcamd"
#define REGU_NAME_CAMMOT	"vddcama"
#define REGU_NAME_CAMAVDD	"RT5033_REGULATORLDO1"
#define REGU_NAME_CAMDVDD	"RT5033_REGULATORDCDC1"
#else
#define REGU_NAME_SUB_CAMDVDD	"vddcamd"
//#define GPIO_SUB_SENSOR_RESET	GPIO_SENSOR_RESET
#define REGU_NAME_CAMAVDD	"vddcama"
#define REGU_NAME_CAMDVDD	"vddcamd"
#define REGU_NAME_CAMMOT	"vddcammot"
#endif

#else // defined(CONFIG_ARCH_SCX35)

#define REGU_NAME_CAMAVDD    "vddcama"
#define REGU_NAME_CAMVIO     "vddcamio"
#define REGU_NAME_CAMDVDD    "vddcamcore"
#define REGU_NAME_CAMMOT     "vddcammot"
#define SENSOR_CLK           "ccir_mclk"

#endif // defined(CONFIG_ARCH_SCX35)

/*#endif*/

#define DEBUG_SENSOR_DRV
#ifdef  DEBUG_SENSOR_DRV
#define SENSOR_PRINT		pr_debug
#else
#define SENSOR_PRINT(...)
#endif
#define SENSOR_PRINT_ERR	printk
#define SENSOR_PRINT_HIGH	printk

#define SENSOR_K_SUCCESS	0
#define SENSOR_K_FAIL		(-1)
#define SENSOR_K_FALSE		0
#define SENSOR_K_TRUE		1

#define _pard(a)                          __raw_readl(a)
#define REG_RD(a)                         __raw_readl(a)
#define REG_WR(a,v)                       __raw_writel(v,a)
#define REG_AWR(a,v)                      __raw_writel((__raw_readl(a) & v), a)
#define REG_OWR(a,v)                      __raw_writel((__raw_readl(a) | v), a)
#define REG_XWR(a,v)                      __raw_writel((__raw_readl(a) ^ v), a)
#define REG_MWR(a,m,v)                                 \
	do {                                            \
		uint32_t _tmp = __raw_readl(a);          \
		_tmp &= ~(m);                            \
		__raw_writel(_tmp | ((m) & (v)), (a));   \
	}while(0)

#define BIT_0	0x01
#define BIT_1	0x02
#define BIT_2	0x04
#define BIT_3	0x08
#define BIT_4	0x10
#define BIT_5	0x20
#define BIT_6	0x40
#define BIT_7	0x80
#define BIT_8	0x0100
#define BIT_9	0x0200
#define BIT_10	0x0400
#define BIT_11	0x0800
#define BIT_12	0x1000
#define BIT_13	0x2000
#define BIT_14	0x4000
#define BIT_15	0x8000
#define BIT_16	0x010000
#define BIT_17	0x020000
#define BIT_18	0x040000
#define BIT_19	0x080000
#define BIT_20	0x100000
#define BIT_21	0x200000
#define BIT_22	0x400000
#define BIT_23	0x800000
#define BIT_24	0x01000000
#define BIT_25	0x02000000
#define BIT_26	0x04000000
#define BIT_27	0x08000000
#define BIT_28	0x10000000
#define BIT_29	0x20000000
#define BIT_30	0x40000000
#define BIT_31	0x80000000

#define LOCAL static
//#define LOCAL

#define PNULL                             ((void *)0)

#define NUMBER_MAX                        0x7FFFFFF

#define SENSOR_MINOR                      MISC_DYNAMIC_MINOR
#define SLEEP_MS(ms)                      msleep(ms)

#define SENSOR_I2C_OP_TRY_NUM	4
#define SENSOR_CMD_BITS_8	1
#define SENSOR_CMD_BITS_16	2
#define SENSOR_I2C_VAL_8BIT	0x00
#define SENSOR_I2C_VAL_16BIT	0x01
#define SENSOR_I2C_REG_8BIT	(0x00 << 1)
#define SENSOR_I2C_REG_16BIT	(0x01 << 1)
#define SENSOR_I2C_CUSTOM	(0x01 << 2)
#define SENSOR_LOW_EIGHT_BIT	0xff

#define SENSOR_WRITE_DELAY	0xffff
#define DEBUG_STR	"Error L %d, %s \n"
#define DEBUG_ARGS	__LINE__,__FUNCTION__
#define SENSOR_MCLK_SRC_NUM	4
#define SENSOR_MCLK_DIV_MAX	4
#define ABS(a)	((a) > 0 ? (a) : -(a))
#define SENSOR_LOWEST_ADDR		0x800
#define SENSOR_ADDR_INVALID(addr)	((uint32_t)(addr) < SENSOR_LOWEST_ADDR)

#define SENSOR_CHECK_ZERO(a)                                      \
	do {                                                       \
		if (SENSOR_ADDR_INVALID(a)) {                       \
			printk("SENSOR, zero pointer \n");           \
			printk(DEBUG_STR, DEBUG_ARGS);               \
			return -EFAULT;                              \
		}                                                   \
	} while(0)

#define SENSOR_CHECK_ZERO_VOID(a)                                 \
	do {                                                       \
		if (SENSOR_ADDR_INVALID(a)) {                       \
			printk("SENSOR, zero pointer \n");           \
			printk(DEBUG_STR, DEBUG_ARGS);               \
			return;                                      \
		}                                                   \
	} while(0)

typedef struct SN_MCLK {
	int clock;
	char *src_name;
} SN_MCLK;

typedef enum {
	SENSOR_MAIN = 0,
	SENSOR_SUB,
	SENSOR_ATV = 5,
	SENSOR_ID_MAX
} SENSOR_ID_E;

struct sensor_mem_tag {
	void *buf_ptr;
	size_t size;
};

uint32_t flash_status =0;

struct sensor_module {
	uint32_t	sensor_id;
	uint32_t	sensor_mclk;
	uint32_t	iopower_on_count;
	uint32_t	avddpower_on_count;
	uint32_t	dvddpower_on_count;
	uint32_t	motpower_on_count;
	uint32_t	mipi_on;
	struct mutex	sensor_lock;
	struct clk	*sensor_clk_mm_i;
	struct clk	*ccir_clk;
	struct clk	*ccir_enable_clk;
	struct clk	*mipi_clk;
	struct i2c_client	*cur_i2c_client;
	struct regulator	*camvio_regulator;
	struct regulator	*camavdd_regulator;
	struct regulator	*camdvdd_regulator;
	struct regulator	*cammot_regulator;
	struct i2c_driver	sensor_i2c_driver;
	struct sensor_mem_tag	sensor_mem;
};

LOCAL const SN_MCLK c_sensor_mclk_tab[SENSOR_MCLK_SRC_NUM] = {
	{96, "clk_96m"},
	{77, "clk_76m8"},
	{48, "clk_48m"},
	{26, "ext_26m"}
};

LOCAL const unsigned short c_sensor_main_default_addr_list[] =
	{SENSOR_MAIN_I2C_ADDR, SENSOR_SUB_I2C_ADDR, I2C_CLIENT_END};

LOCAL const struct i2c_device_id c_sensor_device_id[] = {
	{SENSOR_MAIN_I2C_NAME, 0},
	{SENSOR_SUB_I2C_NAME, 1},
	{}
};

LOCAL struct sensor_module * s_p_sensor_mod = PNULL;

SENSOR_PROJECT_FUNC_T s_sensor_project_func = {PNULL};

// Camera Anti-Banding (enum is also defined at SprdCameraHardwareInterface.h)
typedef enum{
	CAM_BANDFILTER_50HZ_AUTO	= 0,
	CAM_BANDFILTER_50HZ_FIXED	= 1,
	CAM_BANDFILTER_60HZ_AUTO	= 2,
	CAM_BANDFILTER_60HZ_FIXED	= 3,
	CAM_BANDFILTER_LIMIT		= 0x7fffffff,
}CAM_BandFilterMode;

int camera_antibanding_val = CAM_BANDFILTER_60HZ_AUTO; // Default
uint16_t VENDOR_ID = 0xFFFF;

inline int camera_antibanding_get(void) { return camera_antibanding_val; }

int32_t _sensor_is_clk_mm_i_eb(uint32_t is_clk_mm_i_eb)
{
	int ret = 0;
	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	if (NULL == s_p_sensor_mod->sensor_clk_mm_i)
	{
		s_p_sensor_mod->sensor_clk_mm_i = clk_get(NULL, "clk_mm_i");
		if (IS_ERR(s_p_sensor_mod->sensor_clk_mm_i))
		{
			printk("sensor_is_clk_mm_i_eb: get fail.\n");
			return -1;
		}
	}

	if (is_clk_mm_i_eb)
	{
		ret = clk_enable(s_p_sensor_mod->sensor_clk_mm_i);
		if (ret)
		{
			printk("sensor_is_clk_mm_i_eb: enable fail.\n");
			return -1;
		}

#if defined(CONFIG_SPRD_IOMMU)
		{
			sprd_iommu_module_enable(IOMMU_MM);
		}
#endif

	}
	else
	{

#if defined(CONFIG_SPRD_IOMMU)
		{
			sprd_iommu_module_disable(IOMMU_MM);
		}
#endif

		clk_disable(s_p_sensor_mod->sensor_clk_mm_i);
		clk_put(s_p_sensor_mod->sensor_clk_mm_i);
		s_p_sensor_mod->sensor_clk_mm_i = NULL;
	}

	return 0;
}

LOCAL void* _sensor_k_kmalloc(size_t size, unsigned flags)
{
	if (SENSOR_ADDR_INVALID(s_p_sensor_mod))
	{
		printk("SENSOR, zero pointer \n");
		printk(DEBUG_STR, DEBUG_ARGS);
		return PNULL;
	}

	if(PNULL == s_p_sensor_mod->sensor_mem.buf_ptr)
	{
		s_p_sensor_mod->sensor_mem.buf_ptr = kmalloc(size, flags);

		if(PNULL != s_p_sensor_mod->sensor_mem.buf_ptr)
		{
			s_p_sensor_mod->sensor_mem.size = size;
		}

		return s_p_sensor_mod->sensor_mem.buf_ptr;

	}
	else if (size <= s_p_sensor_mod->sensor_mem.size)
	{
		return s_p_sensor_mod->sensor_mem.buf_ptr;
	}
	else
	{
		// Realloc memory
		kfree(s_p_sensor_mod->sensor_mem.buf_ptr);
		s_p_sensor_mod->sensor_mem.buf_ptr = PNULL;
		s_p_sensor_mod->sensor_mem.size = 0;
		s_p_sensor_mod->sensor_mem.buf_ptr = kmalloc(size, flags);

		if (PNULL != s_p_sensor_mod->sensor_mem.buf_ptr)
		{
			s_p_sensor_mod->sensor_mem.size = size;
		}

		return s_p_sensor_mod->sensor_mem.buf_ptr;
	}
}

LOCAL void* _sensor_k_kzalloc(size_t size, unsigned flags)
{
	void *ptr = _sensor_k_kmalloc(size, flags);
	if(PNULL != ptr) {
		 memset(ptr, 0, size);
	}
	return ptr;
}

LOCAL void _sensor_k_kfree(void *p)
{
	/* memory will not be free */
	return;
}

LOCAL uint32_t _sensor_K_get_curId(void)
{
	SENSOR_CHECK_ZERO(s_p_sensor_mod);
	return s_p_sensor_mod->sensor_id;
}

LOCAL int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int res = 0;
	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	SENSOR_PRINT_HIGH(KERN_INFO "SENSOR:sensor_probe E\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		SENSOR_PRINT_HIGH(KERN_INFO "SENSOR: %s: func check failed\n", __FUNCTION__);
		res = -ENODEV;
		goto out;
	}

	s_p_sensor_mod->cur_i2c_client = client;

	SENSOR_PRINT_HIGH(KERN_INFO "sensor_probe, addr 0x%x\n", s_p_sensor_mod->cur_i2c_client->addr);

	return 0;
out:
	return res;
}

LOCAL int sensor_remove(struct i2c_client *client)
{
	return 0;
}

LOCAL int sensor_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	SENSOR_PRINT_HIGH("SENSOR_DRV: detect!");
	strcpy(info->type, client->name);
	return 0;
}

LOCAL int _sensor_k_powerdown(uint32_t power_level, uint32_t sensor_id)
{
	SENSOR_PRINT_HIGH("SENSOR : _sensor_k_powerdown : Power down %d\n", power_level);

	switch (sensor_id)
	{
		case SENSOR_MAIN:
		{
			if (0 == power_level) {
				gpio_direction_output(GPIO_MAIN_SENSOR_PWN, 0);
			} else {
				gpio_direction_output(GPIO_MAIN_SENSOR_PWN, 1);
			}
			break;
		}

		case SENSOR_SUB:
		{
			if (0 == power_level) {
				gpio_direction_output(GPIO_SUB_SENSOR_PWN, 0);
			} else {
				gpio_direction_output(GPIO_SUB_SENSOR_PWN, 1);
			}
			break;
		}

		default:
			SENSOR_PRINT_HIGH("SENSOR : _sensor_k_powerdown : Unsupported sensor ID%d\n", sensor_id);
			break;
	}

	return SENSOR_K_SUCCESS;
}

LOCAL void _sensor_regulator_disable(uint32_t *power_on_count, struct regulator * ptr_cam_regulator)
{
	SENSOR_CHECK_ZERO_VOID(s_p_sensor_mod);

	if(*power_on_count > 0)
	{
		regulator_disable(ptr_cam_regulator);
		(*power_on_count)--;
	}

	SENSOR_PRINT("sensor pwr off done: cnt=0x%x, io=%x, av=%x, dv=%x, mo=%x \n", *power_on_count,
		s_p_sensor_mod->iopower_on_count,
		s_p_sensor_mod->avddpower_on_count,
		s_p_sensor_mod->dvddpower_on_count,
		s_p_sensor_mod->motpower_on_count);
}

LOCAL int _sensor_regulator_enable(uint32_t *power_on_count, struct regulator * ptr_cam_regulator)
{
	int err;
	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	err = regulator_enable(ptr_cam_regulator);
	(*power_on_count)++;

	SENSOR_PRINT("sensor pwr on done: cnt=0x%x, io=%x, av=%x, dv=%x, mo=%x \n", *power_on_count,
		s_p_sensor_mod->iopower_on_count,
		s_p_sensor_mod->avddpower_on_count,
		s_p_sensor_mod->dvddpower_on_count,
		s_p_sensor_mod->motpower_on_count);

	return err;
}

LOCAL int _sensor_k_set_voltage_cammot(uint32_t cammot_val)
{
	int err = 0;
	uint32_t volt_value = 0;
	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	SENSOR_PRINT_HIGH("_sensor_k_set_voltage_cammot : sensor set CAMMOT val %d\n", cammot_val);

	if (NULL == s_p_sensor_mod->cammot_regulator) {
		s_p_sensor_mod->cammot_regulator = regulator_get(NULL, REGU_NAME_CAMMOT);
		if (IS_ERR(s_p_sensor_mod->cammot_regulator)) {
			SENSOR_PRINT_ERR("SENSOR:get cammot.fail\n");
			return SENSOR_K_FAIL;
		}
	}

	switch (cammot_val)
	{
		case SENSOR_VDD_2800MV:
			err = regulator_set_voltage(s_p_sensor_mod->cammot_regulator,
						SENSOER_VDD_2800MV,
						SENSOER_VDD_2800MV);
			volt_value = SENSOER_VDD_2800MV;
			if (err)
				SENSOR_PRINT_ERR("SENSOR:set cammot 2.8 fail\n");
			break;

		case SENSOR_VDD_3000MV:
			err = regulator_set_voltage(s_p_sensor_mod->cammot_regulator,
						SENSOER_VDD_3000MV,
						SENSOER_VDD_3000MV);
			volt_value = SENSOER_VDD_3000MV;
			if (err)
				SENSOR_PRINT_ERR("SENSOR:set cammot 3.0 fail\n");
			break;

#if defined (CONFIG_ARCH_SCX35)

		case SENSOR_VDD_3300MV:
			err = regulator_set_voltage(s_p_sensor_mod->cammot_regulator,
						SENSOER_VDD_3300MV,
						SENSOER_VDD_3300MV);
			volt_value = SENSOER_VDD_3300MV;
			if (err)
				SENSOR_PRINT_ERR("SENSOR:set cammot 3.3 fail\n");
			break;

#else

		case SENSOR_VDD_2500MV:
			err = regulator_set_voltage(s_p_sensor_mod->cammot_regulator,
						SENSOER_VDD_2500MV,
						SENSOER_VDD_2500MV);
			volt_value = SENSOER_VDD_2500MV;
			if (err)
				SENSOR_PRINT_ERR("SENSOR:set cammot 2.5 fail\n");
			break;

#endif

		case SENSOR_VDD_1800MV:
			err = regulator_set_voltage(s_p_sensor_mod->cammot_regulator,
						SENSOER_VDD_1800MV,
						SENSOER_VDD_1800MV);
			volt_value = SENSOER_VDD_1800MV;
			if (err)
				SENSOR_PRINT_ERR("SENSOR:set cammot 1.8 fail\n");
			break;

		case SENSOR_VDD_CLOSED:
		case SENSOR_VDD_UNUSED:
		default:
			SENSOR_PRINT_HIGH("_sensor_k_set_voltage_cammot : Unsupported CAMMOT value %d\n", cammot_val);
			volt_value = 0;
			break;
	}

	if (err) {
		SENSOR_PRINT_ERR("SENSOR:set cammot err!.\n");
		return SENSOR_K_FAIL;
	}

	if (0 != volt_value)
	{
		err = _sensor_regulator_enable(&s_p_sensor_mod->motpower_on_count, s_p_sensor_mod->cammot_regulator);
		if (err)
		{
			regulator_put(s_p_sensor_mod->cammot_regulator);
			s_p_sensor_mod->cammot_regulator = NULL;
			SENSOR_PRINT_ERR("SENSOR:can't en cammot.\n");
			return SENSOR_K_FAIL;
		}
	}
	else
	{
		_sensor_regulator_disable(&s_p_sensor_mod->motpower_on_count, s_p_sensor_mod->cammot_regulator);
		regulator_put(s_p_sensor_mod->cammot_regulator);
		s_p_sensor_mod->cammot_regulator = NULL;
		SENSOR_PRINT("SENSOR:dis cammot.\n");
	}

	return SENSOR_K_SUCCESS;
}

LOCAL int _sensor_k_set_voltage_avdd(uint32_t avdd_val)
{
	int err = 0;
	uint32_t volt_value = 0;
	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	SENSOR_PRINT_HIGH("_sensor_k_set_voltage_avdd : sensor set AVDD val %d\n", avdd_val);

	SENSOR_PRINT("SENSOR:dis dvdd.\n");

	if (NULL == s_p_sensor_mod->camavdd_regulator)
	{
		s_p_sensor_mod->camavdd_regulator = regulator_get(NULL, REGU_NAME_CAMAVDD);
		if (IS_ERR(s_p_sensor_mod->camavdd_regulator))
		{
			SENSOR_PRINT_ERR("SENSOR:get avdd.fail\n");
			return SENSOR_K_FAIL;
		}
	}

	switch (avdd_val)
	{
		case SENSOR_VDD_2800MV:
			err = regulator_set_voltage(s_p_sensor_mod->camavdd_regulator,
						SENSOER_VDD_2800MV,
						SENSOER_VDD_2800MV);
			volt_value = SENSOER_VDD_2800MV;
			if (err)
				SENSOR_PRINT_ERR("SENSOR:set avdd to 2.8 fail\n");
			break;

		case SENSOR_VDD_3000MV:
			err = regulator_set_voltage(s_p_sensor_mod->camavdd_regulator,
						SENSOER_VDD_3000MV,
						SENSOER_VDD_3000MV);
			volt_value = SENSOER_VDD_3000MV;
			if (err)
				SENSOR_PRINT_ERR("SENSOR:set avdd to 3.0 fail\n");
			break;

		case SENSOR_VDD_2500MV:
			err = regulator_set_voltage(s_p_sensor_mod->camavdd_regulator,
						SENSOER_VDD_2500MV,
						SENSOER_VDD_2500MV);
			volt_value = SENSOER_VDD_2500MV;
			if (err)
				SENSOR_PRINT_ERR("SENSOR:set avdd to 2.5 fail\n");
			break;

		case SENSOR_VDD_1800MV:
			err = regulator_set_voltage(s_p_sensor_mod->camavdd_regulator,
						SENSOER_VDD_1800MV,
						SENSOER_VDD_1800MV);
			volt_value = SENSOER_VDD_1800MV;
			if (err)
				SENSOR_PRINT_ERR("SENSOR:set avdd to 1.8 fail\n");
			break;

		case SENSOR_VDD_CLOSED:
		case SENSOR_VDD_UNUSED:
		default:
			SENSOR_PRINT_HIGH("_sensor_k_set_voltage_avdd : Unsupported AVDD value %d\n", avdd_val);
			volt_value = 0;
			break;
	}

	if (err) {
		SENSOR_PRINT_ERR("SENSOR:set avdd err!.\n");
		return SENSOR_K_FAIL;
	}

	if (0 != volt_value)
	{
		err = _sensor_regulator_enable(&s_p_sensor_mod->avddpower_on_count, s_p_sensor_mod->camavdd_regulator);
		if (err)
		{
			regulator_put(s_p_sensor_mod->camavdd_regulator);
			s_p_sensor_mod->camavdd_regulator = NULL;
			SENSOR_PRINT_ERR("SENSOR:can't en avdd.\n");
			return SENSOR_K_FAIL;
		}
	}
	else
	{
		_sensor_regulator_disable(&s_p_sensor_mod->avddpower_on_count, s_p_sensor_mod->camavdd_regulator);
		regulator_put(s_p_sensor_mod->camavdd_regulator);
		s_p_sensor_mod->camavdd_regulator = NULL;
		SENSOR_PRINT("SENSOR:dis avdd.\n");
	}

	return SENSOR_K_SUCCESS;
}

LOCAL int _sensor_k_set_voltage_dvdd(uint32_t dvdd_val, uint32_t sensor_id)
{
	int err = 0;
	uint32_t volt_value = 0;
	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	SENSOR_PRINT_HIGH("_sensor_k_set_voltage_dvdd : Sensor set DVDD val = %d, Sensor ID = %d\n", dvdd_val, sensor_id);

	// if (NULL == s_p_sensor_mod->camdvdd_regulator) {
	switch (sensor_id)
	{
		case SENSOR_MAIN:
		{
			s_p_sensor_mod->camdvdd_regulator = regulator_get(NULL, REGU_NAME_CAMDVDD);
			break;
		}

		case SENSOR_SUB:
		{
			s_p_sensor_mod->camdvdd_regulator = regulator_get(NULL, REGU_NAME_SUB_CAMDVDD);
			break;
		}

		default:
		{
			s_p_sensor_mod->camdvdd_regulator = regulator_get(NULL, REGU_NAME_CAMDVDD);
			break;
		}
	}

	if (IS_ERR(s_p_sensor_mod->camdvdd_regulator))
	{
		SENSOR_PRINT_ERR("_sensor_k_set_voltage_dvdd : get DVDD Fail\n");
		return SENSOR_K_FAIL;
	}
	//}

	switch (dvdd_val)
	{

#if defined (CONFIG_ARCH_SCX35)
		case SENSOR_VDD_1200MV:
		{
			err = regulator_set_voltage(s_p_sensor_mod->camdvdd_regulator,
						SENSOER_VDD_1200MV,
						SENSOER_VDD_1200MV);
			volt_value = SENSOER_VDD_1200MV;
			if (err)
				SENSOR_PRINT_ERR("_sensor_k_set_voltage_dvdd : Set DVDD to 1.2 fail\n");
			break;
		}

#else // defined (CONFIG_ARCH_SCX35)

		case SENSOR_VDD_2800MV:
		{
			err = regulator_set_voltage(s_p_sensor_mod->camdvdd_regulator,
						SENSOER_VDD_2800MV,
						SENSOER_VDD_2800MV);
			volt_value = SENSOER_VDD_2800MV;
			if (err)
				SENSOR_PRINT_ERR("_sensor_k_set_voltage_dvdd : Set DVDD to 2.8 fail\n");
			break;
		}
#endif // defined (CONFIG_ARCH_SCX35)

		case SENSOR_VDD_1800MV:
		{
			err = regulator_set_voltage(s_p_sensor_mod->camdvdd_regulator,
						SENSOER_VDD_1800MV,
						SENSOER_VDD_1800MV);
			volt_value = SENSOER_VDD_1800MV;
			if (err)
				SENSOR_PRINT_ERR("_sensor_k_set_voltage_dvdd : Set DVDD to 1.8 fail\n");
			break;
		}

		case SENSOR_VDD_1500MV:
		{
			err = regulator_set_voltage(s_p_sensor_mod->camdvdd_regulator,
						SENSOER_VDD_1500MV,
						SENSOER_VDD_1500MV);
			volt_value = SENSOER_VDD_1500MV;
			if (err)
					SENSOR_PRINT_ERR("_sensor_k_set_voltage_dvdd : Set DVDD to 1.5 fail\n");
			break;
		}

		case SENSOR_VDD_1300MV:
		{
			err = regulator_set_voltage(s_p_sensor_mod->camdvdd_regulator,
						SENSOER_VDD_1300MV,
						SENSOER_VDD_1300MV);
			volt_value = SENSOER_VDD_1300MV;
			if (err)
				SENSOR_PRINT_ERR("_sensor_k_set_voltage_dvdd : Set DVDD to 1.3 fail\n");
			break;
		}

		case SENSOR_VDD_CLOSED: // Intentionally unuse break
		case SENSOR_VDD_UNUSED:
		default:
			SENSOR_PRINT_HIGH("_sensor_k_set_voltage_dvdd : Unsupported Voltage (%d)\n", dvdd_val);
			volt_value = 0;
			break;
	}

	if (err) {
		SENSOR_PRINT_ERR("_sensor_k_set_voltage_dvdd : Set DVDD Error %d\n", err);
		return SENSOR_K_FAIL;
	}

	if (0 != volt_value)
	{
		err = _sensor_regulator_enable(&s_p_sensor_mod->dvddpower_on_count,  s_p_sensor_mod->camdvdd_regulator);
		if (err)
		{
			regulator_put(s_p_sensor_mod->camdvdd_regulator);
			s_p_sensor_mod->camdvdd_regulator = NULL;
			SENSOR_PRINT_ERR("_sensor_k_set_voltage_dvdd : Can't enable DVDD\n");
			return SENSOR_K_FAIL;
		}
	}
	else
	{
		_sensor_regulator_disable(&s_p_sensor_mod->dvddpower_on_count,  s_p_sensor_mod->camdvdd_regulator);
		regulator_put(s_p_sensor_mod->camdvdd_regulator);
		s_p_sensor_mod->camdvdd_regulator = NULL;
		SENSOR_PRINT("_sensor_k_set_voltage_dvdd : Disable DVDD\n");
	}

	return SENSOR_K_SUCCESS;
}

LOCAL int _sensor_k_set_voltage_iovdd(uint32_t iodd_val)
{
	int err = 0;
	uint32_t volt_value = 0;
	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	SENSOR_PRINT_HIGH("_sensor_k_set_voltage_iovdd : sensor set IOVDD val %d\n", iodd_val);

	if(NULL == s_p_sensor_mod->camvio_regulator)
	{
		s_p_sensor_mod->camvio_regulator = regulator_get(NULL, REGU_NAME_CAMVIO);
		if (IS_ERR(s_p_sensor_mod->camvio_regulator))
		{
			SENSOR_PRINT_ERR("SENSOR:get camvio.fail\n");
			return SENSOR_K_FAIL;
		}
	}

	switch (iodd_val)
	{
		case SENSOR_VDD_2800MV:
			err = regulator_set_voltage(s_p_sensor_mod->camvio_regulator,
						SENSOER_VDD_2800MV,
						SENSOER_VDD_2800MV);
			volt_value = SENSOER_VDD_2800MV;
			if (err)
				SENSOR_PRINT_ERR("SENSOR:set camvio to 2.8 fail\n");
			break;

#if defined (CONFIG_ARCH_SCX35)
		case SENSOR_VDD_2500MV:
			err = regulator_set_voltage(s_p_sensor_mod->camvio_regulator,
						SENSOER_VDD_2500MV,
						SENSOER_VDD_2500MV);
			volt_value = SENSOER_VDD_2500MV;
			if (err)
				SENSOR_PRINT_ERR("SENSOR:set camvio to 2.5 fail\n");
			break;

		case SENSOR_VDD_1500MV:
			err = regulator_set_voltage(s_p_sensor_mod->camvio_regulator,
						SENSOER_VDD_1500MV,
						SENSOER_VDD_1500MV);
			volt_value = SENSOER_VDD_1500MV;
			if (err)
				SENSOR_PRINT_ERR("SENSOR:set camvio to 1.5 fail\n");
			break;

#else // defined (CONFIG_ARCH_SCX35)

		case SENSOR_VDD_3800MV:
			err = regulator_set_voltage(s_p_sensor_mod->camvio_regulator,
						SENSOER_VDD_3800MV,
						SENSOER_VDD_3800MV);
			volt_value = SENSOER_VDD_3800MV;
			if (err)
				SENSOR_PRINT_ERR("SENSOR:set camvio to 3.8 fail\n");
			break;

		case SENSOR_VDD_1200MV:
			err = regulator_set_voltage(s_p_sensor_mod->camvio_regulator,
						  SENSOER_VDD_1200MV,
						  SENSOER_VDD_1200MV);
			volt_value = SENSOER_VDD_1200MV;
			if (err)
				SENSOR_PRINT_ERR("SENSOR:set camvio to 1.2 fail\n");
			break;

#endif // defined (CONFIG_ARCH_SCX35)

		case SENSOR_VDD_1800MV:
			err = regulator_set_voltage(s_p_sensor_mod->camvio_regulator,
						SENSOER_VDD_1800MV,
						SENSOER_VDD_1800MV);
			volt_value = SENSOER_VDD_1800MV;
			if (err)
				SENSOR_PRINT_ERR("SENSOR:set camvio to 1.8 fail\n");
			break;

		case SENSOR_VDD_CLOSED:
		case SENSOR_VDD_UNUSED:
		default :
			SENSOR_PRINT_HIGH("_sensor_k_set_voltage_iovdd : Unsupported IOVDD val %d\n", iodd_val);
			volt_value = 0;
			break;
	}

	if (err)
	{
		SENSOR_PRINT_ERR("SENSOR:set camvio err!.\n");
		return SENSOR_K_FAIL;
	}

	if (0 != volt_value)
	{
		err = _sensor_regulator_enable(&s_p_sensor_mod->iopower_on_count, s_p_sensor_mod->camvio_regulator);
		if (err)
		{
			regulator_put(s_p_sensor_mod->camvio_regulator);
			s_p_sensor_mod->camvio_regulator = NULL;
			SENSOR_PRINT_ERR("SENSOR:can't en camvio.\n");
			return SENSOR_K_FAIL;
		}
	}
	else
	{
		_sensor_regulator_disable(&s_p_sensor_mod->iopower_on_count, s_p_sensor_mod->camvio_regulator);
		regulator_put(s_p_sensor_mod->camvio_regulator);
		s_p_sensor_mod->camvio_regulator = NULL;
		SENSOR_PRINT("SENSOR:dis camvio.\n");
	}

	return SENSOR_K_SUCCESS;
}

LOCAL int _select_sensor_mclk(uint8_t clk_set, char **clk_src_name, uint8_t * clk_div)
{
	int clk_tmp = NUMBER_MAX;
	int src_delta = NUMBER_MAX;
	int src_delta_min = NUMBER_MAX;
	int div_delta_min = NUMBER_MAX;
	uint8_t i = 0;
	uint8_t j = 0;
	uint8_t mark_src = 0;
	uint8_t mark_div = 0;
	uint8_t mark_src_tmp = 0;

	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	SENSOR_PRINT("SENSOR sel mclk %d\n", clk_set);

	if (clk_set > 96 || !clk_src_name || !clk_div) {
		return SENSOR_K_FAIL;
	}

	for (i = 0; i < SENSOR_MCLK_DIV_MAX; i++)
	{
		clk_tmp = (int)(clk_set * (i + 1));
		src_delta_min = NUMBER_MAX;

		for (j = 0 ; j < SENSOR_MCLK_SRC_NUM ; j++)
		{
			src_delta = ABS(c_sensor_mclk_tab[j].clock - clk_tmp);

			if (src_delta < src_delta_min)
			{
				src_delta_min = src_delta;
				mark_src_tmp = j;
			}
		}

		if (src_delta_min < div_delta_min)
		{
			div_delta_min = src_delta_min;
			mark_src = mark_src_tmp;
			mark_div = i;
		}
	}

	SENSOR_PRINT("src %d, div=%d\n", mark_src, mark_div);

	*clk_src_name = c_sensor_mclk_tab[mark_src].src_name;
	*clk_div = mark_div + 1;

	return SENSOR_K_SUCCESS;
}

int32_t _sensor_k_mipi_clk_en(void)
{
	int ret = 0;
	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	if (NULL == s_p_sensor_mod->mipi_clk)
	{
		s_p_sensor_mod->mipi_clk = clk_get(NULL, "clk_dcam_mipi");
	}

	if (IS_ERR(s_p_sensor_mod->mipi_clk))
	{
		printk("SENSOR: get dcam mipi clk error \n");
		return -1;
	}
	else
	{
		ret = clk_enable(s_p_sensor_mod->mipi_clk);
		if (ret)
		{
			printk("SENSOR: enable dcam mipi clk error %d \n", ret);
			return -1;
		}
	}
	return ret;
}

int32_t _sensor_k_mipi_clk_dis(void)
{
	SENSOR_CHECK_ZERO(s_p_sensor_mod);
	if (s_p_sensor_mod->mipi_clk)
	{
		clk_disable(s_p_sensor_mod->mipi_clk);
		clk_put(s_p_sensor_mod->mipi_clk);
		s_p_sensor_mod->mipi_clk = NULL;
	}

	return 0;
}
LOCAL int _sensor_k_set_mclk(uint32_t mclk)
{
	struct clk *clk_parent = NULL;
	int ret;
	char *clk_src_name = NULL;
	uint8_t clk_div;
	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	SENSOR_PRINT_HIGH("SENSOR: set mclk org = %d, clk = %d\n", s_p_sensor_mod->sensor_mclk, mclk);

	if ((0 != mclk) && (s_p_sensor_mod->sensor_mclk != mclk))
	{
		if (s_p_sensor_mod->ccir_clk)
		{
			clk_disable(s_p_sensor_mod->ccir_clk);
			SENSOR_PRINT("###sensor ccir clk off ok.\n");
		}
		else
		{
			s_p_sensor_mod->ccir_clk = clk_get(NULL, SENSOR_CLK);
			if (IS_ERR(s_p_sensor_mod->ccir_clk))
			{
				SENSOR_PRINT_ERR("###: Failed: Can't get clock [ccir_mclk]!\n");
				SENSOR_PRINT_ERR("###: s_sensor_clk = %p.\n",s_p_sensor_mod->ccir_clk);
			}
			else
			{
				SENSOR_PRINT("###sensor ccir clk get ok.\n");
			}
		}

		if (mclk > SENSOR_MAX_MCLK)
		{
			mclk = SENSOR_MAX_MCLK;
		}

		if (SENSOR_K_SUCCESS != _select_sensor_mclk((uint8_t) mclk, &clk_src_name, &clk_div))
		{
			SENSOR_PRINT_ERR("SENSOR:Sensor_SetMCLK select clock source fail.\n");
			return -EINVAL;
		}

		SENSOR_PRINT("clk_src_name=%s, clk_div=%d \n", clk_src_name, clk_div);

		clk_parent = clk_get(NULL, clk_src_name);

		if (!clk_parent)
		{
			SENSOR_PRINT_ERR("###:clock: failed to get clock [%s] by clk_get()!\n", clk_src_name);
			return -EINVAL;
		}

		SENSOR_PRINT("clk_get clk_src_name=%s done\n", clk_src_name);

		ret = clk_set_parent(s_p_sensor_mod->ccir_clk, clk_parent);
		if (ret)
		{
			SENSOR_PRINT_ERR("###:clock: clk_set_parent() failed!parent \n");
			return -EINVAL;
		}

		SENSOR_PRINT("clk_set_parent s_ccir_clk=%s done\n", (char *)(s_p_sensor_mod->ccir_clk));

		ret = clk_set_rate(s_p_sensor_mod->ccir_clk, (mclk * SENOR_CLK_M_VALUE));
		if (ret)
		{
			SENSOR_PRINT_ERR("###:clock: clk_set_rate failed!\n");
			return -EINVAL;
		}

		SENSOR_PRINT("clk_set_rate s_ccir_clk=%s done\n", (char *)(s_p_sensor_mod->ccir_clk));

		ret = clk_enable(s_p_sensor_mod->ccir_clk);
		if (ret)
		{
			SENSOR_PRINT_ERR("###:clock: clk_enable() failed!\n");
		}
		else
		{
			SENSOR_PRINT("######ccir enable clk ok\n");
		}

		if (NULL == s_p_sensor_mod->ccir_enable_clk)
		{
			s_p_sensor_mod->ccir_enable_clk = clk_get(NULL, "clk_ccir");
			if (IS_ERR(s_p_sensor_mod->ccir_enable_clk))
			{
				SENSOR_PRINT_ERR("###: Failed: Can't get clock [clk_ccir]!\n");
				SENSOR_PRINT_ERR("###: ccir_enable_clk = %p.\n", s_p_sensor_mod->ccir_enable_clk);
				return -EINVAL;
			}
			else
			{
				SENSOR_PRINT("###sensor ccir_enable_clk clk_get ok.\n");
			}

			ret = clk_enable(s_p_sensor_mod->ccir_enable_clk);
			if (ret)
			{
				SENSOR_PRINT_ERR("###:clock: clk_enable() failed!\n");
			}
			else
			{
				SENSOR_PRINT("###ccir enable clk ok\n");
			}
		}

		s_p_sensor_mod->sensor_mclk = mclk;
		SENSOR_PRINT("SENSOR: set mclk %d Hz.\n", s_p_sensor_mod->sensor_mclk);
	}
	else if (0 == mclk)
	{
		if (s_p_sensor_mod->ccir_clk)
		{
			clk_disable(s_p_sensor_mod->ccir_clk);
			SENSOR_PRINT("###sensor clk disable ok.\n");
			clk_put(s_p_sensor_mod->ccir_clk);
			SENSOR_PRINT("###sensor clk put ok.\n");
			s_p_sensor_mod->ccir_clk = NULL;
		}

		if (s_p_sensor_mod->ccir_enable_clk)
		{
			clk_disable(s_p_sensor_mod->ccir_enable_clk);
			SENSOR_PRINT("###sensor clk disable ok.\n");
			clk_put(s_p_sensor_mod->ccir_enable_clk);
			SENSOR_PRINT("###sensor clk put ok.\n");
			s_p_sensor_mod->ccir_enable_clk = NULL;
		}
		s_p_sensor_mod->sensor_mclk = 0;
		SENSOR_PRINT("SENSOR: Disable MCLK !!!");
	}
	else
	{
		SENSOR_PRINT("SENSOR: Do nothing !! ");
	}
	SENSOR_PRINT_HIGH("SENSOR: set mclk X\n");

	return 0;
}

LOCAL int _sensor_k_reset(uint32_t level, uint32_t width)
{
	SENSOR_PRINT("SENSOR:_sensor_k_reset, reset_val=%d  camera:%d (0:main 1:sub)\n",level, _sensor_K_get_curId());

	switch (_sensor_K_get_curId())
	{
		case SENSOR_MAIN:
		{
			gpio_direction_output(GPIO_SENSOR_RESET, level);
			gpio_set_value(GPIO_SENSOR_RESET, level);
			SLEEP_MS(width);
			gpio_set_value(GPIO_SENSOR_RESET, !level);
			mdelay(1);
			break;
		}

		case SENSOR_SUB:
		{
			gpio_direction_output(GPIO_SUB_SENSOR_RESET, level);
			gpio_set_value(GPIO_SUB_SENSOR_RESET, level);
			SLEEP_MS(width);
			gpio_set_value(GPIO_SUB_SENSOR_RESET, !level);
			mdelay(1);
			break;
		}

		default:
			SENSOR_PRINT("SENSOR : _sensor_k_reset : Unsupported Current ID\n");
			break;
	}

	return SENSOR_K_SUCCESS;
}

LOCAL int _sensor_k_i2c_init(uint32_t sensor_id)
{
	SENSOR_CHECK_ZERO(s_p_sensor_mod);
	s_p_sensor_mod->sensor_id = sensor_id;
	return SENSOR_K_SUCCESS;
}

LOCAL int _sensor_k_i2c_deInit(uint32_t sensor_id)
{
	SENSOR_CHECK_ZERO(s_p_sensor_mod);
	// s_p_sensor_mod->sensor_id = SENSOR_ID_MAX;
	SENSOR_PRINT_HIGH("-I2C %d OK.\n", sensor_id);
	return SENSOR_K_SUCCESS;
}

LOCAL int _sensor_k_set_rst_level(uint32_t plus_level, uint32_t sensor_id)
{
	SENSOR_PRINT("_sensor_k_set_rst_level : lvl: lvl %d, rst pin %d \n", plus_level, GPIO_SENSOR_RESET);

	switch (sensor_id)
	{
		case SENSOR_MAIN:
		{
			gpio_direction_output(GPIO_SENSOR_RESET, plus_level);
			gpio_set_value(GPIO_SENSOR_RESET, plus_level);
			break;
		}

		case SENSOR_SUB:
		{
			gpio_direction_output(GPIO_SUB_SENSOR_RESET, plus_level);
			gpio_set_value(GPIO_SUB_SENSOR_RESET, plus_level);
			break;
		}

		default:
			SENSOR_PRINT("_sensor_k_set_rst_level : Unsupported sensor ID (%d)\n", sensor_id);
			break;
	}

	return SENSOR_K_SUCCESS;
}

LOCAL int _s5k43_power(uint32_t powerOn)
{
	if(powerOn)
	{
		SENSOR_PRINT_HIGH("_s5k43_power : Power On\n");

		_sensor_k_powerdown(SENSOR_LOW_LEVEL_PWDN, SENSOR_MAIN); // 5M Stby Off
		_sensor_k_set_rst_level(SENSOR_LOW_PULSE_RESET, SENSOR_MAIN); // 5M Reset Off

		SLEEP_MS(1);

		// Sensor AVDD : 2.8V On
		_sensor_k_set_voltage_avdd(SENSOR_VDD_2800MV);

		SLEEP_MS(4);

		// VT Core : 1.8V On
		_sensor_k_set_voltage_dvdd(SENSOR_AVDD_1800MV, SENSOR_SUB);

		SLEEP_MS(1);

		// Sensor I/O : 1.8V On
		_sensor_k_set_voltage_iovdd(SENSOR_AVDD_1800MV);

		// AF : 2.8V On
		_sensor_k_set_voltage_cammot(SENSOR_AVDD_2800MV);

		SLEEP_MS(1);

		// VT STBY Enable
		_sensor_k_powerdown(SENSOR_HIGH_LEVEL_PWDN, SENSOR_SUB);

		SLEEP_MS(1);

		// Mclk Enable
		_sensor_k_set_mclk(SENSOR_DEFALUT_MCLK);

		SLEEP_MS(4);

		// VT Reset Enable
		_sensor_k_set_rst_level(SENSOR_HIGH_PULSE_RESET, SENSOR_SUB);

		SLEEP_MS(2);

		// VT STBY Disable
		_sensor_k_powerdown(SENSOR_LOW_LEVEL_PWDN, SENSOR_SUB);

		SLEEP_MS(1);

		// 5M Core : 1.2V On
		_sensor_k_set_voltage_dvdd(SENSOR_AVDD_1200MV, SENSOR_MAIN);

		SLEEP_MS(1);

		// 5M Stby Enable
		_sensor_k_powerdown(!SENSOR_LOW_LEVEL_PWDN, SENSOR_MAIN);

		SLEEP_MS(1);

		// 5M Reset Enable
		_sensor_k_set_rst_level(!SENSOR_LOW_PULSE_RESET, SENSOR_MAIN);

		SLEEP_MS(5);

		SENSOR_PRINT_HIGH("_s5k43_power : Power On End\n");
	}
	else
	{
		SENSOR_PRINT_HIGH("_s5k43_power : Power Off\n");

		// 5M Reset Disable
		_sensor_k_set_rst_level(SENSOR_LOW_PULSE_RESET, SENSOR_MAIN);

		SLEEP_MS(1);

		// Mclk Disable
		_sensor_k_set_mclk(SENSOR_DISABLE_MCLK);

		SLEEP_MS(5);

		// 5M Stby Disable
		_sensor_k_powerdown(SENSOR_LOW_LEVEL_PWDN, SENSOR_MAIN);

		SLEEP_MS(1);

		// VT Reset Disable
		_sensor_k_set_rst_level(SENSOR_LOW_PULSE_RESET, SENSOR_SUB);

		SLEEP_MS(1);

		// 5M Core : 1.2V Off
		_sensor_k_set_voltage_dvdd(SENSOR_AVDD_CLOSED, SENSOR_MAIN);

		SLEEP_MS(1);

		// Sensor IO : 1.8V Off
		_sensor_k_set_voltage_iovdd(SENSOR_AVDD_CLOSED);

		SLEEP_MS(1);

		// VT Core : 1.8V Off
		_sensor_k_set_voltage_dvdd(SENSOR_AVDD_CLOSED, SENSOR_SUB);

		SLEEP_MS(1);

		// Sensor AVDD : 2.8V Off
		_sensor_k_set_voltage_avdd(SENSOR_AVDD_CLOSED);

		SLEEP_MS(1);

		// AF : 2.8V Off
		_sensor_k_set_voltage_cammot(SENSOR_AVDD_CLOSED);

		SLEEP_MS(1);

		SENSOR_PRINT_HIGH("_s5k43_power : Power Off End\n");
	}

	return 0;
}

LOCAL int _hi702_power(uint32_t powerOn)
{
	if(powerOn)
	{
		SENSOR_PRINT_HIGH("_hi702_power : Power On\n");

		// Sensor AVDD : 2.8VOn
		_sensor_k_set_voltage_avdd(SENSOR_AVDD_2800MV);

		SLEEP_MS(4);

		// VT Core : 1.8V On
		_sensor_k_set_voltage_dvdd(SENSOR_AVDD_1800MV, SENSOR_SUB);

		SLEEP_MS(1);

		// Sensor I/O : 1.8V On
		_sensor_k_set_voltage_iovdd(SENSOR_AVDD_1800MV);

		SLEEP_MS(1);

		// 5M Core : 1.2V On
		_sensor_k_set_voltage_dvdd(SENSOR_AVDD_1200MV, SENSOR_MAIN);

		SLEEP_MS(5);

		// 5M Core : 1.2V Off
		_sensor_k_set_voltage_dvdd(SENSOR_AVDD_CLOSED, SENSOR_MAIN);

		SLEEP_MS(3);

		// VT STBY Enable
		_sensor_k_powerdown(!SENSOR_LOW_LEVEL_PWDN, SENSOR_SUB);

		SLEEP_MS(2);

		// MCLK Enable
		_sensor_k_set_mclk(SENSOR_DEFALUT_MCLK);


		SLEEP_MS(5);

		// VT Reset Enable
		_sensor_k_set_rst_level(!SENSOR_LOW_PULSE_RESET, SENSOR_SUB);

		SLEEP_MS(60);

		SENSOR_PRINT_HIGH("_hi702_power : Power On End\n");
	}
	else
	{
		SENSOR_PRINT_HIGH("_hi702_power : Power Off\n");

		// VT Reset Disable
		_sensor_k_set_rst_level(SENSOR_LOW_PULSE_RESET, SENSOR_SUB);

		SLEEP_MS(2);

		// VT STBY Disable
		_sensor_k_powerdown(SENSOR_LOW_LEVEL_PWDN, SENSOR_SUB);

		SLEEP_MS(2);

		// MCLK Disable
		_sensor_k_set_mclk(SENSOR_DISABLE_MCLK);

		SLEEP_MS(2);

		// Sensor I/O : 1.8V Off
		_sensor_k_set_voltage_iovdd(SENSOR_AVDD_CLOSED);

		SLEEP_MS(1);

		// VT Core : 1.8V Off
		 _sensor_k_set_voltage_dvdd(SENSOR_AVDD_CLOSED, SENSOR_SUB);

		SLEEP_MS(1);

		// Sensor AVDD : 2.8V Off
		_sensor_k_set_voltage_avdd(SENSOR_AVDD_CLOSED);

		SLEEP_MS(1);

		SENSOR_PRINT_HIGH("_hi702_power : Power Off End\n");
	}

	return 0;
}

LOCAL int _Sensor_K_ReadReg(SENSOR_REG_BITS_T_PTR pReg)
{
	struct i2c_msg msg_r[2];
	int i;
	int32_t ret = SENSOR_K_SUCCESS;
	uint16_t w_cmd_num = 0;
	uint16_t r_cmd_num = 0;
	uint16_t reg_addr;
	uint8_t buf_r[2] = { 0 };
	uint8_t cmd[2] = { 0 };

	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	reg_addr = pReg->reg_addr;

	if (SENSOR_I2C_REG_16BIT == (pReg->reg_bits & SENSOR_I2C_REG_16BIT))
	{
		cmd[w_cmd_num++] = (uint8_t) ((reg_addr >> 8) & SENSOR_LOW_EIGHT_BIT);
		cmd[w_cmd_num++] = (uint8_t) (reg_addr & SENSOR_LOW_EIGHT_BIT);
	}
	else
	{
		cmd[w_cmd_num++] = (uint8_t) reg_addr;
	}

	if (SENSOR_I2C_VAL_16BIT == (pReg->reg_bits & SENSOR_I2C_VAL_16BIT))
	{
		r_cmd_num = SENSOR_CMD_BITS_16;
	}
	else
	{
		r_cmd_num = SENSOR_CMD_BITS_8;
	}

	for ( i = 0 ; i < SENSOR_I2C_OP_TRY_NUM ; i++)
	{
		msg_r[0].addr = s_p_sensor_mod->cur_i2c_client->addr;
		msg_r[0].flags = 0;
		msg_r[0].buf = cmd;
		msg_r[0].len = w_cmd_num;
		msg_r[1].addr = s_p_sensor_mod->cur_i2c_client->addr;
		msg_r[1].flags = I2C_M_RD;
		msg_r[1].buf = buf_r;
		msg_r[1].len = r_cmd_num;
		ret = i2c_transfer(s_p_sensor_mod->cur_i2c_client->adapter, msg_r, 2);

		if (ret != 2)
		{
			SENSOR_PRINT_ERR("SENSOR : read reg fail, ret %d, addr 0x%x, reg_addr 0x%x\n", ret, s_p_sensor_mod->cur_i2c_client->addr,reg_addr);
			SLEEP_MS(20);
			ret = SENSOR_K_FAIL;
		}
		else
		{
			pReg->reg_value = (r_cmd_num == 1) ? (uint16_t) buf_r[0] : (uint16_t) ((buf_r[0] << 8) + buf_r[1]);
			ret = SENSOR_K_SUCCESS;
			break;
		}
	}

	return ret;
}

LOCAL int _Sensor_K_WriteReg(SENSOR_REG_BITS_T_PTR pReg)
{
	struct i2c_msg msg_w;
	int i;
	int32_t ret = SENSOR_K_SUCCESS;
	uint32_t index = 0;
	uint32_t cmd_num = 0;
	uint16_t subaddr;
	uint16_t data;
	uint8_t cmd[4] = { 0 };

	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	subaddr = pReg->reg_addr;
	data = pReg->reg_value;

	if (SENSOR_I2C_REG_16BIT ==(pReg->reg_bits & SENSOR_I2C_REG_16BIT))
	{
		cmd[cmd_num++] = (uint8_t) ((subaddr >> 8) & SENSOR_LOW_EIGHT_BIT);
		index++;
		cmd[cmd_num++] =  (uint8_t) (subaddr & SENSOR_LOW_EIGHT_BIT);
		index++;
	}
	else
	{
		cmd[cmd_num++] = (uint8_t) subaddr;
		index++;
	}

	if (SENSOR_I2C_VAL_16BIT == (pReg->reg_bits & SENSOR_I2C_VAL_16BIT))
	{
		cmd[cmd_num++] = (uint8_t) ((data >> 8) & SENSOR_LOW_EIGHT_BIT);
		index++;
		cmd[cmd_num++] = (uint8_t) (data & SENSOR_LOW_EIGHT_BIT);
		index++;
	}
	else
	{
		cmd[cmd_num++] = (uint8_t) data;
		index++;
	}

	if (SENSOR_WRITE_DELAY != subaddr)
	{
		for ( i = 0 ; i < SENSOR_I2C_OP_TRY_NUM ; i++)
		{
			msg_w.addr = s_p_sensor_mod->cur_i2c_client->addr;
			msg_w.flags = 0;
			msg_w.buf = cmd;
			msg_w.len = index;
			ret = i2c_transfer(s_p_sensor_mod->cur_i2c_client->adapter, &msg_w, 1);
			if (ret != 1)
			{
				SENSOR_PRINT_ERR("_Sensor_K_WriteReg failed : i2cAddr = %x, addr = %x, value = %x, bit = %d\n",
						s_p_sensor_mod->cur_i2c_client->addr, pReg->reg_addr, pReg->reg_value, pReg->reg_bits);
				ret = SENSOR_K_FAIL;
				continue;
			}
			else
			{
				ret = SENSOR_K_SUCCESS;
				break;
			}
		}
	}
	else
	{
		SLEEP_MS(data);
	}

	return ret;
}

enum cmr_flash_status {
	FLASH_CLOSE	= 0x0,
	FLASH_OPEN	= 0x1,
	FLASH_TORCH	= 0x2, /* User only set flash to close/open/torch state */
	FLASH_AUTO	= 0x3,
	FLASH_CLOSE_AFTER_OPEN	= 0x10, /* Following is set to sensor */
	FLASH_HIGH_LIGHT	= 0x11,
	FLASH_OPEN_ON_RECORDING	= 0x22,
	FLASH_CLOSE_AFTER_AUTOFOCUS	= 0x30,
	FLASH_STATUS_MAX
};

LOCAL int _sensor_k_set_flash(uint32_t flash_mode)
{
	printk("_sensor_k_set_flash: flash_mode 0x%x  \n", flash_mode);

	switch (flash_mode)
	{
		case FLASH_OPEN:	/* Flash on */
		case FLASH_TORCH:	/* For torch */
			sprd_flash_on();
			break;

		case FLASH_HIGH_LIGHT:
			sprd_flash_high_light();
			break;

		case FLASH_CLOSE_AFTER_OPEN: /* Close flash */
		case FLASH_CLOSE_AFTER_AUTOFOCUS:
		case FLASH_CLOSE:
			sprd_flash_close();
			break;

		default:
			printk("_sensor_k_set_flash unknow mode : flash_mode 0x%x\n", flash_mode);
			break;
	}

	return 0;
}

LOCAL int _sensor_k_get_flash_level(SENSOR_FLASH_LEVEL_T *level)
{
	level->low_light  = SPRD_FLASH_LOW_CUR;
	level->high_light = SPRD_FLASH_HIGH_CUR;
	SENSOR_PRINT("Sensor get flash lvl: low %d, high %d \n", level->low_light, level->high_light);
	return SENSOR_K_SUCCESS;
}

LOCAL int _Sensor_K_SetTorch(uint32_t flash_mode)
{
	printk("_Sensor_K_SetTorch mode %d	\n", flash_mode);

	if(PNULL != s_sensor_project_func.SetFlash)
	{
		printk("_Sensor_K_SetTorch call s_sensor_project_func.SetFlash \n");
		return s_sensor_project_func.SetFlash(flash_mode);
	}

	flash_status = flash_mode;
	printk("_Sensor_K_SetTorch: flash_mode 0x%x  \n", flash_mode);
	return SENSOR_K_SUCCESS;
}

#if 0
LOCAL int _Sensor_K_SetFlash(uint32_t flash_mode)
{
	switch (flash_mode)
	{
		case 1: /* Flash on */
		case 2: /* For torch */
			/* Low light */
			gpio_direction_output(GPIO_SPRD_FLASH_LOW, SPRD_FLASH_ON);
			gpio_set_value(GPIO_SPRD_FLASH_LOW, SPRD_FLASH_ON);
			gpio_direction_output(GPIO_SPRD_FLASH_HIGH, SPRD_FLASH_OFF);
			gpio_set_value(GPIO_SPRD_FLASH_HIGH, SPRD_FLASH_OFF);
			break;

		case 0x11:
			/* High light */
			gpio_direction_output(GPIO_SPRD_FLASH_LOW, SPRD_FLASH_ON);
			gpio_set_value(GPIO_SPRD_FLASH_LOW, SPRD_FLASH_ON);
			gpio_direction_output(GPIO_SPRD_FLASH_HIGH, SPRD_FLASH_ON);
			gpio_set_value(GPIO_SPRD_FLASH_HIGH, SPRD_FLASH_ON);
			break;

		case 0x10: /* Close flash */
		case 0x0:
			/* Close the light */
			gpio_direction_output(GPIO_SPRD_FLASH_LOW, SPRD_FLASH_OFF);
			gpio_set_value(GPIO_SPRD_FLASH_LOW, SPRD_FLASH_OFF);
			gpio_direction_output(GPIO_SPRD_FLASH_HIGH, SPRD_FLASH_OFF);
			gpio_set_value(GPIO_SPRD_FLASH_HIGH, SPRD_FLASH_OFF);
			break;

		default:
			SENSOR_PRINT_ERR("sensor set flash unknown mode:%d \n", flash_mode);
			return SENSOR_K_FALSE;
	}

	SENSOR_PRINT("sensor set flash unknown mode %d  \n", flash_mode);
	return SENSOR_K_SUCCESS;
}
#endif

int _sensor_burst_write_init(SENSOR_REG_T_PTR p_reg_table, uint32_t init_table_size);

LOCAL int _sensor_k_wr_regtab(SENSOR_REG_TAB_PTR pRegTab)
{
	SENSOR_REG_T_PTR sensor_reg_ptr;
	SENSOR_REG_BITS_T reg_bit;
	struct timeval time1, time2;
	int ret = SENSOR_K_SUCCESS;
	int rettmp;
	uint32_t cnt = pRegTab->reg_count;
	uint32_t size;
	uint32_t i;
	char *pBuff = PNULL;

	do_gettimeofday(&time1);

	size = cnt*sizeof(SENSOR_REG_T);
	pBuff = _sensor_k_kmalloc(size, GFP_KERNEL);

	if (PNULL == pBuff)
	{
		ret = SENSOR_K_FAIL;
		SENSOR_PRINT_ERR("sensor W RegTab err:kmalloc fail, cnt %d, size %d\n", cnt, size);
		goto _Sensor_K_WriteRegTab_return;
	}
	else
	{
		SENSOR_PRINT("sensor W RegTab: kmalloc success, cnt %d, size %d \n",cnt, size);
	}

	if (copy_from_user(pBuff, pRegTab->sensor_reg_tab_ptr, size))
	{
		ret = SENSOR_K_FAIL;
		SENSOR_PRINT_ERR("sensor w err:copy user fail, size %d \n", size);
		goto _Sensor_K_WriteRegTab_return;
	}

	sensor_reg_ptr = (SENSOR_REG_T_PTR)pBuff;

	if (0 == pRegTab->burst_mode)
	{
		for ( i = 0 ; i < cnt ; i++)
		{
			reg_bit.reg_addr = sensor_reg_ptr[i].reg_addr;
			reg_bit.reg_value = sensor_reg_ptr[i].reg_value;
			reg_bit.reg_bits = pRegTab->reg_bits;

			rettmp = _Sensor_K_WriteReg(&reg_bit);
			if(SENSOR_K_FAIL == rettmp)
				ret = SENSOR_K_FAIL;
		}
	}
	else if (SENSOR_I2C_BUST_NB == pRegTab->burst_mode)
	{
		printk("CAM %s, Line %d, burst_mode=%d, cnt=%d, start \n", __FUNCTION__, __LINE__, pRegTab->burst_mode, cnt);
		ret = _sensor_burst_write_init(sensor_reg_ptr, pRegTab->reg_count);
		printk("CAM %s, Line %d, burst_mode=%d, cnt=%d end\n", __FUNCTION__, __LINE__, pRegTab->burst_mode, cnt);
	}

_Sensor_K_WriteRegTab_return:

	if (PNULL != pBuff)
		_sensor_k_kfree(pBuff);

	do_gettimeofday(&time2);
	SENSOR_PRINT("sensor w RegTab: done, ret %d, cnt %d, time %d us\n", ret, cnt, (uint32_t)((time2.tv_sec - time1.tv_sec)*1000000+(time2.tv_usec - time1.tv_usec)));

	return ret;
}

LOCAL int _sensor_k_set_i2c_clk(uint32_t clock)
{
	sprd_i2c_ctl_chg_clk(SENSOR_I2C_ID, clock);
	SENSOR_PRINT("sensor set i2c clk %d  \n", clock);
	return SENSOR_K_SUCCESS;
}

LOCAL int _sensor_k_wr_i2c(SENSOR_I2C_T_PTR pI2cTab)
{
	struct i2c_msg msg_w;
	uint32_t cnt = pI2cTab->i2c_count;
	int ret = SENSOR_K_FAIL;
	char *pBuff = PNULL;
	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	pBuff = _sensor_k_kmalloc(cnt, GFP_KERNEL);

	if (PNULL == pBuff)
	{
		SENSOR_PRINT_ERR("sensor W I2C ERR: kmalloc fail, size %d\n", cnt);
		goto sensor_k_writei2c_return;
	}
	else
	{
		SENSOR_PRINT("sensor W I2C: kmalloc success, size %d\n", cnt);
	}

	if (copy_from_user(pBuff, pI2cTab->i2c_data, cnt))
	{
		SENSOR_PRINT_ERR("sensor W I2C ERR: copy user fail, size %d \n", cnt);
		goto sensor_k_writei2c_return;
	}

	msg_w.addr = pI2cTab->slave_addr;
	msg_w.flags = 0;
	msg_w.buf = pBuff;
	msg_w.len = cnt;

	ret = i2c_transfer(s_p_sensor_mod->cur_i2c_client->adapter, &msg_w, 1);

	if (ret != 1)
	{
		SENSOR_PRINT_ERR("SENSOR: w reg fail, ret: %d, addr: 0x%x\n", ret, msg_w.addr);
	}
	else
	{
		ret = SENSOR_K_SUCCESS;
	}

sensor_k_writei2c_return:

	if(PNULL != pBuff)
		_sensor_k_kfree(pBuff);

	SENSOR_PRINT("sensor w done, ret %d \n", ret);

	return ret;
}

LOCAL int _sensor_csi2_error(uint32_t err_id, uint32_t err_status, void* u_data)
{
	int ret = 0;
	printk("V4L2: csi2_error, %d 0x%x \n", err_id, err_status);
	return ret;
}

int sensor_k_open(struct inode *node, struct file *file)
{
	int ret = 0;
	ret = _sensor_is_clk_mm_i_eb(1);
	return ret;
}

int sensor_k_release(struct inode *node, struct file *file)
{
	int ret = 0;

	_sensor_k_set_voltage_cammot(SENSOR_VDD_CLOSED);
	_sensor_k_set_voltage_avdd(SENSOR_VDD_CLOSED);
	_sensor_k_set_voltage_dvdd(SENSOR_VDD_CLOSED, _sensor_K_get_curId());
	_sensor_k_set_voltage_iovdd(SENSOR_VDD_CLOSED);
	_sensor_k_set_mclk(0);
	// _Sensor_K_SetFlash(0);
	ret = _sensor_is_clk_mm_i_eb(0);

	return ret;
}

LOCAL ssize_t sensor_k_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *gpos)
{
	return 0;
}

LOCAL ssize_t sensor_k_write(struct file *filp, const char __user *ubuf, size_t cnt, loff_t *gpos)
{
	struct i2c_msg msg_w;
	int ret = SENSOR_K_FAIL;
	int need_alloc = 1;
	char buf[64];
	char *pBuff = PNULL;

	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	SENSOR_PRINT("sensor w cnt %d, buf %d\n", cnt, sizeof(buf));

	if (cnt < sizeof(buf))
	{
		pBuff = buf;
		need_alloc = 0;
	}
	else
	{
		pBuff = _sensor_k_kmalloc(cnt, GFP_KERNEL);

		if (PNULL == pBuff)
		{
			SENSOR_PRINT_ERR("sensor w ERR: kmalloc fail, size %d \n", cnt);
			goto sensor_k_write_return;
		}
		else
		{
			SENSOR_PRINT("sensor w: kmalloc success, size %d \n", cnt);
		}
	}

	if (copy_from_user(pBuff, ubuf, cnt))
	{
		SENSOR_PRINT_ERR("sensor w ERR: copy user fail, size %d\n", cnt);
		goto sensor_k_write_return;
	}

	printk("sensor clnt addr 0x%x\n", s_p_sensor_mod->cur_i2c_client->addr);
	msg_w.addr = s_p_sensor_mod->cur_i2c_client->addr;
	msg_w.flags = 0;
	msg_w.buf = pBuff;
	msg_w.len = cnt;

	ret = i2c_transfer(s_p_sensor_mod->cur_i2c_client->adapter, &msg_w, 1);
	if (ret != 1)
	{
		SENSOR_PRINT_ERR("SENSOR: w reg fail, ret %d, w addr: 0x%x\n", ret, s_p_sensor_mod->cur_i2c_client->addr);
	}
	else
	{
		ret = SENSOR_K_SUCCESS;
	}

sensor_k_write_return:

	if ((PNULL != pBuff) && need_alloc)
		_sensor_k_kfree(pBuff);

	SENSOR_PRINT("sensor w done, ret %d \n", ret);

	return ret;
}

#define I2C_WRITE_BURST_LENGTH 512

#if defined (CONFIG_ARCH_SCX35)
static int _burst_write(uint16_t reg,  uint16_t val)
{
	struct i2c_client *c = s_p_sensor_mod->cur_i2c_client; // Sensor_GetI2CClien();
	int ret = 0;
	uint8_t data[4];

	printk("SENSOR: _burst_write Init\n");

	if (0 == c)
	{
		SENSOR_PRINT_ERR("SENSOR: _burst_write err, i2c_clnt NULL!.\n");
		return -1;
	}

	data[0] = reg>>8;
	data[1] = reg;
	data[2]=  val>>8;
	data[3] = val;

	if (reg == 0xffff)
	{
		msleep(val); /* Wait for reset to run */
		return 0;
	}

	ret = i2c_master_send(c, data, 4);

	return (ret < 0) ? ret: 0;
}

int _sensor_burst_write_init(SENSOR_REG_T_PTR vals, uint32_t reg_length)
{
	int i = 0, ret=0;
	uint16_t iic_length = 2;
	uint8_t iic_data[3000] = {0};
	struct i2c_client *c = s_p_sensor_mod->cur_i2c_client; // Sensor_GetI2CClien();

	printk("SENSOR: _sensor_burst_write_init\n");

	if (0 == c)
	{
		SENSOR_PRINT_ERR("SENSOR: burst w Init err, i2c_clnt NULL!.\n");
		return -1;
	}

	for( i = 0 ; i < reg_length ; i++ )
	{
		if( vals[i].reg_addr == 0x0F12 )
		{
			iic_data[iic_length] = vals[i].reg_value >>8;
			iic_data[iic_length+1] = vals[i].reg_value & 0x00FF;
			iic_length = iic_length+2;

			if( i == (reg_length-1) )
			{
				iic_data[0]=0x0F;
				iic_data[1]=0x12;
				i2c_master_send(c, iic_data, iic_length);
				iic_length =2;
			}
		}
		else
		{
			if(iic_length !=2)
			{
				iic_data[0]=0x0F;
				iic_data[1]=0x12;
				i2c_master_send(c, iic_data, iic_length);
				iic_length =2;
			}

			ret = _burst_write(vals[i].reg_addr, vals[i].reg_value);
			if (ret < 0)
			{
				printk(KERN_NOTICE "======[_sensor_burst_write_init %d]====== \n", ret );
				return ret;
			}
		}
	}

	return 0;
}

#else // defined (CONFIG_ARCH_SCX35)

int _sensor_burst_write_init(SENSOR_REG_T_PTR p_reg_table, uint32_t init_table_size)
{
	int ret = 0;
	uint32_t rtn = 0;
	uint32_t i = 0;
	uint32_t written_num = 0;
	uint32_t wr_num_once = 0;
	uint16_t wr_reg = 0;
	uint16_t wr_val = 0;
	uint8_t *p_reg_val_tmp = 0;
	struct i2c_msg msg_w;
	struct i2c_client *i2c_client = PNULL;

	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	i2c_client = s_p_sensor_mod->cur_i2c_client;

	printk("SENSOR: burst w Init\n");

	if (0 == i2c_client)
	{
		SENSOR_PRINT_ERR("SENSOR: burst w Init err, i2c_clnt NULL!.\n");
		return -1;
	}

	p_reg_val_tmp = (uint8_t*)_sensor_k_kzalloc(init_table_size*sizeof(uint16_t) + 16, GFP_KERNEL);

	if(PNULL == p_reg_val_tmp)
	{
		SENSOR_PRINT_ERR("_sensor_burst_write_init ERROR:kmalloc is fail, size = %d \n", init_table_size*sizeof(uint16_t) + 16);
		return -1;
	}
	else
	{
		SENSOR_PRINT_HIGH("_sensor_burst_write_init: kmalloc success, size = %d \n", init_table_size*sizeof(uint16_t) + 16);
	}

	while (written_num < init_table_size)
	{
		wr_num_once = 2;

		wr_reg = p_reg_table[written_num].reg_addr;
		wr_val = p_reg_table[written_num].reg_value;

		if (SENSOR_WRITE_DELAY == wr_reg)
		{
			if (wr_val >= 10) {
				msleep(wr_val);
			} else {
				mdelay(wr_val);
			}
		}
		else
		{
			p_reg_val_tmp[0] = (uint8_t)(wr_reg);
			p_reg_val_tmp[1] = (uint8_t)(wr_val);

			if ((0x0e == wr_reg) && (0x01 == wr_val))
			{
				for (i = written_num + 1; i < init_table_size; i++)
				{
					if ((0x0e == wr_reg) && (0x00 == wr_val))
					{
						break;
					}
					else
					{
						wr_val = p_reg_table[i].reg_value;
						p_reg_val_tmp[wr_num_once+1] = (uint8_t)(wr_val);
						wr_num_once ++;
					}
				}
			}

			msg_w.addr = i2c_client->addr;
			msg_w.flags = 0;
			msg_w.buf = p_reg_val_tmp;
			msg_w.len = (uint32_t)(wr_num_once);
			ret = i2c_transfer(i2c_client->adapter, &msg_w, 1);

			if (ret!=1)
			{
				SENSOR_PRINT("SENSOR: s err, val {0x%x 0x%x} {0x%x 0x%x} {0x%x 0x%x} {0x%x 0x%x} {0x%x 0x%x} {0x%x 0x%x}.\n",
					p_reg_val_tmp[0],p_reg_val_tmp[1],p_reg_val_tmp[2],p_reg_val_tmp[3],
					p_reg_val_tmp[4],p_reg_val_tmp[5],p_reg_val_tmp[6],p_reg_val_tmp[7],
					p_reg_val_tmp[8],p_reg_val_tmp[9],p_reg_val_tmp[10],p_reg_val_tmp[11]);
					SENSOR_PRINT("SENSOR: i2c w once err\n");
				rtn = 1;
				break;
			}
		}
		written_num += wr_num_once - 1;
	}
	SENSOR_PRINT("SENSOR: burst w Init OK\n");
	_sensor_k_kfree(p_reg_val_tmp);

	return rtn;
}

#endif // defined (CONFIG_ARCH_SCX35)

LOCAL long sensor_k_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	mutex_lock(&s_p_sensor_mod->sensor_lock);

	switch (cmd)
	{
		case SENSOR_IO_PD:
		{
			uint32_t param[2];
			ret = copy_from_user(param, (uint32_t *) arg, 2*sizeof(uint32_t));

			if (0 == ret)
				ret = _sensor_k_powerdown(param[0], param[1]);
		}
		break;

		case SENSOR_IO_SET_CAMMOT:
		{
			uint32_t vdd_val;
			ret = copy_from_user(&vdd_val, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = _sensor_k_set_voltage_cammot(vdd_val);
		}
		break;

		case SENSOR_IO_SET_AVDD:
		{
			uint32_t vdd_val;
			ret = copy_from_user(&vdd_val, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = _sensor_k_set_voltage_avdd(vdd_val);
		}
		break;

		case SENSOR_IO_SET_DVDD:
		{
			uint32_t param[2];
			ret = copy_from_user(&param, (uint32_t *) arg, 2*sizeof(uint32_t));
			if (0 == ret)
				ret = _sensor_k_set_voltage_dvdd(param[0], param[1]);
		}
		break;

		case SENSOR_IO_SET_IOVDD:
		{
			uint32_t vdd_val;
			ret = copy_from_user(&vdd_val, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = _sensor_k_set_voltage_iovdd(vdd_val);
		}
		break;

		case SENSOR_IO_SET_MCLK:
		{
			uint32_t mclk;
			ret = copy_from_user(&mclk, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = _sensor_k_set_mclk(mclk);
		}
		break;

		case SENSOR_IO_RST:
		{
			uint32_t rst_val[2];
			ret = copy_from_user(rst_val, (uint32_t *) arg, 2*sizeof(uint32_t));
			if (0 == ret)
				ret = _sensor_k_reset(rst_val[0], rst_val[1]);
		}
		break;

		case SENSOR_IO_I2C_INIT:
		{
			uint32_t sensor_id;
			ret = copy_from_user(&sensor_id, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = _sensor_k_i2c_init(sensor_id);
		}
		break;

		case SENSOR_IO_I2C_DEINIT:
		{
			uint32_t sensor_id;
			ret = copy_from_user(&sensor_id, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = _sensor_k_i2c_deInit(sensor_id);
		}
		break;

		case SENSOR_IO_SET_ID:
		{
			ret = copy_from_user(&s_p_sensor_mod->sensor_id, (uint32_t *) arg, sizeof(uint32_t));
		}
		break;

		case SENSOR_IO_RST_LEVEL:
		{
			uint32_t param[2];
			ret = copy_from_user(&param, (uint32_t *) arg, 2*sizeof(uint32_t));
			if (0 == ret)
				ret = _sensor_k_set_rst_level(param[0], param[1]);
		}
		break;

		case SENSOR_IO_I2C_ADDR:
		{
			uint16_t i2c_addr;
			ret = copy_from_user(&i2c_addr, (uint16_t *) arg, sizeof(uint16_t));
			if (0 == ret) {
				s_p_sensor_mod->cur_i2c_client->addr = (s_p_sensor_mod->cur_i2c_client->addr & (~0xFF)) |i2c_addr;
				printk("SENSOR_IO_I2C_ADDR: addr = %x, %x \n", i2c_addr, s_p_sensor_mod->cur_i2c_client->addr);
			}
		}
		break;

		case SENSOR_IO_I2C_READ:
		{
			SENSOR_REG_BITS_T reg;
			ret = copy_from_user(&reg, (SENSOR_REG_BITS_T *) arg, sizeof(SENSOR_REG_BITS_T));

			if (0 == ret)
			{
				ret = _Sensor_K_ReadReg(&reg);
				if(SENSOR_K_FAIL != ret)
				{
					ret = copy_to_user((SENSOR_REG_BITS_T *)arg, &reg, sizeof(SENSOR_REG_BITS_T));
				}
			}
		}
		break;

		case SENSOR_IO_I2C_WRITE:
		{
			SENSOR_REG_BITS_T reg;
			ret = copy_from_user(&reg, (SENSOR_REG_BITS_T *) arg, sizeof(SENSOR_REG_BITS_T));
			if (0 == ret)
			{
				ret = _Sensor_K_WriteReg(&reg);
			}
		}
		break;

		case SENSOR_IO_I2C_WRITE_REGS:
		{
			SENSOR_REG_TAB_T regTab;
			memset((void*)&regTab, 0, sizeof(SENSOR_REG_TAB_T));
			ret = copy_from_user(&regTab, (SENSOR_REG_TAB_T *) arg, sizeof(SENSOR_REG_TAB_T));
			if (0 == ret&&regTab.reg_count>0)
				ret = _sensor_k_wr_regtab(&regTab);
		}
		break;

		case SENSOR_IO_SET_I2CCLOCK:
		{
			uint32_t clock;
			ret = copy_from_user(&clock, (uint32_t *) arg, sizeof(uint32_t));
			if(0 == ret)
			{
				_sensor_k_set_i2c_clk(clock);
			}
		}
		break;

		case SENSOR_IO_I2C_WRITE_EXT:
		{
			SENSOR_I2C_T i2cTab;
			ret = copy_from_user(&i2cTab, (SENSOR_I2C_T *) arg, sizeof(SENSOR_I2C_T));
			if (0 == ret)
				ret = _sensor_k_wr_i2c(&i2cTab);
		}
		break;

		case SENSOR_IO_SET_FLASH:
		{
			uint32_t flash_mode;
			ret = copy_from_user(&flash_mode, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = _sensor_k_set_flash(flash_mode);
		}
		break;

		case SENSOR_IO_GET_FLASH_LEVEL:
		{
			SENSOR_FLASH_LEVEL_T flash_level;
			ret = copy_from_user(&flash_level, (SENSOR_FLASH_LEVEL_T *) arg, sizeof(SENSOR_FLASH_LEVEL_T));
			if (0 == ret)
			{
				ret = _sensor_k_get_flash_level(&flash_level);
				if(SENSOR_K_FAIL != ret)
				{
					ret = copy_to_user((SENSOR_FLASH_LEVEL_T *)arg, &flash_level, sizeof(SENSOR_FLASH_LEVEL_T));
				}
			}
		}
		break;

		case SENSOR_IO_GET_SOCID:
		{
			SENSOR_SOCID_T  Id  ;
			Id.d_die=sci_get_chip_id();
			Id.a_die=sci_get_ana_chip_id()|sci_get_ana_chip_ver();
			SENSOR_PRINT("cpu id 0x%x,0x%x  \n", Id.d_die,Id.a_die);
			ret = copy_to_user((SENSOR_SOCID_T *)arg, &Id, sizeof(SENSOR_SOCID_T));
		}
		break;

		case SENSOR_IO_IF_CFG:
		{
			SENSOR_IF_CFG_T if_cfg;
			ret = copy_from_user((void*)&if_cfg, (SENSOR_IF_CFG_T *)arg, sizeof(SENSOR_IF_CFG_T));
			if (0 == ret)
			{
				if (INTERFACE_OPEN == if_cfg.is_open)
				{
					if (INTERFACE_MIPI == if_cfg.if_type)
					{
						if (0 == s_p_sensor_mod->mipi_on)
						{
							_sensor_k_mipi_clk_en();
							udelay(1);
							csi_api_init(if_cfg.bps_per_lane);
							csi_api_start();
							csi_reg_isr(_sensor_csi2_error, (void*)s_p_sensor_mod);
							csi_set_on_lanes(if_cfg.lane_num);
							s_p_sensor_mod->mipi_on = 1;
							printk("MIPI on, lane %d, bps %d, wait 10us \n", if_cfg.lane_num, if_cfg.bps_per_lane);
						}
						else
						{
							printk("MIPI already on \n");
						}
					}
				}
				else
				{
					if (INTERFACE_MIPI == if_cfg.if_type)
					{
						if (1 == s_p_sensor_mod->mipi_on)
						{
							csi_api_close();
							_sensor_k_mipi_clk_dis();
							s_p_sensor_mod->mipi_on = 0;
							printk("MIPI off \n");
						}
						else
						{
							printk("MIPI already off \n");
						}
					}
				}
			}
		}
		break;

		case SENSOR_IO_POWER_ONOFF:
		{
			uint32_t power_on;
			uint32_t sensorID;

			sensorID = _sensor_K_get_curId();
			printk("sensor_k_ioctl : Current Sensor ID is (%d)\n", sensorID);

			ret = copy_from_user(&power_on, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
			{
				if( sensorID == SENSOR_MAIN)
				{
					ret = _s5k43_power(power_on);
				}
				else if(sensorID == SENSOR_SUB)
				{
					ret = _hi702_power(power_on);
				}
				else
				{
					printk("sensor_k_ioctl : Unsupported Sensor ID(%d)\n", sensorID);
				}
			}
		}
		break;

		default:
			SENSOR_PRINT("sensor_k_ioctl : Invalid command %x\n", cmd);
			break;
	}

	mutex_unlock(&s_p_sensor_mod->sensor_lock);

	return (long)ret;
}

LOCAL struct file_operations sensor_fops = {
	.owner = THIS_MODULE,
	.open = sensor_k_open,
	.read = sensor_k_read,
	.write = sensor_k_write,
	.unlocked_ioctl = sensor_k_ioctl,
	.release = sensor_k_release,
};

LOCAL struct miscdevice sensor_dev = {
	.minor = SENSOR_MINOR,
	.name = "sprd_sensor",
	.fops = &sensor_fops,
};

int sensor_k_probe(struct platform_device *pdev)
{
	int ret = 0;
	uint32_t tmp = 0;
	SENSOR_CHECK_ZERO(s_p_sensor_mod);

	printk(KERN_ALERT "sensor probe called\n");

	ret = misc_register(&sensor_dev);
	if (ret)
	{
		printk(KERN_ERR "can't reg miscdev on minor=%d (%d)\n", SENSOR_MINOR, ret);
		return ret;
	}

	ret = gpio_request(GPIO_MAIN_SENSOR_PWN, "main camera");
	if (ret)
	{
		tmp = GPIO_MAIN_SENSOR_PWN;
		goto gpio_err_exit;
	}
	else
	{
		gpio_direction_output(GPIO_MAIN_SENSOR_PWN, 0);
	}

	ret = gpio_request(GPIO_SUB_SENSOR_PWN, "sub camera");
	if (ret)
	{
		tmp = GPIO_SUB_SENSOR_PWN;
		goto gpio_err_exit;
	}
	else
	{
		gpio_direction_output(GPIO_SUB_SENSOR_PWN, 0);
	}

	ret = gpio_request(GPIO_SENSOR_RESET, "ccirrst");
	if (ret)
	{
		tmp = GPIO_SENSOR_RESET;
		goto gpio_err_exit;
	}
	else
	{
		gpio_direction_output(GPIO_SENSOR_RESET, 0);
	}

	// yogesh
	ret = gpio_request(REARCAM_SENSOR_CORE_EN, "core");
	if (ret)
	{
		tmp = REARCAM_SENSOR_CORE_EN;
		goto gpio_err_exit;
	}
	else
	{
		gpio_direction_output(REARCAM_SENSOR_CORE_EN, 0);
	}

	//gpio_direction_output(REARCAM_SENSOR_CORE_EN, 0);

//#ifndef CONFIG_ARCH_SCX35
	/*ret = gpio_request(CAM_FLASH_ENF_GPIO, "gpioFlashLow");
	if (ret) {
		tmp = CAM_FLASH_ENF_GPIO;
		goto gpio_err_exit;
	}else*/
	/*{
		gpio_direction_input(CAM_FLASH_ENF_GPIO);
	}*/

	/*ret = gpio_request(CAM_FLASH_ENT_GPIO, "gpioFlashHigh");
	if (ret) {
		tmp = CAM_FLASH_ENT_GPIO;
		goto gpio_err_exit;
	}else*/
	/*{
		gpio_direction_input(CAM_FLASH_ENT_GPIO);
	}*/
//#endif

	ret = gpio_request(GPIO_SUB_SENSOR_RESET, "subreset");
	if (ret)
	{
		tmp = GPIO_SUB_SENSOR_RESET;
		goto gpio_err_exit;
	}
	else
	{
		gpio_direction_output(GPIO_SUB_SENSOR_RESET, 0);
	}

	s_p_sensor_mod->sensor_i2c_driver.driver.owner = THIS_MODULE;
	s_p_sensor_mod->sensor_i2c_driver.probe  = sensor_probe;
	s_p_sensor_mod->sensor_i2c_driver.remove = sensor_remove;
	s_p_sensor_mod->sensor_i2c_driver.detect = sensor_detect;
	s_p_sensor_mod->sensor_i2c_driver.driver.name = SENSOR_MAIN_I2C_NAME;
	s_p_sensor_mod->sensor_i2c_driver.id_table = c_sensor_device_id;
	s_p_sensor_mod->sensor_i2c_driver.address_list = &c_sensor_main_default_addr_list[0];

	ret = i2c_add_driver(&s_p_sensor_mod->sensor_i2c_driver);
	if (ret)
	{
		SENSOR_PRINT_ERR("+I2C err %d.\n", ret);
		return SENSOR_K_FAIL;
	}
	else
	{
		SENSOR_PRINT_HIGH("+I2C OK \n");
	}

gpio_err_exit:

	if (ret)
	{
		printk(KERN_ERR "sensor prb fail req gpio %d err %d\n", tmp, ret);
	}
	else
	{
		printk(KERN_ALERT " sensor prb Success\n");
	}

	return ret;
}

LOCAL int sensor_k_remove(struct platform_device *dev)
{
	printk(KERN_INFO "sensor remove called !\n");

#ifndef CONFIG_ARCH_SCX35
	gpio_free(GPIO_SPRD_FLASH_HIGH);
	gpio_free(GPIO_SPRD_FLASH_LOW);
#endif

	gpio_free(GPIO_SUB_SENSOR_RESET);
	gpio_free(CAM_FLASH_ENF_GPIO);
	gpio_free(CAM_FLASH_ENT_GPIO);
	gpio_free(GPIO_SENSOR_RESET);
	gpio_free(GPIO_SUB_SENSOR_PWN);
	gpio_free(GPIO_MAIN_SENSOR_PWN);

	misc_deregister(&sensor_dev);

	printk(KERN_INFO "sensor remove Success !\n");

	return 0;
}

LOCAL struct platform_driver sensor_dev_driver = {
	.probe = sensor_k_probe,
	.remove =sensor_k_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_sensor",
		},
};

#define REAR_SENSOR_NAME	"S5K4EC\n"
#define FRONT_SENSOR_NAME	"SR030PC30\n"
#define SENSOR_TYPE	"SOC"

struct class *camera_class;

static ssize_t Rear_Cam_Sensor_ID(struct device *dev, struct device_attribute *attr, char *buf)
{
	SENSOR_PRINT("Rear_Cam_Sensor_ID\n");
	return  sprintf(buf, REAR_SENSOR_NAME);
}

static ssize_t Cam_Sensor_TYPE(struct device *dev, struct device_attribute *attr, char *buf)
{
	SENSOR_PRINT("Cam_Sensor_TYPE\n");
	return  sprintf(buf, SENSOR_TYPE);
}

static ssize_t Front_Cam_Sensor_ID(struct device *dev, struct device_attribute *attr, char *buf)
{
	SENSOR_PRINT("Front_Cam_Sensor_ID\n");
	return  sprintf(buf, FRONT_SENSOR_NAME);
}

static ssize_t Rear_Cam_store_flash(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	 int value;
	 sscanf(buf, "%d", &value);
	 printk("Rear_Cam_store_flash value = %d\n", value);
	_Sensor_K_SetTorch(value);
	 return size;
}

static ssize_t S5K4ECGX_camera_vendorid_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;
	count = sprintf(buf, "0x%04X", VENDOR_ID);
	printk("%s : vendor ID is 0x%04X\n", __func__, VENDOR_ID);
	return count;
}

static ssize_t S5K4ECGX_camera_vendorid_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int tmp = 0;
	sscanf(buf, "%x", &tmp);
	VENDOR_ID = tmp;
	printk("%s : vendor ID is 0x%04X\n", __func__, VENDOR_ID);
	return size;
}

static ssize_t S5K4ECGX_camera_antibanding_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;
	count = sprintf(buf, "%d", camera_antibanding_val);
	printk("%s : antibanding is %d\n", __func__, camera_antibanding_val);
	return count;
}

static ssize_t S5K4ECGX_camera_antibanding_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int tmp = 0;
	sscanf(buf, "%d", &tmp);
	camera_antibanding_val = tmp;
	printk("%s : antibanding is %d\n", __func__, camera_antibanding_val);
	return size;
}

static DEVICE_ATTR(rear_camfw,	S_IRUGO | S_IXOTH, Rear_Cam_Sensor_ID, NULL);
static DEVICE_ATTR(front_camfw,	S_IRUGO | S_IXOTH, Front_Cam_Sensor_ID, NULL);
static DEVICE_ATTR(rear_type,		S_IRUGO | S_IXOTH, Cam_Sensor_TYPE, NULL);
static DEVICE_ATTR(front_type,	S_IRUGO | S_IXOTH, Cam_Sensor_TYPE, NULL);
static DEVICE_ATTR(rear_flash,	S_IWUSR | S_IWGRP | S_IXOTH, NULL, Rear_Cam_store_flash);

static struct device_attribute S5K4ECGX_camera_vendorid_attr = {
	.attr = {
		.name = "rear_vendorid",
		.mode = (S_IRUSR|S_IRGRP | S_IWUSR|S_IWGRP)},
	.show = S5K4ECGX_camera_vendorid_show,
	.store = S5K4ECGX_camera_vendorid_store
};

static struct device_attribute S5K4ECGX_camera_antibanding_attr = {
	.attr = {
		.name = "Cam_antibanding",
		.mode = (S_IRUSR|S_IRGRP | S_IWUSR|S_IWGRP)},
	.show = S5K4ECGX_camera_antibanding_show,
	.store = S5K4ECGX_camera_antibanding_store
};

int __init sensor_k_init(void)
{
	printk(KERN_INFO "sensor_k_init called !\n");

	s_p_sensor_mod = (struct sensor_module *)kmalloc(sizeof(struct sensor_module), GFP_KERNEL);
	SENSOR_CHECK_ZERO(s_p_sensor_mod);
	memset((void*)s_p_sensor_mod, 0, sizeof(struct sensor_module));
	s_p_sensor_mod->sensor_id = SENSOR_ID_MAX;
	mutex_init(&s_p_sensor_mod->sensor_lock);

	if (platform_driver_register(&sensor_dev_driver) != 0)
	{
		printk("platform device register Failed \n");
		return SENSOR_K_FAIL;
	}

	struct device *dev_t;

	camera_class = class_create(THIS_MODULE, "camera");
	if (IS_ERR(camera_class))
	{
		SENSOR_PRINT("Failed to create camera_class!\n");
		return PTR_ERR( camera_class );
	}

	dev_t = device_create(camera_class, NULL, 0, "%s", "rear");
	if (device_create_file(dev_t, &dev_attr_rear_camfw) < 0)
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_rear_camfw.attr.name);
	if (device_create_file(dev_t, &dev_attr_rear_type) < 0)
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_rear_type.attr.name);
	if (device_create_file(dev_t, &dev_attr_rear_flash) < 0)
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_rear_flash.attr.name);
	if (device_create_file(dev_t, &S5K4ECGX_camera_antibanding_attr) < 0)
		SENSOR_PRINT("Failed to create device file(%s)!\n", S5K4ECGX_camera_antibanding_attr.attr.name);
	if (device_create_file(dev_t, &S5K4ECGX_camera_vendorid_attr) < 0)
		SENSOR_PRINT("Failed to create device file(%s)!\n", S5K4ECGX_camera_vendorid_attr.attr.name);

	dev_t = device_create(camera_class, NULL, 0, "%s", "front");
	if (device_create_file(dev_t, &dev_attr_front_camfw) < 0)
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_front_camfw.attr.name);
	if (device_create_file(dev_t, &dev_attr_front_type) < 0)
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_front_type.attr.name);

	return 0;
}

void sensor_k_exit(void)
{
	printk(KERN_INFO "sensor_k_exit called !\n");
	platform_driver_unregister(&sensor_dev_driver);

	if (SENSOR_ADDR_INVALID(s_p_sensor_mod))
	{
		printk("SENSOR: Invalid addr, 0x%x", (uint32_t)s_p_sensor_mod);
	}
	else
	{
		kfree(s_p_sensor_mod);
		s_p_sensor_mod = NULL;
	}
}

module_init(sensor_k_init);
module_exit(sensor_k_exit);

MODULE_DESCRIPTION("Sensor Driver");
MODULE_LICENSE("GPL");

