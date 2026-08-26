#include "GCodeStreamer.h"

#include <ctype.h>
#include <string.h>

GCodeStreamer::GCodeStreamer(HardwareSerial& serial)
    : _serial(serial),
      _state(State::IDLE),
      _lineLength(0),
      _responseLength(0),
      _sourceLine(0),
      _sentLine(0),
      _lastSendMs(0),
      _lastStatusRequestMs(0),
      _waitingForAck(false) {
  _line[0] = '\0';
  _response[0] = '\0';
  strncpy(_controllerState, "unknown", sizeof(_controllerState));
  _controllerState[sizeof(_controllerState) - 1] = '\0';
  setLastResponse("Idle");
}

void GCodeStreamer::begin() {
  _serial.begin(CNC_BAUD, CNC_UART_CONFIG, CNC_RX_PIN, CNC_TX_PIN);
  Serial.printf("[CNC] UART ready at %lu baud (RX=%d, TX=%d)\n",
                static_cast<unsigned long>(CNC_BAUD), CNC_RX_PIN, CNC_TX_PIN);
}

bool GCodeStreamer::start(const char* path) {
  if (path == nullptr || path[0] != '/') {
    setLastResponse("Invalid job path");
    return false;
  }
  if (isBusy()) {
    setLastResponse("A job is already active");
    return false;
  }

  closeFile();
  _file = SD.open(path, FILE_READ);
  if (!_file || _file.isDirectory()) {
    closeFile();
    setLastResponse("Cannot open G-code file");
    return false;
  }

  // Ignore any old banner/status bytes that preceded this job.  No command is
  // sent to the controller here; the operator must ensure the machine is ready.
  while (_serial.available()) {
    _serial.read();
  }

  _fileName = path;
  _lineLength = 0;
  _responseLength = 0;
  _sourceLine = 0;
  _sentLine = 0;
  _waitingForAck = false;
  _lastStatusRequestMs = 0;
  strncpy(_controllerState, "unknown", sizeof(_controllerState));
  _controllerState[sizeof(_controllerState) - 1] = '\0';
  _state = State::RUNNING;
  setLastResponse("Job started");
  return true;
}

bool GCodeStreamer::pause() {
  if (_state != State::RUNNING) {
    return false;
  }
  _serial.write('!');  // GRBL feed hold realtime command
  _state = State::PAUSED;
  setLastResponse("Feed hold requested");
  return true;
}

bool GCodeStreamer::resume() {
  if (_state != State::PAUSED) {
    return false;
  }
  _serial.write('~');  // GRBL cycle start/resume realtime command
  _state = State::RUNNING;
  setLastResponse("Resume requested");
  return true;
}

bool GCodeStreamer::abort(const char* reason) {
  if (!isBusy()) {
    return false;
  }
  _serial.write('!');
  _serial.write(static_cast<uint8_t>(0x18));  // GRBL soft-reset
  closeFile();
  _waitingForAck = false;
  _state = State::ABORTED;
  setLastResponse(reason == nullptr ? "Job aborted" : reason);
  return true;
}

void GCodeStreamer::update() {
  readController();
  pollMachineStatus();

  if (_state == State::DRAINING) {
    return;
  }
  if (_state != State::RUNNING) {
    return;
  }

  if (_waitingForAck) {
    if (millis() - _lastSendMs > CNC_ACK_TIMEOUT_MS) {
      fail("Timed out waiting for controller response");
    }
    return;
  }

  char command[GCODE_MAX_LINE_LENGTH + 1];
  if (!readNextCommand(command, sizeof(command))) {
    if (_state == State::RUNNING) {
      complete();
    }
    return;
  }

  _serial.print(command);
  _serial.write('\n');
  _sentLine++;
  _waitingForAck = true;
  _lastSendMs = millis();
  setLastResponse(command);
}

void GCodeStreamer::pollMachineStatus() {
  if (!isBusy()) {
    return;
  }
  const unsigned long now = millis();
  if (now - _lastStatusRequestMs >= CNC_STATUS_POLL_MS) {
    _serial.write('?');  // GRBL realtime status query; it does not alter motion.
    _lastStatusRequestMs = now;
  }
}

void GCodeStreamer::readController() {
  while (_serial.available()) {
    const char c = static_cast<char>(_serial.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      _response[_responseLength] = '\0';
      if (_responseLength > 0) {
        handleControllerLine(_response);
      }
      _responseLength = 0;
      continue;
    }
    if (_responseLength + 1 < sizeof(_response)) {
      _response[_responseLength++] = c;
    } else {
      // Keep parsing after a malformed, oversized diagnostic line.
      _responseLength = 0;
      setLastResponse("Controller response too long");
    }
  }
}

void GCodeStreamer::handleControllerLine(char* line) {
  while (*line != '\0' && isspace(static_cast<unsigned char>(*line))) {
    ++line;
  }
  if (*line == '\0') {
    return;
  }

  setLastResponse(line);
  if (*line == '<') {
    const char* stateStart = line + 1;
    const char* stateEnd = stateStart;
    while (*stateEnd != '\0' && *stateEnd != '|' && *stateEnd != '>') {
      ++stateEnd;
    }
    const size_t stateLength = stateEnd - stateStart;
    if (stateLength > 0) {
      const size_t copyLength = stateLength < sizeof(_controllerState) - 1
                                    ? stateLength
                                    : sizeof(_controllerState) - 1;
      memcpy(_controllerState, stateStart, copyLength);
      _controllerState[copyLength] = '\0';
    }
    if (_state == State::DRAINING && startsWithIgnoreCase(stateStart, "idle")) {
      _state = State::COMPLETE;
      setLastResponse("Job complete; controller is idle");
    } else if (isBusy() && startsWithIgnoreCase(stateStart, "alarm")) {
      fail("Controller entered alarm state");
    }
    return;
  }
  if (_waitingForAck && startsWithIgnoreCase(line, "ok")) {
    _waitingForAck = false;
    return;
  }
  if (isBusy() && startsWithIgnoreCase(line, "alarm")) {
    fail(line);
    return;
  }
  if (_waitingForAck && startsWithIgnoreCase(line, "error")) {
    fail(line);
  }
}

