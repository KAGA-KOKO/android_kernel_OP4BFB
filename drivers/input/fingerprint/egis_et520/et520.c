/*
 * Simple synchronous userspace interface to SPI devices
 *
 * Copyright (C) 2006 SWAPP
 *	Andrea Paterniani <a.paterniani@swapp-eng.it>
 * Copyright (C) 2007 David Brownell (simplification, cleanup)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
*
*  et520.spi.c
*  Date: 2016/03/16
*  Version: 0.9.0.1
*  Revise Date:  2016/03/24
*  Copyright (C) 2007-2016 Egis Technology Inc.
*
*/


#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <linux/notifier.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/list.h>

//#include <mach/gpio.h>
//#include <plat/gpio-cfg.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>

#include <linux/fb.h>
#include <linux/pm_wakeup.h>
static struct wakeup_source et512_wake_lock;

//#include <mt-plat/mt_gpio.h>
#include "et520.h"
#include "ets_navi_input.h"

#define EGIS_NAVI_INPUT 1 // 1:open ; 0:close

#define FP_SPI_DEBUG
#define EDGE_TRIGGER_FALLING    0x0
#define	EDGE_TRIGGER_RAISING    0x1
#define	LEVEL_TRIGGER_LOW       0x2
#define	LEVEL_TRIGGER_HIGH      0x3

int g_screen_onoff = 1;
#define GPIO_PIN_IRQ  126
#define GPIO_PIN_RESET 93
#define GPIO_PIN_33V 94

extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);

//void mt_spi_enable_clk(struct mt_spi_t *ms);
//void mt_spi_disable_clk(struct mt_spi_t *ms);

//struct mt_spi_t *egistec_mt_spi_t;

/*
 * FPS interrupt table
 */
 
struct interrupt_desc fps_ints = {0 , 0, "BUT0" , 0};


unsigned int bufsiz = 4096;
//gjx@ODM_WT.BSP.FINGERPRINT 2019/12/19 add for bringup
#define VCC_SUPPLY_USE_REGULATER
#ifdef VCC_SUPPLY_USE_REGULATER
struct regulator *egistec_regulater;
#endif
static int egistec_power_on(void);
//gjx@ODM_WT.BSP.FINGERPRINT 2019/12/19 add for bringup

int gpio_irq;
int request_irq_done = 0;
int egistec_platformInit_done = 0;

#define EDGE_TRIGGER_FALLING    0x0
#define EDGE_TRIGGER_RISING    0x1
#define LEVEL_TRIGGER_LOW       0x2
#define LEVEL_TRIGGER_HIGH      0x3

int egistec_platformInit(struct egistec_data *egistec);
int egistec_platformFree(struct egistec_data *egistec);

struct ioctl_cmd {
int int_mode;
int detect_period;
int detect_threshold;
}; 

static void delete_device_node(void);
static struct egistec_data *g_data;

DECLARE_BITMAP(minors, N_SPI_MINORS);
LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static struct of_device_id egistec_match_table[] = {
	//{ .compatible = "mediatek,finger_fp",},
	{ .compatible = "mediatek,egis_fp", },
	//{ .compatible = "goodix,goodix-fp", },
	{},
};

static struct of_device_id et520_spi_of_match[] = {
	{ .compatible = "mediatek,finger_fp", },
	{},
};


MODULE_DEVICE_TABLE(of, et520_spi_of_match);

// add for dual sensor start
#if 0
static struct of_device_id fpswitch_match_table[] = {
	{ .compatible = "fp_id,fp_id",},
	{},
};
#endif
//add for dual sensor end
MODULE_DEVICE_TABLE(of, egistec_match_table);


/* ------------------------------ Interrupt -----------------------------*/
/*
 * Interrupt description
 */

#define FP_INT_DETECTION_PERIOD  10
#define FP_DETECTION_THRESHOLD	10

static DECLARE_WAIT_QUEUE_HEAD(interrupt_waitq);
#if 1
//static void spi_clk_enable(struct egistec_data *egistec, u8 bonoff)
static void spi_clk_enable(u8 bonoff)
{


	if (bonoff) {
		DEBUG_PRINT("%s line:%d enable spi clk\n", __func__,__LINE__);
		mt_spi_enable_master_clk(g_data->spi);
//		count = 1;
	} else //if ((count > 0) && (bonoff == 0)) 
	{
		DEBUG_PRINT("%s line:%d disable spi clk\n", __func__,__LINE__);

		mt_spi_disable_master_clk(g_data->spi);

//		count = 0;
	}


}

#endif

/*
 *	FUNCTION NAME.
 *		interrupt_timer_routine
 *
 *	FUNCTIONAL DESCRIPTION.
 *		basic interrupt timer inital routine
 *
 *	ENTRY PARAMETERS.
 *		gpio - gpio address
 *
 *	EXIT PARAMETERS.
 *		Function Return
 */

void interrupt_timer_routine(unsigned long _data)
{
	struct interrupt_desc *bdata = (struct interrupt_desc *)_data;

	DEBUG_PRINT("FPS interrupt count = %d", bdata->int_count);
	if (bdata->int_count >= bdata->detect_threshold) {
		bdata->finger_on = 1;
		DEBUG_PRINT("FPS triggered !!!!!!!\n");
	} else {
		DEBUG_PRINT("FPS not triggered !!!!!!!\n");
	}

	bdata->int_count = 0;
	wake_up_interruptible(&interrupt_waitq);
}


