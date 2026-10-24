// ── ARIA Arduino Library v2 ──────────────────────────────────────────────────
// Implements the ARIA protocol over USB Serial for AI-controlled devices.
// Protocol: line-based, newline-terminated, format: T:version|id|payload\n
// Message types: H=Handshake, A=Action, S=SetVar, V=Variable, R=Result,
//                E=Error, N=Notification/Event, D=Discovery, B=Binary/raw64
// ─────────────────────────────────────────────────────────────────────────────

#include "Aria.h"

// Base64 encoding table for raw64 support
static const char B64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// ── Singleton instance ───────────────────────────────────────────────────────
AriaClass ARIA;

// ── Constructor ──────────────────────────────────────────────────────────────
AriaClass::AriaClass()
    : serial_(nullptr),
      version_(ARIA_PROTOCOL_VERSION),
      lineLength_(0),
      discardingLine_(false),
      actionsCount_(0),
      varsCount_(0),
      eventsCount_(0),
      argumentOffset_(0),
      raw64ExpectedChunks_(0),
      raw64ReceivedChunks_(0),
      raw64NextChunk_(0),
      raw64ExpectedLength_(0) {
  deviceId_[0]        = '\0';
  deviceName_[0]      = '\0';
  firmwareVersion_[0] = '\0';
  argBuf_[0]          = '\0';
  raw64Checksum_[0]   = '\0';
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

void AriaClass::begin(Stream &serial, const char *deviceId, uint8_t version) {
  serial_ = &serial;
  version_ = version;
  strncpy(deviceId_, deviceId, ARIA_ID_MAXLEN - 1);
  deviceId_[ARIA_ID_MAXLEN - 1] = '\0';
}

void AriaClass::device(const char *name, const char *firmwareVersion) {
  if (name) {
    strncpy(deviceName_, name, ARIA_NAME_MAXLEN - 1);
    deviceName_[ARIA_NAME_MAXLEN - 1] = '\0';
  }
  if (firmwareVersion) {
    strncpy(firmwareVersion_, firmwareVersion, ARIA_NAME_MAXLEN - 1);
    firmwareVersion_[ARIA_NAME_MAXLEN - 1] = '\0';
  }
}

void AriaClass::loop() {
  if (!serial_) return;
  while (serial_->available()) {
    const char c = serial_->read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (discardingLine_) {
        sendError("protocol", "frame_too_long");
      } else if (lineLength_) {
        lineBuf_[lineLength_] = '\0';
        handleLine(lineBuf_, lineLength_);
      }
      lineLength_ = 0;
      discardingLine_ = false;
      continue;
    }
    if (discardingLine_) continue;
    if (lineLength_ >= sizeof(lineBuf_) - 1) {
      discardingLine_ = true;
      continue;
    }
    lineBuf_[lineLength_++] = c;
  }
}

// ── Registration: Actions ────────────────────────────────────────────────────

bool AriaClass::registerAction(const char *id, AriaActionHandler handler) {
  if (!id || !handler || actionsCount_ >= ARIA_MAX_ACTIONS || findAction(id))
    return false;

  AriaActionEntry &entry = actions_[actionsCount_++];
  strncpy(entry.id, id, ARIA_ID_MAXLEN - 1);
  entry.id[ARIA_ID_MAXLEN - 1] = '\0';
  entry.handler = handler;
  entry.noArgHandler = nullptr;
  entry.noArgs = false;
  return true;
}

bool AriaClass::registerAction(const char *id, AriaNoArgActionHandler handler) {
  if (!id || !handler || actionsCount_ >= ARIA_MAX_ACTIONS || findAction(id)) return false;
  AriaActionEntry &entry = actions_[actionsCount_++];
  strncpy(entry.id, id, ARIA_ID_MAXLEN - 1);
  entry.id[ARIA_ID_MAXLEN - 1] = '\0';
  entry.handler = nullptr;
  entry.noArgHandler = handler;
  entry.noArgs = true;
  return true;
}

// ── Registration: Variables ──────────────────────────────────────────────────