bool GCodeStreamer::readNextCommand(char* destination, size_t destinationSize) {
  while (_file.available()) {
    const int value = _file.read();
    if (value < 0) {
      break;
    }
    const char c = static_cast<char>(value);
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      _line[_lineLength] = '\0';
      _lineLength = 0;
      ++_sourceLine;
      if (cleanGCodeLine(_line, destination, destinationSize)) {
        return true;
      }
      continue;
    }
    if (_lineLength >= GCODE_MAX_LINE_LENGTH) {
      fail("G-code source line exceeds configured limit");
      return false;
    }
    _line[_lineLength++] = c;
  }

  if (_lineLength > 0) {
    _line[_lineLength] = '\0';
    _lineLength = 0;
    ++_sourceLine;
    return cleanGCodeLine(_line, destination, destinationSize);
  }
  return false;
}

bool GCodeStreamer::cleanGCodeLine(const char* source, char* destination,
                                    size_t destinationSize) const {
  size_t out = 0;
  uint8_t parenthesisDepth = 0;
  for (size_t i = 0; source[i] != '\0'; ++i) {
    const char c = source[i];
    if (c == '(') {
      if (parenthesisDepth < 255) {
        ++parenthesisDepth;
      }
      continue;
    }
    if (c == ')') {
      if (parenthesisDepth > 0) {
        --parenthesisDepth;
      }
      continue;
    }
    if (parenthesisDepth > 0 || c == ';' || isspace(static_cast<unsigned char>(c))) {
      if (c == ';' && parenthesisDepth == 0) {
        break;
      }
      continue;
    }
    if (static_cast<unsigned char>(c) < 32) {
      continue;
    }
    if (out + 1 >= destinationSize) {
      return false;
    }
    destination[out++] = c;
  }
  destination[out] = '\0';

  // Percent delimiters are understood by many senders, but are not GRBL
  // commands and would cause an avoidable error.
  return out > 0 && strcmp(destination, "%") != 0;
}

void GCodeStreamer::fail(const char* reason) {
  closeFile();
  _waitingForAck = false;
  _state = State::FAILED;
  setLastResponse(reason == nullptr ? "Job failed" : reason);
}

void GCodeStreamer::complete() {
  closeFile();
  _waitingForAck = false;
  // GRBL acknowledges when it accepts a line, not when final motion ends.
  // Keep the job abortable until a realtime status reply reports Idle.
  _state = State::DRAINING;
  setLastResponse("All lines sent; waiting for controller to become idle");
}

void GCodeStreamer::closeFile() {
  if (_file) {
    _file.close();
  }
}

bool GCodeStreamer::isBusy() const {
  return _state == State::RUNNING || _state == State::PAUSED ||
         _state == State::DRAINING;
}

GCodeStreamer::State GCodeStreamer::state() const {
  return _state;
}

const char* GCodeStreamer::stateName() const {
  switch (_state) {
    case State::IDLE: return "idle";
    case State::RUNNING: return "running";
    case State::PAUSED: return "paused";
    case State::DRAINING: return "finishing";
    case State::COMPLETE: return "complete";
    case State::FAILED: return "failed";
    case State::ABORTED: return "aborted";
  }
  return "unknown";
}

const char* GCodeStreamer::fileName() const {
  return _fileName.c_str();
}

const char* GCodeStreamer::lastResponse() const {
  return _lastResponse;
}

const char* GCodeStreamer::controllerState() const {
  return _controllerState;
}

uint32_t GCodeStreamer::sourceLine() const {
  return _sourceLine;
}

uint32_t GCodeStreamer::sentLine() const {
  return _sentLine;
}

bool GCodeStreamer::waitingForAck() const {
  return _waitingForAck;
}

String GCodeStreamer::statusJson() const {
  String result = "{\"state\":\"";
  result += stateName();
  result += "\",\"file\":";
  result += jsonString(fileName());
  result += ",\"sourceLine\":";
  result += String(sourceLine());
  result += ",\"sentLine\":";
  result += String(sentLine());
  result += ",\"waitingForAck\":";
  result += waitingForAck() ? "true" : "false";
  result += ",\"lastResponse\":";
  result += jsonString(lastResponse());
  result += ",\"controllerState\":";
  result += jsonString(controllerState());
  result += "}";
  return result;
}

void GCodeStreamer::setLastResponse(const char* response) {
  if (response == nullptr) {
    _lastResponse[0] = '\0';
    return;
  }
  strncpy(_lastResponse, response, sizeof(_lastResponse) - 1);
  _lastResponse[sizeof(_lastResponse) - 1] = '\0';
}

bool GCodeStreamer::startsWithIgnoreCase(const char* text, const char* prefix) {
  while (*prefix != '\0') {
    if (*text == '\0' || tolower(static_cast<unsigned char>(*text)) !=
                            tolower(static_cast<unsigned char>(*prefix))) {
      return false;
    }
    ++text;
    ++prefix;
  }
  return true;
}

String GCodeStreamer::jsonString(const char* value) {
  String result = "\"";
  if (value != nullptr) {
    for (const char* p = value; *p != '\0'; ++p) {
      if (*p == '\\' || *p == '\"') {
        result += '\\';
      }
      if (static_cast<unsigned char>(*p) < 32) {
        result += ' ';
      } else {
        result += *p;
      }
    }
  }
  result += "\"";
  return result;
}
