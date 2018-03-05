
/* Course : CSE 438 - Embedded Systems Programming
 * Assignment 3 : SPI Device Programming and Pulse Width Measurement in Kernel Space
 * Team Member1 : Samruddhi Joshi  ASUID : 1213364722
 * Team Member2 : Varun Joshi 	   ASUID : 1212953337 
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>     /* required for pthreads */
#include <semaphore.h>   /* required for semaphores */
#include <sched.h> 	  	 /* required for sched */
#include <unistd.h>
#include <errno.h>

 
#define SPI_DEVICE_NAME "/dev/myspidev"
#define SENSOR_DEVICE_NAME "/dev/sensor"

/* Global Variables Declaration */
pthread_t thread_ID[2];    /* 1 SPI_transmit thread, 1 sensor_detect */
pthread_attr_t thread_attr[2];
int thread_priority[]={50,50};  
struct sched_param param[2];
int rerror[2]; /* to check for error in pthread creation */

pthread_mutex_t lock;
double distance;

/* Local Function Declarations */
void* Func_SPITestTransmit(void *ptr);
void* Func_UltrasonicDetect(void *ptr);
void* Func_SPIDogDisplay(void *ptr);
void* Func_SPIPatternDisplay(void *ptr);

/***********************************************************************
* Description:  Function creates threads for Sensor and LED display.
* 		 User can either print a sequence of patterns on LED Matrix or 
*		 see animation of dog running/walking as distance varies.
***********************************************************************/
int main(void)
{
	int i;

	pthread_mutex_init(&lock, NULL);

	/* Create two pthreads */
	for(i=0; i<2; i++)
	{
	pthread_attr_init(&thread_attr[i]);
	pthread_attr_getschedparam(&thread_attr[i], &param[i]);
	param[i].sched_priority = thread_priority[i];  /* set thread priority */
	pthread_attr_setschedparam(&thread_attr[i], &param[i]);
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &param[i]);
	}
	/* Create threads for Sensor and SPI LED Matrix */
	rerror[1] = pthread_create(&thread_ID[1], NULL, &Func_UltrasonicDetect, NULL);
	rerror[0] = pthread_create(&thread_ID[0], NULL, &Func_SPIPatternDisplay, NULL);		
	if((rerror[0] != 0) || (rerror[1] != 0))
		{
		printf("Error while creating thread \n");
		}

	for(i=0; i<2; i++)
	{
	pthread_join(thread_ID[i],NULL); /* wait for all threads to terminate */
	}

	return 0;
} 

