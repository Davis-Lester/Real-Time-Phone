# Real-Time Phone & Smart Console (RTOS)

**Davis Lester | University of Florida | EEL4745C Final Project**

This project is a multitasking embedded system that simulates a smartphone operating system. Built from the ground up on a custom Real-Time Operating System (**RTOS**), it features biometric security (Face Unlock), internet-connected apps (Weather, Location), a live camera feed, and a fully playable arcade game (Frogger).

The system distributes processing between a **Python Host Server** (running on a laptop) and the **Tiva C MCU**, communicating over a high-speed UART link to enable features typically impossible on a standalone microcontroller.

---

## 📸 Gallery

| **Lock Screen (Face ID)** | **Home Menu** | **Live Camera** |
|:---:|:---:|:---:|
| ![Lock Screen](https://github.com/user-attachments/assets/e13f32cd-f1af-400d-aba5-145b85019224) | ![Home Menu](https://github.com/user-attachments/assets/7eca1aae-322e-457b-b131-85efda329ec3) | ![Camera App](https://github.com/user-attachments/assets/2240b7e1-ee59-4bb9-9a76-ccd5bcfb0a44) |
| *Secure entry with real-time clock* | *Joystick-navigable app grid* | *Video streaming from laptop* |

| **Live Weather** | **Digital Compass** | **Frogger Game** |
|:---:|:---:|:---:|
| ![Weather App](https://github.com/user-attachments/assets/4d2543c2-55f3-4d09-9304-02865dd076d4) | ![Compass App](https://github.com/user-attachments/assets/9c7723cd-30bb-4015-8521-47d4dda47566) | ![Frogger](https://github.com/user-attachments/assets/e79b7798-e694-4dad-a863-efa1c1e517c9) |
| *Live API data via UART* | *Real-time magnetometer data* | *Physics, collisions & sprites* |
---

## 🛠️ System Architecture

### Hardware Stack
* **Microcontroller:** TI Tiva C Series LaunchPad (TM4C123GH6PM - ARM Cortex-M4F)
* **Display:** 1.54" ST7789 TFT LCD (240x240 Resolution, SPI Interface)
* **Sensors:** BMI160 IMU (Magnetometer/Accelerometer) via I2C
* **Input:** Educational BoosterPack MKII (Joystick, Buttons)
* **Expansion:** Multimod Board

### Software Stack
1.  **RTOS (The Kernel):** A custom-written preemptive real-time operating system supporting:
    * **Multithreading:** Handles UI, sensing, and communication concurrently.
    * **Semaphores:** Protects shared resources (I2C bus, SPI Display) from race conditions.
    * **FIFOs:** Enables safe data passing between threads (e.g., Joystick data -> Game Logic).
    * **Priority Scheduling:** Ensures critical tasks (like UART interrupts) preempt background tasks.

2.  **Python Host Server:** A script running on the connected laptop that acts as the "Internet & Camera" provider. It uses:
    * **OpenCV:** For face detection and capturing webcam frames.
    * **Requests:** To fetch live weather and geolocation data APIs.
    * **PySerial:** To packetize and transmit data to the MCU.

---

## 📡 Communication Protocol (UART)

The system relies on a high-speed **460,800 baud** UART connection. The MCU acts as the client, sending single-character commands, and the Python server responds with data packets.

| Command | Direction&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; | Description | Response Data Format |
| :--- | :--- | :--- | :--- |
| `'U'` | Python → MCU | **Unlock Signal.** Sent automatically when OpenCV detects a face. | None (State change) |
| `'T'` | MCU → Python | **Time Request.** Fetches current system time. | String: `"12:45 PM"` (null-terminated) |
| `'W'` | MCU → Python | **Weather Request.** Fetches live weather from `wttr.in`. | String: `"City\nTemp\nCondition\nHum/Wind"` |
| `'C'` | MCU → Python | **Location Request.** Fetches GPS coordinates via IP API. | String: `"Lat: 12.34, Lon: -56.78"` |
| `'P'` | MCU → Python | **Photo Request.** Fetches a single frame from the webcam. | Raw Bytes: RGB565 pixel data (High/Low byte) |

---

## 📂 Project Structure

* `Threads.c`: Main application logic, UI drawing, and app definitions.
* `Camera.py`: Host-side processing for AI, Internet, and Time.
* `RTOS/`: Core OS kernel files (Scheduler, Semaphores, IPC).
* `MultimodDrivers/`: Hardware drivers for ST7789 (Display), BMI160 (IMU), and Buttons.
* `Bitmaps/`: Header files containing pixel arrays for app icons (`Camera.h`, `Weather.h`, etc.).