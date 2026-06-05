import asyncio
from bleak import BleakClient, BleakScanner
import paho.mqtt.client as mqtt
import json
import re

# ==========================================
# 1. CAU HINH THINGSBOARD & MQTT
# ==========================================
THINGSBOARD_HOST = 'vinhiot.duckdns.org' 
THINGSBOARD_PORT = 1883 

ACCESS_TOKEN = 'eCSHbhaGQtAmjWYRbjGx'

# ==========================================
# 2. CAU HINH BLE CUA MACH EFR32
# ==========================================
DEVICE_NAME = "BreathSense-Vinh"
CHAR_UUID = "7E92A002-1F7C-4B89-A2D5-6B8F1D3E4C00" 

# Khoi tao MQTT Client
client = mqtt.Client()
client.username_pw_set(ACCESS_TOKEN)

try:
    print(f"Dang ket noi MQTT toi {THINGSBOARD_HOST}...")
    client.connect(THINGSBOARD_HOST, THINGSBOARD_PORT, 60)
    client.loop_start()
    print("Da ket noi MQTT toi ThingsBoard thanh cong!")
except Exception as e:
    print(f"Loi ket noi MQTT: {e}")

# ==========================================
# 3. HAM XU LY LOGIC KHI NHAN BLE (CHONG RAC BO NHO)
# ==========================================
def notification_handler(sender, data):
    # Giai ma an toan, bo qua byte loi va xoa ky tu null
    text_data = data.decode('utf-8', errors='ignore').replace('\x00', '').strip()

    # --- KICH BAN 1: BAT SU KIEN CHOT NGAY ---
    if text_data.startswith("DayEnd B:"):
        raw_value = text_data.split(":")[1]
        
        # Dung Regex de gap dung con so ra khoi dong rac bo nho
        match = re.search(r'\d+(\.\d+)?', raw_value)
        
        if match:
            baseline_val = float(match.group())
            print("\n" + "="*50)
            print(f"[BLE] >>> KET THUC NGAY <<<")
            print(f"      He thong da cap nhat Baseline moi: {baseline_val}")
            print("="*50 + "\n")
            
            # Day len ThingsBoard de ve bieu do Baseline
            payload = {"baseline_ewma": baseline_val}
            client.publish("v1/devices/me/telemetry", json.dumps(payload))
            print(f"[MQTT] Da day Baseline len server: {json.dumps(payload)}\n")
        else:
            print(f"[LOI] Nhan duoc DayEnd nhung khong tim thay so: {repr(raw_value)}")
            
        return # Thoat ham luon, khong xu ly tiep ben duoi

    # --- KICH BAN 2: XU LY TIENG HO VA CANH BAO ---
    print(f"[BLE] Nhan duoc: {text_data}")
    parts = text_data.split(" #")
    
    if len(parts) >= 2: # Dung >= de phong ngua rac phia sau chuoi
        event_type = parts[0]
        
        # Dung Regex de gap dung con so dem Event Count
        count_match = re.search(r'\d+', parts[1])
        if count_match:
            count = int(count_match.group())
            
            # Tao JSON payload thong minh hon cho ThingsBoard
            payload = {"event_count": count}
            
            if event_type == "Warning":
                payload["breath_event"] = "Cough" # Ban chat van la tieng ho
                payload["is_alert"] = True        # Bat co canh bao do
                print(" -> [CANH BAO DO] Da vuot nguong, gui yeu cau bao dong!")
            else:
                payload["breath_event"] = event_type
                payload["is_alert"] = False       # Trang thai binh thuong

            # Publish len telemetry cua ThingsBoard
            client.publish("v1/devices/me/telemetry", json.dumps(payload))
            print(f"[MQTT] Da day len server: {json.dumps(payload)}\n")

# ==========================================
# 4. HAM QUAN LY KET NOI BLE
# ==========================================
def handle_disconnect(_):
    print("\n[CANH BAO] Da mat ket noi voi mach EFR32!")

async def main():
    # Vong lap lon nhat: Giu cho script khong bao gio chet
    while True:
        print(f"\nDang quet tim thiet bi BLE '{DEVICE_NAME}'...")
        devices = await BleakScanner.discover()
        
        target_device = None
        for d in devices:
            if d.name == DEVICE_NAME:
                target_device = d
                break

        # Neu khong thay mach, doi 3 giay roi quet lai tu dau
        if not target_device:
            print("Khong tim thay mach. Dang thu quet lai...")
            await asyncio.sleep(3)
            continue

        print(f"Da tim thay mach ({target_device.address}). Dang ket noi...")
        
        try:
            # Gan ham handle_disconnect de theo doi trang thai
            async with BleakClient(target_device.address, disconnected_callback=handle_disconnect) as ble_client:
                print("Da ket noi BLE thanh cong!")
                
                await ble_client.start_notify(CHAR_UUID, notification_handler)
                print("Da Subscribe! Hay thao tac tren mach EFR32...")

                # Vong lap duy tri: Chi chay khi ket noi BLE con song
                while ble_client.is_connected:
                    await asyncio.sleep(1)
                    
        except Exception as e:
            # Bat moi loi (vi du mach dot ngot sap nguon gay loi timeout)
            print(f"[LOI] Xay ra su co ket noi: {e}")
        
        # Thoat khoi vong lap duy tri -> Doi 2 giay -> Quay lai vong lap quet lon nhat
        print("Se tu dong quet lai sau 2 giay...\n")
        await asyncio.sleep(2)

if __name__ == "__main__":
    asyncio.run(main())