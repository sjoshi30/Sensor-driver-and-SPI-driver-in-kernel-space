/* Course : CSE 438 - Embedded Systems Programming
 * Assignment 3 : Device Driver to implement SPI Led Matrix in kernel space
 * Team Member1 : Samruddhi Joshi  ASUID : 1213364722
 * Team Member2 : Varun Joshi 	   ASUID : 1212953337 
 */

#define _GNU_SOURCE
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/cdev.h>

#define DRIVER_NAME  "spidev"
#define DEVICE_NAME  "myspidev"
#define GPIO_VALUE_HIGH 1
#define GPIO_VALUE_LOW  0
#define MINOR_NUMBER    0
#define MAJOR_NUMBER   154 

struct spi_device_id temp_spi[] = {
	{"spidev",0},
	{}
};

/* Structure for SPIDev */
struct spidev_data{
	dev_t devt;
	struct spi_device *spi;
	char pattern_buffer[10][8];
	unsigned int sequence_buffer[10][2];
};

static struct gpio request_gpio_arr[] = 
{
	{15, 0, "GPIO15"},
	{24, 0, "GPIO24"},
	{42, 0, "GPIO42"},
	{30, 0, "GPIO30"},
	{72, 0, "GPIO72"},
	{44, 0, "GPIO44"},
	{46, 0, "GPIO46"},
};

struct class *led_dev_class;          /* Tie with the device model */
static struct spidev_data *spidev;
static struct task_struct *ktask;
static unsigned bufsize = 4096;
static unsigned int busyFlag = 0;
struct spi_message msg;
unsigned char tx[2]={0};
unsigned char rx[2]={0};
struct spi_transfer tr = {
			.tx_buf = &tx[0],
			.rx_buf = &rx[0],
			.len = 2,
			.cs_change = 1,
			.bits_per_word = 8,
			.speed_hz = 500000,
			 };

/* Local Function Declarations */
int kthread_display(void *data);


/***********************************************************************
 * Description : Local function to add the new message to queue and send 
 * it on SPI bus 
 **********************************************************************/
static void transfer(unsigned char address, unsigned char data)
{
    tx[0] = address;
    tx[1] = data;
    
    /* Message initialised */
	spi_message_init(&msg);
	/* Message added to tail */
	spi_message_add_tail(&tr, &msg);
	/* Enable the Chip Select and send the message */
	gpio_set_value_cansleep(15, GPIO_VALUE_LOW);
	spi_sync(spidev->spi, &msg);
	gpio_set_value_cansleep(15, GPIO_VALUE_HIGH);
}

/***********************************************************************
 * Description : Resposible for LED Matrix initialisation when device is 
 * opened
 * Arguments - inode: Inode structure
 * 			   file: File Pointer
 **********************************************************************/
static int led_open(struct inode *inode, struct file *file)
{
	int i=0;

	busyFlag = 0;

	/* Init Display sequence */
	transfer(0x0F, 0x01);
	/* Clear the Display */
	transfer(0x0F, 0x00);
	/* Enable mode B */
	 transfer(0x09, 0x00);
	/* Define Intensity */
	transfer(0x0A, 0x04);	
	/* Only scan 7 digit */
	 transfer(0x0B, 0x07);
	/* Turn on chip */
	 transfer(0x0C, 0x01);

	/*Clear the LED Display */
	for(i=1; i < 9; i++)
	{
		transfer(i, 0x00);
	}

	return 0;
}

/***********************************************************************
 * Description : Function to free the resources when the device is closed
 * Arguments - inode: Inode structure
 * 			   file: File Pointer
 **********************************************************************/
static int led_release(struct inode *inode, struct file *file)
{
    unsigned char i=0;

    busyFlag = 0;
   
    /* Clear the Display */
	for(i=1; i < 9; i++)
	{
		transfer(i, 0x00);
	}
	/* Unexport the GPIOs */
	gpio_free(42);
	gpio_free(30);
	gpio_free(15);
	gpio_free(24);
	
	printk(KERN_DEBUG"spidev is closing\n");
	return 0;
}

/***********************************************************************
 * Description : Function to copy from user the patterns which are used 
 * in printing
 **********************************************************************/
