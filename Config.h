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

constexpr float HOUR_NUMBER_BASE_R     = 84.0f; // Rest radius for hour digits
constexpr float HOUR_NUMBER_LIFT_R     = 5.0f;  // Maximum radial lift during magnification
constexpr float HOUR_NUMBER_SCALE_BASE = 1.00f; // Rest scale for emerging hour digits
constexpr float HOUR_NUMBER_SCALE_PEAK = 2.05f; // Magnified peak scale for active hour digit (clearly legible)
constexpr float HOUR_HAND_LENGTH       = 40.0f; // Leaves clearance from hour digits
constexpr float MINUTE_HAND_LENGTH   = 58.0f; // Leaves clearance from hour digits
constexpr float SECOND_HAND_LENGTH   = 70.0f; // Reaches radius 70, leaving 11px margin before digits
constexpr float SECOND_HAND_TAIL_LEN = 12.0f; // Counterweight tail length
constexpr int CENTER_PIN_RADIUS      = 4;

// Orbital Seconds Comet Particle (Mode 1)
constexpr float ORBITAL_PARTICLE_RADIUS = 108.0f; // Outer perimeter satellite orbit (clearly visible inside bezel)
constexpr int   ORBITAL_TRAIL_STEPS     = 28;     // Number of trailing tail segments
constexpr float ORBITAL_TRAIL_ARC_STEP  = 0.0090f;// Angular distance per trail step (~14.5 deg total tail)

// Date Complication Constraints (Mode 1)
constexpr float DATE_WINDOW_RADIUS   = 48.0f; // Positioned along 6 o'clock axis
constexpr int DATE_BOX_HALF_WIDTH    = 18;
constexpr int DATE_BOX_HALF_HEIGHT   = 5;

// Weather Complication Constraints (Sendangadi, Mlati, Sleman, DIY)
constexpr float WEATHER_ICON_RADIUS        = 48.0f; // Positioned along 12 o'clock axis
constexpr float WEATHER_LATITUDE           = -7.734f;
constexpr float WEATHER_LONGITUDE          = 110.362f;
constexpr uint32_t WEATHER_UPDATE_INTERVAL = 900000; // Poll every 15 minutes

// Sensor Arc Gauge Constraints (Mode 2)
constexpr int SENSOR_GAUGE_RADIUS    = 96;
constexpr float TEMP_GAUGE_MIN       = 15.0f; // Minimum temperature on circular arc
constexpr float TEMP_GAUGE_MAX       = 40.0f; // Maximum temperature on circular arc
constexpr float HUM_GAUGE_MIN        = 10.0f; // Minimum humidity on circular arc
constexpr float HUM_GAUGE_MAX        = 100.0f;// Maximum humidity on circular arc

// MPU6050 Motion and Orientation Tuning
constexpr uint8_t MPU_ADDR_PRIMARY   = 0x68;
constexpr uint8_t MPU_ADDR_SECONDARY = 0x69;
constexpr float ROTATION_OFFSET_DEG  = -180.0f; // Calibration offset for upright orientation
constexpr bool INVERT_Y              = true;
constexpr bool INVERT_X              = false;
constexpr bool SWAP_XY               = false;

constexpr float IDLE_MOTION_DEADBAND = 1.2f; // Smooth tremor-suppressing deadband
constexpr float IDLE_ZERO_SNAP       = 4.5f; // Solid resting upright lock with hysteresis
constexpr float MAX_SPEED_DEG_FRAME  = 45.0f; // Instantaneous 1:1 rotation slew rate

// Color Palette Definitions (RGB565 format)
constexpr uint16_t COLOR_BLACK       = 0x0000;
constexpr uint16_t COLOR_WHITE       = 0xFFFF;
constexpr uint16_t COLOR_GREEN       = 0x07E0;
constexpr uint16_t COLOR_AMBER       = 0xFDA0;
constexpr uint16_t COLOR_DARKGRAY    = 0x18E3;

