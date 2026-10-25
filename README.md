# DESK-8266

## Overview
DESK-8266 is an embedded smart desktop instrument and environmental dashboard powered by an ESP8266 microcontroller and a GC9A01 240x240 circular IPS LCD. It features an analog clock with sub-second macOS Dock-style magnification waves, 360-degree real-time gyroscope orientation tracking, and an ambient temperature/humidity dashboard switchable via capacitive touch.

---

## Features
- **Mode 1: Internet-Synced Analog Clock (Default)**
  - Bold Rounded Pill Hands: Hour (40px, 5px wide convex pill hand with rounded tip), Minute (58px, 3px solid baton/pill hand with rounded tip in `#b7c3bf` Sage Silver), Second (70px, 3px tapered body in `#3bbb8c` Mint Green with 12px counterweight tail) anchored by a 4px center cap.
  - 3D Parallax Crystal Glass Layering: Clock hands and center cap float forward towards the viewer (+1.8px along in-plane tilt normal) relative to the hour digits and recessed aperture, creating optical stereoscopic depth inside the watch case.
  - Dial Perimeter Hour Dots: Discrete circular markers at all 12 hour positions ($R = 108$ px); 12, 3, 6, and 9 o'clock highlighted as small circles in `#6BAF84` Sage Green, with intermediate hours (1, 2, 4, 5, 7, 8, 10, 11) marked in White.
  - Dynamic macOS Dock Magnification Wave: Active hour digits emerge dynamically as the second/minute hand approaches, radially gliding ($R = 84 \to 89$ px), smoothly expanding up to a refined 1.50x scale, and blooming in brilliant white / mint green via Hermite cubic color blending at 60 FPS.
  - Zero Inactive Text Clutter: Inactive hour numbers are completely hidden (0% opacity) on the deep black dial, ensuring a pristine minimalist face free from distracting gray text.
  - Pure Minimalist Dress Dial: Pure dial face free from distracting date apertures for optimal hand sweep aesthetics.
  - Zero hand-to-digit collisions: hands operate within $R \le 70$ px while numbers occupy $R \ge 81$ px.
  - Pure Solid Black theme background (`#000000` / `0x0000`) matching boot/connecting screen for maximum OLED-like contrast.
- **Mode 2: Environment Dashboard & Instrument Cluster**
  - Displays real-time temperature (0.1 °C resolution) and humidity (% RH) from a DHT22 sensor against the matching Pure Solid Black background.
  - Live Localized Weather Icon: Positioned exclusively at 12 o'clock ($R = 48$ px) above temperature readout for Sendangadi, Mlati, Sleman (Sun, Moon, Cloud, Rain, Thunder) fetched via Open-Meteo REST API.
  - Live Localized Weather Icon: Positioned exclusively at 12 o'clock ( = 54$ px) above temperature readout for Sendangadi, Mlati, Sleman (Sun, Moon, Cloud, Rain, Thunder) enlarged to 24x24 px for high desk visibility.
  - Dynamic Scale In/Out: Weather icon (1.5x), temperature text (2.8x bold hero), and humidity text (1.5x) scale smoothly with continuous opacity blending during transitions, resting rock-solid and stable when active.
  - Dual circular arc gauges ($R = 96$ px):
    - **Left Arc**: Temperature thermometer gauge ($15^\circ\text{C}$ to $40^\circ\text{C}$).
    - **Right Arc**: Humidity hygrometer gauge ($10\%$ to $100\%$) rendered in 3px thick `#6BAF84` Botanical Sage Green (`0x6D70`).
    - Gauges dynamically fill from bottom to top, precalculated once per frame with 0 trigonometric operations inside the band rendering loop and a 0.05° sub-degree threshold for silky smooth 60 FPS tilt rotation.
