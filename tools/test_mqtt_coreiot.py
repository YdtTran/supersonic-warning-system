import argparse
import json
import time
import sys
import random
import os

# Đảm bảo UTF-8 cho Windows console
if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8')
    sys.stderr.reconfigure(encoding='utf-8')

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("[ERROR] Thư viện paho-mqtt chưa được cài đặt.")
    print("Vui lòng kích hoạt môi trường conda 'mqtt-coreiot' hoặc sử dụng Python tại:")
    print(r"D:\miniconda\envs\mqtt-coreiot\python.exe")
    sys.exit(1)


DEFAULT_BROKER = "app.coreiot.io"
DEFAULT_PORT = 1883
DEFAULT_TOPIC = "v1/devices/me/telemetry"


def load_config_keys():
    """Tự động đọc file config/keys.json ( local config được gitignore )"""
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    config_path = os.path.join(base_dir, "config", "keys.json")
    if os.path.exists(config_path):
        try:
            with open(config_path, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception as e:
            print(f"[WARNING] Không thể đọc file config/keys.json: {e}")
    return {}


def parse_args():
    cfg = load_config_keys()
    default_broker = cfg.get("COREIOT_BROKER", DEFAULT_BROKER)
    default_port = cfg.get("COREIOT_PORT", DEFAULT_PORT)
    default_token = os.environ.get("COREIOT_TOKEN", "") or cfg.get("COREIOT_DEVICE_TOKEN", "")
    default_topic = cfg.get("COREIOT_TELEMETRY_TOPIC", DEFAULT_TOPIC)

    parser = argparse.ArgumentParser(description="CoreIoT (ThingsBoard) MQTT Test Client")
    parser.add_argument("--broker", default=default_broker, help="MQTT broker host (default: app.coreiot.io)")
    parser.add_argument("--port", type=int, default=default_port, help="MQTT broker port (default: 1883)")
    parser.add_argument("--token", default=default_token, help="Device access token / key (or set COREIOT_TOKEN env var / config/keys.json)")
    parser.add_argument("--topic", default=default_topic, help="Telemetry topic (default: v1/devices/me/telemetry)")
    parser.add_argument("--distance", type=float, default=None, help="Fixed distance value in cm (default: random 10-200cm)")
    parser.add_argument("--loop", action="store_true", help="Send data continuously in a loop")
    parser.add_argument("--interval", type=float, default=2.0, help="Publish interval in seconds for loop mode (default: 2.0s)")
    return parser.parse_args()


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[MQTT] Kết nối thành công đến {userdata['broker']}:{userdata['port']}!")
    else:
        print(f"[MQTT ERROR] Kết nối thất bại với mã lỗi rc={rc}")


def on_publish(client, userdata, mid):
    print(f"[MQTT] Đã phát gói tin thành công (mid={mid}).")


def main():
    args = parse_args()
    token = args.token

    if not token or token.startswith("<"):
        print("[ERROR] Chưa cung cấp Device Access Token hợp lệ!")
        print("Vui lòng tạo/cập nhật file 'config/keys.json' từ template 'config/keys.template.json':")
        print("   copy config/keys.template.json config/keys.json")
        print("hoặc truyền token qua tham số '--token <ACCESS_TOKEN>'.")
        sys.exit(1)

    userdata = {"broker": args.broker, "port": args.port}

    # Đánh dấu che token khi hiển thị log
    masked_token = token[:3] + "..." + token[-3:] if len(token) > 6 else "***"

    print("=" * 60)
    print("      CoreIoT (ThingsBoard) MQTT Test Publisher")
    print("=" * 60)
    print(f"Broker Host:  {args.broker}:{args.port}")
    print(f"Access Token: {masked_token}")
    print(f"Topic:        {args.topic}")
    print(f"Loop Mode:    {args.loop} (Interval: {args.interval}s)")
    print("=" * 60)

    client = mqtt.Client(userdata=userdata)
    client.username_pw_set(token)
    client.on_connect = on_connect
    client.on_publish = on_publish

    try:
        client.connect(args.broker, args.port, keepalive=60)
    except Exception as e:
        print(f"[ERROR] Không thể kết nối tới MQTT broker: {e}")
        sys.exit(1)

    client.loop_start()
    time.sleep(1)  # Đợi callback connect

    try:
        if not args.loop:
            distance = args.distance if args.distance is not None else round(random.uniform(10.0, 150.0), 2)
            payload = {
                "distance_cm": distance,
                "warning_status": "NORMAL" if distance > 30 else "WARNING",
                "rssi": -65,
                "timestamp": int(time.time() * 1000)
            }
            payload_str = json.dumps(payload)
            print(f"[SEND] Topic: {args.topic} | Payload: {payload_str}")
            info = client.publish(args.topic, payload_str, qos=1)
            info.wait_for_publish(timeout=5)
            if info.is_published():
                print("[SUCCESS] Đã gửi thành công 1 gói dữ liệu đến CoreIoT!")
            else:
                print("[WARNING] Chưa xác nhận phản hồi từ broker (Timeout).")
        else:
            print("[INFO] Đang gửi dữ liệu liên tục... Nhấn Ctrl+C để dừng.")
            counter = 1
            while True:
                distance = args.distance if args.distance is not None else round(random.uniform(10.0, 200.0), 2)
                status = "DANGER" if distance < 20 else ("WARNING" if distance < 50 else "NORMAL")
                payload = {
                    "distance_cm": distance,
                    "warning_status": status,
                    "seq": counter,
                    "timestamp": int(time.time() * 1000)
                }
                payload_str = json.dumps(payload)
                print(f"[SEND #{counter}] Payload: {payload_str}")
                client.publish(args.topic, payload_str, qos=1)
                counter += 1
                time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\n[INFO] Đã dừng bởi người dùng.")
    finally:
        client.loop_stop()
        client.disconnect()
        print("[MQTT] Đã ngắt kết nối thành công.")


if __name__ == "__main__":
    main()
