/*  
 * HC SR04 Ultrasonic Ranging Module driver
 * Copyright (C) 2014  Zoltan Hanisch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/fcntl.h>
#include <linux/mman.h>
#include <stdbool.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/ktime.h>

#define DRIVER_AUTHOR "Zoltan Hanisch <zoltan.hanisch.dev@gmail.com>"
#define DRIVER_DESC   "HC SR04 Ultrasonic Ranging Module driver"

//#define DEBUG

#ifdef DEBUG
#define MSG_TO_LOG(...) \
    pr_info(__VA_ARGS__)
#else
#define MSG_TO_LOG(...) ;
#endif

MODULE_LICENSE("GPL V2");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

#define MAJORDEV 240
#define MINORDEV 0

// Set up
static int __init us_service_init(void);
static void __exit us_service_cleanup(void);
static struct cdev cdev;

//For accessing the gpio
#define GPIO_ECHO 3 // GPIO 3 the echo pin
#define GPIO_TRIGGER 2 // GPIO 2 trigger pin
char *GPIO_ECHO_DESC = "GPIO_ECHO";

// For measuring
volatile unsigned int measured_data = 0;
volatile ktime_t start; // starting time of measurement
volatile ktime_t end; // ending time of measurement
ssize_t get_data(struct file *, char *, size_t, loff_t *); // helper function of reading the data by user space program
ssize_t start_measuring(struct file *, char *, size_t, loff_t *); // helper function for starting the measurement by user space program
volatile int measuring_in_progress; // helper variable

// Interrupt handling
static irqreturn_t irq_handler(int irq, void *dev_id, struct pt_regs *regs);
void intr_config(void);
void intr_release();
short int irq_echo_gpio = 0; // Interrupt variable

//*******************************************************************
static struct file_operations usec_fops = {
    .owner = THIS_MODULE,
    .read = get_data,
    .write = start_measuring,
};
//*******************************************************************
static int __init us_service_init(void)
{
    int err_code;
    dev_t devnum = MKDEV(MAJORDEV, MINORDEV);

    // registers the character major/minor
    err_code = register_chrdev_region(devnum, 1, "us_service");
    if (err_code)
    {
        goto error;
    }
    // sets up the chardev control structure
    cdev_init(&cdev, &usec_fops);
    cdev.owner = THIS_MODULE;
    cdev.ops = &usec_fops;
    err_code = cdev_add(&cdev, devnum, 1);
    if (err_code)
    {
        goto error;
    }
    pr_info("Ultra sonic service module initialized;\n");
    
    gpio_direction_output(GPIO_TRIGGER, 1);

    intr_config();
    return 0;

error:
    us_service_cleanup();
    pr_err("us_service module failed to initialize, err = %d\n", err_code);
    return -ENODEV;
}
//*******************************************************************
static void __exit us_service_cleanup(void)
{
    dev_t devnum = MKDEV(MAJORDEV, MINORDEV);

    unregister_chrdev_region(devnum, 1);
    intr_release();
}
//*******************************************************************
ssize_t start_measuring(struct file *filp, char __user * buff, size_t count, loff_t * offset)
{
    int counter = 0; // helper variable to detect faulty triggering process

    if (measuring_in_progress) // measuring in progress!
    {
        pr_info("Error! Measuring hasn't been finished.");
        return 0;
    }
    
    measured_data = 0;

    gpio_set_value(GPIO_TRIGGER, 0);
    udelay(10);
    gpio_set_value(GPIO_TRIGGER, 1);

    // Waiting for the low-level
    while(counter++ <1000000 && (gpio_get_value(GPIO_ECHO) != 0));

    if (counter >= 1000000)
    {
        pr_info("Timeout in start_measuring()\n");
        return 0;
    }
    MSG_TO_LOG("Starting ultrasonic measuring... %d\n", counter);
    measuring_in_progress = 1;
    start = ktime_get();

    return 0;
}
//*******************************************************************
ssize_t get_data(struct file *filp, char __user * buff, size_t count,
                 loff_t * offset)
{
    unsigned int dist = measured_data;

    if (count != 4 || measuring_in_progress)
    {
        pr_info("Ultrasonic measuring error\n");
        return 0;
    }

    MSG_TO_LOG("get_data(): %u\n", dist);
    return count - copy_to_user(buff, &dist, sizeof(unsigned int));
}
//*******************************************************************
void intr_config()
{
    if (gpio_request(GPIO_ECHO, GPIO_ECHO_DESC))
    {
        pr_info("GPIO request faiure: %s\n", GPIO_ECHO_DESC);
        return;
    }

    if ( (irq_echo_gpio = gpio_to_irq(GPIO_ECHO)) < 0 )
    {
        pr_info("GPIO to IRQ mapping faiure %s\n", GPIO_ECHO_DESC);
        return;
    }

    pr_info(KERN_NOTICE "Mapped int %d\n", GPIO_ECHO);

    if (request_irq(irq_echo_gpio,
                    (irq_handler_t ) irq_handler,
                    IRQF_TRIGGER_RISING,
                    GPIO_ECHO_DESC,
                    NULL))
    {
        pr_info("Irq Request failure\n");
        return;
    }

    pr_info("Configuration of intr of ultra sonic suceeded\n");
}
//*******************************************************************
void intr_release()
{
    pr_info("Release itr\n");
    free_irq(irq_echo_gpio, NULL);
    gpio_free(irq_echo_gpio);

    return;
}
//*******************************************************************
static irqreturn_t irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
    unsigned long flags;

    if (!measuring_in_progress)
    {
        pr_info("%lld: Not expected trigger of irq handler", ktime_to_ns(ktime_get()));
        return IRQ_HANDLED;
    }
    MSG_TO_LOG("us_service irq handler was triggered\n");

    local_irq_save(flags);

    end = ktime_get();
    s64 diff_time = ktime_to_ns(ktime_sub(end, start));
    measured_data =  (unsigned int) diff_time;

    measuring_in_progress = 0;

    local_irq_restore(flags);
    //
    return IRQ_HANDLED;
}
//*******************************************************************
module_init(us_service_init);
module_exit(us_service_cleanup);
