# 🤖 Mobile Robot with Ultra-Wideband Localization

An autonomous mobile robot platform using **Ultra-Wideband (UWB)** indoor positioning for real-time localization and waypoint navigation, controlled via a web-based dashboard.

---

## 📋 Overview

This project implements a **differential-drive mobile robot** capable of navigating autonomously within an indoor environment. The system uses **DW1000 UWB modules** for centimeter-level positioning and a **BNO055 IMU** for heading estimation. A real-time **React web dashboard** provides visualization, manual control, and waypoint-based path planning.

### System Architecture

```
┌──────────────┐     WebSocket      ┌──────────────┐      UART       ┌──────────────┐
│  Robot Map   │◄──────────────────►│    ESP32      │◄──────────────►│    STM32      │
│  Web (React) │   (commands/data)  │  Controller   │  (velocity/    │   F103C8T6    │
└──────────────┘                    └──────┬───────┘   heading)      └──────┬───────┘
                                           │                                │
                                      UART │                           I2C  │  STEP/DIR
                                           │                                │
                                    ┌──────▼───────┐                 ┌──────▼───────┐
                                    │  UWB Tag      │                │  TMC2209 x2   │
                                    │  (DW1000)     │                │  + BNO055 IMU │
                                    └──────┬───────┘                └──────────────┘
                                           │
                                    TWR Ranging
                                           │
                              ┌────────────┼────────────┐
                              │            │            │
                        ┌─────▼──┐   ┌─────▼──┐   ┌────▼───┐
                        │Anchor 1│   │Anchor 2│   │Anchor 3│ ... (up to 4)
                        │(DW1000)│   │(DW1000)│   │(DW1000)│
                        └────────┘   └────────┘   └────────┘
```

---

## ✨ Features

- **UWB Indoor Positioning** — Trilateration-based 2D localization using DW1000 anchors with EMA filtering and spike rejection
- **Autonomous Navigation** — Multi-waypoint path following with dual PID controllers (linear velocity + angular velocity)
- **Web-Based Dashboard** — Real-time robot map with drag-to-pan, scroll-to-zoom, route drawing, and live telemetry (React + Vite + TailwindCSS + DaisyUI)
- **Differential Drive Control** — ROS-style `(v, ω)` velocity commands with acceleration ramping for smooth motion
- **BNO055 IMU Integration** — Euler angle heading with I2C bus auto-recovery on sensor lockup
- **Emergency Stop** — Hardware E-Stop button (GPIO 25) with debounce, blocks all motor commands when active
- **TMC2209 Stepper Drivers** — Silent StealthChop mode via single-wire UART configuration
- **Safety Systems** — WebSocket heartbeat watchdog, signal-loss auto-stop, and checksum-verified UART communication
- **Live PID Tuning** — Adjust PID gains in real-time from the web dashboard without reflashing
- **Heading Calibration** — One-click zero-heading calibration via the web interface
- **OTA Updates** — Over-the-air firmware updates for the ESP32 Controller

---

## 📁 Project Structure

```
Mobile-Robot-with-Ultra-Wideband-Localization/
│
├── ESP32/
│   ├── ESP_Controller/          # Main robot controller (ESP32)
│   │   ├── ESP_Controller.ino   # WiFi, WebSocket, navigation, E-Stop logic
│   │   └── Settings.h           # Pin definitions, network config
│   │
│   ├── UWB_Tag_System/          # UWB Tag mounted on robot (ESP32 + DW1000)
│   │   └── UWB_Tag_System.ino   # TWR ranging, trilateration, position output
│   │
│   ├── AnchorA1/                # UWB Anchor 1 firmware
│   ├── AnchorA2/                # UWB Anchor 2 firmware
│   ├── AnchorA3/                # UWB Anchor 3 firmware
│   ├── AnchorA4/                # UWB Anchor 4 firmware
│   └── AdelayTag_Anchor/        # Antenna delay calibration tool
│
├── STM32/
│   └── f103_2209/               # STM32F103 motor controller (STM32CubeIDE)
│       └── Core/Src/main.c      # Differential drive, TMC2209, BNO055, UART protocol
│
├── Robot Map Web/               # Web-based control dashboard
│   ├── src/
│   │   ├── App.jsx              # Main application logic
│   │   ├── MapCanvas.jsx        # Interactive 2D map with coordinate transforms
│   │   ├── Joystick.jsx         # Virtual joystick for manual control
│   │   └── SettingsPanel.jsx    # PID tuning and configuration panel
│   ├── backend/                 # Backend utilities
│   ├── package.json             # Dependencies (React, Vite, TailwindCSS, DaisyUI)
│   └── vite.config.js           # Vite configuration
│
├── LICENSE                      # MIT License
└── README.md                    # This file
```

---

## 🔧 Hardware Components