static irqreturn_t fp_eint_func(int irq, void *dev_id)
{
	if (!fps_ints.int_count)
		mod_timer(&fps_ints.timer,jiffies + msecs_to_jiffies(fps_ints.detect_period));
	fps_ints.int_count++;
//	DEBUG_PRINT("-----------   zq fp fp_eint_func  , fps_ints.int_count=%d",fps_ints.int_count);
	return IRQ_HANDLED;
}

static irqreturn_t fp_eint_func_ll(int irq , void *dev_id)
{
	DEBUG_PRINT("[egis]fp_eint_func_ll\n");
	fps_ints.finger_on = 1;
	//fps_ints.int_count = 0;
	disable_irq_nosync(gpio_irq);
	fps_ints.drdy_irq_flag = DRDY_IRQ_DISABLE;
	wake_up_interruptible(&interrupt_waitq);
	return IRQ_RETVAL(IRQ_HANDLED);
}



/*
 *	FUNCTION NAME.
 *		Interrupt_Init
 *
 *	FUNCTIONAL DESCRIPTION.
 *		button initial routine
 *
 *	ENTRY PARAMETERS.
 *		int_mode - determine trigger mode
 *			EDGE_TRIGGER_FALLING    0x0
 *			EDGE_TRIGGER_RAISING    0x1
 *			LEVEL_TRIGGER_LOW        0x2
 *			LEVEL_TRIGGER_HIGH       0x3
 *
 *	EXIT PARAMETERS.
 *		Function Return int
 */

int Interrupt_Init(struct egistec_data *egistec,int int_mode,int detect_period,int detect_threshold)
{

	int err = 0;
	int status = 0;
    struct device_node *node = NULL;
	
DEBUG_PRINT("FP --  %s mode = %d period = %d threshold = %d\n",__func__,int_mode,detect_period,detect_threshold);
DEBUG_PRINT("FP --  %s request_irq_done = %d gpio_irq = %d  pin = %d  \n",__func__,request_irq_done,gpio_irq, egistec->irqPin);


	fps_ints.detect_period = detect_period;
	fps_ints.detect_threshold = detect_threshold;
	fps_ints.int_count = 0;
	fps_ints.finger_on = 0;


	if (request_irq_done == 0)
	{
//		gpio_irq = gpio_to_irq(egistec->irqPin);
        node = of_find_matching_node(node, egistec_match_table);
//        printk("ttt-fp_irq number %d\n", node? 1:2);
 
        if (node){
                gpio_irq = irq_of_parse_and_map(node, 0);
                printk("fp_irq number %d\n", gpio_irq);
		}else
		printk("node = of_find_matching_node fail error  \n");
	
		if (gpio_irq < 0) {
			DEBUG_PRINT("%s gpio_to_irq failed\n", __func__);
			status = gpio_irq;
			goto done;
		}


		DEBUG_PRINT("[Interrupt_Init] flag current: %d disable: %d enable: %d\n",
		fps_ints.drdy_irq_flag, DRDY_IRQ_DISABLE, DRDY_IRQ_ENABLE);
			
		if (int_mode == EDGE_TRIGGER_RISING){
		DEBUG_PRINT("%s EDGE_TRIGGER_RISING\n", __func__);
		err = request_irq(gpio_irq, fp_eint_func,IRQ_TYPE_EDGE_RISING,"fp_detect-eint", egistec);
			if (err){
				pr_err("request_irq failed==========%s,%d\n", __func__,__LINE__);
				}				
		}
		else if (int_mode == EDGE_TRIGGER_FALLING){
			DEBUG_PRINT("%s EDGE_TRIGGER_FALLING\n", __func__);
			err = request_irq(gpio_irq, fp_eint_func,IRQ_TYPE_EDGE_FALLING,"fp_detect-eint", egistec);
			if (err){
				pr_err("request_irq failed==========%s,%d\n", __func__,__LINE__);
				}	
		}
		else if (int_mode == LEVEL_TRIGGER_LOW) {
			DEBUG_PRINT("%s LEVEL_TRIGGER_LOW\n", __func__);
			err = request_irq(gpio_irq, fp_eint_func_ll,IRQ_TYPE_LEVEL_LOW,"fp_detect-eint", egistec);
			if (err){
				pr_err("request_irq failed==========%s,%d\n", __func__,__LINE__);
				}
		}
		else if (int_mode == LEVEL_TRIGGER_HIGH){
			DEBUG_PRINT("%s LEVEL_TRIGGER_HIGH\n", __func__);
			err = request_irq(gpio_irq, fp_eint_func_ll,IRQ_TYPE_LEVEL_HIGH,"fp_detect-eint", egistec);
			if (err){
				pr_err("request_irq failed==========%s,%d\n", __func__,__LINE__);
				}
		}
		DEBUG_PRINT("[Interrupt_Init]:gpio_to_irq return: %d\n", gpio_irq);
		DEBUG_PRINT("[Interrupt_Init]:request_irq return: %d\n", err);
	
		fps_ints.drdy_irq_flag = DRDY_IRQ_ENABLE;
		enable_irq_wake(gpio_irq);
		request_irq_done = 1;
	}
		
		
	if (fps_ints.drdy_irq_flag == DRDY_IRQ_DISABLE){
		fps_ints.drdy_irq_flag = DRDY_IRQ_ENABLE;
		enable_irq_wake(gpio_irq);
		enable_irq(gpio_irq);
	}
done:
	return 0;
}