bool AriaClass::addVariable(const char *id, AriaVarType type, void *ptr,
                            float min, float max, bool bounded) {
  if (!id || !ptr || varsCount_ >= ARIA_MAX_VARS || findVar(id))
    return false;

  AriaVarEntry &v = vars_[varsCount_++];
  strncpy(v.id, id, ARIA_ID_MAXLEN - 1);
  v.id[ARIA_ID_MAXLEN - 1] = '\0';
  v.type = type;
  v.ptr = ptr;
  v.min = min;
  v.max = max;
  v.bounded = bounded;
  return true;
}

bool AriaClass::variable(const char *id, bool *ptr) {
  return addVariable(id, ARIA_BOOL, ptr);
}

bool AriaClass::variable(const char *id, int *ptr, int min, int max) {
  return addVariable(id, ARIA_INT, ptr, (float)min, (float)max, true);
}

bool AriaClass::variable(const char *id, float *ptr, float min, float max) {
  return addVariable(id, ARIA_FLOAT, ptr, min, max, true);
}

bool AriaClass::variable(const char *id, String *ptr) {
  return addVariable(id, ARIA_STRING, ptr);
}

bool AriaClass::variableJson(const char *id, String *ptr) {
  return addVariable(id, ARIA_JSON, ptr);
}

bool AriaClass::variableRaw64(const char *id, String *ptr) {
  return addVariable(id, ARIA_RAW64, ptr);
}

// ── Registration: Events ─────────────────────────────────────────────────────

bool AriaClass::event(const char *id) {
  if (!id || eventsCount_ >= ARIA_MAX_EVENTS || findEvent(id))
    return false;

  AriaEventEntry &entry = events_[eventsCount_++];
  strncpy(entry.id, id, ARIA_ID_MAXLEN - 1);
  entry.id[ARIA_ID_MAXLEN - 1] = '\0';
  return true;
}

void AriaClass::raiseEvent(const char *id) {
  raiseEvent(id, nullptr);
}

void AriaClass::raiseEvent(const char *id, const char *payload) {
  if (!serial_ || !findEvent(id)) return;
  printHeader('N', id);
  if (payload && payload[0] != '\0') serial_->print(payload);
  serial_->print('\n');
}

// ── printHeader helper ────────────────────────────────────────────────────────
// Writes "T:version|id|" to serial. Centralises the pattern used by every
// outgoing message, and lets the compiler inline / optimise it once.
void AriaClass::printHeader(char type, const char *id) {
  serial_->print(type);
  serial_->print(':');
  serial_->print(version_);
  serial_->print('|');
  serial_->print(id);
  serial_->print('|');
}

// ── Results ──────────────────────────────────────────────────────────────────

void AriaClass::result(const char *id, bool value) {
  if (!serial_) return;
  printHeader('R', id);
  serial_->print(F("b:"));
  serial_->print(value ? 1 : 0);
  serial_->print('\n');
}

void AriaClass::result(const char *id, int value) {
  if (!serial_) return;
  printHeader('R', id);
  serial_->print(F("i:"));
  serial_->print(value);
  serial_->print('\n');
}

void AriaClass::result(const char *id, float value) {
  if (!serial_) return;
  printHeader('R', id);
  serial_->print(F("f:"));
  serial_->print(value);
  serial_->print('\n');
}

void AriaClass::result(const char *id, const String &value, AriaVarType type) {
  if (!serial_) return;
  printHeader('R', id);
  if      (type == ARIA_JSON)  serial_->print(F("j:"));
  else if (type == ARIA_RAW64) serial_->print(F("r:"));
  else                         serial_->print(F("s:"));
  serial_->print(value);
  serial_->print('\n');
}

// ── Variable sync ────────────────────────────────────────────────────────────

void AriaClass::sync(const char *id) {
  AriaVarEntry *v = findVar(id);
  if (v) writeValue('V', id, v);
}