- **Mode 3: Camera-Style Settings Rotary Dial**
  - Inspired by classic mirrorless/DSLR camera top-plate rotary dials (Leica, Sony, Nikon).
  - **Knurled Perimeter Hash Marks**: Fine radial tick marks spanning a $220^\circ$ upper arc ($R = 104 \to 114$ px) with a bold 12 o'clock index pointer.
  - **Dynamic Pill Capsule Badge**: Active setting highlighted at the 12 o'clock detent inside a $46 \times 16$ px rounded capsule badge (`[ MAX ]`, `[ MID ]`, `[ ECO ]`, `[ PEEK ]`, `[ STAY ]`, `[ NITE ]`, `[ INFO ]`, `[ EXIT ]`).
  - **Rotary Badges Track**: Surrounding items glide along a circular path ($R = 74$ px) as 15 px circular button badges (`(H)`, `(M)`, `(E)`, `(P)`, `(S)`, `(N)`, `(i)`, `(X)`).
  - **Smooth Rotary Easing**: Each capacitive touch advance triggers an ease-out angular rotary glide into the detent.
  - **Live Settings Controls**:
    - `MAX` / `MID` / `ECO`: Adjusts PWM backlight brightness live (100%, 60%, 25%).
    - `PEEK` / `STAY`: Enables or disables the reciprocal auto-glance feature.
    - `NITE`: Toggles flip-to-stealth night stand dimming.
    - `INFO`: Live diagnostics showing IP address, WiFi RSSI (dBm), and temperature.
    - `EXIT`: Clean return to Clock mode (also triggered by 800ms long press or 15s idle timeout).
- **Continuous Reversible Mode Switch Transition**
  - Fully bidirectional and interruptible state machine: if the user interrupts a mode transition halfway (e.g. at 40% progress), the animation smoothly reverses towards the target without sudden stops, snaps, or lost visual states.
  - Hermite cubic aperture bloom & hand retraction over 280ms with zero flickering and rock-solid 60 FPS.
- **Flip-to-Stealth / Night Stand Mode**
- **Intelligent Reciprocal Auto-Glance (*Auto-Peek*)**
  - **Clock Mode Glance**: Periodically (every 60 seconds), the screen smoothly transitions to the Weather/Sensors dashboard for 16 seconds to provide a comfortable environmental status update, then seamlessly returns to the clock.
  - **Weather Mode Glance**: If the user manually toggled to the Weather page, periodically every 60 seconds it reciprocally peeks at the clock for 16 seconds and smoothly returns to the weather dashboard.
  - **Zero Fighting Touch Priority**: Tapping the capacitive touch sensor at any moment immediately cancels the peek and sets the active mode to the user's explicit preference.
  - **Night-Stand Suppressed**: Automatically paused when the device is inverted upside down for sleeping.
  - Inverting the device upside down (pitch $145^\circ - 215^\circ$) automatically dims the backlight to 20% and transitions the palette to warm monochrome amber on pitch black for comfortable night viewing.
- **Full 360° Gyroscope Horizon Tracking**
  - MPU6050 hardware 20Hz Digital Low Pass Filter (DLPF) eliminates mechanical vibration and hand tremor.
  - Continuous deadband subtraction ($1.2^\circ$ threshold) and adaptive velocity curve eliminate limit-cycle jitter.
  - Solid $4.5^\circ$ zero-snap with $6.5^\circ$ hysteresis locks upright firmly when resting on a desk.
  - 80 MHz high-speed SPI bus for sub-12ms frame delivery.
- **Capacitive Touch Mode Toggle**
  - TTP223 touch sensor on GPIO 3 (RX) provides clean 250ms debounced switching between Clock and Sensor modes.
  - Retains the authentic, designed color palette at all times without intrusive color tinting.
- **Zero-Flicker Band-Buffered Graphics Engine**
  - Screen divided into 6 horizontal bands ($240 \times 40$ pixels = 19.2 KB RAM).
  - RAM usage preserved at 61% with $>30$ KB free dynamic heap to guarantee WiFi stack stability without watchdog timeouts.

---

## Architecture
The firmware is structured into deep modules with well-defined boundaries and minimal public surfaces:

```
+-----------------------------------------------------------------+
|                          Desk-8266.ino                          |
|         (Application Orchestrator, Setup & Main Loop)           |
+--------+----------------+------------------+--------------------+
         |                |                  |                    |
         v                v                  v                    v
+----------------+ +--------------+ +-----------------+ +-----------------+
|  MotionSensor  | |  Environment | |   NetworkTime   | | DisplayRenderer |
|  (MPU6050 I2C, | |    Sensor    | |   (WiFi STA,    | | (GC9A01 6-band  |
|  Filter & Snap)| | (DHT22 Read) | |   NTP Client)   | |  Graphics Eng)  |
+----------------+ +--------------+ +-----------------+ +--------+--------+
                                                                 |
                                                                 v
                                                        +-----------------+
                                                        |    font5x7.h    |
                                                        |  (Bitmap Font)  |
                                                        +-----------------+
```

- **MotionSensor**: Encapsulates all I2C register configuration, sensor waking, axis mapping, shock suppression, and deadband filtering. Exposes only filtered pitch angle.
- **EnvironmentSensor**: Manages DHT22 communication and throttles polling to 1000ms intervals with NaN validation.
- **NetworkTime**: Handles non-blocking Wi-Fi association, connection timeouts, reconnection retries, and NTP network synchronization. Exposes discrete clock time and sub-second millisecond counters.
- **DisplayRenderer**: Owns the GC9A01 driver and 19.2 KB band buffer. Implements Bresenham line rendering, fixed-point rotated text rendering, and hand/dial geometry.
- **Config.h**: Centralized compile-time configuration isolating hardware pins, Wi-Fi credentials, tuning constants, and color definitions from application logic.

---

## Project Structure
```
Desk-8266/
├── AI_RULES.md             # Code architecture and engineering standards
├── Config.h                # Hardware pinouts, geometry, colors, and tuning constants
├── secrets.h               # Private Wi-Fi credentials (ignored by git via .gitignore)
├── secrets.example.h       # Template Wi-Fi credentials for version control
├── Desk-8266.ino           # Main entry point and lifecycle coordinator
├── DisplayRenderer.h       # Graphics rendering engine interface
├── DisplayRenderer.cpp     # 6-band buffer rendering, clock hands, and dock animation
├── EnvironmentSensor.h     # DHT22 sensor interface
├── EnvironmentSensor.cpp   # Sensor polling and cached readings
├── MotionSensor.h          # MPU6050 gyroscope/accelerometer interface
├── MotionSensor.cpp        # I2C driver, coordinate transformations, and zero-snap
├── NetworkTime.h           # Wi-Fi station and SNTP time manager interface
├── NetworkTime.cpp         # Connection management and sub-second time tracking
├── WeatherService.h        # Open-Meteo REST API weather client interface
├── WeatherService.cpp      # Non-blocking weather poller and WMO code mapper
├── weather_icons.h         # PROGMEM 16x16 weather bitmap icons
├── font5x7.h               # PROGMEM ASCII 5x7 bitmap font
└── README.md               # Technical architecture and developer guide
```

---

## Requirements
### Hardware
- **MCU**: ESP8266 (NodeMCU v2/v3, Wemos D1 Mini, or generic ESP-12E/F).
- **Display**: GC9A01 240x240 SPI Round IPS LCD.
- **IMU**: MPU6050 6-axis accelerometer/gyroscope module.
- **Sensor**: DHT22 (AM2302) temperature and humidity sensor.
- **Input**: TTP223 capacitive touch sensor module.

### Software & Libraries
- **Arduino IDE** or **Arduino CLI** (`arduino-cli` >= 0.35.0).
- **ESP8266 Core for Arduino** (v3.1.2 or later).
- **Adafruit GC9A01A** library.
- **Adafruit GFX Library**.
- **DHT sensor library** (by Adafruit).

---

## Installation & Pin Configuration
Connect the hardware peripherals according to the pinout defined in `Config.h`:

| Peripheral | Pin Label | ESP8266 GPIO | Description |
|---|---|---|---|
| GC9A01 CS | CS | GPIO 15 (D8) | SPI Chip Select |
| GC9A01 DC | DC | GPIO 2 (D4) | Data / Command Select |
| GC9A01 RST | RST | GPIO 16 (D0) | Hardware Reset |
| GC9A01 BL | BL | GPIO 12 (D6) | Backlight Enable (Active High) |
| TTP223 Touch | SIG | GPIO 3 (RX) | Digital Input |
| DHT22 Data | OUT | GPIO 1 (TX) | Data Line (Serial TX disabled) |
| MPU6050 SDA | SDA | GPIO 4 (D2) | I2C Data (Wire) |
| MPU6050 SCL | SCL | GPIO 5 (D1) | I2C Clock (Wire) |

