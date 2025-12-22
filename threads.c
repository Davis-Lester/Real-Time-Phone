// File: threads.c
// Author: Davis Lester
// Last Edited: 12/22/2025
// Description: Threads for phone usage

//╭━━━┳╮╱╱╱╱╱╱╱╱╱╱╱╭━━━━┳╮╱╱╱╱╱╱╱╱╱╱╱╭╮╭━━━╮╱╱╱╱╭╮
//┃╭━╮┃┃╱╱╱╱╱╱╱╱╱╱╱┃╭╮╭╮┃┃╱╱╱╱╱╱╱╱╱╱╱┃┃┃╭━╮┃╱╱╱╱┃┃
//┃╰━╯┃╰━┳━━┳━╮╭━━╮╰╯┃┃╰┫╰━┳━┳━━┳━━┳━╯┃┃┃╱╰╋━━┳━╯┣━━╮
//┃╭━━┫╭╮┃╭╮┃╭╮┫┃━┫╱╱┃┃╱┃╭╮┃╭┫┃━┫╭╮┃╭╮┃┃┃╱╭┫╭╮┃╭╮┃┃━┫
//┃┃╱╱┃┃┃┃╰╯┃┃┃┃┃━┫╱╱┃┃╱┃┃┃┃┃┃┃━┫╭╮┃╰╯┃┃╰━╯┃╰╯┃╰╯┃┃━┫
//╰╯╱╱╰╯╰┻━━┻╯╰┻━━╯╱╱╰╯╱╰╯╰┻╯╰━━┻╯╰┻━━╯╰━━━┻━━┻━━┻━━╯

//************************************Includes***************************************/

// Local Files
#include "./threads.h"
#include "./MultimodDrivers/multimod.h"
#include "./MultimodDrivers/GFX_Library.h"

// Photos
#include "Frogger.h"
#include "Compass.h"
#include "Weather.h"
#include "Camera.h"

// General Includes
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

// Driverlib
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "driverlib/gpio.h"
#include "inc/hw_memmap.h"

//*************************************Defines***************************************/

// GENERAL CONSTANTS
#define UART_BASE       UART0_BASE
#define LOCATION_BUF_SIZE 128

// Screen Dimensions
#define MAX_SCREEN_X    240
#define MAX_SCREEN_Y    280

// App IDs
#define APP_NONE        0
#define APP_CAMERA      1
#define APP_COMPASS     2
#define APP_WEATHER     3
#define APP_FROGGER     4

// Grid Layout
#define BOX_WIDTH       80
#define BOX_HEIGHT      80
#define BOX_GAP         30
#define START_X         30
#define START_Y         50

// FROGGER CONSTANTS
#define GRID_SIZE       20
#define NUM_LANES       11
#define GAME_WIDTH      240
#define GAME_HEIGHT     (NUM_LANES * GRID_SIZE)
#define FROG_DRAW_SIZE  (GRID_SIZE - 4)
#define FROG_OFFSET     2
#define MAX_ENTITIES    30
#define SPAWN_RATE      25

// Colors
#define COLOR_BG        0x0000
#define COLOR_BOX       0x7BEF
#define COLOR_SELECT    0xF800
#define COLOR_TEXT      0xFFFF
#define COLOR_CYAN      0x07FF
#define COLOR_YELLOW    0xFFE0

// Frogger Colors
#define COLOR_GREEN     0x07E0
#define COLOR_RED       0xF800
#define COLOR_ROAD      0x39E7
#define COLOR_RIVER     0x001F
#define COLOR_GRASS     0x2660
#define COLOR_LOG       0xA145
#define COLOR_CAR_YEL   0xE7E0
#define COLOR_CAR_BLU   0x001F

// Frogger Game Layout
const uint16_t LANE_COLORS[NUM_LANES] = {
    COLOR_GRASS, COLOR_RIVER, COLOR_RIVER, COLOR_RIVER, COLOR_RIVER,
    COLOR_GRASS, COLOR_ROAD,  COLOR_ROAD,  COLOR_ROAD,  COLOR_ROAD,
    COLOR_GRASS
};

// Compass Visuals
#define COMPASS_CENTER_X  120
#define COMPASS_CENTER_Y  140
#define COMPASS_RADIUS    50
#define NEEDLE_LENGTH     45
#define COLOR_CIRCLE      0xFFFF
#define COLOR_NEEDLE      0xF800
#define M_PI              3.14159265358979323846

// Magnetometer Constants
#define INTERFERANCE_CONFIGURATION_REGISTER 0x6B
#define MAGNETOMETER_I2C_ENABLE 0x20

#define MAGNETOMETER_POWER_REGISTER 0x4B
#define MAGNETOMETER_POWER_ENABLE 0x01

#define MAGNETOMETER_OPERATIONAL_MODE 0x4C
#define MAGNETOMETER_NORMAL_OPERATION 0x00

#define MAGNETOMETER_X_Y_REPITIONS 0x51
#define MAGNETOMETER_X_Y_9_REPITIONS 0x04

#define MAGNETOMETER_Z_REPITIONS 0x52
#define MAGNETOMETER_Z_15_REPITIONS 0x0E

#define MAGNETOMETER_SETUP 0x4C
#define MAGNETOMETER_6_BYTE_BURST 0x02

#define MAGNETOMETER_DATA_READ_ADDRESS 0x4D

// Buttons
#define BUTTON_SELECT_MASK  0x02
#define BUTTON_HOME_MASK    0x10