void AriaClass::writeValue(char messageType, const char *id, AriaVarEntry *v) {
  if (!serial_) return;
  if (messageType == 'V' && v->type == ARIA_RAW64) {
    sendRaw64Encoded(id, *(String *)v->ptr);
    return;
  }

  printHeader(messageType, id);

  switch (v->type) {
    case ARIA_BOOL:
      serial_->print(F("b:"));
      serial_->print(*(bool *)v->ptr ? 1 : 0);
      break;
    case ARIA_INT:
      serial_->print(F("i:"));
      serial_->print(*(int *)v->ptr);
      break;
    case ARIA_FLOAT:
      serial_->print(F("f:"));
      serial_->print(*(float *)v->ptr);
      break;
    case ARIA_JSON:
      serial_->print(F("j:"));
      serial_->print(*(String *)v->ptr);
      break;
    case ARIA_RAW64:
      serial_->print(F("r:"));
      serial_->print(*(String *)v->ptr);
      break;
    case ARIA_STRING:
    default:
      serial_->print(F("s:"));
      serial_->print(*(String *)v->ptr);
      break;
  }

  serial_->print('\n');
}

// ── Binary data (raw64 / base64) ─────────────────────────────────────────────

void AriaClass::sendRaw64(const char *id, const uint8_t *data, size_t length) {
  if (!serial_ || !data || length == 0) return;
  const size_t encodedLength = ((length + 2) / 3) * 4;
  if (encodedLength > ARIA_MAX_RAW64_LENGTH) {
    sendError(id, "raw64_size_limit");
    return;
  }
  String encoded;
  encoded.reserve(encodedLength);
  size_t i = 0;
  while (i < length) {
    uint8_t b0 = data[i++];
    uint8_t b1 = (i < length) ? data[i++] : 0;
    uint8_t b2 = (i < length) ? data[i++] : 0;

    uint8_t a0 = b0 >> 2;
    uint8_t a1 = ((b0 & 0x03) << 4) | (b1 >> 4);
    uint8_t a2 = ((b1 & 0x0F) << 2) | (b2 >> 6);
    uint8_t a3 = b2 & 0x3F;

    encoded += B64_TABLE[a0];
    encoded += B64_TABLE[a1];

    if (i - 1 < length) {
      encoded += B64_TABLE[a2];
    } else {
      encoded += '=';
    }
    if (i < length) {
      encoded += B64_TABLE[a3];
    } else {
      encoded += '=';
    }
  }
  sendRaw64Encoded(id, encoded);
}

void AriaClass::sendRaw64Encoded(const char *id, const String &encoded) {
  if (!serial_ || !id || encoded.length() == 0) return;
  if (encoded.length() > ARIA_MAX_RAW64_LENGTH) {
    sendError(id, "raw64_size_limit");
    return;
  }
  const size_t chunkSize = ARIA_RAW64_CHUNK_SIZE;
  char cs[9];
  if (encoded.length() <= chunkSize) {
    printHeader('B', id);
    serial_->print(F("r:"));
    serial_->print(encoded);
    serial_->print('\n');
    return;
  }
  const size_t totalChunks = (encoded.length() + chunkSize - 1) / chunkSize;
  computeChecksum(encoded, cs);
  printHeader('B', id);
  serial_->print(F("start:"));
  serial_->print(totalChunks);
  serial_->print(':');
  serial_->print(encoded.length());
  serial_->print(':');
  serial_->print(cs);
  serial_->print('\n');
  for (size_t offset = 0, index = 0; offset < encoded.length(); offset += chunkSize, ++index) {
    printHeader('B', id);
    serial_->print(F("chunk:"));
    serial_->print(index);
    serial_->print(':');
    serial_->println(encoded.substring(offset, offset + chunkSize));
  }
  printHeader('B', id);
  serial_->print(F("end:\n"));
}

// Writes 8-hex-digit FNV-1a hash of value into out[9] (including NUL).
void AriaClass::computeChecksum(const String &value, char out[9]) {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < value.length(); i++) {
    hash ^= (uint8_t)value.charAt(i);
    hash *= 16777619UL;
  }
  snprintf(out, 9, "%08lx", (unsigned long)hash);
}

// ── Handshake response ───────────────────────────────────────────────────────

