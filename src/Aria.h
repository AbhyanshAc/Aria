#ifndef ARIA_H
#define ARIA_H

#include <Arduino.h>

// ── Protocol & Library Info ──────────────────────────────────────────────────
#define ARIA_PROTOCOL_VERSION 2
#define ARIA_LIBRARY_VERSION "2.0.0"

// ── Memory-optimised defaults for 2 KB SRAM AVR (UNO / Nano) ────────────────
// Override BEFORE #include "Aria.h" to expand for larger boards (ESP32, Mega).
// Reducing these is the biggest lever for saving RAM on ATmega328P.
#ifndef ARIA_MAX_ACTIONS
#define ARIA_MAX_ACTIONS   3      // UNO default; override for larger boards
#endif
#ifndef ARIA_MAX_VARS
#define ARIA_MAX_VARS      5      // UNO default; override for larger boards
#endif
#ifndef ARIA_MAX_EVENTS
#define ARIA_MAX_EVENTS    3      // UNO default; override for larger boards
#endif
#ifndef ARIA_MAX_RAW64_LENGTH
#define ARIA_MAX_RAW64_LENGTH 256 // bounded heap use for incoming raw64 frames
#endif
#ifndef ARIA_LINE_BUFFER_SIZE
#define ARIA_LINE_BUFFER_SIZE 192 // fits a 128-character raw64 chunk plus framing
#endif
#ifndef ARIA_RAW64_CHUNK_SIZE
#define ARIA_RAW64_CHUNK_SIZE 128
#endif
#define ARIA_ID_MAXLEN    16      // was 24 — saves 8 B per entry across all tables
#define ARIA_NAME_MAXLEN  24      // was 32

// ── Message type letters ─────────────────────────────────────────────────────
//  H  = Handshake       (phone → device, device → phone)
//  A  = Action call      (phone → device)
//  S  = Set variable     (phone → device)
//  V  = Variable value   (device → phone, phone → device for request)
//  R  = Result           (device → phone)
//  E  = Error            (device → phone)
//  N  = Notification / Event  (device → phone, async push)
//  D  = Discovery query  (phone → device, device → phone)
//  B  = Binary / raw64   (bidirectional)
//  Q  = AI question      (device → phone)
//  P  = AI reply         (phone → device)

// ── Types ────────────────────────────────────────────────────────────────────
typedef void (*AriaActionHandler)(const char *arguments);
typedef void (*AriaNoArgActionHandler)();
typedef void (*AriaAIResponseHandler)(const char *requestId, const char *response);

// uint8_t backing saves 1–3 B per AriaVarEntry on AVR
enum AriaVarType : uint8_t {
  ARIA_BOOL,
  ARIA_INT,
  ARIA_FLOAT,
  ARIA_STRING,
  ARIA_JSON,
  ARIA_RAW64
};

struct AriaActionEntry {
  char id[ARIA_ID_MAXLEN];
  AriaActionHandler handler;
  AriaNoArgActionHandler noArgHandler;
  bool noArgs;
};

struct AriaVarEntry {
  char id[ARIA_ID_MAXLEN];
  AriaVarType type;
  void *ptr;
  float min;
  float max;
  bool bounded;
};

struct AriaEventEntry {
  char id[ARIA_ID_MAXLEN];
};

// ── Main class ───────────────────────────────────────────────────────────────
class AriaClass {
public:
  AriaClass();

  // Lifecycle
  void begin(Stream &serial, const char *deviceId = "aria",
             uint8_t protocolVersion = ARIA_PROTOCOL_VERSION);
  void loop();

  // Device Information
  void device(const char *name, const char *firmwareVersion = "1.0.0");

  // Registration
  bool registerAction(const char *id, AriaActionHandler handler);
  bool registerAction(const char *id, AriaNoArgActionHandler handler);
  bool reg(const char *id, AriaActionHandler handler) { return registerAction(id, handler); }

  // Variable registration (overloads by type)
  bool variable(const char *id, bool *ptr);
  bool variable(const char *id, int *ptr, int minVal, int maxVal);
  bool variable(const char *id, float *ptr, float minVal, float maxVal);
  bool variable(const char *id, String *ptr);
  bool variableJson(const char *id, String *ptr);
  bool variableRaw64(const char *id, String *ptr);

  // Event registration & emission
  bool event(const char *id);
  void raiseEvent(const char *id);
  void raiseEvent(const char *id, const char *payload);

  // Action results (sent back to phone)
  void result(const char *actionId, bool value);
  void result(const char *actionId, int value);
  void result(const char *actionId, float value);
  void result(const char *actionId, const String &value, AriaVarType type = ARIA_STRING);

  // Push current variable value to phone
  void sync(const char *variableId);

  // Binary data (raw64 / base64)
  void sendRaw64(const char *id, const uint8_t *data, size_t length);

  // Ask the companion app's configured AI. Callback strings are temporary and
  // must be copied if they need to be retained.
  bool askAI(const char *question, const char *requestId = "ask");
  void onAIResponse(AriaAIResponseHandler handler) { aiResponseHandler_ = handler; }

  // Getters
  uint8_t getProtocolVersion() const { return version_; }
  const char *getDeviceId() const { return deviceId_; }
  int getArgInt();
  float getArgFloat();
  String getArgString();

private:
  Stream  *serial_;
  uint8_t  version_;
  char     lineBuf_[ARIA_LINE_BUFFER_SIZE];
  uint16_t lineLength_;
  bool     discardingLine_;
  AriaAIResponseHandler aiResponseHandler_;
  char deviceId_[ARIA_ID_MAXLEN];
  char deviceName_[ARIA_NAME_MAXLEN];
  char firmwareVersion_[ARIA_NAME_MAXLEN];

  // Fixed-size arg buffer: avoids heap String for action payloads (saves ~12 B heap header + fragmentation)
  char    argBuf_[32];
  uint8_t argumentOffset_;

  // raw64 reassembly
  String   raw64Buffer_;          // heap-allocated but bounded by ARIA_MAX_RAW64_LENGTH
  uint16_t raw64ExpectedChunks_;
  uint16_t raw64ReceivedChunks_;
  uint16_t raw64NextChunk_;
  char     raw64Checksum_[9];     // was String — saves 12 B + heap; just needs 8 hex digits + NUL
  size_t   raw64ExpectedLength_;

  AriaActionEntry actions_[ARIA_MAX_ACTIONS];
  uint8_t actionsCount_;

  AriaVarEntry vars_[ARIA_MAX_VARS];
  uint8_t varsCount_;

  AriaEventEntry events_[ARIA_MAX_EVENTS];
  uint8_t eventsCount_;

  // Internal helpers
  bool addVariable(const char *id, AriaVarType type, void *ptr,
                   float min = 0, float max = 0, bool bounded = false);

  // handleLine takes a raw char buffer to avoid constructing a temporary String
  void handleLine(const char *line, uint16_t len);
  void handleHandshake();
  void handleDiscovery();
  void writeValue(char messageType, const char *id, AriaVarEntry *entry);
  void sendRaw64Encoded(const char *id, const String &encoded);
  void computeChecksum(const String &value, char out[9]); // writes 8-hex FNV hash into out
  void sendError(const char *id, const char *reason);
  void resetRaw64();
  // Prints "T:version|id|" prefix — avoids repeating F() strings
  void printHeader(char type, const char *id);

  AriaActionEntry *findAction(const char *id);
  AriaVarEntry *findVar(const char *id);
  AriaEventEntry *findEvent(const char *id);
};

extern AriaClass ARIA;

#endif