// Frogger Entity Structure
typedef struct {
    float x;
    float prev_x;
    uint16_t y;
    float speed;
    uint8_t width_pixels;
    uint16_t color;
    bool is_log;
    bool active;
} Entity_t;

// Global State
volatile bool is_unlocked = false;
volatile uint8_t current_app = APP_NONE;
volatile uint8_t selected_icon_idx = 0;
volatile bool take_photo_flag = false;

volatile Entity_t frogger_entities[MAX_ENTITIES];

//*************************************Helper Functions***************************************/

/// @brief Displays a photo to the ST7789 Screen
/// @param x_pos X position to print the photo
/// @param y_pos Y position to print the photo
/// @param bitmap pointer to a bitmap array of RGB565 hexadecimal values representing the phtoo
/// @param w Width of the photo
/// @param h Height of the photo
void display_photo(uint16_t x_pos, uint16_t y_pos, const uint8_t *bitmap, uint16_t w, uint16_t h) {
    uint16_t color;
    uint32_t index = 0;

    // Loop through all pixels
    for (uint16_t y = 0; y < h; y++) {
        for (uint16_t x = 0; x < w; x++) {

            // Fix X and Y positions to not print off the screen
            if ((x_pos + x >= MAX_SCREEN_X) || (y_pos + y >= MAX_SCREEN_Y)) { 
                index += 2;
                continue;
            }

            // Little Endian Printing
            uint8_t low_byte  = bitmap[index];
            uint8_t high_byte = bitmap[index + 1];
            color = (uint16_t)((high_byte << 8) | low_byte);

            // Print to Screen
            ST7789_DrawPixel(x_pos + x, y_pos + (h - 1 - y), color);
            index += 2;
        }
    }
}

/// @brief Check the entity collision state for the frog and an entity
/// @param fx Frog X position
/// @param fy Frog Y position
/// @param ex Entity X position
/// @param ey Entity Y position
/// @param ew Entity width
/// @return True or False depending on if the entity and the frog are intersecting
bool CheckCollision(float fx, float fy, float ex, float ey, float ew) {

    // Added small tollerances for the edge of the frog
    // Basically checks each edge and accounts for the width of the entity
    return (fx + 2 < ex + ew && fx + 14 > ex && fy < ey + 18 && fy + 16 > ey + 2);
}

/// @brief Drawing the compass as a background function
/// @param heading_deg The degree to draw the compass needle at
void DrawCompass(double heading_deg) {
    // Inital state
    static int16_t prev_tip_x = COMPASS_CENTER_X;
    static int16_t prev_tip_y = COMPASS_CENTER_Y;

    // Convert degrees to radians
    double angle_rad = (heading_deg * (M_PI / 180.0)) + M_PI;

    // Calculate position of the end of the compass
    int16_t tip_x = COMPASS_CENTER_X + (int16_t)(cos(angle_rad) * NEEDLE_LENGTH);
    int16_t tip_y = COMPASS_CENTER_Y - (int16_t)(sin(angle_rad) * NEEDLE_LENGTH);

    // Wait for display
    RTOS_WaitSemaphore(&sem_Display);

    // Clear previous needle
    ST7789_DrawLine(COMPASS_CENTER_X, COMPASS_CENTER_Y, prev_tip_x, prev_tip_y, COLOR_BG);

    // Draw outer circle
    ST7789_DrawCircle(COMPASS_CENTER_X, COMPASS_CENTER_Y, COMPASS_RADIUS, COLOR_CIRCLE);

    // Draw centerline at bottom of compass
    ST7789_DrawLine(COMPASS_CENTER_X, COMPASS_CENTER_Y - COMPASS_RADIUS, COMPASS_CENTER_X, COMPASS_CENTER_Y - COMPASS_RADIUS + 5, COLOR_CIRCLE);
    
    // Draw line for North direction
    ST7789_DrawLine(COMPASS_CENTER_X, COMPASS_CENTER_Y, tip_x, tip_y, COLOR_NEEDLE);

    // Release display
    RTOS_SignalSemaphore(&sem_Display);

    // Update previous / current state
    prev_tip_x = tip_x;
    prev_tip_y = tip_y;
}

/// @brief Function for printing home screen
void DrawHome_Static(void) {

    // Local variables
    uint16_t x, y;
    char *labels[4] = {"Camera", "Compass", "Weather", "Frogger"};

    // Reset screen
    ST7789_DrawRectangle(0, 0, MAX_SCREEN_X, MAX_SCREEN_Y, COLOR_BG);

    // Title
    display_setCursor(80, 260);
    // Use Transparent Text (GFX Lib has bug in Bg logic for Size > 1)
    display_setTextColor(COLOR_TEXT);
    display_setTextSize(1);
    char *title = "HOME MENU";
    while(*title) display_print(*title++);

    // Loop for printing app icons and text
    for(int i = 0; i < 4; i++) {

        // Initalize Variables
        int row = i / 2; // Integer division, either 0 or 1
        int col = i % 2; // Alternates between 0 and 1

        // Printing logic
        x = START_X + col * (BOX_WIDTH + BOX_GAP);
        y = START_Y + row * (BOX_HEIGHT + BOX_GAP);

        // Display photos
        if (i == 0) display_photo(x, y, Camera_map, Camera_PHOTO_WIDTH, Camera_PHOTO_HEIGHT);
        else if (i == 1) display_photo(x, y, Compass_map, Compass_PHOTO_WIDTH, Compass_PHOTO_HEIGHT);
        else if (i == 2) display_photo(x, y, Weather_map, Weather_PHOTO_WIDTH, Weather_PHOTO_HEIGHT);
        else if (i == 3) display_photo(x, y, Frogger_map, Frogger_PHOTO_WIDTH, Frogger_PHOTO_HEIGHT);

        // Add label underneath each photo
        display_setCursor(x + 10, y - 10);
        char *ptr = labels[i];
        while(*ptr) display_print(*ptr++);
    }
}