void AriaClass::handleHandshake() {
  if (!serial_) return;

  printHeader('H', deviceId_);
  serial_->print(F("a:"));
  for (uint8_t i = 0; i < actionsCount_; i++) {
    if (i) serial_->print(',');
    serial_->print(actions_[i].id);
  }
  serial_->print(F(";v:"));
  for (uint8_t i = 0; i < varsCount_; i++) {
    if (i) serial_->print(',');
    serial_->print(vars_[i].id);
  }
  serial_->print(F(";e:"));
  for (uint8_t i = 0; i < eventsCount_; i++) {
    if (i) serial_->print(',');
    serial_->print(events_[i].id);
  }
  serial_->print('\n');
}

// ── Discovery response ──────────────────────────────────────────────────────

void AriaClass::handleDiscovery() {
  if (!serial_) return;

  // Info line — inline the fixed sub-IDs directly; F() cannot be passed to
  // printHeader(char, const char*) because F() returns __FlashStringHelper*.
  serial_->print('D'); serial_->print(':'); serial_->print(version_);
  serial_->print(F("|info|"));
  serial_->print(F("id="));    serial_->print(deviceId_);
  serial_->print(F(",name=")); serial_->print(deviceName_);
  serial_->print(F(",fw="));   serial_->print(firmwareVersion_);
  serial_->print(F(",lib="));  serial_->print(F(ARIA_LIBRARY_VERSION));
  serial_->print('\n');

  // Actions
  serial_->print('D'); serial_->print(':'); serial_->print(version_);
  serial_->print(F("|actions|"));
  for (uint8_t i = 0; i < actionsCount_; i++) {
    if (i) serial_->print(',');
    serial_->print(actions_[i].id);
  }
  serial_->print('\n');

  // Variables
  serial_->print('D'); serial_->print(':'); serial_->print(version_);
  serial_->print(F("|variables|"));
  for (uint8_t i = 0; i < varsCount_; i++) {
    if (i) serial_->print(',');
    serial_->print(vars_[i].id);
    serial_->print(':');
    switch (vars_[i].type) {
      case ARIA_BOOL:  serial_->print('b'); break;
      case ARIA_INT:   serial_->print('i'); break;
      case ARIA_FLOAT: serial_->print('f'); break;
      case ARIA_JSON:  serial_->print('j'); break;
      case ARIA_RAW64: serial_->print('r'); break;
      default:         serial_->print('s'); break;
    }
  }
  serial_->print('\n');

  // Events
  serial_->print('D'); serial_->print(':'); serial_->print(version_);
  serial_->print(F("|events|"));
  for (uint8_t i = 0; i < eventsCount_; i++) {
    if (i) serial_->print(',');
    serial_->print(events_[i].id);
  }
  serial_->print('\n');

  // End
  serial_->print('D'); serial_->print(':'); serial_->print(version_);
  serial_->print(F("|end|\n"));
}

// ── Message dispatcher ──────────────────────────────────────────────────────
// Operates on a raw null-terminated C-string to avoid constructing a temporary
// heap String. All sub-strings are parsed with pointer arithmetic / strncmp.

