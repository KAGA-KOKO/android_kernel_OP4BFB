/***************************************************************
** Copyright (C),  2018,  OPPO Mobile Comm Corp.,  Ltd
** VENDOR_EDIT
** File : oppo_display_private_api.h
** Description : oppo display private api implement
** Version : 1.0
** Date : 2018/03/20
** Author : Jie.Hu@PSW.MM.Display.Stability
**
** ------------------------------- Revision History: -----------
**  <author>        <data>        <version >        <desc>
**   Hu.Jie          2018/03/20        1.0           Build this moudle
**   Guo.Ling        2018/10/11        1.1           Modify for SDM660
**   Guo.Ling        2018/11/27        1.2           Modify for mt6779
******************************************************************/
#include "oppo_display_private_api.h"

/*
 * we will create a sysfs which called /sys/kernel/oppo_display,
 * In that directory, oppo display private api can be called
 */

unsigned long oppo_display_brightness = 0;
unsigned int oppo_set_brightness = 0;
unsigned int aod_light_mode = 0;
extern int primary_display_aod_backlight(int level);
extern int primary_display_setbacklight_nolock(unsigned int level);
extern void _primary_path_switch_dst_lock(void);
extern void _primary_path_switch_dst_unlock(void);
extern void _primary_path_lock(const char *caller);
extern void _primary_path_unlock(const char *caller);
extern bool primary_display_get_fp_hbm_state(void);

static ssize_t oppo_display_get_brightness(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	if (oppo_display_brightness > LED_FULL || oppo_display_brightness < LED_OFF) {
		oppo_display_brightness = LED_OFF;
	}
	//printk(KERN_INFO "oppo_display_get_brightness = %ld\n",oppo_display_brightness);
	return sprintf(buf, "%ld\n", oppo_display_brightness);
}

static ssize_t oppo_display_set_brightness(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t num)
{
	int ret;

	ret = kstrtouint(buf, 10, &oppo_set_brightness);

	printk("%s %d\n", __func__, oppo_set_brightness);

	if (oppo_set_brightness > LED_FULL || oppo_set_brightness < LED_OFF) {
		return num;
	}

	_primary_path_switch_dst_lock();
	_primary_path_lock(__func__);
	primary_display_setbacklight_nolock(oppo_set_brightness);
	_primary_path_unlock(__func__);
	_primary_path_switch_dst_unlock();

	return num;
}

static ssize_t oppo_display_get_max_brightness(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	//printk(KERN_INFO "oppo_display_get_max_brightness = %d\n",LED_FULL);
	return sprintf(buf, "%u\n", LED_FULL);
}

static ssize_t oppo_get_aod_light_mode(struct device *dev,
		struct device_attribute *attr, char *buf) {

	printk(KERN_INFO "oppo_get_aod_light_mode = %d\n",aod_light_mode);

	return sprintf(buf, "%d\n", aod_light_mode);
}

static ssize_t oppo_set_aod_light_mode(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	unsigned int temp_save = 0;
	int ret = 0;

	ret = kstrtouint(buf, 10, &temp_save);
	if (primary_display_get_fp_hbm_state()) {
		printk(KERN_INFO "oppo_set_aod_light_mode = %d return on hbm\n",temp_save);
		return count;
	}
	aod_light_mode = temp_save;
	ret = primary_display_aod_backlight(aod_light_mode);
	printk(KERN_INFO "oppo_set_aod_light_mode = %d\n",temp_save);

	return count;
}

int oppo_panel_alpha = 0;
int oppo_underbrightness_alpha = 0;
int alpha_save = 0;
struct ba {
	u32 brightness;
	u32 alpha;
};

struct ba brightness_alpha_lut[] = {
	{0, 0xff},
	{1, 0xee},
	{2, 0xe8},
	{3, 0xe6},
	{4, 0xe5},
	{6, 0xe4},
	{10, 0xe0},
	{20, 0xd5},
	{30, 0xce},
	{45, 0xc6},
	{70, 0xb7},
	{100, 0xad},
	{150, 0xa0},
	{227, 0x8a},
	{300, 0x80},
	{400, 0x6e},
	{500, 0x5b},
	{600, 0x50},
	{800, 0x38},
	{1023, 0x18},
};

static int interpolate(int x, int xa, int xb, int ya, int yb)
{
	int bf, factor, plus;
	int sub = 0;

	bf = 2 * (yb - ya) * (x - xa) / (xb - xa);
	factor = bf / 2;
	plus = bf % 2;
	if ((xa - xb) && (yb - ya))
		sub = 2 * (x - xa) * (x - xb) / (yb - ya) / (xa - xb);

	return ya + factor + plus + sub;
}

