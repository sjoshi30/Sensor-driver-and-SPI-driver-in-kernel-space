/* Course : CSE 438 - Embedded Systems Programming
 * Assignment 3 : SPI Device Programming and Pulse Width Measurement
 * Team Member1 : Samruddhi Joshi  ASUID : 1213364722
 * Team Member2 : Varun Joshi 	   ASUID : 1212953337 
*/

#INPUT FILES#
spidev.c
main.c
sensor_driver.c
Makefile

#OUTPUT FILES#
Output

#PRE_REQUISITES#
--------------------------------------------------------------------------------------------
#Download the zip file from GitHub repository and extract it to a local folder in your host

#Ensure the usb serial cable is recognised, ethernet cable is connected, board is powered up and sd card boot image is inserted

#Hardware Setup#
--------------------------------------------------------------------------------------------
#Connect the Ultrasonic sensor 
Trigger - IO0
Echo - IO3
Vcc - 5V
Gnd - Gnd

#Connect the LED Matrix
DIN - IO11
SCK - IO13
CS  - IO12
Vcc - 5V
Gnd - Gnd

#Software Setup#
--------------------------------------------------------------------------------------------
#SET UP PUTTY#
#Check the ttyUSB number and enter same below
sudo chmod 777 /dev/ttyUSB#
#Give command -> sudo  putty
#Select serial and set serial line = /dev/ttyUSB# and baudrate(speed) = 115200
example : serial line = /dev/ttyUSB0 and baudrate(speed) = 115200
#Once putty terminal opens press enter->root

#SET STATIC IP#
#HOST
#In terminal check device name by command ifconfig(eg-enp0s3)
#Set static ip by command -> sudo ifconfig enp0s3 192.168.1.2 netmask 255.255.0.0 up

#TARGET
#In terminal check device name by command ifconfig(eg:enp0s20f6)
#In putty terminal set static ip by command -> ifconfig enp0s20f6 192.168.1.5 netmask 255.255.0.0 up

#COMPILE THE CODE AND EXECUTE THE BINARY#
#On the host,navigate to the folder where you copied all the input files 
#To compile for Galileo board use command on terminal -> make TEST_TARGET=Galileo

#TO EXECUTE ON HARDWARE#
1. Copy the binary file and module files created from host to the target through the command
example : scp spidev.ko sensor_driver.ko output root@192.168.1.5:/home/root/spi

2. Remove the existing spidev.ko using 'rmmod spidev.ko'

3. Insert the copied modules 
'insmod spidev.ko' 
'insmod sensor_driver.ko'

4. Run the binary output by entering the command
'./output'

#MAKE CLEAN - To clean the output files
On host, enter command "make clean" on terminal to remove the output files 


#ANIMATION#
---------------------------------------------------------------------------------
1. Enable the thread 'Func_SPIPatternDisplay'
As the distance increases between the sensor and the obstacle, different pattern i.e. among letters A-J is displayed.

2. Enable the thread 'Func_SPIDogDisplay'
When the obstacle moves towards the sensor, the dog walks/runs right depending on the proximity to the sensor. 
When the obstacle moves away from the sensor, the dog walks/runs left depending on the proximity to the sensor.
(Dog runs for distance < 35 cms)

3. Enable the thread 'Func_SPITestTransmit'
The Pattern mentioned in the Sequence Buffer will be displayed with the delay mentioned.