void AriaClass::handleLine(const char *line, uint16_t len) {
  if (len < 5) return;               // shortest valid frame: "H:2|x|\0"

  // Parse: TYPE ':' VERSION '|' ID '|' PAYLOAD
  if (line[1] != ':') return;
  char type = line[0];

  // Find first and second '|'
  const char *p1 = strchr(line + 2, '|');
  if (!p1) return;
  const char *p2 = strchr(p1 + 1, '|');
  if (!p2) return;

  uint8_t incomingVersion = (uint8_t)atoi(line + 2);
  if (incomingVersion != version_) {
    // Plain literals — sendError takes const char*, not __FlashStringHelper*
    sendError("protocol", "version_mismatch");
    return;
  }

  // Extract id into a small stack buffer (ARIA_ID_MAXLEN bytes)
  char id[ARIA_ID_MAXLEN];
  uint8_t idLen = (uint8_t)min((int)(p2 - p1 - 1), ARIA_ID_MAXLEN - 1);
  memcpy(id, p1 + 1, idLen);
  id[idLen] = '\0';

  // Payload starts after p2
  const char *payload = p2 + 1;

  if (type == 'H') { handleHandshake(); return; }
  if (type == 'D') { handleDiscovery(); return; }

  if (type == 'A') {
    AriaActionEntry *action = findAction(id);
    if (action) {
      strncpy(argBuf_, payload, sizeof(argBuf_) - 1);
      argBuf_[sizeof(argBuf_) - 1] = '\0';
      argumentOffset_ = 0;
      if (action->noArgs) action->noArgHandler();
      else action->handler(argBuf_);
    } else {
      sendError(id, "unknown_action");
    }
    return;
  }

  if (type == 'V') {
    AriaVarEntry *v = findVar(id);
    if (v) writeValue('V', id, v);
    else   sendError(id, "unknown_variable");
    return;
  }

  if (type == 'S') {
    AriaVarEntry *v = findVar(id);
    if (!v) { sendError(id, "unknown_variable"); return; }
    if (strlen(payload) < 2 || payload[1] != ':') { sendError(id, "bad_payload"); return; }

    char typePrefix = payload[0];
    const char *value = payload + 2;
    bool typeMismatch = false;

    if      (v->type == ARIA_BOOL   && typePrefix == 'b') { *(bool *)v->ptr   = (value[0] == '1' || strcmp(value, "true") == 0); }
    else if (v->type == ARIA_INT    && typePrefix == 'i') { int n = atoi(value);    if (v->bounded) n = constrain(n, (int)v->min, (int)v->max); *(int *)v->ptr = n; }
    else if (v->type == ARIA_FLOAT  && typePrefix == 'f') { float n = atof(value);  if (v->bounded) n = constrain(n, v->min, v->max);           *(float *)v->ptr = n; }
    else if (v->type == ARIA_STRING && typePrefix == 's') { *(String *)v->ptr = String(value); }
    else if (v->type == ARIA_JSON   && typePrefix == 'j') { *(String *)v->ptr = String(value); }
    else if (v->type == ARIA_RAW64  && typePrefix == 'r') {
      if (strlen(value) > ARIA_MAX_RAW64_LENGTH) {
        sendError(id, "raw64_size_limit");
        return;
      }
      *(String *)v->ptr = String(value);
    }
    else typeMismatch = true;

    if (typeMismatch) { sendError(id, "type_mismatch"); return; }
    writeValue('V', id, v);
    return;
  }

  if (type == 'B') {
    AriaVarEntry *v = findVar(id);
    if (!v || v->type != ARIA_RAW64) { sendError(id, "unknown_raw64_variable"); return; }

    if (strncmp(payload, "r:", 2) == 0) {
      if (strlen(payload + 2) > ARIA_MAX_RAW64_LENGTH) {
        sendError(id, "raw64_size_limit");
        return;
      }
      *(String *)v->ptr = String(payload + 2);
      writeValue('V', id, v);
    } else if (strncmp(payload, "start:", 6) == 0) {
      const char *h = payload + 6;
      const char *c1 = strchr(h, ':');
      if (!c1) { sendError(id, "raw64_bad_start"); resetRaw64(); return; }
      const char *c2 = strchr(c1 + 1, ':');
      if (!c2) { sendError(id, "raw64_bad_start"); resetRaw64(); return; }
      long expectedChunks = atol(h);
      long expectedLength = atol(c1 + 1);
      if (expectedChunks <= 0 || expectedChunks > 65535L ||
          expectedLength < 0  || expectedLength > ARIA_MAX_RAW64_LENGTH) {
        sendError(id, "raw64_size_limit"); resetRaw64(); return;
      }
      raw64Buffer_          = "";
      raw64ExpectedChunks_  = (uint16_t)expectedChunks;
      raw64ExpectedLength_  = (size_t)expectedLength;
      raw64ReceivedChunks_  = 0;
      raw64NextChunk_       = 0;
      strncpy(raw64Checksum_, c2 + 1, 8);
      raw64Checksum_[8]     = '\0';
    } else if (strncmp(payload, "chunk:", 6) == 0) {
      const char *sep = strchr(payload + 6, ':');
      if (!sep) { sendError(id, "raw64_bad_chunk"); return; }
      if (!raw64ExpectedChunks_) { sendError(id, "raw64_chunk_without_start"); return; }
      long index = atol(payload + 6);
      if (index != raw64NextChunk_) { sendError(id, "raw64_sequence_error"); resetRaw64(); return; }
      const char *chunk = sep + 1;
      size_t chunkLen = strlen(chunk);
      if (raw64Buffer_.length() + chunkLen > raw64ExpectedLength_) {
        sendError(id, "raw64_length_overflow"); resetRaw64(); return;
      }
      raw64Buffer_ += chunk;
      raw64ReceivedChunks_++;
      raw64NextChunk_++;
    } else if (strncmp(payload, "end:", 4) == 0 || strcmp(payload, "end:") == 0) {
      char cs[9];
      computeChecksum(raw64Buffer_, cs);
      if (raw64ExpectedLength_ == raw64Buffer_.length() &&
          (raw64Checksum_[0] == '\0' || strcmp(cs, raw64Checksum_) == 0) &&
          (!raw64ExpectedChunks_ || raw64ReceivedChunks_ == raw64ExpectedChunks_)) {
        *(String *)v->ptr = raw64Buffer_;
        writeValue('V', id, v);
      } else {
        sendError(id, "raw64_integrity_error");
      }
      resetRaw64();
    } else {
      sendError(id, "raw64_bad_frame");
    }
    return;
  }
}