/// @brief Updates the cursor for which app is selected
/// @param prev_idx Previous cursor location
/// @param curr_idx Current cursor location
void UpdateHome_Cursor(uint8_t prev_idx, uint8_t curr_idx) {
    
    uint16_t x, y; // Cursor X and Y position


    if (prev_idx != 255) {

        int row = prev_idx / 2; // Integer division, either 0 or 1
        int col = prev_idx % 2; // Alternates between 0 and 1

        // Printing logic
        x = START_X + col * (BOX_WIDTH + BOX_GAP);
        y = START_Y + row * (BOX_HEIGHT + BOX_GAP);

        // Clear cursor
        ST7789_DrawRectangle(x - 2, y - 2, BOX_WIDTH + 4, BOX_HEIGHT + 4, COLOR_BG);

        // Redraw app
        if (prev_idx == 0) display_photo(x, y, Camera_map, Camera_PHOTO_WIDTH, Camera_PHOTO_HEIGHT);
        else if (prev_idx == 1) display_photo(x, y, Compass_map, Compass_PHOTO_WIDTH, Compass_PHOTO_HEIGHT);
        else if (prev_idx == 2) display_photo(x, y, Weather_map, Weather_PHOTO_WIDTH, Weather_PHOTO_HEIGHT);
        else if (prev_idx == 3) display_photo(x, y, Frogger_map, Frogger_PHOTO_WIDTH, Frogger_PHOTO_HEIGHT);
    }

    int row = curr_idx / 2; // Integer division, either 0 or 1
    int col = curr_idx % 2; // Alternates between 0 and 1

    // Printing logic
    x = START_X + col * (BOX_WIDTH + BOX_GAP);
    y = START_Y + row * (BOX_HEIGHT + BOX_GAP);

    // Draw cursor
    ST7789_DrawRectangle(x - 2, y - 2, BOX_WIDTH + 4, BOX_HEIGHT + 4, COLOR_SELECT);

    // Redraw app
    if (curr_idx == 0) display_photo(x, y, Camera_map, Camera_PHOTO_WIDTH, Camera_PHOTO_HEIGHT);
    else if (curr_idx == 1) display_photo(x, y, Compass_map, Compass_PHOTO_WIDTH, Compass_PHOTO_HEIGHT);
    else if (curr_idx == 2) display_photo(x, y, Weather_map, Weather_PHOTO_WIDTH, Weather_PHOTO_HEIGHT);
    else if (curr_idx == 3) display_photo(x, y, Frogger_map, Frogger_PHOTO_WIDTH, Frogger_PHOTO_HEIGHT);
}

//*************************************Threads***************************************/

// App Functions

// 1. Camera App
void Camera_App(void) {
    
    // Local variables
    uint16_t x, y;               // X and Y location for each pixel
    uint8_t high_byte, low_byte; // Individual bytes for pixel color for proper screen transmission
    uint16_t pixel_color;        // Individual pixel colors (combined high and low byte)

    // Wait for semaphore
    RTOS_WaitSemaphore(&sem_Display);

    // Reset screen color
    ST7789_DrawRectangle(0, 0, MAX_SCREEN_X, MAX_SCREEN_Y, COLOR_BG);

    // Print instructions
    display_setCursor(80, 150);
    display_setTextColor(COLOR_TEXT);
    char *msg1 = "CAMERA READY";
    while(*msg1) display_print(*msg1++);
    display_setCursor(60, 130);
    char *msg2 = "Press BTN1 to Snap";
    while(*msg2) display_print(*msg2++);

    // Release semaphore
    RTOS_SignalSemaphore(&sem_Display);

    // Reset flag
    take_photo_flag = false;

    // Ensure that photo is not sent or recieved outside of the app
    while(current_app == APP_CAMERA) {

        // Wait for flag
        if (take_photo_flag) {

            // Reset flag
            take_photo_flag = false;

            // Wait on seampahore
            RTOS_WaitSemaphore(&sem_Display);

            // Display information for debugging and also for user
            display_setCursor(80, 150);
            display_setTextColor(COLOR_SELECT);
            char *msg3 = "CAPTURING...";
            while(*msg3) display_print(*msg3++);

            // Release semaphore
            RTOS_SignalSemaphore(&sem_Display);

            // Signal photo
            UARTCharPut(UART_BASE, 'P');

            // Wait on semaphore
            RTOS_WaitSemaphore(&sem_Display);

            // Display photo
            for (y = 0; y < 240; y++) {
                for (x = 0; x < 240; x++) {
                    high_byte = (uint8_t)UARTCharGet(UART_BASE);
                    low_byte  = (uint8_t)UARTCharGet(UART_BASE);
                    pixel_color = (uint16_t)((high_byte << 8) | low_byte);
                    ST7789_DrawPixel(x, 240 - 1 - y, pixel_color);
                }
            }

            // Release semaphore
            RTOS_SignalSemaphore(&sem_Display);
        }

        // Small delay to release RTOS
        sleep(50);
    }
}

