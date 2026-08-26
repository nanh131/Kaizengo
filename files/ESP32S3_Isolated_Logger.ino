// ============================================================================
// ESP32-S3 CNC Wi-Fi Bridge
// Upload, select and stream G-code files to a GRBL-compatible CNC controller.
// See README.md before wiring a machine.
// ============================================================================

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "GCodeStreamer.h"
#include "IsolatedInputs.h"
#include "SDLogger.h"
#include "WebInterface.h"

HardwareSerial CncSerial(1);
WebServer webServer(HTTP_PORT);
IsolatedInputs inputs;
SDLogger logger;
GCodeStreamer streamer(CncSerial);
WebInterface web(webServer, streamer, inputs, logger);

namespace {
bool usingFallbackAp = false;

bool hasStationCredentials() {
  return strlen(WIFI_SSID) > 0 && strcmp(WIFI_SSID, "CHANGE_ME_WIFI") != 0;
}

bool connectNetwork() {
  if (hasStationCredentials()) {
    Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(WIFI_HOSTNAME);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const unsigned long started = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - started < WIFI_CONNECT_TIMEOUT_MS) {
      delay(250);
      Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print(F("[WiFi] Station IP: "));
      Serial.println(WiFi.localIP());
      usingFallbackAp = false;
      return true;
    }
    Serial.println(F("[WiFi] Station connection failed; starting setup AP."));
  } else {
    Serial.println(F("[WiFi] Station credentials not configured; starting setup AP."));
  }

  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.setHostname(WIFI_HOSTNAME);
  if (!WiFi.softAP(FALLBACK_AP_SSID, FALLBACK_AP_PASSWORD)) {
    Serial.println(F("[WiFi] ERROR: could not start fallback AP."));
    return false;
  }
  usingFallbackAp = true;
  Serial.print(F("[WiFi] Setup AP: "));
  Serial.print(FALLBACK_AP_SSID);
  Serial.print(F("  IP: "));
  Serial.println(WiFi.softAPIP());
  return true;
}

void processIsolatedInputs() {
  inputs.update();
  for (uint8_t channel = 0; channel < NUM_CHANNELS; ++channel) {
    if (!inputs.hasChanged(channel)) {
      continue;
    }

    const bool active = inputs.isActive(channel);
    Serial.printf("[Input] CH%u -> %s\n", channel + 1,
                  active ? "ACTIVE" : "INACTIVE");
    logger.logEvent(channel, active);

    if (channel == ESTOP_INPUT_CHANNEL && active) {
      // This provides a software stop only.  It is deliberately never treated
      // as a substitute for the series-wired hardware emergency-stop circuit.
      streamer.abort("Software safety input active");
    }
    if (channel == DOOR_INPUT_CHANNEL && active) {
      // A door/guard input can request GRBL feed hold.  It never resumes a job.
      streamer.pause();
    }
    inputs.clearChangedFlag(channel);
  }
}
}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println(F("=== ESP32-S3 CNC Wi-Fi Bridge ==="));

  inputs.begin();
  if (!logger.begin()) {
    Serial.println(F("[SD] Warning: web interface is online, but SD features are disabled."));
  }

  streamer.begin();
  connectNetwork();

  if (MDNS.begin(WIFI_HOSTNAME)) {
    MDNS.addService("http", "tcp", HTTP_PORT);
    Serial.printf("[mDNS] http://%s.local/\n", WIFI_HOSTNAME);
  } else {
    Serial.println(F("[mDNS] Unable to start; use the IP address instead."));
  }

  web.begin(usingFallbackAp);
  Serial.println(F("[System] Ready. Verify the CNC is safe before starting any job."));
}

void loop() {
  processIsolatedInputs();
  streamer.update();
  web.handleClient();
  // Give controller responses an additional chance to be processed after a
  // potentially slow HTTP request (for example, a small upload chunk).
  streamer.update();
  delay(1);
}
