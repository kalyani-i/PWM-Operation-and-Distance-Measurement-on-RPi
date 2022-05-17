#include <stdio.h>    // for printf
#include <fcntl.h>    // for open
#include <unistd.h> 
#include <errno.h> 
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <gpiod.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <assert.h> // assert
#include <fcntl.h>    // for open

#include "project2.h"
//#include "dprintf.h"
//#define DEBUG

#define GREEN_LED 25    //Define the led pins
#define BLUE_LED 23
#define RED_LED 25

#define Trigger_Pin 22      //defining pins for ultrasonic sensor
#define echo_pin 23
#define TRIGGER_BURST_NS 10000
#define MAX32 4294967295

int expire_count = 0;                       //Declaring variable
uint32_t duty_red,duty_green;
uint32_t period_n = PWM_PERIOD * 1000000;
uint32_t loop_i = 0, count =0, soft_pwm=0;
uint32_t duty_red, duty_green = PWM_PERIOD, duty_blue;      //declaring variable to store user input
struct gpiod_chip *chip;
struct gpiod_line *lineG;
char command[20];                           //declaring variable to store the initial command


int trigger, echo;
struct gpiod_line *lineT, *lineE;
int req;
int i= 0;


float vel= 13042.8, distance,sum_distance ,frequency;

static uint32_t get_clock_freq()            //function to get clock frequecy
{                               
    int fd;                                 //used in Mode 1
    char buffer[20];
    fd = open("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq",O_RDONLY); //path for cpu freq
    read(fd,&buffer,sizeof(buffer));
    close(fd);
    return (atoi(buffer));                  
}

void ts_sub(struct timespec *ts1, struct timespec *ts2, struct timespec *ts3) //function to subtract ts1 and ts2
{
    ts3->tv_sec = ts1->tv_sec - ts2->tv_sec;                                 //provided by professor no changes made
    
    if(ts1->tv_nsec >= ts2->tv_nsec) {
		ts3->tv_nsec = ts1->tv_nsec - ts2->tv_nsec;	
	} else
	{	
		ts3->tv_sec--; 
		ts3->tv_nsec = 1e9 + ts1->tv_nsec - ts2->tv_nsec;	
    }
}
static inline uint32_t ccnt_read (void)           //ccnt_read                     
{                                                           //provided no cahnges made
  uint32_t cc = 0;
  __asm__ volatile ("mrc p15, 0, %0, c9, c13, 0":"=r" (cc));
  return cc;
}

int pwm_export(int pwm_channel)				//function to export gpio pins
{   
    int fd;                                    //initalizing the file descriptor
    char channel_no[2];
    sprintf(channel_no,"%d",pwm_channel);           //to convert user input to string
    // printf("Exporting Channel: %s\n",channel_no);
    fd = open("/sys/class/pwm/pwmchip0/export",O_WRONLY); // opens the filename using the given mode
    // write(fd,&channel_str,1);
    if(fd == -1)
    {
        printf("channel not exported\n");                   //incase of error
        return -1;
    }
    // write(fd,&channel_no,1);
    if(write(fd,&channel_no,1) == -1)               //writing on the path
    {
        printf("Error writing in file \n ");
    }
    close(fd);                                          //close the file descriptor
    return 0;
}

void start_HCSR(int mode)                       //function for ultrasonic sensor
{
    struct timespec next,c0,c1,c2;                  //defining variables
    uint32_t t0,t1;
    clock_gettime(CLOCK_MONOTONIC, &next);              //to get clock time

    next.tv_sec += ((next.tv_nsec + TRIGGER_BURST_NS )/1000000000);
    next.tv_nsec += ((next.tv_nsec + TRIGGER_BURST_NS )/1000000000);

    gpiod_line_set_value(lineT,1);              //setting the line to high
    clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&next,NULL); // keeping the line high for 10us
    gpiod_line_set_value(lineT,0);                  //setting the line to low

    if(mode == 0)                                   //loop if input is mode 0
    {
        while(!(gpiod_line_get_value(lineE)))      //checking the value of lineE
        {

        }
        clock_gettime(CLOCK_MONOTONIC,&c0);         //storing time in c0

        while(gpiod_line_get_value(lineE))      //checking the value of line E
        {

        }
        clock_gettime(CLOCK_MONOTONIC,&c1);     //storing time in c1

        ts_sub(&c1,&c0,&c2);                    //calling teh sub function
        distance = ((c2.tv_nsec + c2.tv_sec*1000000000)*(vel/1000000000))/2;  //formula for distance
        sum_distance += distance;
        printf("The distance is %0.3f inches\n",distance);          //printing the distance value
    }
    else                                                //loop if the Mode is 1
    {                                                    //similar as Mode 0
        while(!(gpiod_line_get_value(lineE)))           
        {

        }
        t0 = ccnt_read();                                      //the time here is fetched from cpu
        while((gpiod_line_get_value(lineE)))
        {

        }
        t1 = ccnt_read();
        frequency = get_clock_freq();
        if(t1<t0)
            distance = (vel*(MAX32-t0 +t1))/(2*frequency*1000);
            
        else
            distance = (vel*(t1-t0))/(2*frequency*1000);            //formula to calculate distance
        sum_distance += distance;
        printf("The distance is %0.3f inches\n",distance);      //printing the values 
     }
    
}