static long led_ioctl(struct file *file, unsigned int arg, unsigned long cmd)
{
	int i=0, j=0;
	char writeBuffer[10][8];
	int ret;
    /* Copy the pattern buffer from the user application */
	ret = copy_from_user((void *)&writeBuffer, (void *)arg, sizeof(writeBuffer));

	if(ret != 0)
	{
		printk("Failure : %d number of bytes that could not be copied.\n",ret);
	}

	for(i=0;i<10;i++)
	{
		for(j=0;j<8;j++)
		{/* Store internally in global buffer */
			spidev->pattern_buffer[i][j] = writeBuffer[i][j];
		}
	}
	
	return ret;
}

/***********************************************************************
 * Description : Initiates the sequence write (in kthread) and returns
 * Arguments - 
 * 				file: File Pointer
 * 				buf: Buffer
 * 				count: Size of Buffer
 * 				ppos: Position Pointer
 **********************************************************************/
ssize_t led_write(struct file *file, const char* buf,
           size_t count, loff_t *ppos)
{
	int ret= 0, i=0, j=0;
	unsigned int tempBuffer[20];

	
	/* Is the previous write completed ?*/
	if(busyFlag == 1)
	{
		return -EBUSY;
	}
	if (count > bufsize)
	{
		return -EMSGSIZE;
	}	
	
	/* Copy the sequence from the user application */				
	ret = copy_from_user((void *)&tempBuffer, (void *)buf, sizeof(tempBuffer));
	for(i=0;i<20;i=i+2)
	{
		/* Store internally in global buffer */
		j=i/2;
		spidev->sequence_buffer[j][0] = tempBuffer[i];
		spidev->sequence_buffer[j][1] = tempBuffer[i+1];
	}
	if(ret != 0)
	{
		printk("Failure : %d bytes that could not be copied.\n",ret);
	}
	
	busyFlag = 1;
    /* Kthread_run creates and wakes up the task */
    ktask =  kthread_run(&kthread_display, (void *)tempBuffer,"kthread_spi_led");
   
	return ret;
}

/***********************************************************************
 * Description : This kernel thread actually writes the sequence 
 * in background. Finds the required pattern from the buffer and uses 
 * local API transfer to write to spi bus
 **********************************************************************/
int kthread_display(void *data)
{
	int i=0, j=0, k=0;
	
	/*If the initial sequence is 0,0 do not print anything */
	if(spidev->sequence_buffer[0][0] == 0 && spidev->sequence_buffer[0][1] == 0)
	{
		for(k=1; k < 9; k++)
		{
			transfer(k, 0x00);
		}
		busyFlag = 0;
		goto sequenceEnd;
	}
	
	/*If sequence pattern followed by 0,0 is present, then display the pattern in loop upto 0,0.*/
	for(j=0;j<10;j++) /* loop for sequence order */
	{
		for(i=0;i<10;i++)/* loop for pattern number */
		{	/*Found the required sequence from the pattern buffer */
			if(spidev->sequence_buffer[j][0] == i)
			{
				if((spidev->sequence_buffer[j][0] == 0) && (spidev->sequence_buffer[j][1] == 0))
				{/* 0,0 detected, clear the display*/
					busyFlag = 0;
					goto sequenceEnd;
				}
				else
				{
					/* Display the pattern for each of the 8 led rows*/
					transfer(0x01, spidev->pattern_buffer[i][0]);
					
					transfer(0x02, spidev->pattern_buffer[i][1]);
					
					transfer(0x03, spidev->pattern_buffer[i][2]);
					
					transfer(0x04, spidev->pattern_buffer[i][3]);
					
					transfer(0x05, spidev->pattern_buffer[i][4]);
					
					transfer(0x06, spidev->pattern_buffer[i][5]);
					
					transfer(0x07, spidev->pattern_buffer[i][6]);
					
					transfer(0x08, spidev->pattern_buffer[i][7]);
					
					msleep(spidev->sequence_buffer[j][1]);	
				}
			}
		}
	}
	sequenceEnd:
	busyFlag = 0;
	return 0;
}

/***********************************************************************
 * Description : Function to perform per-device initialisation, allocating
 * resource, registering the device with kernel as block. (Actual device)
 * Arguments - spi_device to be probed
 **********************************************************************/