/***********************************************************************
* Description - Function to display pattern of dog walking/running based
* on the distance measured by the ultrasonic sensor
***********************************************************************/
void *Func_SPIDogDisplay(void *ptr)
{
	int fd,return_val=0;
	
	double prevDistance = 0, currDistance = 0, diffDistance = 0, threshold = 0;
	char newDirection = 'L', oldDirection = 'L';
	unsigned int timeToDisplay = 0;
	unsigned int sequenceBuffer[4];
	char patternBuffer[4][8] = {
		/* Four patterns for Dog */
		{0x08, 0x90, 0xf0, 0x10, 0x10, 0x37, 0xdf, 0x98}, /* Right Walk */
		{0x20, 0x10, 0x70, 0xd0, 0x10, 0x97, 0xff, 0x18}, /* Right Run */
		{0x98, 0xdf, 0x37, 0x10, 0x10, 0xf0, 0x90, 0x08}, /* Left Walk */
		{0x18, 0xff, 0x97, 0x10, 0xd0, 0x70, 0x10, 0x20}, /* Left Run */
		};
		
	fd = open(SPI_DEVICE_NAME, O_RDWR);
	if(fd < 0)
	{
		printf("Can not open device file fd_spi.\n");
		return 0;
	}
	
	/* Send the Dog pattern to kernel */	
	ioctl(fd, patternBuffer, sizeof(patternBuffer));

	while(1)
	{
		pthread_mutex_lock(&lock);
		currDistance = distance;
		pthread_mutex_unlock(&lock);

		diffDistance = currDistance - prevDistance;
		threshold = currDistance / 10.0;

		printf("Current Distance is %f \n",currDistance);

		/* Determine the direction */
		if((diffDistance > -threshold) && (diffDistance < threshold))
			{/* Object moves in same direction as before */
				newDirection = oldDirection;
			}/* Object moves towards the sensor */
		else if(diffDistance > threshold)
			{
				newDirection = 'R';
			}/* Object moves away from sensor */
		else if(diffDistance < -threshold)
			{
				newDirection = 'L';
			}
		
		/* Determine the delay */
		if(currDistance > 5 )
			{
				/* Walking pattern*/
				timeToDisplay = 400;
			}
		else
			{	/* Running pattern*/
				timeToDisplay = 100;
			}
		
		if(newDirection == 'R')
		{
			/* Populate the sequence buffer to execute correspopnding sequence */
			sequenceBuffer[0] = 0;
			sequenceBuffer[1] = timeToDisplay;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			while(1)
			{
				/* Send the buffer to SPI LED matrix */	
				return_val=write(fd, sequenceBuffer, sizeof(sequenceBuffer));
				if(return_val<0)
				{}/* Keep trying */
				else
				{
					break;
				}
				usleep(10000);
			}
			/* Populate the sequence buffer to execute correspopnding sequence */
			sequenceBuffer[0] = 1;
			sequenceBuffer[1] = timeToDisplay;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			while(1)
			{	
				/* Send the buffer to SPI LED matrix */
				return_val=write(fd, sequenceBuffer, sizeof(sequenceBuffer));
				if(return_val<0)
				{}/* Keep trying */
				else
				{
					break;
				}
				usleep(10000);
			}  
		}
		else if(newDirection == 'L')
		{/* Populate the sequence buffer to execute correspopnding sequence */
			sequenceBuffer[0] = 2;
			sequenceBuffer[1] = timeToDisplay;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			while(1)
			{	
				/* Send the buffer to SPI LED matrix */
				return_val=write(fd, sequenceBuffer, sizeof(sequenceBuffer));
				if(return_val<0)
				{}/* Keep trying */
				else
				{
					break;
				}
				usleep(10000);
			}
			/* Populate the sequence buffer to execute correspopnding sequence */
			sequenceBuffer[0] = 3;
			sequenceBuffer[1] = timeToDisplay;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			while(1)
			{	
				/* Send the buffer to SPI LED matrix */
				return_val=write(fd, sequenceBuffer, sizeof(sequenceBuffer));
				if(return_val<0)
				{}/* Keep trying */
				else
				{
					break;
				}
				usleep(10000);
			}
		}
		prevDistance = currDistance;
		oldDirection = newDirection;
		usleep(10000);
	}
	
	close(fd);
	pthread_exit(0);
}

