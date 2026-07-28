import sys
import time
import serial

def read_esp32_serial(port='COM9', baudrate=115200, duration=5):
    """
    Reads serial logs from an ESP32 target port for a given duration.
    """
    print(f"[ESP32 Debug] Opening serial port {port} at {baudrate} baud...")
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        start_time = time.time()
        print(f"[ESP32 Debug] Listening for {duration} seconds...\n" + "="*50)
        while time.time() - start_time < duration:
            line = ser.readline()
            if line:
                decoded = line.decode('utf-8', errors='ignore')
                sys.stdout.write(decoded)
                sys.stdout.flush()
        ser.close()
        print("="*50 + "\n[ESP32 Debug] Serial capture finished.")
    except Exception as e:
        print(f"[ESP32 Debug Error] Failed to read serial port {port}: {e}")

if __name__ == '__main__':
    port = sys.argv[1] if len(sys.argv) > 1 else 'COM9'
    duration = int(sys.argv[2]) if len(sys.argv) > 2 else 5
    read_esp32_serial(port=port, duration=duration)