/*
 *	FUNCTION NAME.
 *		Interrupt_Free
 *
 *	FUNCTIONAL DESCRIPTION.
 *		free all interrupt resource
 *
 *	EXIT PARAMETERS.
 *		Function Return int
 */

int Interrupt_Free(struct egistec_data *egistec)
{
	DEBUG_PRINT("%s\n", __func__);
	fps_ints.finger_on = 0;
	
	if (fps_ints.drdy_irq_flag == DRDY_IRQ_ENABLE) {
		DEBUG_PRINT("%s (DISABLE IRQ)\n", __func__);
		disable_irq_nosync(gpio_irq);

		del_timer_sync(&fps_ints.timer);
		fps_ints.drdy_irq_flag = DRDY_IRQ_DISABLE;
	}
	return 0;
}

/*
 *	FUNCTION NAME.
 *		fps_interrupt_re d
 *
 *	FUNCTIONAL DESCRIPTION.
 *		FPS interrupt read status
 *
 *	ENTRY PARAMETERS.
 *		wait poll table structure
 *
 *	EXIT PARAMETERS.
 *		Function Return int
 */

unsigned int fps_interrupt_poll(
struct file *file,
struct poll_table_struct *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &interrupt_waitq, wait);
	if (fps_ints.finger_on) {
		mask |= POLLIN | POLLRDNORM;
	}
	return mask;
}

void fps_interrupt_abort(void)
{
	DEBUG_PRINT("%s\n", __func__);
	fps_ints.finger_on = 0;
	wake_up_interruptible(&interrupt_waitq);
}

/*-------------------------------------------------------------------------*/
static void egistec_reset(struct egistec_data *egistec)
{
	int state;
	DEBUG_PRINT("%s\n", __func__);
	
	//#ifdef CONFIG_OF
	state = pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_reset_low);
	DEBUG_PRINT("state = %d, %s\n", state, __func__);
	if(state)
	{
		DEBUG_PRINT("can't get reset state, %s\n", __func__);
	}
	mdelay(15);
	pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_reset_high);
	mdelay(200);
	//#endif	
	
}

static ssize_t egistec_read(struct file *filp,
	char __user *buf,
	size_t count,
	loff_t *f_pos)
{
	/*Implement by vendor if needed*/
	return 0;
}

static ssize_t egistec_write(struct file *filp,
	const char __user *buf,
	size_t count,
	loff_t *f_pos)
{
	/*Implement by vendor if needed*/
	return 0;
}

static int egistec_fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	unsigned int blank;
	int retval = 0;

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != FB_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */)
		return 0;

	blank = *(int *)evdata->data;
	DEBUG_PRINT(" enter, blank=0x%x\n", blank);

	switch (blank) {
	case FB_BLANK_UNBLANK:
		g_screen_onoff = 1;
		DEBUG_PRINT("lcd on notify  ------\n");
		break;

	case FB_BLANK_POWERDOWN:
		g_screen_onoff = 0;
		DEBUG_PRINT("lcd off notify  ------\n");
		break;

	default:
		DEBUG_PRINT("other notifier, ignore\n");
		break;
	}
	return retval;
}