/***********************************************************************
* Description - Function to send display pattern depending upon the distance
* detected by the sensor. A-J pattern are displayed as the distance increases.
***********************************************************************/
void* Func_SPIPatternDisplay(void *ptr)
{
	int ret,fd,retValue;
	int i,j,k,currDistance;
	
    char patternBuffer [10][8] = {
		{ 0x7C, 0x7E, 0x13, 0x13, 0x7E, 0x7C, 0x00, 0x00 }, // 'A'
		{ 0x41, 0x7F, 0x7F, 0x49, 0x49, 0x7F, 0x36, 0x00 }, // 'B'
		{ 0x1C, 0x3E, 0x63, 0x41, 0x41, 0x63, 0x22, 0x00 }, // 'C'
		{ 0x41, 0x7F, 0x7F, 0x41, 0x63, 0x3E, 0x1C, 0x00 }, // 'D'
		{ 0x41, 0x7F, 0x7F, 0x49, 0x5D, 0x41, 0x63, 0x00 }, // 'E'
		{ 0x41, 0x7F, 0x7F, 0x49, 0x1D, 0x01, 0x03, 0x00 }, // 'F'
		{ 0x1C, 0x3E, 0x63, 0x41, 0x51, 0x73, 0x72, 0x00 }, // 'G'
		{ 0x7F, 0x7F, 0x08, 0x08, 0x7F, 0x7F, 0x00, 0x00 }, // 'H'
		{ 0x00, 0x41, 0x7F, 0x7F, 0x41, 0x00, 0x00, 0x00 }, // 'I'
		{ 0x30, 0x70, 0x40, 0x41, 0x7F, 0x3F, 0x01, 0x00 }, // 'J'
	};
	unsigned int sequenceBuffer[4] ;
	fd = open(SPI_DEVICE_NAME, O_RDWR);
	printf("fd is %d\n",fd );
	if(fd < 0)
	{
		printf("Can not open device file fd_spi.\n");
		return 0;
	}

	ret = ioctl(fd, patternBuffer, sizeof(patternBuffer));

	while(1)
	{
		pthread_mutex_lock(&lock);
        currDistance = distance;
        pthread_mutex_unlock(&lock);

        printf("Current Distance is %f \n",currDistance);

        if((currDistance>0)&&(currDistance<10))
        {/* Populate the sequence buffer to execute correspopnding sequence */
            sequenceBuffer[0] = 0;
			sequenceBuffer[1] = 50;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			while(1)
			{
				/* Send the buffer to SPI LED matrix */	
				retValue=write(fd, sequenceBuffer, sizeof(sequenceBuffer));
				if(retValue<0)
				{}/* Keep trying */
				else
				{
					break;
				}
				usleep(10000);
			}
        }
        else if((currDistance>10)&&(currDistance<20))
        {/* Populate the sequence buffer to execute correspopnding sequence */
           	sequenceBuffer[0] = 1;
			sequenceBuffer[1] = 15;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			while(1)
			{
				/* Send the buffer to SPI LED matrix */	
				retValue=write(fd, sequenceBuffer, sizeof(sequenceBuffer));
				if(retValue<0)
				{}/* Keep trying */
				else
				{
					break;
				}
				usleep(10000);
			}
		}
        else if((currDistance>20)&&(currDistance<30))
        {/* Populate the sequence buffer to execute correspopnding sequence */
            sequenceBuffer[0] = 2;
			sequenceBuffer[1] = 50;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			while(1)
			{
				/* Send the buffer to SPI LED matrix */	
				retValue=write(fd, sequenceBuffer, sizeof(sequenceBuffer));
				if(retValue<0)
				{}/* Keep trying */
				else
				{
					break;
				}
				usleep(10000);
			}
        }
        else if((currDistance>30)&&(currDistance<40))
        {/* Populate the sequence buffer to execute correspopnding sequence */
            sequenceBuffer[0] = 3;
			sequenceBuffer[1] = 50;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			while(1)
			{
				/* Send the buffer to SPI LED matrix */	
				retValue=write(fd, sequenceBuffer, sizeof(sequenceBuffer));
				if(retValue<0)
				{}/* Keep trying */
				else
				{
					break;
				}
				usleep(10000);
			}
        }
        else if((currDistance>40)&&(currDistance<50))
        {/* Populate the sequence buffer to execute correspopnding sequence */
           	sequenceBuffer[0] = 4;
			sequenceBuffer[1] = 50;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			while(1)
			{
				/* Send the buffer to SPI LED matrix */	
				retValue=write(fd, sequenceBuffer, sizeof(sequenceBuffer));
				if(retValue<0)
				{}/* Keep trying */
				else
				{
					break;
				}
				usleep(10000);
			}
		}
        else if((currDistance>50)&&(currDistance<60))
        {/* Populate the sequence buffer to execute correspopnding sequence */
           	sequenceBuffer[0] = 5;
			sequenceBuffer[1] = 50;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			while(1)
			{
				/* Send the buffer to SPI LED matrix */	
				retValue=write(fd, sequenceBuffer, sizeof(sequenceBuffer));
				if(retValue<0)
				{}/* Keep trying */
				else
				{
					break;
				}
				usleep(10000);
			}
		}
		else if((currDistance>60)&&(currDistance<70))
        {/* Populate the sequence buffer to execute correspopnding sequence */
           	sequenceBuffer[0] = 6;
			sequenceBuffer[1] = 50;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			while(1)
			{
				/* Send the buffer to SPI LED matrix */	
				retValue=write(fd, sequenceBuffer, sizeof(sequenceBuffer));
				if(retValue<0)
				{}/* Keep trying */
				else
				{
					break;
				}
				usleep(10000);
			}
		}
		else if((currDistance>70)&&(currDistance<80))
        {/* Populate the sequence buffer to execute correspopnding sequence */
           	sequenceBuffer[0] = 7;
			sequenceBuffer[1] = 50;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			while(1)
			{
				/* Send the buffer to SPI LED matrix */	
				retValue=write(fd, sequenceBuffer, sizeof(sequenceBuffer));
				if(retValue<0)
				{}/* Keep trying */
				else
				{
					break;
				}
				usleep(10000);
			}
		}
 		else if((currDistance>80)&&(currDistance<90))
        {/* Populate the sequence buffer to execute correspopnding sequence */
           	sequenceBuffer[0] = 8;
			sequenceBuffer[1] = 50;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			while(1)
			{
				/* Send the buffer to SPI LED matrix */	
				retValue=write(fd, sequenceBuffer, sizeof(sequenceBuffer));
				if(retValue<0)
				{}/* Keep trying */
				else
				{
					break;
				}
				usleep(10000);
			}
		}
        else
        {/* Populate the sequence buffer to execute correspopnding sequence */
        	sequenceBuffer[0] = 9;
			sequenceBuffer[1] = 50;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			while(1)
			{
				/* Send the buffer to SPI LED matrix */	
				retValue=write(fd, sequenceBuffer, sizeof(sequenceBuffer));
				if(retValue<0)
				{}/* Keep trying */
				else
				{
					break;
				}
				usleep(10000);
			}
        }
    }
    close(fd);
    pthread_exit(0);
}


