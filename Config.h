#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Hardware Pin Assignments
constexpr uint8_t PIN_TFT_CS         = 15; // D8: SPI Chip Select
constexpr uint8_t PIN_TFT_DC         = 2;  // D4: Data / Command
constexpr uint8_t PIN_TFT_RST        = 16; // D0: Hardware Reset
constexpr uint8_t PIN_TFT_BL         = 12; // D6: Backlight Control
constexpr uint8_t PIN_TOUCH_TTP223   = 3;  // RX (GPIO 3): Digital Input from Touch Sensor
constexpr uint8_t PIN_DHT22_DATA     = 1;  // TX (GPIO 1): DHT22 Bi-directional Data
constexpr uint8_t PIN_I2C_SDA        = 4;  // IO4 (GPIO 4): MPU6050 I2C Data
constexpr uint8_t PIN_I2C_SCL        = 5;  // IO5 (GPIO 5): MPU6050 I2C Clock

// Network and NTP Time Configuration
#if __has_include("secrets.h")
  #include "secrets.h"
  constexpr const char* WIFI_SSID      = SECRET_WIFI_SSID;
  constexpr const char* WIFI_PASS      = SECRET_WIFI_PASS;
#elif __has_include("secrets.example.h")
  #include "secrets.example.h"
  constexpr const char* WIFI_SSID      = SECRET_WIFI_SSID;
  constexpr const char* WIFI_PASS      = SECRET_WIFI_PASS;
#else
  constexpr const char* WIFI_SSID      = "YOUR_WIFI_SSID";
  constexpr const char* WIFI_PASS      = "YOUR_WIFI_PASSWORD";
#endif

constexpr uint32_t WIFI_TIMEOUT_MS   = 20000;
constexpr long TIMEZONE_OFFSET_SEC   = 7 * 3600; // WIB (UTC+7)
constexpr int DAYLIGHT_OFFSET_SEC    = 0;
constexpr const char* NTP_HOST_1     = "pool.ntp.org";
constexpr const char* NTP_HOST_2     = "time.google.com";
constexpr const char* NTP_HOST_3     = "id.pool.ntp.org";

// Display and Geometry Constraints (GC9A01 Round 240x240 Display)
constexpr int SCREEN_WIDTH           = 240;
constexpr int SCREEN_HEIGHT          = 240;
constexpr int SCREEN_CENTER_X        = 120;
constexpr int SCREEN_CENTER_Y        = 120;
constexpr int BAND_HEIGHT            = 40; // 6 horizontal bands of 240x40 pixels
constexpr int NUM_BANDS              = 6;

constexpr int BEZEL_OUTER_RADIUS     = 117;
constexpr int BEZEL_INNER_RADIUS     = 116;

constexpr int GYRO_TICK_OUTER_RADIUS = 114;
constexpr int GYRO_TICK_MAJOR_INNER  = 104; // Applied to 12, 3, 6, 9 markers
constexpr int GYRO_TICK_MINOR_INNER  = 108; // Applied to intermediate markers

constexpr float HOUR_NUMBER_BASE_R   = 84.0f; // Rest radius for hour digits
constexpr float HOUR_NUMBER_LIFT_R   = 7.0f;  // Maximum radial lift during magnification
constexpr float HOUR_HAND_LENGTH     = 40.0f; // Leaves clearance from hour digits
constexpr float MINUTE_HAND_LENGTH   = 58.0f; // Leaves clearance from hour digits
constexpr float SECOND_HAND_LENGTH   = 70.0f; // Reaches radius 70, leaving 11px margin before digits
constexpr float SECOND_HAND_TAIL_LEN = 12.0f; // Counterweight tail length
constexpr int CENTER_PIN_RADIUS      = 3;

// MPU6050 Motion and Orientation Tuning
constexpr uint8_t MPU_ADDR_PRIMARY   = 0x68;
constexpr uint8_t MPU_ADDR_SECONDARY = 0x69;
constexpr float ROTATION_OFFSET_DEG  = -180.0f; // Calibration offset for upright orientation
constexpr bool INVERT_Y              = true;
constexpr bool INVERT_X              = false;
constexpr bool SWAP_XY               = false;

constexpr float IDLE_MOTION_DEADBAND = 3.2f; // Minimum degrees change before updating pitch
constexpr float IDLE_ZERO_SNAP       = 8.0f; // Upright resting tolerance snapped to 0.0 deg
constexpr float MAX_SPEED_DEG_FRAME  = 12.0f; // Max rotation step per frame to prevent jumping

// Color Palette Definitions (RGB565 format)
constexpr uint16_t COLOR_BLACK       = 0x0000;
constexpr uint16_t COLOR_WHITE       = 0xFFFF;
constexpr uint16_t COLOR_GREEN       = 0x07E0;
constexpr uint16_t COLOR_AMBER       = 0xFDA0;
constexpr uint16_t COLOR_DARKGRAY    = 0x18E3;

// Operating Modes
enum class AppMode : uint8_t {
  Clock   = 0, // Mode 1: Analog Clock with Dock Magnification
  Sensors = 1  // Mode 2: Temperature & Humidity Sensor Dashboard
};

#endif // CONFIG_H
