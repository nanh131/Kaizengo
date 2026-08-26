#ifndef ISOLATED_INPUTS_H
#define ISOLATED_INPUTS_H

#include <Arduino.h>
#include "config.h"

// Trạng thái nội bộ của từng kênh sau khi đã lọc rung (debounce)
struct ChannelState {
  bool stableState;             // trạng thái đã ổn định (true = ACTIVE)
  bool lastRawState;            // trạng thái đọc thô ở lần quét trước
  unsigned long lastChangeMs;   // thời điểm trạng thái thô đổi lần cuối
  bool justChanged;             // cờ báo "vừa có sự kiện đổi trạng thái"
};

// Lớp quản lý các kênh ngõ vào đã được cách ly quang (PC817)
class IsolatedInputs {
  public:
    IsolatedInputs();

    // Gọi 1 lần trong setup()
    void begin();

    // Gọi liên tục trong loop() để cập nhật & lọc rung tín hiệu
    void update();

    // Trạng thái hiện tại (đã lọc rung) của 1 kênh
    bool isActive(uint8_t channel) const;

    // true đúng 1 lần ngay sau khi kênh vừa đổi trạng thái ổn định
    bool hasChanged(uint8_t channel) const;

    // Xoá cờ "vừa đổi trạng thái" sau khi đã xử lý xong sự kiện
    void clearChangedFlag(uint8_t channel);

  private:
    ChannelState _channels[NUM_CHANNELS];
    bool readRaw(uint8_t channel) const;
};

#endif // ISOLATED_INPUTS_H