int bl_to_alpha(int brightness)
{
	int level = ARRAY_SIZE(brightness_alpha_lut);
	int i = 0;
	int alpha;

	for (i = 0; i < ARRAY_SIZE(brightness_alpha_lut); i++){
		if (brightness_alpha_lut[i].brightness >= brightness)
			break;
	}

	if (i == 0)
		alpha = brightness_alpha_lut[0].alpha;
	else if (i == level)
		alpha = brightness_alpha_lut[level - 1].alpha;
	else
		alpha = interpolate(brightness,
			brightness_alpha_lut[i-1].brightness,
			brightness_alpha_lut[i].brightness,
			brightness_alpha_lut[i-1].alpha,
			brightness_alpha_lut[i].alpha);
	return alpha;
}

int brightness_to_alpha(int brightness)
{
	int alpha;

	if (brightness <= 3)
		return alpha_save;

	alpha = bl_to_alpha(brightness);

	alpha_save = alpha;

	return alpha;
}

int oppo_get_panel_brightness_to_alpha(void)
{
	if (oppo_panel_alpha)
		return oppo_panel_alpha;

	return brightness_to_alpha(oppo_display_brightness);
}

static ssize_t oppo_display_get_dim_alpha(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	if (!primary_display_get_fp_hbm_state())
		return sprintf(buf, "%d\n", 0);

	oppo_underbrightness_alpha = oppo_get_panel_brightness_to_alpha();

	return sprintf(buf, "%d\n", oppo_underbrightness_alpha);
}

static ssize_t oppo_display_set_dim_alpha(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%x", &oppo_panel_alpha);
	return count;
}

int oppo_dc_alpha = 0;
#if defined(CONFIG_OPPO_SPECIAL_BUILD)
int oppo_dc_enable = 1;
#else
int oppo_dc_enable = 0;
#endif
static ssize_t oppo_display_get_dc_enable(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", oppo_dc_enable);
}

static ssize_t oppo_display_set_dc_enable(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%x", &oppo_dc_enable);
	return count;
}

static ssize_t oppo_display_get_dim_dc_alpha(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", oppo_dc_alpha);
}

static ssize_t oppo_display_set_dim_dc_alpha(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%x", &oppo_dc_alpha);
	return count;
}

static struct kobject *oppo_display_kobj;

static DEVICE_ATTR(oppo_brightness, S_IRUGO|S_IWUSR, oppo_display_get_brightness, oppo_display_set_brightness);
static DEVICE_ATTR(oppo_max_brightness, S_IRUGO|S_IWUSR, oppo_display_get_max_brightness, NULL);

static DEVICE_ATTR(aod_light_mode_set, S_IRUGO|S_IWUSR, oppo_get_aod_light_mode, oppo_set_aod_light_mode);
static DEVICE_ATTR(dim_alpha, S_IRUGO|S_IWUSR, oppo_display_get_dim_alpha, oppo_display_set_dim_alpha);
static DEVICE_ATTR(dimlayer_bl_en, S_IRUGO|S_IWUSR, oppo_display_get_dc_enable, oppo_display_set_dc_enable);
static DEVICE_ATTR(dim_dc_alpha, S_IRUGO|S_IWUSR, oppo_display_get_dim_dc_alpha, oppo_display_set_dim_dc_alpha);

/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *oppo_display_attrs[] = {
	&dev_attr_oppo_brightness.attr,
	&dev_attr_oppo_max_brightness.attr,
	&dev_attr_aod_light_mode_set.attr,
	&dev_attr_dim_alpha.attr,
	&dev_attr_dimlayer_bl_en.attr,
	&dev_attr_dim_dc_alpha.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group oppo_display_attr_group = {
	.attrs = oppo_display_attrs,
};

static int __init oppo_display_private_api_init(void)
{
	int retval;

	oppo_display_kobj = kobject_create_and_add("oppo_display", kernel_kobj);
	if (!oppo_display_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(oppo_display_kobj, &oppo_display_attr_group);
	if (retval)
		kobject_put(oppo_display_kobj);

	return retval;
}

static void __exit oppo_display_private_api_exit(void)
{
	kobject_put(oppo_display_kobj);
}

module_init(oppo_display_private_api_init);
module_exit(oppo_display_private_api_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Hujie <hujie@oppo.com>");