// 2. Compass App
void Compass_App(void) {

    const uint8_t active_addr = 0x13;
    const uint8_t DATA_START = 0x42;
    uint8_t raw[6];
    char location_header[LOCATION_BUF_SIZE];
    uint32_t location_timer = 0;

    // Wait on semaphore
    RTOS_WaitSemaphore(&sem_Display);

    // Clear screen
    ST7789_DrawRectangle(0, 0, MAX_SCREEN_X, MAX_SCREEN_Y, COLOR_BG);

    // Display app title
    display_setCursor(80, 260);
    display_setTextColor(COLOR_TEXT);
    char *title = "COMPASS";
    while(*title) display_print(*title++);
    RTOS_SignalSemaphore(&sem_Display);

    // Draw initial compass (needle facing 0)
    DrawCompass(0);

    // Wait on I2C semaphore for communication with Accelerometer
    RTOS_WaitSemaphore(&sem_I2C);

    // Initalize Magnetometer
    // PRECISE TIMING REQUIRED
    // Initalize sensor each time thread is active and disable it at the end rather than at the begenning
    // The BMI sensor uses a "STANDBY" mode which still draws current from the processor and uses the I2C bus
    // I2C Bus can be used elsewhere, and by keeping it active when there is no need for data can cause hangs, and potentially crash the RTOS
    // The threads already use a lot of memory so by keeping this bus active and storing data uses up memory that can be used elsewhere
    // This sensor is VERY buggy since you need to use double I2C to initalize it, so re-initializing it ensures a proper setup each time

    // Configure the Magnetometer to be connected via auxilary I2C pins
    BMI160_WriteRegister(INTERFERANCE_CONFIGURATION_REGISTER, MAGNETOMETER_I2C_ENABLE);
    SysCtlDelay(100000);

    // Enable magnetometer Normal Mode and enable secondary I2C on-chip
    BMI160_MagSetPmuMode(1);
    SysCtlDelay(100000);

    // USE MANUAL WRITE, Automatic mode does not work
    // Enable the Magnetometer
    BMI160_MagManualWrite(active_addr, MAGNETOMETER_POWER_REGISTER, MAGNETOMETER_POWER_ENABLE);
    SysCtlDelay(100000);

    // Set normal mode operation
    BMI160_MagManualWrite(active_addr, MAGNETOMETER_OPERATIONAL_MODE, MAGNETOMETER_NORMAL_OPERATION);
    SysCtlDelay(100000);

    // Set X and Y repetitions to 9
    BMI160_MagManualWrite(active_addr, MAGNETOMETER_X_Y_REPITIONS, MAGNETOMETER_X_Y_9_REPITIONS);

    // Set Z repetitions to 15, Z is a lot more noisy so more repitions for accuraccy
    // Not needed, but setup for future use, and required to setup for sensor to work
    BMI160_MagManualWrite(active_addr, MAGNETOMETER_Z_REPITIONS, MAGNETOMETER_Z_15_REPITIONS);

    // Enable 6 byte bursts (X, Y, Z)
    BMI160_WriteRegister(MAGNETOMETER_SETUP, MAGNETOMETER_6_BYTE_BURST);

    // Enable chip
    BMI160_WriteRegister(MAGNETOMETER_DATA_READ_ADDRESS, DATA_START);

    // Release Semaphore
    RTOS_SignalSemaphore(&sem_I2C);

    // Ensure that location data is not sent outside of the app
    while(current_app == APP_COMPASS) {

        // Wait on semaphore
        RTOS_WaitSemaphore(&sem_I2C);

        // Ensure magnetometer is connected to I2C
        BMI160_WriteRegister(INTERFERANCE_CONFIGURATION_REGISTER, MAGNETOMETER_I2C_ENABLE);

        // Check if data is ready
        int rc = BMI160_MagManualRead(active_addr, DATA_START, 6, raw);

        // Data is ready if bits are 0
        if (rc == 0) {

            // Extract data into individual bytes
            int16_t x = (int16_t)((raw[1] << 8) | raw[0]);
            int16_t y = (int16_t)((raw[3] << 8) | raw[2]);
            // Z byte is ommitted and not needed

            // As long as X and Y are nonzero, convert result to radians
            if (x != 0 || y != 0) {

                // Convert result to radians
                double heading_deg = atan2((double)y, (double)x) * (180.0 / M_PI);

                // Convert negative headings to positive
                if (heading_deg < 0.0) heading_deg += 360.0;

                // Draw compass
                DrawCompass(heading_deg);
            }
        }

        // Release Semaphore
        RTOS_SignalSemaphore(&sem_I2C);

        // Update location every 20 cycles
        if (location_timer++ > 20) {

            // Reset timer
            location_timer = 0;

            // Send character to recieve location
            UARTCharPut(UART_BASE, 'C');

            // Recieve location
            for (int i = 0; i < LOCATION_BUF_SIZE; i++) {
                location_header[i] = UARTCharGet(UART_BASE);
            }

            // Add null terminator for screen printing
            location_header[LOCATION_BUF_SIZE - 1] = '\0';

            // Wait for semaphore
            RTOS_WaitSemaphore(&sem_Display);

            // Display location
            ST7789_DrawRectangle(10, 40, 220, 20, 0x0000);
            display_setCursor(10, 50);
            char *ptr = location_header;
            while(*ptr) display_print(*ptr++);

            // Release semaphore
            RTOS_SignalSemaphore(&sem_Display);
        }

        // Small delay for RTOS
        sleep(100);
    }
}