// Custom Theme Palette (User Specified Hex Colors)
constexpr uint16_t COLOR_CLOCK_BG      = 0x0882; // Deep Obsidian Spruce (#0a110e) - rich dark tone for IPS LCD
constexpr uint16_t COLOR_HAND_SECOND   = 0xFFFF; // Pure White Second Hand
constexpr uint16_t COLOR_HAND_HOUR_MIN = 0xB617; // #b7c3bf Sage Silver Hour & Minute Hands
constexpr uint16_t COLOR_SAGE_GREEN    = 0x6D70; // #6BAF84 Distinct Botanical Sage Green (clearly green on IPS LCD)
constexpr uint16_t COLOR_FADED_GRAY    = 0x9CD3; // #9c9c9c Soft faded gray (muted, non-glaring)

// Weather Icon Colors
constexpr uint16_t COLOR_WEATHER_SUN     = 0xFDA0; // Warm amber gold
constexpr uint16_t COLOR_WEATHER_MOON    = 0xDEFB; // Moonlit pearl silver
constexpr uint16_t COLOR_WEATHER_CLOUD   = 0xFFFF; // Crisp white
constexpr uint16_t COLOR_WEATHER_RAIN    = 0x3DD1; // Mint aqua rain
constexpr uint16_t COLOR_WEATHER_THUNDER = 0xFDA0; // Amber thunder bolt

// Night Stand Mode Tuning
constexpr float    NIGHT_STAND_PITCH_MIN  = 145.0f; // Inverted / upside down tilt threshold
constexpr float    NIGHT_STAND_PITCH_MAX  = 215.0f;
constexpr uint8_t  NIGHT_STAND_BRIGHTNESS = 40;     // Dim night-stand PWM (out of 255)
constexpr uint8_t  FULL_STAND_BRIGHTNESS  = 255;    // Normal day PWM (out of 255)
constexpr uint16_t COLOR_NIGHT_AMBER      = 0xDAA0; // Warm amber monochrome for night stand
constexpr uint16_t COLOR_NIGHT_BG         = 0x0000; // Pitch black background for night stand

// Transition Timing
constexpr unsigned long MODE_TRANSITION_MS = 280;   // 280ms buttery smooth morph between modes

// Auto-Glance / Auto-Peek Configuration (Occasional reciprocal peek between Clock & Weather)
constexpr unsigned long AUTO_PEEK_INTERVAL_MS = 60000; // 60 seconds between auto-glances
constexpr unsigned long AUTO_PEEK_DURATION_MS = 7000;  // 7 seconds glance duration

// Settings Rotary Dial Constraints (Mode 3 - Clustered camera dial as in reference image)
constexpr int   SETTINGS_NUM_ITEMS    = 7;      // BRIGHT, GLANCE, NIGHT, GYRO, THEME, INFO, EXIT
constexpr float SETTINGS_STEP_ANGLE   = 0.48f;  // Spacious 27.5-degree step for clear separation
constexpr int   SETTINGS_KNURL_TICKS  = 61;     // Radial perimeter tick marks matching reference image
constexpr float SETTINGS_TRACK_RADIUS = 75.0f;  // Radial distance for surrounding item badges
constexpr float SETTINGS_APEX_RADIUS  = 81.0f;  // Radial distance for active capsule pill badge
constexpr int   SETTINGS_CAPSULE_HW   = 26;     // Capsule half-width (52px total width)
constexpr int   SETTINGS_CAPSULE_HH   = 11;     // Capsule half-height (22px total height)

// Brightness Preset Levels (10%, 40%, 60%, 100%)
constexpr uint8_t BRIGHTNESS_LEVEL_COUNT = 4;
constexpr uint8_t BRIGHTNESS_LEVELS[4]   = { 26, 102, 153, 255 };

// Operating Modes
enum class AppMode : uint8_t {
  Clock    = 0, // Mode 1: Analog Clock with Dock Magnification
  Sensors  = 1, // Mode 2: Temperature & Humidity Sensor Dashboard
  Settings = 2  // Mode 3: Camera-Style Settings Rotary Dial
};

#endif // CONFIG_H
