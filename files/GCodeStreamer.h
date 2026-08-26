#ifndef GCODE_STREAMER_H
#define GCODE_STREAMER_H

#include <Arduino.h>
#include <SD.h>
#include "config.h"

// Streams a G-code file to a GRBL-compatible controller over UART.  It sends
// the next command only after receiving an "ok", which favors predictability
// and safety over maximum throughput.
class GCodeStreamer {
 public:
  enum class State : uint8_t { IDLE, RUNNING, PAUSED, DRAINING, COMPLETE, FAILED, ABORTED };

  explicit GCodeStreamer(HardwareSerial& serial);

  void begin();
  void update();

  bool start(const char* path);
  bool pause();
  bool resume();
  bool abort(const char* reason = "Stopped by operator");

  bool isBusy() const;
  State state() const;
  const char* stateName() const;
  const char* fileName() const;
  const char* lastResponse() const;
  const char* controllerState() const;
  uint32_t sourceLine() const;
  uint32_t sentLine() const;
  bool waitingForAck() const;

  // JSON object without outer application state, for /api/status.
  String statusJson() const;

 private:
  HardwareSerial& _serial;
  File _file;
  State _state;
  String _fileName;
  char _line[GCODE_MAX_LINE_LENGTH + 1];
  size_t _lineLength;
  char _response[128];
  size_t _responseLength;
  char _lastResponse[128];
  uint32_t _sourceLine;
  uint32_t _sentLine;
  unsigned long _lastSendMs;
  unsigned long _lastStatusRequestMs;
  bool _waitingForAck;
  char _controllerState[16];

  void readController();
  void pollMachineStatus();
  void handleControllerLine(char* line);
  bool readNextCommand(char* destination, size_t destinationSize);
  bool cleanGCodeLine(const char* source, char* destination, size_t destinationSize) const;
  void fail(const char* reason);
  void complete();
  void closeFile();
  void setLastResponse(const char* response);
  static bool startsWithIgnoreCase(const char* text, const char* prefix);
  static String jsonString(const char* value);
};

#endif  // GCODE_STREAMER_H
