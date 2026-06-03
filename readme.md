# BreathSense

BreathSense is a Bluetooth Low Energy (BLE) firmware for the Silicon Labs
**EFR32MG21** SoC. It acts as a BLE peripheral that reports **breath events**
(Inhale / Exhale / Cough) to a connected client over a custom GATT service.
Two on-board LEDs show the connection state, and a push-button is used to
trigger a breath event.

The project is built on the Silicon Labs *Bluetooth – SoC Empty* example and
runs on top of **FreeRTOS**.

> Status: experimental. The breath events are currently generated on a button
> press for demonstration/bring-up; the same code path is where a real
> respiratory sensor would feed data into the BLE notifications.

---

## Features

- BLE peripheral that advertises as **`BreathSense-Vinh`** and is connectable.
- Custom **BreathSense GATT service** with three characteristics:
  - **Breath Event** – Read / Notify. Sends a short text label of each event
    (e.g. `Inhale #12`).
  - **Config** – Read / Write. Lets the client configure mode, threshold and
    sample rate.
  - **Device Status** – Read / Notify. Reports device state, battery %, error
    code and a running event counter.
- FreeRTOS task that wakes on a button interrupt and pushes a BLE notification.
- Two status LEDs: one for *connected*, one for *disconnected*.
- Automatically restarts advertising after a disconnection.

---

## Hardware

| Item        | Value                                   |
|-------------|-----------------------------------------|
| MCU / SoC   | `EFR32MG21A010F512IM32` (Arm Cortex-M33, 512 KB flash) |
| Radio       | Bluetooth Low Energy                    |
| SDK         | Silicon Labs Simplicity SDK `2025.12.3` |
| RTOS        | FreeRTOS                                |

### Pin assignment

| Function                  | Instance    | GPIO          |
|---------------------------|-------------|---------------|
| Button (trigger event)    | `bt_01`     | **PD04**      |
| LED – connected           | `led_CN`    | **PA00**      |
| LED – disconnected        | `led_disCN` | **PA04**      |

> Pins are defined in `config/sl_simple_button_bt_01_config.h`,
> `config/sl_simple_led_led_CN_config.h` and
> `config/sl_simple_led_led_disCN_config.h`. Adjust them to match your board.

---

## BLE GATT service

**BreathSense Service** — UUID `7E92A001-1F7C-4B89-A2D5-6B8F1D3E4C00`

| Characteristic | UUID (128-bit)                          | Properties     | Payload |
|----------------|-----------------------------------------|----------------|---------|
| Breath Event   | `7E92A002-1F7C-4B89-A2D5-6B8F1D3E4C00`  | Read, Notify   | UTF-8 string, e.g. `"Inhale #12"` |
| Config         | `7E92A003-1F7C-4B89-A2D5-6B8F1D3E4C00`  | Read, Write    | `breath_config_t` (4 bytes) |
| Device Status  | `7E92A004-1F7C-4B89-A2D5-6B8F1D3E4C00`  | Read, Notify   | `device_status_t` (8 bytes) |

The standard **Generic Access** (`0x1800`) and **Device Information**
(`0x180A`) services are also present.

### Data formats

```c
// Breath event types (also encoded in the read snapshot)
typedef enum {
  BREATH_INHALE  = 0x01,
  BREATH_EXHALE  = 0x02,
  BREATH_COUGH   = 0x03,
  BREATH_UNKNOWN = 0xFF
} breath_event_type_t;

// Config characteristic (writable by the client)
typedef struct __attribute__((packed)) {
  uint8_t mode;         // operating mode
  uint8_t threshold;    // detection threshold
  uint8_t sample_rate;  // sampling rate
  uint8_t reserved;
} breath_config_t;      // 4 bytes

// Device Status characteristic (read + notify)
typedef struct __attribute__((packed)) {
  uint8_t  state;        // device state
  uint8_t  battery_pct;  // battery level, 0-100 %
  uint16_t error_code;   // last error code
  uint32_t event_count;  // total breath events
} device_status_t;       // 8 bytes
```

---

## How it works

1. **Boot** — the firmware sets the device name, creates an advertising set
   and starts connectable advertising (interval ≈ 100 ms). The
   *disconnected* LED turns on.
2. **Connection** — on connect, the *connected* LED turns on and the
   *disconnected* LED turns off.
3. **Subscription** — when the client enables notifications on the Breath
   Event / Device Status characteristics, the device starts pushing data.
4. **Button press (PD04)** — the button interrupt gives a FreeRTOS semaphore.
   A dedicated `breath_task` wakes up and, if a client is connected and
   subscribed, sends a notification: it cycles through Inhale → Exhale →
   Cough, increments the event counter and updates the status.
5. **Disconnection** — advertising restarts automatically and the
   *disconnected* LED turns on again.

### Source layout

| File                                          | Purpose |
|-----------------------------------------------|---------|
| `app.c`                                       | BLE event handling, GATT read/write/notify, LED + button logic |
| `app_freertos.c`                              | FreeRTOS task and semaphore for breath events |
| `app.h`                                       | Application interface |
| `main.c`                                      | Entry point / kernel start |
| `config/btconf/gatt_configuration.btconf`     | GATT database definition |
| `sl_gatt_service_device_information_override.c` | Device Information service overrides |

---

## Build & flash

This is a Simplicity Studio project. The easiest route:

1. Install **Simplicity Studio 5** with the **Simplicity SDK** (Bluetooth).
2. *File → Open Projects from File System…* and select this folder, or import
   `breathsense_mg21.slcp`.
3. Build (the hammer icon), then **Flash** to an EFR32MG21 board (the
   pre-built image is produced under `cmake_gcc/build/`).

To build from the command line with the bundled GCC toolchain:

```bash
cd cmake_gcc
cmake --preset project                  # configure
cmake --build --preset default_config   # build
```

> Build output in `cmake_gcc/build/` is intentionally excluded from git via
> `.gitignore`.

---

## Try it

1. Flash the firmware to the board.
2. Open a BLE scanner app (e.g. **Simplicity Connect / EFR Connect**, nRF
   Connect) and connect to **`BreathSense-Vinh`**.
3. Enable notifications on the **Breath Event** and **Device Status**
   characteristics.
4. Press the button (**PD04**) — you should receive a notification such as
   `Inhale #1`, then `Exhale #2`, `Cough #3`, and so on.
5. Optionally write 4 bytes to the **Config** characteristic to change the
   mode / threshold / sample rate.

---

## License

The Silicon Labs source files in this project are distributed under the
**Zlib** license (see the header in each source file). Application code added
on top follows the same terms unless stated otherwise.