// 3. Weather App
void Weather_App(void) {
    
    // Local variables
    char weather_buffer[LOCATION_BUF_SIZE];
    uint32_t weather_timer = 100;

    // Wait on semaphore
    RTOS_WaitSemaphore(&sem_Display);

    // Reset screen
    ST7789_DrawRectangle(0, 0, MAX_SCREEN_X, MAX_SCREEN_Y, COLOR_BG);

    // Display Weather app name
    display_setCursor(80, 260);
    display_setTextColor(COLOR_TEXT);
    char *msg = "WEATHER";
    while(*msg) display_print(*msg++);

    // Add loading screen for weather app (and debug)
    display_setCursor(80, 100);
    char *load = "Loading...";
    while(*load) display_print(*load++);
    RTOS_SignalSemaphore(&sem_Display);

    // Ensure weather data is not being sent outside of the app
    while(current_app == APP_WEATHER) {

        // Update weather every 50 cycles
        if (weather_timer++ > 50) {

            // Reset timer
            weather_timer = 0;

            // Signal for weather transmission
            UARTCharPut(UART_BASE, 'W');

            // Recieve weather data
            for(int i = 0; i < LOCATION_BUF_SIZE; i++) {
                weather_buffer[i] = UARTCharGet(UART_BASE);
            }

            // Add null terminator for proper C style character arrays
            weather_buffer[LOCATION_BUF_SIZE - 1] = '\0';

            // Reset pointers
            char *city = weather_buffer;
            char *temp = NULL;
            char *cond = NULL;
            char *detail = NULL;
            char *country = NULL;            

            // ******************  DATA PARSING *************************** //

            // strchr finds the first instance of a character in a string

            // Search for the first newline in city
            char *p1 = strchr(city, '\n');

            // Run as long as P1 exists, and is not the end of the string
            if(p1) { 
                *p1 = 0;       // Replace '\n' with NULL
                temp = p1 + 1; // Set the start of the next string (Temperature)
            }

            // Run as long as temp exists, and is not the end of the string
            if(temp) {
                char *p2 = strchr(temp, '\n'); // Search for first newline in temp
                if(p2) {
                    *p2 = 0;       // Replace '\n' with NULL
                    cond = p2 + 1; // Set the start of the next string (Condition)
                }
            }

            // Run as long as cond exists, and is not the end of the string
            if(cond) {
                char *p3 = strchr(cond, '\n'); // Search for first newline in cond
                if(p3) {
                    *p3 = 0;         // Replace '\n' with NULL
                    detail = p3 + 1; // Set the start of the next string (Details)
                }
            }

            // Search for the first comma in the city
            char *comma1 = strchr(city, ',');

            if(comma1) {
                char *comma2 = strchr(comma1 + 1, ','); // Search for the second comma in city
                if(comma2) {
                    *comma2 = 0;          // Replace comma with NULL
                    country = comma2 + 1; // Start the next string to be country to be printed on a different line

                    // Retain rest of string
                    while(*country == ' ') {
                        country++;
                    }
                }
            }

            // Wait on semaphore
            RTOS_WaitSemaphore(&sem_Display);

            // Clear screen
            ST7789_DrawRectangle(0, 20, 240, 220, COLOR_BG);

            // Use Transparent Text

            // Output Temperature
            display_setCursor(10, 240);
            display_setTextSize(5);
            display_setTextColor(COLOR_TEXT);
            char *ptr = temp;
            while(ptr && *ptr) {
                display_print(*ptr++);
            }

            // Output Condition
            display_setCursor(10, 190);
            display_setTextSize(2);
            display_setTextColor(COLOR_YELLOW);
            ptr = cond;
            while(ptr && *ptr) {
                display_print(*ptr++);
            }

            // Output City
            display_setCursor(10, 160);
            display_setTextSize(2);
            display_setTextColor(COLOR_CYAN);
            ptr = city;
            while(ptr && *ptr){ 
                display_print(*ptr++);
            }

            // Output Country (if country provided)
            if (country) {
                display_setCursor(10, 140);
                display_setTextSize(2);
                display_setTextColor(COLOR_CYAN);
                ptr = country;
                while(ptr && *ptr) {
                    display_print(*ptr++);
                }
            }

            // Output Details
            display_setCursor(10, 110);
            display_setTextSize(1);
            display_setTextColor(COLOR_TEXT);
            ptr = detail;
            while(ptr && *ptr) {
                display_print(*ptr++);
            }

            // Release semaphore
            RTOS_SignalSemaphore(&sem_Display);
        }

        // Small sleep to release RTOS
        sleep(100);
    }
}

