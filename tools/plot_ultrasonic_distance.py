#!/usr/bin/env python3
"""
===============================================================================
 HỆ THỐNG GIÁM SÁT KHOẢNG CÁCH SIÊU ÂM THỜI GIAN THỰC (ULTRASONIC LIVE PLOTTER)
===============================================================================
 Real-time Ultrasonic Distance Serial Line Plotter for ESP32 & JSN-SR04T:
 - Parses Serial log output from ESP32 or raw UART distance stream.
===============================================================================
"""

import sys
import os
import time
import re
import argparse
from collections import deque

# Đảm bảo Encoding UTF-8 trên Terminal Windows
if sys.platform == 'win32':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    except Exception:
        pass

import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt
import matplotlib.animation as animation


def list_com_ports():
    """Liệt kê tất cả cổng COM đang hoạt động trên hệ thống."""
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("[!] Khong tim thay cong COM nao tren may tinh!")
        return None
    
    print("\n--- DANH SACH CONG COM KHA DUNG ---")
    for i, p in enumerate(ports):
        print(f" [{i + 1}] {p.device} - {p.description}")
    print("------------------------------------\n")
    return ports


def parse_arguments():
    parser = argparse.ArgumentParser(description="Real-time Ultrasonic Distance Serial Plotter")
    parser.add_argument("--port", type=str, default=None, help="Com port name (e.g. COM3, COM9)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (Default: 115200 for ESP32)")
    parser.add_argument("--window", type=int, default=100, help="Number of data points on graph window (Default: 100)")
    parser.add_argument("--max-dist", type=float, default=450.0, help="Maximum Y-axis distance in cm (Default: 450cm)")
    parser.add_argument("--min-dist", type=float, default=0.0, help="Minimum Y-axis distance in cm")
    return parser.parse_args()