static long egistec_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int retval = 0;
	struct egistec_data *egistec;
	struct ioctl_cmd data;
	int status = 0;

	memset(&data, 0, sizeof(data));
	printk("%s  ---------   zq  001  ---  cmd = 0x%X \n", __func__, cmd);
	egistec = filp->private_data;


	if(!egistec_platformInit_done)
	/* platform init */
	status = egistec_platformInit(egistec);
	if (status != 0) {
		pr_err("%s platforminit failed\n", __func__);
		//goto egistec_probe_platformInit_failed;
	}



	switch (cmd) {
	case INT_TRIGGER_INIT:

		if (copy_from_user(&data, (int __user *)arg, sizeof(data))) {
			retval = -EFAULT;
		goto done;
		}

		//DEBUG_PRINT("fp_ioctl IOCTL_cmd.int_mode %u,
		//		IOCTL_cmd.detect_period %u,
		//		IOCTL_cmd.detect_period %u (%s)\n",
		//		data.int_mode,
		//		data.detect_period,
		//		data.detect_threshold, __func__);

		DEBUG_PRINT("fp_ioctl >>> fp Trigger function init\n");
		retval = Interrupt_Init(egistec, data.int_mode,data.detect_period,data.detect_threshold);
		DEBUG_PRINT("fp_ioctl trigger init = %x\n", retval);
	break;

	case FP_SENSOR_RESET:
			//gpio_request later
			DEBUG_PRINT("fp_ioctl ioc->opcode == FP_SENSOR_RESET --");
			egistec_reset(egistec);
		goto done;
	case INT_TRIGGER_CLOSE:
			DEBUG_PRINT("fp_ioctl <<< fp Trigger function close\n");
			retval = Interrupt_Free(egistec);
			DEBUG_PRINT("fp_ioctl trigger close = %x\n", retval);
		goto done;
	case INT_TRIGGER_ABORT:
			DEBUG_PRINT("fp_ioctl <<< fp Trigger function close\n");
			fps_interrupt_abort();
		goto done;
	case FP_FREE_GPIO:
			DEBUG_PRINT("fp_ioctl <<< FP_FREE_GPIO -------  \n");
			egistec_platformFree(egistec);
		goto done;
	case FP_WAKELOCK_ENABLE: //0Xb1
			DEBUG_PRINT("fp_ioctl <<< FP_WAKELOCK_TIMEOUT_ENABLE  \n");
			//wake_lock_timeout(&et512_wake_lock, msecs_to_jiffies(1000));
			__pm_wakeup_event(&et512_wake_lock, msecs_to_jiffies(1000));
		goto done;
	case FP_WAKELOCK_DISABLE: //0Xb2
			DEBUG_PRINT("fp_ioctl <<< FP_WAKELOCK_TIMEOUT_DISABLE  \n");
			//wake_unlock(&et512_wake_lock);
			__pm_relax(&et512_wake_lock);
		goto done;
	case FP_SPICLK_ENABLE:
			DEBUG_PRINT("fp_ioctl <<< FP_SPICLK_ENABLE -------  \n");
			spi_clk_enable(1);
			//mt_spi_enable_clk(egistec_mt_spi_t);
		goto done;
	case FP_SPICLK_DISABLE:
			DEBUG_PRINT("fp_ioctl <<< FP_SPICLK_DISABLE -------  \n");
			spi_clk_enable(0);
			//mt_spi_disable_clk(egistec_mt_spi_t);
		goto done;
	case DELETE_DEVICE_NODE:
			DEBUG_PRINT("fp_ioctl <<< DELETE_DEVICE_NODE -------  \n");
			delete_device_node();
		goto done;
	case GET_SCREEN_ONOFF:
			DEBUG_PRINT("fp_ioctl <<< GET_SCREEN_ONOFF  \n");
			data.int_mode = g_screen_onoff;
		if (copy_to_user((int __user *)arg, &data, sizeof(data))) {
			retval = -EFAULT;
			goto done;
		}

		goto done;

	default:
	retval = -ENOTTY;
	break;
	}



done:

	DEBUG_PRINT("%s ----------- zq done  \n", __func__);
	return (retval);
}

