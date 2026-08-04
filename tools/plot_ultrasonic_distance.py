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
    parser.add_argument("--port", type=str, default="COM12", help="Com port name (Default: COM12)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (Default: 115200 for ESP32)")
    parser.add_argument("--window", type=int, default=100, help="Number of data points on graph window (Default: 100)")
    parser.add_argument("--max-dist", type=float, default=None, help="Y-axis ban đầu (cm) trước khi có đủ dữ liệu để tự scale. Bỏ trống để tool tự chọn.")
    parser.add_argument("--min-dist", type=float, default=None, help="Y-axis ban đầu (cm), tương tự --max-dist")
    return parser.parse_args()
class RealtimePlotter:
    def __init__(self, port, baud, window_size, min_dist, max_dist):
        self.port = port
        self.baud = baud
        self.window_size = window_size
        # Khung nhìn Y hiện tại (cm): sẽ được tool tự nới theo giá trị lớn nhất ghi nhận được (autoscale)
        self.view_min = min_dist if min_dist is not None else 0.0
        self.view_max = max_dist if max_dist is not None else 50.0
        # Giá trị lớn nhất ghi nhận được trong khung thời gian (time frame) đang hiển thị (cm)
        self.max_seen = None
        self.ani = None
        # Đệm chứa dữ liệu thời gian thực
        self.timestamps = deque(maxlen=window_size)
        self.raw_distances = deque(maxlen=window_size)
        self.filtered_distances = deque(maxlen=window_size)
        self.start_time = time.time()
        # Thống kê
        self.total_samples = 0
        self.failed_samples = 0
        # Mở cổng Serial
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.02)
            print(f"[OK] Da mo thanh cong cong {self.port} at {self.baud} Baud!")
        except Exception as e:
            print(f"[ERROR] Khong the mo cong Serial {self.port}: {e}")
            sys.exit(1)
        # Cấu hình Giao diện Matplotlib (Giao diện Dark Mode hiện đại)
        plt.style.use('dark_background')
        self.fig, self.ax = plt.subplots(figsize=(10, 6))
        self.fig.canvas.manager.set_window_title("Hệ Thống Giám Sát Khoảng Cách Siêu Âm - Live Plotter")
        # Đường biểu diễn dữ liệu (Line Chart): Raw (nhiễu) vs Filtered (Kalman)
        self.line_raw, = self.ax.plot([], [], color='#FF6E40', linewidth=1.0, alpha=0.6, label='Raw dist (cm)')
        self.line_filtered, = self.ax.plot([], [], color='#00E5FF', linewidth=2.0, label='Filtered dist - Kalman (cm)')
        # Thiết lập nhãn & khung đồ thị
        self.ax.set_title("ĐỒ THỊ KHOẢNG CÁCH THỜI GIAN THỰC (JSN-SR04T / ESP32-S3)", fontsize=14, fontweight='bold', pad=15, color='#00E5FF')
        self.ax.set_xlabel(f"Mẫu gần nhất (0-{window_size}, mới nhất bên phải)", fontsize=11, color='#B0BEC5')
        self.ax.set_ylabel("Khoảng cách (cm)", fontsize=11, color='#B0BEC5')
        self.ax.set_xlim(0, window_size - 1)
        self.ax.set_ylim(self.view_min, self.view_max)
        self.ax.grid(True, linestyle='--', alpha=0.3, color='#455A64')
        # Thẻ thông tin hiển thị chỉ số thời gian thực (Text Box Overlay)
        self.info_text = self.ax.text(
            0.02, 0.95, "", transform=self.ax.transAxes, fontsize=10,
            verticalalignment='top', bbox=dict(boxstyle='round,pad=0.5', facecolor='#1A2634', alpha=0.8, edgecolor='#00E5FF')
        )
        self.ax.legend(loc='upper right', facecolor='#1A2634', edgecolor='#00E5FF')
    def parse_serial_line(self, line_str):
        """
        Phân tích chuỗi log từ main.cpp:
        "Raw dist: 1234.5 mm | Filtered dist: 1230.2 mm"
        Trả về (raw_cm, filtered_cm); giá trị nào không tìm thấy trả về None.
        Mọi giá trị đều được quy đổi về cm để hiển thị.
        """
        raw_val = None
        filtered_val = None
        match_raw = re.search(r'Raw dist:\s*(\d+(?:\.\d+)?)\s*mm', line_str, re.IGNORECASE)
        if match_raw:
            raw_val = float(match_raw.group(1)) / 10.0
        match_filtered = re.search(r'Filtered dist:\s*(\d+(?:\.\d+)?)\s*mm', line_str, re.IGNORECASE)
        if match_filtered:
            filtered_val = float(match_filtered.group(1)) / 10.0
        # Fallback: dòng chỉ có 1 giá trị "XX mm" hoặc "XX cm" -> coi là raw
        if raw_val is None and filtered_val is None:
            match_cm = re.search(r'(\d+(?:\.\d+)?)\s*cm', line_str, re.IGNORECASE)
            if match_cm:
                raw_val = float(match_cm.group(1))
            else:
                match_mm = re.search(r'(\d+(?:\.\d+)?)\s*mm', line_str, re.IGNORECASE)
                if match_mm:
                    raw_val = float(match_mm.group(1)) / 10.0
        return raw_val, filtered_val
    def read_data(self):
        """Đọc dòng dữ liệu mới nhất từ UART."""
        while self.ser.in_waiting > 0:
            try:
                raw_line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if not raw_line:
                    continue
                raw_val, filtered_val = self.parse_serial_line(raw_line)
                if raw_val is not None or filtered_val is not None:
                    curr_time = time.time() - self.start_time
                    self.timestamps.append(curr_time)
                    self.raw_distances.append(raw_val if raw_val is not None else float('nan'))
                    self.filtered_distances.append(filtered_val if filtered_val is not None else float('nan'))
                    self.total_samples += 1
                else:
                    if "[FAIL]" in raw_line or "TIMEOUT" in raw_line:
                        self.failed_samples += 1
            except Exception:
                pass
    def _autoscale_y(self, all_values):
        """
        Scale trục Y (thanh đo, đơn vị cm) theo giá trị LỚN NHẤT trong khung thời gian
        (time frame / window) đang hiển thị trên đồ thị (self.window_size mẫu gần nhất):
        - Trục Y luôn bắt đầu từ 0.
        - Đỉnh trục Y = max hiện tại trong window * 1.1 (đệm 10%), cập nhật lại theo
          từng frame nên khi giá trị lớn nhất trôi ra khỏi window, thanh đo sẽ co lại theo.
        """
        if not all_values:
            return
        data_max = max(all_values)
        new_view_max = max(data_max * 1.1, 1.0)
        # Chỉ set lại ylim khi thay đổi đáng kể để tránh giật hình mỗi frame
        if self.max_seen is None or abs(new_view_max - self.view_max) > self.view_max * 0.03:
            self.max_seen = data_max
            self.view_min = 0.0
            self.view_max = new_view_max
            self.ax.set_ylim(self.view_min, self.view_max)
    def update_plot(self, frame):
        """Hàm cập nhật đồ thị cho Matplotlib FuncAnimation (chỉ vẽ lại phần dữ liệu đổi - blit)."""
        self.read_data()
        if self.timestamps:
            # Trục X cố định theo số mẫu (không đổi xlim mỗi frame) để blit hoạt động mượt
            n = len(self.raw_distances)
            x_data = list(range(self.window_size - n, self.window_size))
            raw_y = list(self.raw_distances)
            filtered_y = list(self.filtered_distances)
            self.line_raw.set_data(x_data, raw_y)
            self.line_filtered.set_data(x_data, filtered_y)
            # Tính toán thống kê (dựa trên giá trị đã lọc Kalman, bỏ qua NaN)
            valid_filtered = [v for v in filtered_y if v == v]  # loại NaN
            valid_raw = [v for v in raw_y if v == v]
            self._autoscale_y(valid_raw + valid_filtered)
            if valid_filtered:
                current_val = valid_filtered[-1]
                min_val = min(valid_filtered)
                max_val = max(valid_filtered)
                avg_val = sum(valid_filtered) / len(valid_filtered)
                info_str = (
                    f"CHỈ SỐ THỜI GIAN THỰC (Kalman):\n"
                    f" ► Hiện tại  : {current_val:7.1f} cm\n"
                    f" ► Trung bình: {avg_val:7.1f} cm\n"
                    f" ► Thấp nhất : {min_val:7.1f} cm\n"
                    f" ► Cao nhất  : {max_val:7.1f} cm\n"
                    f" ► Max ghi nhận: {self.max_seen:7.1f} cm\n"
                    f" ► Tổng mẫu  : {self.total_samples} (Lỗi: {self.failed_samples})"
                )
                self.info_text.set_text(info_str)
        return self.line_raw, self.line_filtered, self.info_text
    def run(self):
        """Khởi chạy animation đồ thị thời gian thực.
        blit=False vì backend TkAgg trên máy này không hỗ trợ đầy đủ blit
        (AttributeError: 'FigureCanvasBase' object has no attribute 'restore_region').
        Bù lại bằng trục X cố định + chỉ scale Y khi cần để vẫn mượt.
        """
        self.ani = animation.FuncAnimation(
            self.fig, self.update_plot,
            interval=20, blit=False, cache_frame_data=False
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
