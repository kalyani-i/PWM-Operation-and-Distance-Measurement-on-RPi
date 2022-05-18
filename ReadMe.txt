Assignment 2
---------------
Subject - CSE 438
Assignment 2
Author - Kalyani Niwrutti Ingole

---------------

ReadMe
###
System requirement

gcc Compiler 
Raspberry pi 4B 2GB
SD card 16GB

######

PWM Operation and Distance Measurement on RPi

######
Build

We have made a make file that includes the compiler and other commands that generate any warning needed
Import the header file provided in the main assignment file for compilation
Few other header like #include <time.h> #include <signal.h> needs to be included as the part 1 i.e RGB requires the use of timer for software pwm
To check for errors and compilation of the code use 'make all' command

######
Execution
make sure the rpi is powered ON
Do the wiring of the rpi with RGB led and Ultrasonic sensor. 
Connect the green Led to GPIO 25, Trigger to 22 and Echo to 23.
before the exceution run the following command in the terminal
"sudo insmod cycle_count_mod.ko"  - To add kernel object ko root
To run the file use 'sudo ./assignment'
once the code is exceuted without to get the desired out put write the following command
	1] rgb x y z 
	where x,y,z are the duty cycle the user has to input from 0 to 100
	2] dist x,y
	where x is the number of measurement the user wants to get and y is the mode in which user wants to operate
	3] exit
	writting this in the terminal will terminate the code.
After the termination of code use the following command
"sudo rmmod cycle_count_mod.ko" - to remove kernel object from root