class RealtimePlotter:
    def __init__(self, port, baud, window_size, min_dist, max_dist):
        self.port = port
        self.baud = baud
        self.window_size = window_size
        self.min_dist = min_dist
        self.max_dist = max_dist

        # Đệm chứa dữ liệu thời gian thực
        self.timestamps = deque(maxlen=window_size)
        self.distances = deque(maxlen=window_size)
        self.start_time = time.time()

        # Thống kê
        self.total_samples = 0
        self.failed_samples = 0

        # Mở cổng Serial
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.1)
            print(f"[OK] Da mo thanh cong cong {self.port} at {self.baud} Baud!")
        except Exception as e:
            print(f"[ERROR] Khong the mo cong Serial {self.port}: {e}")
            sys.exit(1)

        # Cấu hình Giao diện Matplotlib (Giao diện Dark Mode hiện đại)
        plt.style.use('dark_background')
        self.fig, self.ax = plt.subplots(figsize=(10, 6))
        self.fig.canvas.manager.set_window_title("Hệ Thống Giám Sát Khoảng Cách Siêu Âm - Live Plotter")

        # Đường biểu diễn dữ liệu (Line Chart)
        self.line, = self.ax.plot([], [], color='#00E5FF', linewidth=2.0, label='Khoảng cách (cm)')

        # Thiết lập nhãn & khung đồ thị
        self.ax.set_title("ĐỒ THỊ KHOẢNG CÁCH THỜI GIAN THỰC (JSN-SR04T / ESP32-S3)", fontsize=14, fontweight='bold', pad=15, color='#00E5FF')
        self.ax.set_xlabel("Thời gian (giây)", fontsize=11, color='#B0BEC5')
        self.ax.set_ylabel("Khoảng cách (cm)", fontsize=11, color='#B0BEC5')
        self.ax.set_ylim(self.min_dist, self.max_dist)
        self.ax.grid(True, linestyle='--', alpha=0.3, color='#455A64')

        # Thẻ thông tin hiển thị chỉ số thời gian thực (Text Box Overlay)
        self.info_text = self.ax.text(
            0.02, 0.95, "", transform=self.ax.transAxes, fontsize=10,
            verticalalignment='top', bbox=dict(boxstyle='round,pad=0.5', facecolor='#1A2634', alpha=0.8, edgecolor='#00E5FF')
        )

        self.ax.legend(loc='upper right', facecolor='#1A2634', edgecolor='#00E5FF')

    def parse_serial_line(self, line_str):
        """
        Phân tích chuỗi đọc từ Serial.
        Hỗ trợ mẫu log ESP32: "Sensor 1 (GPIO 43):  450 mm ( 45.0 cm) [OK]"
        """
        # Mẫu 1: Tìm "XX.X cm" hoặc "XX cm"
        match_cm = re.search(r'(\d+(?:\.\d+)?)\s*cm', line_str, re.IGNORECASE)
        if match_cm:
            return float(match_cm.group(1))

        # Mẫu 2: Tìm "XX mm" -> chuyển sang cm
        match_mm = re.search(r'(\d+(?:\.\d+)?)\s*mm', line_str, re.IGNORECASE)
        if match_mm:
            return float(match_mm.group(1)) / 10.0

        # Mẫu 3: Chuỗi số thuần
        try:
            val = float(line_str.strip())
            return val
        except ValueError:
            return None

    def read_data(self):
        """Đọc dòng dữ liệu mới nhất từ UART."""
        if not self.ser or not self.ser.is_open:
            return

        try:
            while self.ser.in_waiting > 0:
                raw_line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if not raw_line:
                    continue

                dist_cm = self.parse_serial_line(raw_line)
                if dist_cm is not None:
                    curr_time = time.time() - self.start_time
                    self.timestamps.append(curr_time)
                    self.distances.append(dist_cm)
                    self.total_samples += 1
                else:
                    if "[FAIL]" in raw_line or "TIMEOUT" in raw_line:
                        self.failed_samples += 1
        except Exception:
            pass

    def update_plot(self, frame):
        """Hàm cập nhật đồ thị cho Matplotlib FuncAnimation."""
        self.read_data()

        if self.timestamps:
            x_data = list(self.timestamps)
            y_data = list(self.distances)

            self.line.set_data(x_data, y_data)

            # Tự động trượt trục X theo thời gian thực
            if x_data[-1] > 10:
                self.ax.set_xlim(x_data[-1] - 10, x_data[-1] + 1)
            else:
                self.ax.set_xlim(0, 10)

            # Tính toán thống kê
            current_val = y_data[-1]
            min_val = min(y_data)
            max_val = max(y_data)
            avg_val = sum(y_data) / len(y_data)

            # Cập nhật khung thông tin
            info_str = (
                f"CHỈ SỐ THỜI GIAN THỰC:\n"
                f" ► Hiện tại  : {current_val:6.1f} cm\n"
                f" ► Trung bình: {avg_val:6.1f} cm\n"
                f" ► Thấp nhất : {min_val:6.1f} cm\n"
                f" ► Cao nhất  : {max_val:6.1f} cm\n"
                f" ► Tổng mẫu  : {self.total_samples} (Lỗi: {self.failed_samples})"
            )
            self.info_text.set_text(info_str)

        return self.line, self.info_text

    def run(self):
        """Khởi chạy animation đồ thị thời gian thực."""
        ani = animation.FuncAnimation(
            self.fig, self.update_plot, interval=50, blit=False, cache_frame_data=False
        )
        plt.tight_layout()
        plt.show()

        if self.ser and self.ser.is_open:
            self.ser.close()
            print("[INFO] Da dong ket noi Serial.")


def main():
    args = parse_arguments()

    port = args.port
    if port is None:
        ports = list_com_ports()
        if not ports:
            sys.exit(1)
        if len(ports) == 1:
            port = ports[0].device
            print(f"[*] Tu dong chon cong COM duy nhat: {port}")
        else:
            try:
                choice = int(input("Nhap so thu tu cong COM muon ket noi: "))
                port = ports[choice - 1].device
            except (ValueError, IndexError):
                print("[ERROR] Lua chon khong hop le!")
                sys.exit(1)

    print(f"\n🚀 Dang khoi chay Do thi Khoang cach Sieu am tren cong {port}...")
    plotter = RealtimePlotter(
        port=port,
        baud=args.baud,
        window_size=args.window,
        min_dist=args.min_dist,
        max_dist=args.max_dist
    )
    plotter.run()


if __name__ == "__main__":
    main()