// 4. Frogger App
void Frogger_App(void) {

    // Local variables

    // Calculate frog positions
    float frog_x = (GAME_WIDTH / 2) - (GRID_SIZE / 2);
    float frog_y = (NUM_LANES - 1) * GRID_SIZE;
    float prev_frog_x = frog_x;
    float prev_frog_y = frog_y;
    int move_cooldown = 0; // Reset timer

    // Loop through entities and set all of them as inactive
    for(int i = 0; i < MAX_ENTITIES; i++) {
        frogger_entities[i].active = false;
    }

    // Reset spawn timer
    uint32_t spawn_timer = 0;

    // Wait on sempahore
    RTOS_WaitSemaphore(&sem_Display);

    // Draw Grass
    ST7789_DrawRectangle(0, GAME_HEIGHT, GAME_WIDTH, MAX_SCREEN_Y - GAME_HEIGHT, COLOR_GRASS);

    // Draw lanes according to specifications in defines
    for(uint8_t i = 0; i < NUM_LANES; i++) {
        ST7789_DrawRectangle(0, i * GRID_SIZE, GAME_WIDTH, GRID_SIZE, LANE_COLORS[i]);
    }

    // Draw frog
    ST7789_DrawRectangle((int16_t)frog_x + FROG_OFFSET, (int16_t)frog_y + FROG_OFFSET, FROG_DRAW_SIZE, FROG_DRAW_SIZE, COLOR_GREEN);

    // Release semaphore
    RTOS_SignalSemaphore(&sem_Display);

    // Choose a random seed (course code)
    srand(4745);

    // Ensure game logic is not running while app is inactive
    while(current_app == APP_FROGGER) {
        
        // Spawn an entity every spawn rate
        if (spawn_timer++ > SPAWN_RATE) {

            // Reset timer
            spawn_timer = 0;

            // Find a "slot" to put the entity in the entity array
            int slot = -1;
            for (int i = 0; i < MAX_ENTITIES; i++) {
                if (!frogger_entities[i].active) {
                    slot = i;
                    break; 
                }
            }

            // Set entity to be alive
            if (slot != -1) {

                // Create entity object
                volatile Entity_t *e = &frogger_entities[slot];

                // Set entity to be alive
                e->active = true;

                // Create random width
                e->width_pixels = ((rand() % 3) + 2) * GRID_SIZE;

                // Choose random lane
                int lane = (rand() % 8) + 1;

                // Cap lane
                if (lane >= 5) {
                    lane++;
                }

                // Set the Y position to the lane chosen
                e->y = lane * GRID_SIZE;

                // Change state based on lane position
                e->is_log = (lane <= 4);

                // Set the color based on type
                if (e->is_log) {
                    e->color = COLOR_LOG;
                } else {
                    e->color = (rand() % 2) ? COLOR_CAR_YEL : COLOR_CAR_BLU;
                }

                // Set random speed
                e->speed = ((rand() % 3) + 1) * 0.5f;

                // Alternate lane directions and direction
                if (lane % 2 == 0) {
                    e->x = GAME_WIDTH;
                    e->speed = -e->speed;
                }
                else { 
                    e->x = - (float)e->width_pixels;
                }

                // Update positions
                e->prev_x = e->x;
            }
        }

        // Wait on semaphore
        RTOS_WaitSemaphore(&sem_Display);

        // Update entities
        for (int i = 0; i < MAX_ENTITIES; i++) {

            // Define object
            volatile Entity_t *e = &frogger_entities[i];

            // Do not update if entity is inactive
            if (!e->active) {
                continue;
            }

            int16_t old_x = (int16_t)e->prev_x; // Read previous position
            e->x += e->speed;                   // Update position based on speed
            int16_t new_x = (int16_t)e->x;      // Set new position

            // If entity exits screen, set it to inactive
            if (e->speed > 0 && e->x > GAME_WIDTH) {
                e->active = false;
            } else if (e->speed < 0 && (e->x + e->width_pixels) < 0) {
                e->active = false;
            }

            // Redraw objects, only the amount that they changed, not redrawing the entire object
            if (e->active) {
                if (abs(new_x - old_x) >= 1) {
                    ST7789_DrawRectangle(old_x, e->y, e->width_pixels, GRID_SIZE, LANE_COLORS[e->y / GRID_SIZE]);
                    ST7789_DrawRectangle(new_x, e->y, e->width_pixels, GRID_SIZE, e->color);
                    e->prev_x = e->x;
                }
            } else {
                ST7789_DrawRectangle(old_x, e->y, e->width_pixels, GRID_SIZE, LANE_COLORS[e->y / GRID_SIZE]);
            }
        }

        // Update position
        prev_frog_x = frog_x;
        prev_frog_y = frog_y;

        // Read joystick
        uint32_t joy = JOYSTICK_GetXY();
        int16_t jx = (int16_t)((joy >> 16) & 0xFFFF);
        int16_t jy = (int16_t)(joy & 0xFFFF);

        // Joystick Constants
        int center = 2048;
        int joy_dead = 1000;

        // Only allow user to move every so often
        if (move_cooldown > 0) {
            move_cooldown--;
        } else {

            // Move frog up
            if (jx > (center + joy_dead)) { 
                frog_y += GRID_SIZE;
                move_cooldown = 4; // Reset timer
            }

            // Move frog down
            if (jx < (center - joy_dead)) {
                frog_y -= GRID_SIZE;
                move_cooldown = 4; // Reset timer
            }

            // Move frog left
            if (jy > (center + joy_dead)) {
                frog_x -= GRID_SIZE;
                move_cooldown = 4; // Reset timer
            }

            // Move frog right
            if (jy < (center - joy_dead)) {
                frog_x += GRID_SIZE;
                move_cooldown = 4; // Reset timer
            }
        }

        // Block frog X movement
        if (frog_x < 0) {
            frog_x = 0;
        }

        // Block frog X movement
        if (frog_x > GAME_WIDTH - GRID_SIZE) {
            frog_x = GAME_WIDTH - GRID_SIZE;
        }

        // Block frog Y movement
        if (frog_y < 0) {
            frog_y = 0;
        }

        // Block frog Y movement
        if (frog_y > GAME_HEIGHT - GRID_SIZE) {
            frog_y = GAME_HEIGHT - GRID_SIZE;
        }

        // Death constants
        bool safe_on_log = false;
        bool hit_car = false;
        int lane_idx = (int)(frog_y / GRID_SIZE);
        bool on_river = (lane_idx >= 1 && lane_idx <= 4);

        // Loop through all entities
        for(int i = 0; i < MAX_ENTITIES; i++) {

            // Read entity
            volatile Entity_t *e = &frogger_entities[i];

            // Ignore inactive entities
            if (!e->active) {
                continue;
            }

            // Check colision with entities
            if (CheckCollision(frog_x, frog_y, e->x, e->y, e->width_pixels)) {

                // Set frog to safe on a log
                if (e->is_log) {
                    safe_on_log = true;
                    frog_x += e->speed; // Set speed of frog to log speed
                }
                else {
                    hit_car = true; // Frog not safe if hitting car :'(
                }
            }
        }

        // Reset frog death flag
        bool frog_died = false;

        // Kill frog if hit car
        if (hit_car) {
            frog_died = true;
        }

        // Kill frog if in river and not on log
        if (on_river && !safe_on_log) {
            frog_died = true;
        }

        if (frog_died) {

            // Signal frog death
            ST7789_DrawRectangle(0, 0, 240, 240, COLOR_RED);

            // Release RTOS when game is over to check other conditions, user does not need to play again IMMEDIATLEY
            sleep(200);

            // Reset frog position
            frog_x = (GAME_WIDTH / 2) - (GRID_SIZE / 2);
            frog_y = (NUM_LANES - 1) * GRID_SIZE;

            // Redraw lanes
            for(uint8_t i = 0; i < NUM_LANES; i++) {
                ST7789_DrawRectangle(0, i * GRID_SIZE, GAME_WIDTH, GRID_SIZE, LANE_COLORS[i]);
            }

            ST7789_DrawRectangle(0, GAME_HEIGHT, GAME_WIDTH, MAX_SCREEN_Y - GAME_HEIGHT, COLOR_GRASS);
        }
        else if (frog_y == 0) {

            // Signal victory
            ST7789_DrawRectangle(0, 0, 240, 240, COLOR_TEXT);

            // Release RTOS when game is over to check other conditions, user does not need to play again IMMEDIATLEY
            sleep(200);

            // Reset frog position
            frog_x = (GAME_WIDTH / 2) - (GRID_SIZE / 2);
            frog_y = (NUM_LANES - 1) * GRID_SIZE;

            // Redraw lanes
            for(uint8_t i = 0; i < NUM_LANES; i++) {
                ST7789_DrawRectangle(0, i * GRID_SIZE, GAME_WIDTH, GRID_SIZE, LANE_COLORS[i]);
            }

            ST7789_DrawRectangle(0, GAME_HEIGHT, GAME_WIDTH, MAX_SCREEN_Y - GAME_HEIGHT, COLOR_GRASS);
        }
        else {

            // Redraw frog with change in position
            if (abs((int)frog_x - (int)prev_frog_x) > 0 || abs((int)frog_y - (int)prev_frog_y) > 0) {
                ST7789_DrawRectangle((int16_t)prev_frog_x, (int16_t)prev_frog_y, GRID_SIZE, GRID_SIZE, LANE_COLORS[(int)(prev_frog_y/GRID_SIZE)]);
            }

            ST7789_DrawRectangle((int16_t)frog_x + FROG_OFFSET, (int16_t)frog_y + FROG_OFFSET, FROG_DRAW_SIZE, FROG_DRAW_SIZE, COLOR_GREEN);
        }

        // Release semaphore
        RTOS_SignalSemaphore(&sem_Display);

        // Release RTOS, small sleep for low latency
        sleep(60);
    }
}