static int spidev_probe(struct spi_device *spi)
{
	struct device *dev;
	
	/* Allocate driver data */
	spidev = kzalloc(sizeof(*spidev), GFP_KERNEL);
	if (!spidev)
		return -ENOMEM;

	/* Initialize the driver data */
	spidev->spi = spi;

	spidev->devt = MKDEV(MAJOR_NUMBER, MINOR_NUMBER);

    dev = device_create(led_dev_class, &spi->dev, spidev->devt, spidev, DEVICE_NAME);

    if(dev == NULL)
    {
		printk("Device Creation Failed\n");
		kfree(spidev);
		return -1;
	}
	
	printk("LED Driver Probed \n");
	
	return 0;
}

/***********************************************************************
 * Description : When the device is disconnected, this function releases
 * the device resources
 * Arguments - spi device to be removed
 **********************************************************************/
static int spidev_remove(struct spi_device *spi)
{
	device_destroy(led_dev_class, spidev->devt);
	kfree(spidev);
	printk("SPI LED Driver Removed.\n");
	return 0;
}

/* This is the driver that will be inserted */
static struct spi_driver spi_led_driver = {
		 .id_table = temp_spi,
         .driver = {
         .name =         "spidev",
         .owner =        THIS_MODULE,
         },
         .probe =        spidev_probe,
         .remove =       spidev_remove,
};


static struct file_operations led_fops = {
  .owner   			= THIS_MODULE,
  .write   			= led_write,
  .open    			= led_open,
  .release 			= led_release,
  .unlocked_ioctl   = led_ioctl,
};

/***********************************************************************
 * Description : Creates and initialises the spi device when the module
 * is inserted. Also initialises the MOSI, MISO, CS, CLK pins and the 
 * corresponding mux
 **********************************************************************/
static int __init led_driver_init(void)
{
	
	int retValue;
	
	/* Register the Device */
	retValue = register_chrdev(MAJOR_NUMBER, DEVICE_NAME, &led_fops);
	if(retValue < 0)
	{
		printk("Device Registration Failed\n");
		return -1;
	}
	
	/* Create the class */
	led_dev_class = class_create(THIS_MODULE, DEVICE_NAME);
	if(led_dev_class == NULL)
	{
		printk("Class Creation Failed\n");
		unregister_chrdev(MAJOR_NUMBER, spi_led_driver.driver.name);
		return -1;
	}
	
	/* Register the Driver */
	retValue = spi_register_driver(&spi_led_driver);
	if(retValue < 0)
	{
		printk("Driver Registraion Failed\n");
		class_destroy(led_dev_class);
		unregister_chrdev(MAJOR_NUMBER, spi_led_driver.driver.name);
		return -1;
}
	
	gpio_free(15);
	gpio_free(24);
	gpio_free(30);
	gpio_free(42);
	gpio_free(72);
	gpio_free(44);
	gpio_free(46);
	
	/* Export the GPIO Pins */
	gpio_request_array(request_gpio_arr,ARRAY_SIZE(request_gpio_arr));
	
	/* Set the direction for the relevant GPIO pins*/
	gpio_direction_output(15,0);
	gpio_direction_output(30,0);
	gpio_direction_output(42,0);
	gpio_direction_output(24,0);
	gpio_direction_output(72,0);
	gpio_direction_output(44,0);
	gpio_direction_output(46,0);
	
	/* Set the Value of GPIO Pins */	
	gpio_set_value_cansleep(15, 1);
	gpio_set_value_cansleep(30, 0);
	gpio_set_value_cansleep(42, 0);
	gpio_set_value_cansleep(24, 0);
	gpio_set_value_cansleep(72, 0);
	gpio_set_value_cansleep(44, 1);
	gpio_set_value_cansleep(46, 1);

	printk("Led Matrix Driver created\n");
	
	return 0;
	
}

/***********************************************************************
 * Description : Exits the driver safely when the module is removed from
 * kernel
 **********************************************************************/
void __exit led_driver_exit(void)
{
	
	spi_unregister_driver(&spi_led_driver);
	/* Destroy device */
	device_destroy(led_dev_class, spidev->devt);
	/* Delete cdev entries */
	unregister_chrdev(MAJOR_NUMBER, spi_led_driver.driver.name);
	/* Destroy driver_class */
	class_destroy(led_dev_class);
	/* Release the major number */
	
	printk("Spi Led Driver removed \n");
}


module_init(led_driver_init);
module_exit(led_driver_exit);
MODULE_LICENSE("GPL v2");