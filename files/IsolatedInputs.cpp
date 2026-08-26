#include "IsolatedInputs.h"

IsolatedInputs::IsolatedInputs() {}

void IsolatedInputs::begin() {
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    // Module PC817 đã có sẵn điện trở pull-up phía OUTPUT trên board
    pinMode(ISO_INPUT_PINS[i], INPUT);

    bool raw = readRaw(i);
    _channels[i].stableState  = raw;
    _channels[i].lastRawState = raw;
    _channels[i].lastChangeMs = millis();
    _channels[i].justChanged  = false;
  }
}

bool IsolatedInputs::readRaw(uint8_t channel) const {
  int level = digitalRead(ISO_INPUT_PINS[channel]);
  bool active = ISO_INPUT_ACTIVE_LOW ? (level == LOW) : (level == HIGH);
  return active;
}

void IsolatedInputs::update() {
  unsigned long now = millis();

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    bool raw = readRaw(i);

    // Nếu tín hiệu thô thay đổi, ghi lại thời điểm để bắt đầu đếm debounce
    if (raw != _channels[i].lastRawState) {
      _channels[i].lastRawState = raw;
      _channels[i].lastChangeMs = now;
    }

    // Nếu tín hiệu đã ổn định đủ lâu -> chấp nhận là trạng thái mới
    if ((now - _channels[i].lastChangeMs) >= DEBOUNCE_MS) {
      if (raw != _channels[i].stableState) {
        _channels[i].stableState = raw;
        _channels[i].justChanged = true;
      }
    }
  }
}

bool IsolatedInputs::isActive(uint8_t channel) const {
  if (channel >= NUM_CHANNELS) return false;
  return _channels[channel].stableState;
}

bool IsolatedInputs::hasChanged(uint8_t channel) const {
  if (channel >= NUM_CHANNELS) return false;
  return _channels[channel].justChanged;
}

void IsolatedInputs::clearChangedFlag(uint8_t channel) {
  if (channel >= NUM_CHANNELS) return;
  _channels[channel].justChanged = false;
}