| Component | Role | Interface |
|---|---|---|
| **ESP32** (Controller) | Central hub — WiFi, WebSocket, navigation logic | UART to STM32 & UWB Tag |
| **ESP32 + DW1000** (UWB Tag) | On-robot UWB tag for TWR ranging | SPI (DW1000), UART (to Controller) |
| **ESP32 + DW1000** (Anchors ×4) | Fixed reference points for trilateration | SPI (DW1000) |
| **STM32F103C8T6** | Low-level motor control & IMU reading | UART, I2C, GPIO |
| **TMC2209** (×2) | Silent stepper motor drivers | Single-wire UART |
| **BNO055** | 9-DOF IMU for heading (Euler angles) | I2C (400 kHz) |
| **Emergency Stop Button** | Hardware safety cutoff | GPIO 25 (INPUT_PULLUP) |

---

## 📡 Communication Protocol

### ESP32 ↔ STM32 (UART, 115200 baud)

All messages include an XOR checksum: `PAYLOAD*HH\n` where `HH` is the hex checksum.

| Direction | Command | Format | Description |
|---|---|---|---|
| ESP → STM | Velocity | `V,<v>,<w>*HH\n` | Set linear (mm/s) and angular velocity |
| ESP → STM | Stop | `S*HH\n` | Emergency stop |
| ESP → STM | Heartbeat | `H*HH\n` | Keep-alive signal (100ms interval) |
| STM → ESP | Heading | `A=<angle>*HH\n` | BNO055 Euler heading (degrees) |
| STM → ESP | Acknowledge | `OK*HH\n` | Command confirmation |

### Web ↔ ESP32 (WebSocket, port 81)

| Direction | Command | Description |
|---|---|---|
| Web → ESP | `FWD`, `BWD`, `LEFT`, `RIGHT`, `ROTL`, `ROTR` | Manual drive commands |
| Web → ESP | `STOP` | Stop all motors |
| Web → ESP | `GOTO:x1:y1:x2:y2:...` | Multi-waypoint navigation route |
| Web → ESP | `PID:kp_v:ki_v:kd_v:kp_w:ki_w:kd_w` | Live PID parameter update |
| Web → ESP | `SET_ZERO` | Calibrate heading to zero |
| Web → ESP | `PING` | Heartbeat |
| ESP → Web | `{"type":"uwb", "x":..., "y":...}` | Robot position |
| ESP → Web | `{"type":"data", "angle":...}` | Robot heading |
| ESP → Web | `{"type":"emer", "state":0/1}` | Emergency stop state |
| ESP → Web | `{"type":"nav_debug", ...}` | Navigation debug telemetry |

---

## 🚀 Getting Started

### Prerequisites

- [Arduino IDE](https://www.arduino.cc/ide) or [PlatformIO](https://platformio.org/) — for ESP32 firmware
- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) — for STM32 firmware
- [Node.js](https://nodejs.org/) (v18+) — for the web dashboard

### 1. Flash UWB Anchors

Flash each anchor (A1–A4) with its respective firmware from `ESP32/AnchorA1/` through `ESP32/AnchorA4/`. Place them at known positions in the environment.

### 2. Flash UWB Tag

Flash the UWB Tag firmware from `ESP32/UWB_Tag_System/` to the ESP32+DW1000 module mounted on the robot.

### 3. Flash ESP32 Controller

Update `ESP32/ESP_Controller/Settings.h` with your WiFi credentials, then flash `ESP_Controller.ino`.

```cpp
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASS "YOUR_PASSWORD"
```

### 4. Flash STM32

Open the STM32CubeIDE project at `STM32/f103_2209/` and flash it to the STM32F103C8T6.

### 5. Run the Web Dashboard

```bash
cd "Robot Map Web"
npm install
npm run dev
```

Open the displayed URL in your browser. Configure the WebSocket connection to point to the ESP32's IP address.

---

## 🧭 Navigation

The robot uses a **dual-PID controller** for autonomous navigation:

- **Linear PID** — Controls forward speed based on distance to target
- **Angular PID** — Controls turning rate based on heading error

The system supports **multi-waypoint routes** drawn directly on the web map. The robot navigates through each waypoint sequentially, using a larger acceptance radius (30cm) for intermediate points and a tighter threshold (5cm) for the final destination.

---

## 🛡️ Safety Features

1. **Hardware Emergency Stop** — Physical button cuts motor power and blocks all commands
2. **WebSocket Heartbeat** — Auto-stops if web client disconnects (300ms timeout)
3. **UART Watchdog** — Auto-stops if ESP32 signal is lost (200ms timeout)
4. **Checksum Verification** — XOR checksum on all UART messages to reject corrupted data
5. **I2C Bus Recovery** — Automatic recovery from BNO055 sensor lockup

---

## 📄 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

**Copyright (c) 2025 Auttaphan Namphai**
