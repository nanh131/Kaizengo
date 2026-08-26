#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "config.h"

// Lớp quản lý việc khởi tạo thẻ SD và ghi log sự kiện dạng CSV
class SDLogger {
  public:
    SDLogger();

    // Khởi tạo SPI + thẻ SD. Trả về false nếu không phát hiện được thẻ.
    bool begin();

    // Ghi 1 dòng log: thời gian (ms), số kênh, trạng thái
    void logEvent(uint8_t channel, bool active);

    // Kiểm tra thẻ SD có sẵn sàng để ghi hay không
    bool isReady() const;

  private:
    bool _ready;
    void writeHeaderIfNeeded();
};

#endif // SD_LOGGER_H
