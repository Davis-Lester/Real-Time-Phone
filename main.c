// File: main.c
// Author: Davis Lester
// Last Edited: 12/22/2025
// Description: Final project main file for apps

//╭━━━┳╮╱╱╱╱╱╱╱╱╱╱╱╭━╮╭━╮╱╱╱╱╱╱╱╭━━━╮╱╱╱╱╭╮
//┃╭━╮┃┃╱╱╱╱╱╱╱╱╱╱╱┃┃╰╯┃┃╱╱╱╱╱╱╱┃╭━╮┃╱╱╱╱┃┃
//┃╰━╯┃╰━┳━━┳━╮╭━━╮┃╭╮╭╮┣━━┳┳━╮╱┃┃╱╰╋━━┳━╯┣━━╮
//┃╭━━┫╭╮┃╭╮┃╭╮┫┃━┫┃┃┃┃┃┃╭╮┣┫╭╮╮┃┃╱╭┫╭╮┃╭╮┃┃━┫
//┃┃╱╱┃┃┃┃╰╯┃┃┃┃┃━┫┃┃┃┃┃┃╭╮┃┃┃┃┃┃╰━╯┃╰╯┃╰╯┃┃━┫
//╰╯╱╱╰╯╰┻━━┻╯╰┻━━╯╰╯╰╯╰┻╯╰┻┻╯╰╯╰━━━┻━━┻━━┻━━╯

//************************************Includes***************************************/
#include "RTOS/RTOS.h"
#include "MultimodDrivers/multimod.h"
#include "threads.h"

// Driverlib includes
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"

//*************************************Defines***************************************/
// Semaphores defined in threads.h
semaphore_t UARTSemaphore;
semaphore_t sem_I2C;
semaphore_t sem_Display;
semaphore_t sem_Button;
semaphore_t sem_Camera;

//************************************MAIN*******************************************/
int main(void) {

    // 1. Disable interrupts globally to prevent early firing before OS launch
    IntMasterDisable();

    // 2. Set Clock to 50MHz or 80MHz (Required before Multimod init)
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    // 3. Initialize RTOS
    RTOS_Init();

    // 4. Initialize Multimod & Peripherals
    // This calls miscelanious sensor and peripheral initalization functions
    // The interrupts will pend until RTOS_Launch enables them.
    multimod_init();

    // 5. Initialize Semaphores
    RTOS_InitSemaphore(&sem_Button, 0);    // Start Blocked (Waiting for ISR)
    RTOS_InitSemaphore(&sem_Camera, 0);    // Start Blocked
    RTOS_InitSemaphore(&sem_I2C, 1);       // Start FREE (1)
    RTOS_InitSemaphore(&sem_Display, 1);   // Start FREE (1)
    RTOS_InitSemaphore(&UARTSemaphore, 1); // Start FREE (1)

    // 6. Add Threads
    // Priority 0 is highest (reserved for RTOS), 1 is high, 255 is lowest

    // IDLE Thread (Always required)
    RTOS_AddThread(Idle_Thread, 255, "Idle");

    // HOME Thread (Main Controller)
    // Handles Joystick, Grid Drawing, and launching Apps
    RTOS_AddThread(Home_Thread, 1, "Home");

    // BUTTON Thread
    // Handles selection (Enter) and exiting apps
    RTOS_AddThread(Read_Buttons, 2, "Buttons");

    // 7. Register Interrupts
    // Button Interrupt (Port E)
    RTOS_Add_APeriodicEvent(Button_Handler, 5, INT_GPIOE);

    // 8. Launch OS
    RTOS_Launch();

    // Spin while RTOS takes over
    while (1);
}
