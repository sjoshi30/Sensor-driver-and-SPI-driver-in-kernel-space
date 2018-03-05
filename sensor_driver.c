/* Course : CSE 438 - Embedded Systems Programming
 * Assignment 3 : Device Driver to implement Ultrasonic sensor in kernel space
 * Team Member1 : Samruddhi Joshi  ASUID : 1213364722
 * Team Member2 : Varun Joshi 	   ASUID : 1212953337 
 */
 
#define _GNU_SOURCE
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <asm/errno.h>
#include <linux/math64.h>

/* User defined constants and Macros */
#define DEVICE_NAME "sensor"
#define GP_IO0 11  			
#define GP_IO3 14  			
#define GP_IO0_MUX 32  		
#define GP_IO3_MUX 16  		
#define GPIO_DIRECTION_IN  1
#define GPIO_DIRECTION_OUT 0
#define GPIO_VALUE_LOW  0
#define GPIO_VALUE_HIGH 1
#define RISE_DETECTION 0
#define FALL_DETECTION 1

static unsigned char Edge = RISE_DETECTION;

static struct sensor_dev
{
	struct cdev cdev;               	/* The cdev structure */
	char *name;                  		/* Name of device */
	unsigned int BUSY_FLAG;		  		/* Busy Flag Status */
	unsigned long long timeRising;		/* TimeStamp to record Start Time */
	unsigned long long timeFalling;		/* TimeStamp to record End Time */
	int irq; 							/* IRQ Vector Number*/
} *sensor_dev;

static dev_t sensor_id;
struct class *sensor_dev_class;          /* Tie with the device model */


/***********************************************************************
 * Description : rdtsc() function is used to calulcate the number of clock ticks
 * and measure the time. 
 **********************************************************************/
static __inline__ unsigned long long my_rdtsc(void)
 {
     unsigned long lo, hi;
     __asm__ __volatile__ ( "rdtsc" : "=a" (lo), "=d" (hi) ); 
     return( (unsigned long long)lo | ((unsigned long long)hi << 32) );
 }	

/***********************************************************************
* Description: Interrupt Service routine to capture the rising edge and
* falling edge timestamps
***********************************************************************/
irqreturn_t mysensor_interrupt(int irq, void *dev_id)
{
	
	if(Edge==RISE_DETECTION)
	{
		/* Capture the rising edge time and change the interrupt detect to Falling edge*/
		sensor_dev->timeRising = my_rdtsc();
	    irq_set_irq_type(irq, IRQF_TRIGGER_FALLING);
	    Edge=FALL_DETECTION;
	}
	else
	{
		/* Capture the falling edge time and change the interrupt detect to Rising edge*/
		sensor_dev->timeFalling = my_rdtsc();
	    irq_set_irq_type(irq, IRQF_TRIGGER_RISING);
	    Edge=RISE_DETECTION;
		sensor_dev->BUSY_FLAG = 0;
	}
	
	return IRQ_HANDLED;
}

/***********************************************************************
* Description - This is function that will be called when the device is
* 				opened
* Arguments - inode: Inode structure
* 			  file: File Pointer
***********************************************************************/
int sensor_open(struct inode *inode, struct file *file)
{
	int irq_number;
	int irq_rising_edge;
	int retValue;
	struct sensor_dev *sensor_devp;

	/* Get the per-device structure that contains this cdev */
	sensor_devp = container_of(inode->i_cdev, struct sensor_dev, cdev);
	
	/* Easy access to cmos_devp from rest of the entry points */
	file->private_data = sensor_devp;
	
	/* Free the GPIO Pins */
	gpio_free(GP_IO0);
	gpio_free(GP_IO3);
	gpio_free(GP_IO0_MUX);
	gpio_free(GP_IO3_MUX);
	
	/* Set GPIO pins directions and values */
	gpio_request_one(GP_IO0_MUX, GPIOF_OUT_INIT_LOW , "gpio32");
	gpio_request_one(GP_IO3_MUX, GPIOF_OUT_INIT_LOW , "gpio16");
	gpio_request_one(GP_IO0, GPIOF_OUT_INIT_LOW , "gpio11");
	gpio_request_one(GP_IO3, GPIOF_IN , "gpio14");
	
	/* Set GPIO pins values */
	gpio_set_value_cansleep(76, GPIO_VALUE_LOW);
	gpio_set_value_cansleep(64, GPIO_VALUE_LOW);

	gpio_set_value_cansleep(GP_IO0, GPIO_VALUE_LOW);
	gpio_set_value_cansleep(GP_IO3, GPIO_VALUE_LOW);
	gpio_set_value_cansleep(GP_IO0_MUX, GPIO_VALUE_LOW);
	gpio_set_value_cansleep(GP_IO3_MUX, GPIO_VALUE_HIGH);

	/* Map your GPIO to an IRQ */
	irq_number = gpio_to_irq(GP_IO3);
	
	if(irq_number < 0)
	{
		printk("Gpio %d cannot be used as interrupt",GP_IO3);
		retValue=-EINVAL;
	}

	sensor_devp->irq = irq_number;
	sensor_devp->BUSY_FLAG = 0;
	sensor_devp->timeRising=0;
	sensor_devp->timeFalling=0;
	
	/* Request an interrupt Service Routine */
	/* request_irq arguments : requested interrupt, 
				   (irq_handler_t) irqHandler, // pointer to handler function
                   IRQF_TRIGGER_RISING, // interrupt mode flag
                   "irqHandler",        // used in /proc/interrupts
                   devp);               // the *dev_id shared interrupt lines */
	irq_rising_edge = request_irq(irq_number, mysensor_interrupt, IRQF_TRIGGER_RISING, "mysensor_interrupt", sensor_dev);
	printk(KERN_INFO"IRQ NUMBER IS %d\n",irq_number);
	if(irq_rising_edge)
	{
		printk("Unable to set irq %d; error %d\n", irq_number, irq_rising_edge);
		return 0;
	}
	
	return 0;
}

