// threads.h
// Date Created: 2023-07-26
// Date Updated: 2023-07-26
// Threads

#ifndef THREADS_H_
#define THREADS_H_

/************************************Includes***************************************/

#include "./RTOS/RTOS.h"

/************************************Includes***************************************/

/*************************************Defines***************************************/

// --- FIFO Indices  ---


/*************************************Defines***************************************/

/***********************************Semaphores**************************************/

semaphore_t UARTSemaphore;
semaphore_t sem_I2C;
semaphore_t sem_Display;
semaphore_t sem_Button;
semaphore_t sem_Camera;

// --- Button Masks and GPIO (Fixes errors in Speaker_Thread and Read_Buttons) ---
#define BUTTON_1K_MASK      0x01
#define BUTTON_2K_MASK      0x02
#define BUTTON_3K_MASK      0x04
#define STOP_BUTTON_MASK    0x08
#define BUTTON_PORT_BASE    GPIO_PORTF_BASE // Example GPIO base for buttons
#define BUTTON_PINS         GPIO_PIN_4 | GPIO_PIN_0 // Example pins

// --- Timer/DAC Constants (Fixes errors in Speaker_Thread and DAC_Timer_Handler) ---
#define CLK_FREQ_HZ         80000000

/***********************************Semaphores**************************************/

/***********************************Structures**************************************/
/***********************************Structures**************************************/


/*******************************Background Threads**********************************/

void Idle_Thread(void);

void DrawHomeScreen(void);
void Camera_App(void);
void Compass_App(void);
void Weather_App(void);
void Frogger_App(void);
void Home_Thread(void);
void Read_Buttons(void);
void Button_Handler(void);

/*******************************Background Threads**********************************/

/********************************Periodic Threads***********************************/


/********************************Periodic Threads***********************************/

/*******************************Aperiodic Threads***********************************/


/*******************************Aperiodic Threads***********************************/


#endif /* THREADS_H_ */