/***********************************************************************
* Description - Function to send Sequence of pre-defined patterns to SPI
*				LED Matrix
***********************************************************************/
void* Func_SPITestTransmit(void *ptr)
{
	int ret,fd,retValue;
	
    char patternBuffer [10][8] = {
		{ 0x7C, 0x7E, 0x13, 0x13, 0x7E, 0x7C, 0x00, 0x00 }, // 'A'
		{ 0x41, 0x7F, 0x7F, 0x49, 0x49, 0x7F, 0x36, 0x00 }, // 'B'
		{ 0x1C, 0x3E, 0x63, 0x41, 0x41, 0x63, 0x22, 0x00 }, // 'C'
		{ 0x41, 0x7F, 0x7F, 0x41, 0x63, 0x3E, 0x1C, 0x00 }, // 'D'
		{ 0x41, 0x7F, 0x7F, 0x49, 0x5D, 0x41, 0x63, 0x00 }, // 'E'
		{ 0x41, 0x7F, 0x7F, 0x49, 0x1D, 0x01, 0x03, 0x00 }, // 'F'
		{ 0x1C, 0x3E, 0x63, 0x41, 0x51, 0x73, 0x72, 0x00 }, // 'G'
		{ 0x7F, 0x7F, 0x08, 0x08, 0x7F, 0x7F, 0x00, 0x00 }, // 'H'
		{ 0x00, 0x41, 0x7F, 0x7F, 0x41, 0x00, 0x00, 0x00 }, // 'I'
		{ 0x30, 0x70, 0x40, 0x41, 0x7F, 0x3F, 0x01, 0x00 }, // 'J'
	};
	unsigned int sequenceBuffer[20] = {0, 700, 1, 700, 2, 700, 3, 700, 5, 700, 6, 700, 7, 700, 8, 700, 9, 700, 0, 0};

	fd = open(SPI_DEVICE_NAME, O_RDWR);
	printf("fd is %d\n",fd );
	if(fd < 0)
	{
		printf("Cannot open device file fd_spi.\n");
		return 0;
	}
	/* Send the pattern to Kernel space*/
	ret = ioctl(fd, patternBuffer, sizeof(patternBuffer));
	
	while(1) 
	{
	/* Trigger write for the pre-defined sequence */
	retValue = write(fd, sequenceBuffer, sizeof(sequenceBuffer));
	usleep(50000);
	}
	
	close(fd);
    pthread_exit(0);
}


/***********************************************************************
* Description:  Function to measure the distance between the object and 
* 				ultrasonic sensor. The write gives trigger and read polls 
*				the echo for pulsewidth signal
***********************************************************************/
void* Func_UltrasonicDetect(void *ptr)
{
	int fd, retVal;
	fd = open(SENSOR_DEVICE_NAME, O_RDWR);
	int pulseWidth = 0;
	char* writeBuf = (char *)malloc(10);
	printf("Inside Sensor thread\n");
	while(1)
	{
		/* Generate trigger pulse */
		printf("Give trigger\n");
		retVal = write(fd,writeBuf,sizeof(writeBuf));
		if(retVal < 0)
		{
			printf("Write ERROR is : \n",retVal);
		}
		usleep(100000);
		/* Read the echo pin */
		retVal = read(fd,&pulseWidth,sizeof(pulseWidth));
		if(retVal < 0)
		{
			printf("Read ERROR is : \n",retVal);
		}
		usleep(100000);

		/* Calculate the distance */
		pthread_mutex_lock(&lock);
		distance = (pulseWidth / 400) * 0.017;
		pthread_mutex_unlock(&lock);

		usleep(600000);
	}
	close(fd);
	pthread_exit(0);
}