String AriaClass::getArgString() {
  uint8_t bufLen = strlen(argBuf_);
  if (argumentOffset_ >= bufLen) return "";
  const char *start = argBuf_ + argumentOffset_;
  const char *comma = strchr(start, ',');
  uint8_t tokenLen = comma ? (uint8_t)(comma - start) : (uint8_t)(bufLen - argumentOffset_);
  // Arduino WString has no String(const char*, size_t) ctor — use a stack buffer.
  char tokBuf[48];
  if (tokenLen >= sizeof(tokBuf)) tokenLen = sizeof(tokBuf) - 1;
  memcpy(tokBuf, start, tokenLen);
  tokBuf[tokenLen] = '\0';
  argumentOffset_ += tokenLen + (comma ? 1 : 0);
  String token(tokBuf);
  if (token.length() > 1 && token.charAt(1) == ':') return token.substring(2);
  return token;
}

int   AriaClass::getArgInt()   { return getArgString().toInt(); }
float AriaClass::getArgFloat() { return getArgString().toFloat(); }

void AriaClass::sendError(const char *id, const char *reason) {
  if (!serial_) return;
  serial_->print(F("E:"));
  serial_->print(version_);
  serial_->print('|');
  // Avoid mixing const char* and __FlashStringHelper* in a ternary (yields void*).
  if (id)     serial_->print(id);       else serial_->print(F("protocol"));
  serial_->print('|');
  if (reason) serial_->print(reason);   else serial_->print(F("error"));
  serial_->print('\n');
}

void AriaClass::resetRaw64() {
  raw64Buffer_         = "";
  raw64ExpectedLength_ = 0;
  raw64ExpectedChunks_ = 0;
  raw64ReceivedChunks_ = 0;
  raw64NextChunk_      = 0;
  raw64Checksum_[0]    = '\0';
}

// ── Lookup helpers ───────────────────────────────────────────────────────────

AriaActionEntry *AriaClass::findAction(const char *id) {
  for (uint8_t i = 0; i < actionsCount_; i++) {
    if (!strncmp(actions_[i].id, id, ARIA_ID_MAXLEN)) return &actions_[i];
  }
  return nullptr;
}

AriaVarEntry *AriaClass::findVar(const char *id) {
  for (uint8_t i = 0; i < varsCount_; i++) {
    if (!strncmp(vars_[i].id, id, ARIA_ID_MAXLEN)) return &vars_[i];
  }
  return nullptr;
}

AriaEventEntry *AriaClass::findEvent(const char *id) {
  for (uint8_t i = 0; i < eventsCount_; i++) {
    if (!strncmp(events_[i].id, id, ARIA_ID_MAXLEN)) return &events_[i];
  }
  return nullptr;
}