#ifdef CONFIG_COMPAT
static long egistec_compat_ioctl(struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	return egistec_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define egistec_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int egistec_open(struct inode *inode, struct file *filp)
{
	struct egistec_data *egistec;
	int			status = -ENXIO;

	DEBUG_PRINT("%s\n", __func__);
	printk("%s  ---------   zq    \n", __func__);
	
	mutex_lock(&device_list_lock);

	list_for_each_entry(egistec, &device_list, device_entry)
	{
		if (egistec->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}
	if (status == 0) {
		if (egistec->buffer == NULL) {
			egistec->buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (egistec->buffer == NULL) {
//				dev_dbg(&egistec->spi->dev, "open/ENOMEM\n");
				status = -ENOMEM;
			}
		}
		if (status == 0) {
			egistec->users++;
			filp->private_data = egistec;
			nonseekable_open(inode, filp);
		}
	} else {
		pr_debug("%s nothing for minor %d\n"
			, __func__, iminor(inode));
	}
	mutex_unlock(&device_list_lock);
	return status;
}

static int egistec_release(struct inode *inode, struct file *filp)
{
	struct egistec_data *egistec;

	DEBUG_PRINT("%s\n", __func__);

	mutex_lock(&device_list_lock);
	egistec = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	egistec->users--;
	if (egistec->users == 0) {
		int	dofree;

		kfree(egistec->buffer);
		egistec->buffer = NULL;

		/* ... after we unbound from the underlying device? */
		spin_lock_irq(&egistec->spi_lock);
		dofree = (egistec->pd == NULL);
		spin_unlock_irq(&egistec->spi_lock);

		if (dofree)
			kfree(egistec);		
	}
	mutex_unlock(&device_list_lock);
	return 0;

}

int egistec_platformFree(struct egistec_data *egistec)
{
	int status = 0;
	DEBUG_PRINT("%s\n", __func__);
	if (egistec_platformInit_done != 1)
	return status;
	if (egistec != NULL) {
		if (request_irq_done==1)
		{
		free_irq(gpio_irq, NULL);
		request_irq_done = 0;
		}
	gpio_free(egistec->irqPin);
	gpio_free(GPIO_PIN_RESET);
	}

	egistec_platformInit_done = 0;

	DEBUG_PRINT("%s successful status=%d\n", __func__, status);
	return status;
}


int egistec_platformInit(struct egistec_data *egistec)
{
	int status = 0;
	DEBUG_PRINT("%s\n", __func__);

	if (egistec != NULL) {

		/* Initial Reset Pin
		status = gpio_request(egistec->rstPin, "reset-gpio");
		if (status < 0) {
			pr_err("%s gpio_requset egistec_Reset failed\n",
				__func__);
			goto egistec_platformInit_rst_failed;
		}
		*/
//		gpio_direction_output(egistec->rstPin, 1);
//		if (status < 0) {
//			pr_err("%s gpio_direction_output Reset failed\n",
//					__func__);
//			status = -EBUSY;
//			goto egistec_platformInit_rst_failed;
//		}
		
		//added to initialize it as high
//		gpio_set_value(GPIO_PIN_RESET, 1);
//		msleep(30);
		
//		gpio_set_value(GPIO_PIN_RESET, 0);
//		msleep(30);
//		gpio_set_value(GPIO_PIN_RESET, 1);
//		msleep(20);

		/* initial 33V power pin */
//		gpio_direction_output(egistec->vcc_33v_Pin, 1);
//		gpio_set_value(egistec->vcc_33v_Pin, 1);

/*		status = gpio_request(egistec->vcc_33v_Pin, "33v-gpio");
		if (status < 0) {
			pr_err("%s gpio_requset egistec_Reset failed\n",
				__func__);
			goto egistec_platformInit_rst_failed;
		}
		gpio_direction_output(egistec->vcc_33v_Pin, 1);
		if (status < 0) {
			pr_err("%s gpio_direction_output Reset failed\n",
					__func__);
			status = -EBUSY;
			goto egistec_platformInit_rst_failed;
		}

		gpio_set_value(egistec->vcc_33v_Pin, 1);
*/


		/* Initial IRQ Pin
		status = gpio_request(egistec->irqPin, "irq-gpio");
		if (status < 0) {
			pr_err("%s gpio_request egistec_irq failed\n",
				__func__);
			goto egistec_platformInit_irq_failed;
		}
		*/
/*		
		status = gpio_direction_input(egistec->irqPin);
		if (status < 0) {
			pr_err("%s gpio_direction_input IRQ failed\n",
				__func__);
//			goto egistec_platformInit_gpio_init_failed;
		}
*/
//gjx@ODM_WT.BSP.FINGERPRINT 2019/12/19 add for bringup
    egistec_power_on();


	}

	egistec_platformInit_done = 1;

	DEBUG_PRINT("%s successful status=%d\n", __func__, status);
	return status;
/*
egistec_platformInit_gpio_init_failed:
	gpio_free(egistec->irqPin);
//	gpio_free(egistec->vcc_33v_Pin);
egistec_platformInit_irq_failed:
	gpio_free(egistec->rstPin);
egistec_platformInit_rst_failed:
*/
	pr_err("%s is failed\n", __func__);
	return status;
}



static int egistec_parse_dt(struct device *dev,
	struct egistec_data *data)
{
//	struct device_node *np = dev->of_node;
	int errorno = 0;


#ifdef CONFIG_OF
	int ret;

	struct device_node *node = NULL;
	struct platform_device *pdev = NULL;

	printk(KERN_ERR "%s, from dts pinctrl\n", __func__);

	node = of_find_compatible_node(NULL, NULL, "mediatek,egis_fp");
	if (node) {
		pdev = of_find_device_by_node(node);
		if (pdev) {
			data->pinctrl_gpios = devm_pinctrl_get(&pdev->dev);
			if (IS_ERR(data->pinctrl_gpios)) {
				ret = PTR_ERR(data->pinctrl_gpios);
				printk(KERN_ERR "%s can't find fingerprint pinctrl\n", __func__);
				return ret;
			}

			data->pins_irq = pinctrl_lookup_state(data->pinctrl_gpios, "default");
			//printk(KERN_ERR "irq pinctrl_gpios %d, data->pins_irq %d\n", data->pinctrl_gpios, data->pins_irq);
			if (IS_ERR(data->pins_irq)) {
				ret = PTR_ERR(data->pins_irq);
				printk(KERN_ERR "%s can't find fingerprint pinctrl irq\n", __func__);
				return ret;
			}
/*			data->pins_miso_spi = pinctrl_lookup_state(data->pinctrl_gpios, "miso_spi");
			if (IS_ERR(data->pins_miso_spi)) {
				ret = PTR_ERR(data->pins_miso_spi);
				printk(KERN_ERR "%s can't find fingerprint pinctrl miso_spi\n", __func__);
				//return ret;
			}
			data->pins_miso_pullhigh = pinctrl_lookup_state(data->pinctrl_gpios, "miso_pullhigh");
			if (IS_ERR(data->pins_miso_pullhigh)) {
				ret = PTR_ERR(data->pins_miso_pullhigh);
				printk(KERN_ERR "%s can't find fingerprint pinctrl miso_pullhigh\n", __func__);
				//return ret;
			}
			data->pins_miso_pulllow = pinctrl_lookup_state(data->pinctrl_gpios, "miso_pulllow");
			if (IS_ERR(data->pins_miso_pulllow)) {
				ret = PTR_ERR(data->pins_miso_pulllow);
				printk(KERN_ERR "%s can't find fingerprint pinctrl miso_pulllow\n", __func__);
				//return ret;
			}

*/
			data->pins_reset_high = pinctrl_lookup_state(data->pinctrl_gpios, "reset_high");
			if (IS_ERR(data->pins_reset_high)) {
				ret = PTR_ERR(data->pins_reset_high);
				printk(KERN_ERR "%s can't find fingerprint pinctrl reset_high\n", __func__);
				return ret;
			}
			data->pins_reset_low = pinctrl_lookup_state(data->pinctrl_gpios, "reset_low");
			if (IS_ERR(data->pins_reset_low)) {
				ret = PTR_ERR(data->pins_reset_low);
				printk(KERN_ERR "%s can't find fingerprint pinctrl reset_low\n", __func__);
				return ret;
			}
//gjx@ODM_WT.BSP.FINGERPRINT 2019/12/19 add for bringup
#ifdef VCC_SUPPLY_USE_REGULATER
            egistec_regulater = regulator_get(NULL, "vmch");
            if (egistec_regulater == NULL) {
                printk("egistec enter regulator_get vmch fail \n");
                return -1;
            }
            printk("egistec enter regulator_get vmch success \n");
#endif
//gjx@ODM_WT.BSP.FINGERPRINT 2019/12/19 add for bringup
			//printk(KERN_ERR "rest pinctrl_gpios %d, data->pins_reset_high %d, data->pins_reset_low %d\n", data->pinctrl_gpios, data->pins_reset_high, data->pins_reset_low);
			printk(KERN_ERR "%s, get pinctrl success!\n", __func__);
		} else {
			printk(KERN_ERR "%s platform device is null\n", __func__);
		}
	} else {
		printk(KERN_ERR "%s device node is null\n", __func__);
	}



#endif
	return 0;






	DEBUG_PRINT("%s is successful\n", __func__);
	return errorno;
//dt_exit:
	pr_err("%s is failed\n", __func__);
	return errorno;
}
//gjx@ODM_WT.BSP.FINGERPRINT 2019/12/19 add for bringup
static int egistec_power_on(void)
{
#ifdef VCC_SUPPLY_USE_REGULATER
	int ldo_vout_status;

	ldo_vout_status = regulator_set_voltage(egistec_regulater, 3300000, 3300000);
	if (ldo_vout_status < 0) {
		printk("egistec regulator_set_voltage fail \n");
		return -1;
	}
	printk("egistec  regulator_set_voltage success \n");

	ldo_vout_status = regulator_enable(egistec_regulater);
	if (ldo_vout_status < 0) {
		printk("egitec enter cdfinger probe regulator_enable r \n");
		return -1;
	}
	printk("egistec regulator_enable success \n");
#endif

	return 0 ;
}
//gjx@ODM_WT.BSP.FINGERPRINT 2019/12/19 add for bringup

static const struct file_operations egistec_fops = {
	.owner = THIS_MODULE,
	.write = egistec_write,
	.read = egistec_read,
	.unlocked_ioctl = egistec_ioctl,
	.compat_ioctl = egistec_compat_ioctl,
	.open = egistec_open,
	.release = egistec_release,
	.llseek = no_llseek,
	.poll = fps_interrupt_poll
};

/*-------------------------------------------------------------------------*/

static struct class *egistec_class;

/*-------------------------------------------------------------------------*/

static int egistec_probe(struct platform_device *pdev);
static int egistec_remove(struct platform_device *pdev);




typedef struct {
	struct spi_device      *spi;
	struct class           *class;
	struct device          *device;
//	struct cdev            cdev;
	dev_t                  devno;
	u8                     *huge_buffer;
	size_t                 huge_buffer_size;
	struct input_dev       *input_dev;
} et520_data_t;



/* -------------------------------------------------------------------- */
static int et520_spi_probe(struct spi_device *spi)
{
//	struct device *dev = &spi->dev;
	int error = 0;
	et520_data_t *et520 = NULL;
	/* size_t buffer_size; */

	printk(KERN_ERR "et520_spi_probe enter++++++\n");
#if 1
	et520 = kzalloc(sizeof(*et520), GFP_KERNEL);
	if (!et520) {
		/*
		dev_err(&spi->dev,
		"failed to allocate memory for struct et520_data\n");
		*/
		return -ENOMEM;
	}
	printk(KERN_INFO"%s\n", __func__);

	spi_set_drvdata(spi, et520);
	printk(KERN_INFO"%s et520 probe\n", __func__);
#endif	
	g_data->spi = spi;
	printk(KERN_INFO"%s  et520 probe !\n", __func__);
//	egistec_mt_spi_t=spi_master_get_devdata(spi->master);

	return error;
}

/* -------------------------------------------------------------------- */
static int et520_spi_remove(struct spi_device *spi)
{
	et520_data_t *et520 = spi_get_drvdata(spi);
	spi_clk_enable(0);
//	pr_debug("%s\n", __func__);

//	et520_manage_sysfs(et520, spi, false);

	//et520_sleep(et520, true);

	//cdev_del(&et520->cdev);

	//unregister_chrdev_region(et520->devno, 1);

	//et520_cleanup(et520, spi);
	kfree(et520);

	return 0;
}


static struct spi_driver spi_driver = {
	.driver = {
		.name	= "et520",
		.owner	= THIS_MODULE,
		.of_match_table = et520_spi_of_match,
        .bus	= &spi_bus_type,
	},
	.probe	= et520_spi_probe,
	.remove	= et520_spi_remove
};

struct spi_board_info spi_board_devs[] __initdata = {
    [0] = {
        .modalias="et520",
        .bus_num = 0,
        .chip_select=1,
        .mode = SPI_MODE_0,
    },
};


static struct platform_driver egistec_driver = {
	.driver = {
		.name		= "et520",
		.owner		= THIS_MODULE,
		.of_match_table = egistec_match_table,
	},
    .probe =    egistec_probe,
    .remove =   egistec_remove,
};


static int egistec_remove(struct platform_device *pdev)
{
//	#if EGIS_NAVI_INPUT
	struct device *dev = &pdev->dev;
	struct egistec_data *egistec = dev_get_drvdata(dev);
//	#endif
	
    DEBUG_PRINT("%s(#%d)\n", __func__, __LINE__);
	free_irq(gpio_irq, NULL);
	
	#if EGIS_NAVI_INPUT
	uinput_egis_destroy(egistec);
	sysfs_egis_destroy(egistec);
	#endif
	
	del_timer_sync(&fps_ints.timer);
	request_irq_done = 0;
	kfree(egistec);
    return 0;
}
extern char* saved_command_line;

static int egistec_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct egistec_data *egistec;
	int status = 0;
	unsigned long minor;
	
	DEBUG_PRINT("%s initial\n", __func__);
	pr_info("%s initial\n", __func__);
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(ET520_MAJOR, "et520", &egistec_fops);
	if (status < 0) {
			pr_err("%s register_chrdev error.\n", __func__);
			return status;
	}

	egistec_class = class_create(THIS_MODULE, "et520");
	
	if (IS_ERR(egistec_class)) {
		pr_err("%s class_create error.\n", __func__);
		unregister_chrdev(ET520_MAJOR, egistec_driver.driver.name);
		return PTR_ERR(egistec_class);
	}

	/* Allocate driver data */
	egistec = kzalloc(sizeof(*egistec), GFP_KERNEL);
	if (egistec== NULL) {
		pr_err("%s - Failed to kzalloc\n", __func__);
		return -ENOMEM;
	}

	egistec->notifier.notifier_call = egistec_fb_notifier_callback;
	status = fb_register_client(&egistec->notifier);
	if (status){
		DEBUG_PRINT(" register fb failed, retval=%d\n", status);
	}

	/* device tree call */
	if (pdev->dev.of_node) {
		status = egistec_parse_dt(&pdev->dev, egistec);
		if (status) {
			pr_err("%s - Failed to parse DT\n", __func__);
			goto egistec_probe_parse_dt_failed;
		}
	}


//	egistec->rstPin = GPIO_PIN_RESET;
//	egistec->irqPin = GPIO_PIN_IRQ;
//	egistec->vcc_33v_Pin = GPIO_PIN_33V;



	/* Initialize the driver data */
	egistec->pd = pdev;
	g_data = egistec;
	//wake_lock_init(&et512_wake_lock, WAKE_LOCK_SUSPEND, "et512_wake_lock");
	wakeup_source_init(&et512_wake_lock, "et512_wake_lock");
	DEBUG_PRINT("egistec_wake_lock\n");
	DEBUG_PRINT("egistec_module_probe\n");
	spin_lock_init(&egistec->spi_lock);
	mutex_init(&egistec->buf_lock);
	mutex_init(&device_list_lock);

	INIT_LIST_HEAD(&egistec->device_entry);
#if 1
	/* platform init */
	status = egistec_platformInit(egistec);
	if (status != 0) {
		pr_err("%s platforminit failed\n", __func__);
		goto egistec_probe_platformInit_failed;
	}
#endif

#if 0 //gpio_request later
	/* platform init */
	status = egistec_platformInit(egistec);
	if (status != 0) {
		pr_err("%s platforminit failed\n", __func__);
		goto egistec_probe_platformInit_failed;
	}
#endif

	fps_ints.drdy_irq_flag = DRDY_IRQ_DISABLE;

	/*
	 * If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *fdev;
		egistec->devt = MKDEV(ET520_MAJOR, minor);
		fdev = device_create(egistec_class, &pdev->dev, egistec->devt,
					egistec, "esfp0");
		status = IS_ERR(fdev) ? PTR_ERR(fdev) : 0;
	} else {
		dev_dbg(&pdev->dev, "no minor number available!\n");
		status = -ENODEV;
	}
	if (status == 0) {
		set_bit(minor, minors);
		list_add(&egistec->device_entry, &device_list);
	}

	mutex_unlock(&device_list_lock);

	if (status == 0){
		dev_set_drvdata(dev, egistec);
	}
	else {
		goto egistec_probe_failed;
	}

	////gpio_request later
	//egistec_reset(egistec);

	fps_ints.drdy_irq_flag = DRDY_IRQ_DISABLE;

	/* the timer is for ET310 */
	setup_timer(&fps_ints.timer, interrupt_timer_routine,(unsigned long)&fps_ints);
	add_timer(&fps_ints.timer);

/*	

	struct device_node *node = NULL;
	int value;

	node = of_find_compatible_node(NULL, NULL, "goodix,goodix-fp");

	mt_spi_enable_master_clk(gf_dev->spi);

*/





	#if EGIS_NAVI_INPUT
	/*
	 * William Add.
	 */
	sysfs_egis_init(egistec);
	uinput_egis_init(egistec);
	#endif

	DEBUG_PRINT("%s : initialize success %d\n", __func__, status);	

	request_irq_done = 0;
	return status;

egistec_probe_failed:
	device_destroy(egistec_class, egistec->devt);
	class_destroy(egistec_class);

egistec_probe_platformInit_failed:
egistec_probe_parse_dt_failed:
	kfree(egistec);
	pr_err("%s is failed\n", __func__);
	return status;
}

static void delete_device_node(void)
{
	//int retval;
	DEBUG_PRINT("%s\n", __func__);
	//del_timer_sync(&fps_ints.timer);
	//spi_clk_enable(0);
	device_destroy(egistec_class, g_data->devt);
	DEBUG_PRINT("device_destroy \n");
	list_del(&g_data->device_entry);
	DEBUG_PRINT("list_del \n");
	class_destroy(egistec_class);
	DEBUG_PRINT("class_destroy\n");
        //spi_unregister_driver(&spi_driver);
	//DEBUG_PRINT("spi_unregister_driver\n");
	unregister_chrdev(ET520_MAJOR, egistec_driver.driver.name);
	DEBUG_PRINT("unregister_chrdev\n");
	g_data = NULL;
	//platform_driver_unregister(&egistec_driver);
	//DEBUG_PRINT("platform_driver_unregister\n");
}


static int __init egis520_init(void)
{
	int status = 0;
	int rc = 0;
	pr_err("[%s] save_command_line =%s.\n", __func__, saved_command_line);
	if(!strstr(saved_command_line,"fingerprint=egis")){
		return -7;
	}
//liukangping@huaqin.com add fp id gpio 20180716 begin
   //  int sim_gpio = -1;
     // int sim_val = 1;
    //struct device_node *node = NULL;
//liukangping@huaqin.com add fp id gpio 20180716 end
//add for dual sensor start
#if 0
//    struct device_node *fp_id_np = NULL;
    //int fp_id_gpio, fp_id_gpio_value;
/*
    fp_id_np = of_find_matching_node(fp_id_np, fpswitch_match_table);
    if (fp_id_np)
	fp_id_gpio = of_get_named_gpio(fp_id_np, "egis,id-gpio", 0);
    if (fp_id_gpio < 0) {
	return status;
    } else {
	pr_info("%s: fp_id pin=%d\n",__func__, fp_id_gpio);
    }
*/
	
   //fp_id_gpio = 174;

    //gpio_direction_input(fp_id_gpio);
    //fp_id_gpio_value = gpio_get_value(fp_id_gpio);

  //liukangping@huaqin.com add fp id gpio 20180716 begin
       node = of_find_compatible_node(NULL, NULL, "mediatek,fp_id");
       sim_gpio = of_get_named_gpio(node, "fp-id-gpios", 0);
       sim_val = __gpio_get_value(sim_gpio);
   //liukangping@huaqin.com add fp id gpio 20180716 end
    pr_info("%s, Get FP_ID from GPIO_PIN is / FP_ID = %d.\n", __func__,sim_val);

    if(sim_val==1){
        
      pr_info("%s, Need to register egistec FP driver\n", __func__);
    }else{
        
        pr_info("%s, Don't need to register egistec FP driver\n", __func__);
        return status;
    }

#endif
//add for dual sensor end



	printk(KERN_INFO "%s\n", __func__);
	

     status = platform_driver_register(&egistec_driver);
	 
	pr_info("%s, status=%d", __func__, status);
	printk(KERN_INFO "%s   01  \n", __func__);
	printk(KERN_ERR "%s   01  \n", __func__);	
	
/*	fpc_irq_platform_device = platform_device_register_simple(
							FPC_IRQ_DEV_NAME,
							0,
							NULL,
							0);

	if (IS_ERR(fpc_irq_platform_device))
		return PTR_ERR(fpc_irq_platform_device); */
	rc = spi_register_board_info(spi_board_devs, ARRAY_SIZE(spi_board_devs));
	
	printk(KERN_ERR "%s  02  \n", __func__);

	if (rc)
	{
		printk(KERN_ERR "spi register board info%s\n", __func__);		
	        return -EINVAL;
	}

	if (spi_register_driver(&spi_driver))
	{
		printk(KERN_ERR "register spi driver fail%s\n", __func__);
		return -EINVAL;
	}

//			spi_clk_enable(1);
//	mt_spi_enable_clk(egistec_mt_spi_t);//temp
//	printk(KERN_ERR "spi enabled----\n");

     return status;
}

static void __exit egis520_exit(void)
{

      platform_driver_unregister(&egistec_driver);
      spi_unregister_driver(&spi_driver);
}

module_init(egis520_init);
module_exit(egis520_exit);

MODULE_AUTHOR("Wang YuWei, <robert.wang@egistec.com>");
MODULE_DESCRIPTION("SPI Interface for ET520");
MODULE_LICENSE("GPL");