int pwm_unexport(int pwm_channel)				//self-explanatory function to export gpio pins
{   
    int fd;                                        //defining the file descriptor
    char channel_no[2];
    sprintf(channel_no,"%d",pwm_channel);           //to convert user input to string
    // printf("UnExporting Channel: %s\n",channel_no);
    fd = open("/sys/class/pwm/pwmchip0/unexport",O_WRONLY);  // opens the filename using the mode
    // write(fd,&channel_no,1);
    if(fd == -1)
    {
        perror("channel not unexported\n");         //checking for error 
        return -1;
    }
    if(write(fd,&channel_no,1) == -1)           //writing in the unexport path
    {
        perror("Error writing");                //checking for errors
    }
    close(fd);
    return 0;
}

int pwm_period(int pwm_channel,int period)
{
    int fd;                             //defing variable
    char path[256],buffer[20];
    //int buffer_size = 20;
    ssize_t bytes;
    sprintf(path,"/sys/class/pwm/pwmchip0/pwm%d/period",pwm_channel); //getting the path name
    // printf("Path for channel %d period: %s\n",pwm_channel,path);
    fd = open(path,O_WRONLY);                                  //opening the path in write mode
    // sprintf(period_c,"%d",period);
    // write(fd,buffer,strlen(period_c));
    if(fd == -1)
    {
        perror("Cannot open rwuested path");                    //checking for errors
        return -1;
    }
    bytes = snprintf(buffer,20,"%d",period);
    // write(fd,buffer,bytes);
    // printf("\nBuffer name : %s and buffer bytes : %d\n",buffer,bytes);
    if(write(fd,buffer,bytes) == -1)
    {
        perror("Cannot set period");
    }
    close(fd);                                  //close fd
    return 0;
}

int pwm_duty(int channel,int percentage)      //function for duty cycle
{
    int fd;                                     //similar as pwm_period
    int duty;                                       
    duty = (percentage * period_n)/100 ;    //to cnvert user input from percentage to duty cycle
    char path[256],buffer[30];
    int buffer_size = 20;
    ssize_t bytes;
    sprintf(path,"/sys/class/pwm/pwmchip0/pwm%d/duty_cycle",channel);       //setting up the path
    // printf("Path for channel %d duty: %s\n",channel,path);
    fd = open(path,O_WRONLY);                           //opening the path
    if(fd == -1)
    {
        perror("Cannot open requested path");
        return -1;
    }
    bytes = snprintf(buffer,buffer_size,"%d",duty);
    // write(fd,buffer,bytes);
    // printf("\nBuffer name : %s and buffer bytes : %d\n",buffer,bytes);
    if(write(fd,buffer,bytes) == -1)                //writing the duty cycle 
        perror("Cannot set duty cycle");
    close(fd);                                  //close fd
    return 0;
}

int pwm_enable(int channel,int enable) //function to enable pwm
{
    int fd;                         //similar to pwm_period
    char path[256],buffer[30];          //defining the variables
    int buffer_size = 20;
    ssize_t bytes;
    sprintf(path,"/sys/class/pwm/pwmchip0/pwm%d/enable",channel);  //setting up teh path
    // printf("Path for channel %d enable: %s\n",channel,path);
    fd = open(path,O_WRONLY);                   //opeing file in write mode
    if(fd==-1)
    {
        perror("Cannot open requested path");
        return -1;
    }
    bytes = snprintf(buffer,buffer_size,"%d",enable);
    // snprintf(buffer,buffer_size,"%d",enable);
    // write(fd,buffer,bytes);
    // printf("\nBuffer name : %s and buffer bytes : %d\n",buffer,bytes);
    if(write(fd,&buffer,bytes) == -1)      //writing file to pwm enable path
    {
        //perror("Cannot enable pwm");

    }
    close(fd);
    return 0;
}

int pwm_disable(int channel,int disable)   //function to diable pwm
{
    int fd;                                                     //similar to pwm enable
    char path[256],buffer[30];
    int buffer_size = 20;
    ssize_t bytes;
    sprintf(path,"/sys/class/pwm/pwmchip0/pwm%d/enable",channel);
    // printf("Path for channel %d enable: %s\n",channel,path);
    fd = open(path,O_WRONLY);
    if(fd == -1)
    {
        perror("Cannot open requested path");
        return -1;
    }
    bytes = snprintf(buffer,buffer_size,"%d",disable);
    // snprintf(buffer,buffer_size,"%d",disable);
    // write(fd,buffer,bytes);
    // printf("\nBuffer name : %s and buffer bytes : %d\n",buffer,bytes);
    if(write(fd,&buffer,bytes) == -1)
    {
        //perror("Cannot disable pwm");
    }
    close(fd);
    return 0;
}

