import serial
import cv2
import struct
import time
import numpy as np
import requests
from datetime import datetime

# --- CONFIGURATION ---
SERIAL_PORT = 'COM10'   # Check Device Manager
BAUD_RATE = 460800
IMG_WIDTH = 240        # Full Screen
IMG_HEIGHT = 240
LOCATION_BUF_SIZE = 128 

# --- STATES ---
STATE_LOCKED = 0
STATE_UNLOCKED = 1
current_state = STATE_LOCKED

# US State Abbreviations Map
STATE_MAP = {
    "Alabama": "AL", "Alaska": "AK", "Arizona": "AZ", "Arkansas": "AR", "California": "CA",
    "Colorado": "CO", "Connecticut": "CT", "Delaware": "DE", "Florida": "FL", "Georgia": "GA",
    "Hawaii": "HI", "Idaho": "ID", "Illinois": "IL", "Indiana": "IN", "Iowa": "IA",
    "Kansas": "KS", "Kentucky": "KY", "Louisiana": "LA", "Maine": "ME", "Maryland": "MD",
    "Massachusetts": "MA", "Michigan": "MI", "Minnesota": "MN", "Mississippi": "MS", "Missouri": "MO",
    "Montana": "MT", "Nebraska": "NE", "Nevada": "NV", "New Hampshire": "NH", "New Jersey": "NJ",
    "New Mexico": "NM", "New York": "NY", "North Carolina": "NC", "North Dakota": "ND", "Ohio": "OH",
    "Oklahoma": "OK", "Oregon": "OR", "Pennsylvania": "PA", "Rhode Island": "RI", "South Carolina": "SC",
    "South Dakota": "SD", "Tennessee": "TN", "Texas": "TX", "Utah": "UT", "Vermont": "VT",
    "Virginia": "VA", "Washington": "WA", "West Virginia": "WV", "Wisconsin": "WI", "Wyoming": "WY"
}

def get_current_time():
    """Returns current time string (e.g., '12:45 PM')."""
    return datetime.now().strftime("%I:%M %p").lstrip('0')

def get_device_location():
    try:
        response = requests.get('http://ip-api.com/json/', timeout=2)
        data = response.json()
        if data['status'] == 'success':
            return f"Lat: {data['lat']}, Lon: {data['lon']}"
        else:
            return "Loc: Unknown Region"
    except:
        return "Loc: Unavailable"

def get_weather():
    try:
        url = "http://wttr.in?format=%l|%C|%t|%h|%w"
        response = requests.get(url, timeout=2)
        
        if response.status_code == 200:
            raw = response.text
            clean = raw.replace('°', '').replace('+', '')
            clean = clean.encode('ascii', 'ignore').decode('ascii')
            
            parts = clean.split('|')
            if len(parts) >= 5:
                city_raw = parts[0].strip()
                cond = parts[1].strip()
                temp = parts[2].strip()
                hum  = parts[3].strip()
                wind = parts[4].strip()
                
                # Abbreviate State
                for state, abbr in STATE_MAP.items():
                    if state in city_raw:
                        city_raw = city_raw.replace(state, abbr)
                        break
                
                final_str = f"{city_raw}\n{temp}\n{cond}\nHum:{hum} Wind:{wind}"
                return final_str
            else:
                return "Unknown\n--\nError\n--"
        else:
            return "Error\n--\nAPI Fail\n--"
    except:
        return "Offline\n--\nNo Conn\n--"

def convert_to_rgb565(frame):
    frame = cv2.resize(frame, (IMG_WIDTH, IMG_HEIGHT))
    frame = cv2.flip(frame, 1)
    b, g, r = cv2.split(frame)
    r = r.astype(np.uint16)
    g = g.astype(np.uint16)
    b = b.astype(np.uint16)
    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return rgb565.astype(np.dtype('>u2')).tobytes()

def run_server():
    global current_state
    
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        print(f"Connected to {SERIAL_PORT} at {BAUD_RATE}")
    except:
        print(f"Error: Could not open {SERIAL_PORT}")
        return

    face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')
    cap = cv2.VideoCapture(0)

    print("--- PHONE LOCKED: Show Face to Unlock ---")

    while True:
        ret, frame = cap.read()
        if not ret: continue

        if current_state == STATE_LOCKED:
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            faces = face_cascade.detectMultiScale(gray, 1.3, 5)
            for (x, y, w, h) in faces:
                cv2.rectangle(frame, (x, y), (x+w, y+h), (255, 0, 0), 2)
            cv2.imshow("Face Scanner", frame)
            
            # --- TIME REQUEST HANDLER ---
            if ser.in_waiting > 0:
                try:
                    cmd = ser.read().decode('utf-8', errors='ignore')
                    if cmd == 'T': 
                        print("[MCU] Time Request Received.")
                        time_str = get_current_time()
                        t_bytes = time_str.encode('utf-8')
                        
                        # Pad with Nulls
                        if len(t_bytes) < LOCATION_BUF_SIZE:
                            t_bytes += b'\x00' * (LOCATION_BUF_SIZE - len(t_bytes))
                        else:
                            t_bytes = t_bytes[:LOCATION_BUF_SIZE]
                            
                        ser.write(t_bytes)
                        print(f"Sent Time: {time_str}")
                        
                    # Handle Unlock if MCU sends 'U' manually
                    elif cmd == 'U':
                        current_state = STATE_UNLOCKED
                        cv2.destroyAllWindows()
                except Exception as e:
                    print(f"Error reading in Lock: {e}")

            if len(faces) > 0:
                print("Face Detected! Unlocking...")
                ser.write(b'U')
                current_state = STATE_UNLOCKED
                cv2.destroyAllWindows()
                print("--- PHONE UNLOCKED: Ready ---")
                time.sleep(1)

        else: # UNLOCKED
            if ser.in_waiting > 0:
                try:
                    command = ser.read().decode('utf-8', errors='ignore')
                    
                    if command == 'P': # Photo
                        print("[MCU] Photo Request.")
                        ret, frame = cap.read()
                        img_data = convert_to_rgb565(frame)
                        print("Sending photo...")
                        CHUNK_SIZE = 1024 
                        for i in range(0, len(img_data), CHUNK_SIZE):
                            ser.write(img_data[i:i+CHUNK_SIZE])
                        print("Done.")

                    elif command == 'C': # Compass Location
                        print("[MCU] Location Request.")
                        loc_str = get_device_location()
                        loc_bytes = loc_str.encode('utf-8')
                        if len(loc_bytes) < LOCATION_BUF_SIZE:
                            loc_bytes += b'\x00' * (LOCATION_BUF_SIZE - len(loc_bytes))
                        else:
                            loc_bytes = loc_bytes[:LOCATION_BUF_SIZE]
                        ser.write(loc_bytes)
                        print(f"Sent: {loc_str}")

                    elif command == 'W': # Weather
                        print("[MCU] Weather Request.")
                        weath_str = get_weather()
                        w_bytes = weath_str.encode('utf-8')
                        if len(w_bytes) < LOCATION_BUF_SIZE:
                            w_bytes += b'\x00' * (LOCATION_BUF_SIZE - len(w_bytes))
                        else:
                            w_bytes = w_bytes[:LOCATION_BUF_SIZE]
                        ser.write(w_bytes)
                        print(f"Sent:\n{weath_str}")

                except Exception as e:
                    print(f"Error: {e}")
                    pass
        
        # Keep OpenCV window responsive
        cv2.waitKey(1)

if __name__ == "__main__":
    run_server()