/***********************************************************************
* Description - This is is used by the driver to close anything which 
* 				has been opened and used during driver execution.
* 
* Arguments - inode: Inode structure
* 			  file: File Pointer
***********************************************************************/
int sensor_release(struct inode *inode, struct file *file)
{
	struct sensor_dev *sensor_devp = file->private_data;
	
	sensor_dev->BUSY_FLAG = 0;

	/* Release the resources */
	free_irq(sensor_devp->irq,sensor_devp);
	gpio_free(GP_IO0);
	gpio_free(GP_IO3);
	gpio_free(GP_IO0_MUX);
	gpio_free(GP_IO3_MUX);
	
	printk("Sensor is closing\n");
	return 0;
}

/***********************************************************************
* Description - This function is used to generate a trigger pulse for the
* 				sensor.
* Arguments - 
* 				file: File Pointer
* 				buf: Buffer
* 				count: Size of Buffer
* 				ppos: Position Pointer
***********************************************************************/
ssize_t sensor_write(struct file *file, const char* msg,
           size_t count, loff_t *ppos)
{
	int ret = 0;
	struct sensor_dev *sensor_devp = file->private_data;
	
	if(sensor_dev->BUSY_FLAG == 1)
	{
		ret = -EBUSY;
		return ret;
	}
	
	/* Generate a 10us trigger pulse */
	gpio_set_value_cansleep(GP_IO0, GPIO_VALUE_HIGH);
	udelay(15);
	gpio_set_value_cansleep(GP_IO0, GPIO_VALUE_LOW);
	sensor_devp->BUSY_FLAG = 1;

	return ret;
}

/***********************************************************************
* Description - To poll the echo pin and calculate the distance which is 
* 				passed to user application
* Arguments - 
* 				file: File Pointer
* 				msg: Buffer
* 				count: Size of Buffer
* 				ppos: Position Pointer
***********************************************************************/
ssize_t sensor_read(struct file *file, char* msg,
           size_t count, loff_t *ppos)
{
	struct sensor_dev *sensor_devp = file->private_data;
	int ret=0;
	//unsigned int temp;
	unsigned long long Difftime;
	
	if(sensor_devp->BUSY_FLAG == 1)
	{
		return -EBUSY;
	}
	else
	{
		if(sensor_devp->timeRising == 0 && sensor_devp->timeFalling == 0)
		{
			printk("Trigger is not initiated \n");
		}
		else
		{
			
			Difftime = (sensor_devp->timeFalling) - (sensor_devp->timeRising);
			ret = copy_to_user((void *)msg, (const void *)&Difftime, sizeof(Difftime));
		}
	}
	printk(KERN_DEBUG"In read\n");
	
	return ret;
}


static struct file_operations sensor_fops =
{
		.owner = THIS_MODULE,			/* Owner */
		.open = sensor_open,            /* Open method */
		.release = sensor_release,      /* Release method */
		.write = sensor_write,          /* Write method */
		.read = sensor_read				/* Read method */
};

/***********************************************************************
* Description - This function is called to initialize the Ultrasonic 
* 				sensor.
***********************************************************************/
int __init sensor_driver_init(void)
{
	int ret;
	
	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&sensor_id, 0, 1, DEVICE_NAME) < 0) {
		printk(KERN_DEBUG"Can't register device\n"); 
		return -1;
	}
	
	/* Populate sysfs entries */
	sensor_dev_class = class_create(THIS_MODULE, DEVICE_NAME);

	sensor_dev = (struct sensor_dev*)kmalloc(sizeof(struct sensor_dev), GFP_KERNEL);
	if (!sensor_dev) {
	printk("Bad Kmalloc\n"); 
	return -ENOMEM;
	}

	sensor_dev-> name = "sensor";

	/* Send uevents to udev, so it'll create /dev nodes */
	if((device_create(sensor_dev_class, NULL, MKDEV(MAJOR(sensor_id), MINOR(sensor_id)), NULL, DEVICE_NAME) == NULL))
	{
		class_destroy(sensor_dev_class);
		unregister_chrdev_region(sensor_id,1);
		printk(KERN_DEBUG"Cannot register device");
	}

	/* Connect the file operations with the cdev */
	cdev_init(&sensor_dev->cdev,&sensor_fops);
	sensor_dev->cdev.owner = THIS_MODULE;
	/* Connect the major/minor number to the cdev */
	ret = cdev_add(&sensor_dev->cdev, MKDEV(MAJOR(sensor_id), MINOR(sensor_id)), 1);
	if (ret) {
	printk("Bad cdev\n");
	return ret;
	}
		
	printk("Sensor Driver created\n");
	
	return 0;
}

/***********************************************************************
* Description - This function is called when the driver is about to exit.
* 			It is cleaning fuction 
***********************************************************************/
void __exit sensor_driver_exit(void)
{
	/* Destroy device */
	device_destroy (sensor_dev_class, MKDEV(MAJOR(sensor_id), MINOR(sensor_id)));
	/* Delete cdev entries */
	cdev_del(&sensor_dev->cdev);
	kfree(sensor_dev);
	/* Destroy driver_class */
	class_destroy(sensor_dev_class);
	/* Release the major number */
	unregister_chrdev_region(sensor_id, 1);

	printk("Sensor Driver removed \n");
}

module_init(sensor_driver_init);
module_exit(sensor_driver_exit);
MODULE_LICENSE("GPL v2");