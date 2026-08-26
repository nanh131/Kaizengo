#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// CNC Wi-Fi Bridge - configuration for ESP32-S3
// Change all values marked CHANGE_ME before installing the device.
// ============================================================================

// ------------------------- Network and web security -------------------------
// Leave WIFI_SSID empty to start only the fallback access point.
#define WIFI_SSID "VNPT"
#define WIFI_PASSWORD "dunghoiem@"
#define WIFI_HOSTNAME "cnc-bridge"
#define WIFI_CONNECT_TIMEOUT_MS 20000UL

// Fallback AP is enabled when the station connection cannot be made.
// Use a password of at least eight characters.
#define FALLBACK_AP_SSID "CNC-Bridge-Setup"
#define FALLBACK_AP_PASSWORD "coderthanhcong"

// HTTP Basic Authentication. Do not keep the default password on a real network.
#define WEB_USERNAME "admin"
#define WEB_PASSWORD "coderthanhcong"
#define HTTP_PORT 80

// ------------------------------- SD card (SPI) ------------------------------
// Use a 3.3 V compatible SD module. Do not feed 5 V logic into ESP32 pins.
#define SD_CS_PIN 10
#define SD_SCK_PIN 12
#define SD_MOSI_PIN 11
#define SD_MISO_PIN 13

// ---------------------------- CNC serial (GRBL) -----------------------------
// This firmware implements the GRBL-compatible UART protocol. It does not
// control USB-only, Mach3, or proprietary CNC controllers without a gateway.
// GPIO17 (TX) -> isolated UART RX of the controller
// GPIO18 (RX) <- isolated UART TX of the controller
#define CNC_TX_PIN 17
#define CNC_RX_PIN 18
#define CNC_BAUD 115200
#define CNC_UART_CONFIG SERIAL_8N1

// A line is sent only after the controller answers "ok". This is deliberately
// conservative and avoids overflowing a GRBL serial buffer.
#define CNC_ACK_TIMEOUT_MS 30000UL
#define CNC_STATUS_POLL_MS 500UL
#define GCODE_MAX_LINE_LENGTH 255
#define MAX_UPLOAD_BYTES (16UL * 1024UL * 1024UL)

// ---------------------------- Isolated PC817 inputs -------------------------
// OUT1..OUT4 of a PC817 input module, connected to the ESP32 logic side.
// These are monitoring inputs only; they are NOT a hardware emergency stop.
#define NUM_CHANNELS 4
static const uint8_t ISO_INPUT_PINS[NUM_CHANNELS] = {4, 5, 6, 7};
#define ISO_INPUT_ACTIVE_LOW true
#define DEBOUNCE_MS 30UL

// Set a channel number (0..3) only if that PC817 input is wired as a monitored
// safety signal. -1 disables the software reaction. The actual E-stop circuit
// must be hard-wired to remove CNC motion power independently of this ESP32.
#define ESTOP_INPUT_CHANNEL -1
#define DOOR_INPUT_CHANNEL -1

// ---------------------------------- Logging ---------------------------------
#define LOG_FILE_NAME "/isolated_log.csv"
#define SERIAL_BAUD 115200

#endif  // CONFIG_H