// System Threads

void Home_Thread(void) {

    // Local variables
    uint32_t joystick_raw;
    int16_t joy_x, joy_y;
    uint8_t prev_selection = 255;

    // Buffer for time
    char time_buffer[LOCATION_BUF_SIZE];
    uint32_t time_timer = 50;

    // Wait for semaphore
    RTOS_WaitSemaphore(&sem_Display);

    // Clear screen
    ST7789_DrawRectangle(0, 0, MAX_SCREEN_X, MAX_SCREEN_Y, 0x0000);

    // Release semaphore
    RTOS_SignalSemaphore(&sem_Display);

    // Display for "Lock Screen"
    while(!is_unlocked) {

        // Fetch Time
        if (time_timer++ > 50) {
            
            // Reset timer
            time_timer = 0;

            // Signal to recieve time
            UARTCharPut(UART_BASE, 'T');

            // Recieve time
            for(int i = 0; i < LOCATION_BUF_SIZE; i++) {
                time_buffer[i] = UARTCharGet(UART_BASE);
            }

            // Add null terminator for proper C character array
            time_buffer[LOCATION_BUF_SIZE - 1] = '\0';

            // Wait for semaphore
            RTOS_WaitSemaphore(&sem_Display);

            // Clear area again (sanity check)
            ST7789_DrawRectangle(0, 60, 240, 60, 0x0000);

            // Draw Time (Y=100) -> Draws up to 72 (Size 4)
            // Safe within 60-120

            // Display time
            display_setCursor(30, 100);
            display_setTextColor(COLOR_TEXT); // Transparent Text
            display_setTextSize(4);
            char *t = time_buffer;
            while(*t) display_print(*t++);
            RTOS_SignalSemaphore(&sem_Display);
        }

        // Lock Screen Messages
        if (time_timer == 10) {

            // Wait for semaphore
            RTOS_WaitSemaphore(&sem_Display);

            // Clear Lower Area: Y=140 to 220
            ST7789_DrawRectangle(0, 140, 240, 80, 0x0000);

            // Display Lockscreen Text

            // Text Y=180 (Draws up to 164)
            display_setCursor(40, 180);
            display_setTextColor(COLOR_TEXT); // Transparent Text
            display_setTextSize(2);
            char *l1 = "PHONE LOCKED!";
            while(*l1) display_print(*l1++);

            // Text Y=210
            display_setCursor(20, 210);
            display_setTextSize(1);
            char *l2 = "Show face to camera to unlock";
            while(*l2) display_print(*l2++);
            RTOS_SignalSemaphore(&sem_Display);
        }

        // Recieve unlocked status
        if (UARTCharsAvail(UART_BASE)) {
            char c = UARTCharGet(UART_BASE);
            if (c == 'U') is_unlocked = true;
        }

        // Release RTOS
        sleep(50);
    }

    // Main Menu Loop
    while(1) {
        if (current_app != APP_NONE) {

            // Run Camera App
            if (current_app == APP_CAMERA) {
                Camera_App();
            }
            // Run Compass App
            else if (current_app == APP_COMPASS) {
                Compass_App();
            }
            // Run Weather App
            else if (current_app == APP_WEATHER) {
                Weather_App();
            }
            // Run Frogger App
            else if (current_app == APP_FROGGER) {
                Frogger_App();
            }
            // Reset selection
            prev_selection = 255;
        }

        // Wait for semaphore
        RTOS_WaitSemaphore(&sem_Display);

        if (prev_selection == 255) {
            DrawHome_Static();                         // Draw "Home Screen"
            UpdateHome_Cursor(255, selected_icon_idx); // Update cursor
        } else if (prev_selection != selected_icon_idx) {
            // Update cursor
            UpdateHome_Cursor(prev_selection, selected_icon_idx);
        }

        // Release semaphore
        RTOS_SignalSemaphore(&sem_Display);

        // Update selection
        prev_selection = selected_icon_idx;

        // Read Joystick
        joystick_raw = JOYSTICK_GetXY();
        joy_x = (int16_t)((joystick_raw >> 16) & 0xFFFF);
        joy_y = (int16_t)(joystick_raw & 0xFFFF);

        // Joystick Values
        int center = 2048;
        int threshold = 1000;

        // Update X position between apps
        if (joy_x > (center + threshold)) {
            if (selected_icon_idx < 2) {
                selected_icon_idx += 2;
            }
        } else if (joy_x < (center - threshold)) {
            if (selected_icon_idx >= 2) {
                selected_icon_idx -= 2;
            }
        }

        // Update Y position between apps
        if (joy_y > (center + threshold)) { 
            if (selected_icon_idx % 2 != 0) {
                selected_icon_idx--;
            }
        } else if (joy_y < (center - threshold)) {
            if (selected_icon_idx % 2 == 0) {
                selected_icon_idx++;
            }
        }

        // Release RTOS
        sleep(150);
    }
}