> **Note**: GPIO 1 (TX) is dedicated to the DHT22 data line. Hardware UART Serial transmission is disabled to prevent protocol conflicts on the pin.

---

## Configuration
All application settings are declared in `Config.h`, while private credentials reside in `secrets.h`:
- **Wi-Fi Credentials (`secrets.h`)**:
  ```cpp
  constexpr const char* SECRET_WIFI_SSID = "YourSSID";
  constexpr const char* SECRET_WIFI_PASS = "YourPassword";
  ```
  *(This file is excluded from git commits via `.gitignore` to prevent credential leaks).*
- **Timezone**:
  ```cpp
  constexpr long TIMEZONE_OFFSET_SEC = 7 * 3600; // Default: WIB (UTC+7)
  ```
- **Gyro Snap Tolerance**:
  ```cpp
  constexpr float IDLE_ZERO_SNAP = 8.0f; // Snaps to 0.0° when resting within ±8.0°
  ```

---

## Usage
1. Power the device via micro-USB (5V / 500mA).
2. The initial boot screen displays a flicker-free connection progress bar and Wi-Fi status.
3. Upon connection, the acquired local IP is displayed, and NTP time is fetched.
4. The system transitions to **Mode 1 (Analog Clock)**:
   - Second hand ticks once per second.
   - Digits smoothly magnify as the second hand approaches.
   - Tilting the device rotates the entire dial dynamically.
5. **Tap the capacitive touch sensor** to toggle between **Mode 1 (Clock)** and **Mode 2 (Sensor Dashboard)**.

---

## Development & Build
Compile using `arduino-cli`:
```powershell
arduino-cli compile --fqbn esp8266:esp8266:generic "e:\Project\Arduino\Project\Desk-8266"
```

Upload to the board:
```powershell
arduino-cli upload -p COMx --fqbn esp8266:esp8266:generic "e:\Project\Arduino\Project\Desk-8266"
```

---

## Troubleshooting
- **Device bootloops or resets to "Connecting"**:
  Ensure the full-screen canvas buffer is not enlarged beyond 6 bands ($240 \times 40$). ESP8266 requires at least 25 KB of free heap for WPA2/lwIP background buffers.
- **Display shows tilted angle while resting on desk**:
  Ensure `IDLE_ZERO_SNAP` is set to at least $8.0^\circ$ in `Config.h` to absorb breadboard or desk surface pitch variations.
- **Sensor readings show `--.- ^C`**:
  Verify the DHT22 data line is connected to GPIO 1 (TX) and has a 4.7kΩ pull-up resistor to 3.3V if the module does not include one.

---

## Architecture Decisions
- **ADR 01: 6-Band Horizontal Rendering Buffer**: A full $240 \times 240$ 16-bit framebuffer requires 115.2 KB of RAM, exceeding the ESP8266's entire 80 KB RAM capacity. Allocating 4 bands (28.8 KB) left insufficient heap for the WiFi stack, triggering Soft WDT resets. A 6-band buffer ($240 \times 40$ = 19.2 KB) uses only 61% static RAM, leaving $>30$ KB heap for lwIP and guaranteeing stability.
- **ADR 02: Separation of Discrete Ticking and Continuous Magnification**: Traditional analog quartz watch aesthetics rely on crisp 1-second ticks (`sValHand`), while the macOS Dock effect requires sub-second floating-point progression (`sValAnim`) at ~35 FPS to glide smoothly across adjacent digits. Both timelines are derived simultaneously from system time and `millis()`.
- **ADR 03: Symmetric Coordinate Rounding**: Asymmetric truncation `(int)(val + 0.5f)` produced opposite integer displacements when crossing zero, causing horizontal hands at 3 o'clock to slant across band boundaries. Standard `roundf()` enforces strict mathematical symmetry.
