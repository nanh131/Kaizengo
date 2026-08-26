#include "SDLogger.h"

SDLogger::SDLogger() : _ready(false) {}

bool SDLogger::begin() {
  // ESP32-S3: có thể chỉ định lại chân SPI bằng SPI.begin(sck, miso, mosi, cs)
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(F("[SDLogger] Khong the khoi tao the SD! Kiem tra day noi/dinh dang FAT32."));
    _ready = false;
    return false;
  }

  _ready = true;
  writeHeaderIfNeeded();
 Serial.println("Đang khởi tạo thẻ MicroSD...");
if (!SD.begin(SD_CS_PIN)) {
    Serial.println("[CẢNH BÁO] Không nhận diện được thẻ SD! Vui lòng kiểm tra lại module.");
    // Không dùng while(1) ở đây, cứ để nó chạy tiếp xuống dưới
} else {
    Serial.println("[OK] Khởi tạo thẻ SD thành công.");
}
}

bool SDLogger::isReady() const {
  return _ready;
}

void SDLogger::writeHeaderIfNeeded() {
  if (!SD.exists(LOG_FILE_NAME)) {
    File f = SD.open(LOG_FILE_NAME, FILE_WRITE);
    if (f) {
      f.println(F("timestamp_ms,channel,state"));
      f.close();
    }
  }
}

void SDLogger::logEvent(uint8_t channel, bool active) {
  if (!_ready) return;

  File f = SD.open(LOG_FILE_NAME, FILE_APPEND);
  if (f) {
    f.print(millis());
    f.print(",");
    f.print(channel);
    f.print(",");
    f.println(active ? "ACTIVE" : "INACTIVE");
    f.close();
  } else {
    Serial.println(F("[SDLogger] Loi khi mo file de ghi log!"));
  }
}