// Button read
void Read_Buttons(void) {

    // Local variable for button state
    uint8_t buttons;
    uint8_t prev_buttons = 0;

    while(1) {

        // Wait for aperiodic signal
        RTOS_WaitSemaphore(&sem_Button);

        // Recieve what button was pressed
        buttons = MultimodButtons_Get();

        // Button logic
        if ((buttons & BUTTON_SELECT_MASK) && !(prev_buttons & BUTTON_SELECT_MASK)) {
            if (current_app == APP_NONE) {

                // Select apps
                if (selected_icon_idx == 0) {
                    current_app = APP_CAMERA;
                } else if (selected_icon_idx == 1) {
                    current_app = APP_COMPASS;
                } else if (selected_icon_idx == 2) {
                    current_app = APP_WEATHER;
                } else if (selected_icon_idx == 3) {
                    current_app = APP_FROGGER;
                }

            }

            // Button 1 also used for camera
            else if (current_app == APP_CAMERA) {
                take_photo_flag = true;
            }
        }

        // Button 4 for returning home
        if ((buttons & BUTTON_HOME_MASK) && !(prev_buttons & BUTTON_HOME_MASK)) {
            current_app = APP_NONE;
        }

        // Update Button state
        prev_buttons = buttons;

        // Clear and enable interrupts
        GPIOIntClear(BUTTON_PORT_BASE, BUTTON_PINS);
        GPIOIntEnable(BUTTON_PORT_BASE, BUTTON_PINS);

        // Release RTOS
        sleep(50);
    }
}

// Idle Thread, REQUIRED for RTOS
void Idle_Thread(void) {
    while(1);
}

// Aperioidic button handler
void Button_Handler(void) {

    // Disable interrupts
    GPIOIntDisable(BUTTON_PORT_BASE, BUTTON_PINS);

    // Signal semaphore for periodic thread
    RTOS_SignalSemaphore(&sem_Button);

    // Recieve what button was pressed
    uint32_t status = GPIOIntStatus(GPIO_PORTE_BASE, 1);

    // Clear interrupt
    GPIOIntClear(GPIO_PORTE_BASE, status);
}