void timer_callback(int sig) {    //function for timer call back
    
    if (sig == SIGUSR1)  //function starts when signal defined is received
    {
        count++;
        soft_pwm++;     //counter to track green led
        if (count % STEP_DURATION == 0)   // pattern for execution is defined in the following line of code
            loop_i++;
        
        if (soft_pwm == PWM_PERIOD)
        {
            soft_pwm = 0;
        }

        if (loop_i & 1)            //Red Led
        {   
                                    //pwm enable
            pwm_enable(0,1);
        }
        else
        {
                                    //disable
            pwm_disable(0,0);
        }

        if ((loop_i >> 1) & 1)      //Green Led
        {
            if (duty_green)
            {
                if (soft_pwm == 0)
                {
                    gpiod_line_set_value(lineG, 1); //setting green led to 1
                }
                if (soft_pwm == duty_green)
                {
                    gpiod_line_set_value(lineG, 0);
                }
            }
            else
            {
                gpiod_line_set_value(lineG, 0); //setting pgio green pin to 0
            }
        }
        else
        {
            gpiod_line_set_value(lineG, 0);  
        }

        if ((loop_i >> 2) & 1)              //Blue Led
        {   
                                                //pwm enable
            pwm_enable(1,1);
        }
        else
        {
                                                //pwm disable
            pwm_disable(1,0);

        }
  }
}

int main(void) //main function
{

    timer_t timer_id; //initializing timer id
    int mode;           //declaring variable

    int req;
    
    pwm_export(0);              // export pwm
    pwm_export(1);
    pwm_disable(0,0);           // disable pwm
    pwm_disable(1,0);
    pwm_period(0,period_n);     // write period for pwm0
    pwm_period(1,period_n);        //write period for pwm 1
    pwm_duty(0,period_n);         // write duty for pwm0
    pwm_duty(1,period_n);          //write duty for pwm1

    chip = gpiod_chip_open("/dev/gpiochip0");       //opening gpio
    if (!chip)                                      //checking for errors
        return -1;
    
    lineG = gpiod_chip_get_line(chip, GREEN_LED);  //configuring the writing to get line
    lineT = gpiod_chip_get_line(chip,Trigger_Pin );
    lineE = gpiod_chip_get_line(chip, echo_pin);

    if(!lineG||!lineT || !lineE)        //checking for error 
    {
        gpiod_chip_close(chip);
        return -1;
    }
    req = gpiod_line_request_output(lineG, "gpio_state", GPIOD_LINE_ACTIVE_STATE_LOW); // setting the led green pin to low as output line
    req = gpiod_line_request_output(lineT, "gpio_state", GPIOD_LINE_ACTIVE_STATE_LOW);  // setting the the trigger pin to low
    req = gpiod_line_request_input(lineE, "gpio_state");            //getting the echo pin as input
    int measurement;


    if (req) {                  //checking for errors
        gpiod_chip_close(chip);
        return -1;
    }

    struct sigevent sev;                //defining variable for timer
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGUSR1;
    sev.sigev_value.sival_ptr = &timer_id;
    signal(SIGUSR1, timer_callback);  // calling the call back function
    gpiod_line_set_value(lineG, 0);

    int ret = timer_create(CLOCK_MONOTONIC, &sev, &timer_id);
    if (ret == -1)                                              //checking for errors
    {
        fprintf(stderr, "Error timer_create: %s\n", strerror(errno));
        return 1;
    }
    
    struct itimerspec its;
    its.it_value.tv_sec =  0;
    its.it_value.tv_nsec = 1000000;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    timer_settime(timer_id, TIMER_ABSTIME , &its, NULL);    //setting up the timmer
    while(1)                //continous loop
    {
        scanf("%s",command);            //to wait for user command

        if(!strcmp(command, "exit"))        //checking for command
            break;
        else if(!strcmp(command,"rgb"))        //checking for command
        {
            scanf("%d",&duty_red);          //storing the input values
            scanf("%d",&duty_green);          
            scanf("%d",&duty_blue);    
            duty_green = (PWM_PERIOD* duty_green)/100;          //setting the duty cycle for green led
            pwm_duty(0,duty_red);           //calling the pwm duty
            pwm_duty(1,duty_blue);
        }
        else if(!strcmp(command,"dist"))  //similar as rgb
        {
            scanf("%d",&measurement);
            scanf("%d",&mode);
            sum_distance=0;
            for(i=0;i<measurement;i++)      //running the 
            {
                start_HCSR(mode);
            }
            printf("The Average Distance is: %.2f\n", (double)(sum_distance / measurement));
        }

    }
    timer_delete(timer_id);        //deleting the timer

    pwm_disable(0,0);           //disabling pwm0
    pwm_disable(1,0);               //disabling pw1
    pwm_unexport(0);        //unexporting pwm channels
    pwm_unexport(1);
    gpiod_chip_close(chip);         //closing chip
    return 0;
}

