#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>

// WiFi credentials and Pi URL live in secrets.h (gitignored).
// Copy secrets.h.template → secrets.h and fill in your values.
#include "secrets.h"

// UART pins to STM32 — match your physical wiring.
// If using the same wiring as the old hardcoded sketch: RX=3, TX=1
// Current breadboard wiring (seq_test_A): RX=16, TX=17
#define STM_UART_RX  16
#define STM_UART_TX  17

#define START_BYTE   0xAA
#define END_BYTE     0x55
#define SHA256_SIZE  32    // bytes appended to .bin by the Pi compiler
#define META_SIZE    40    // SequenceMetadata_t: version(1) + step_count(1) + seq_name(16) + part_num(12) + machine_id(8) + reserved(2)
#define MAX_PAYLOAD  300   // max total fetched size (meta + blob + SHA256_SIZE)
#define STEP_SIZE    5     // sizeof(SequenceStep_t): uint8 relay_mask + uint32 duration_ms

HardwareSerial SerialSTM(1);  // UART1

// --- forward declarations ---
bool fetchAndSend();
void uploadLogs();
void forwardHeartbeat(const String& line);
void lookupEmployee(const String& line);
bool postJson(const String& url, const String& body);
void flushPendingLogs();

// In-memory buffer for one log batch that failed to POST (e.g. brief WiFi blip).
// Flushed automatically at the start of the next upload attempt.
static String pendingLogs = "";

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== ESP32 WiFi Sequence Bridge ===");

    SerialSTM.begin(115200, SERIAL_8N1, STM_UART_RX, STM_UART_TX);
    Serial.printf("STM32 UART: RX=GPIO%d TX=GPIO%d\n", STM_UART_RX, STM_UART_TX);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nWiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.println("Waiting for REQUEST_SEQUENCE from STM32...");
}

void loop() {
    if (SerialSTM.available()) {
        // readStringUntil blocks up to 1s for '\n' — fine since STM32 sends full line atomically
        String cmd = SerialSTM.readStringUntil('\n');
        cmd.trim();  // strips the trailing \r

        Serial.printf("[BRIDGE] STM32: '%s'\n", cmd.c_str());

        if (cmd == "REQUEST_SEQUENCE") {
            Serial.println("[BRIDGE] Fetching sequence from Pi...");
            if (!fetchAndSend()) {
                Serial.println("[BRIDGE] Fetch/send FAILED");
            }
        } else if (cmd == "UPLOAD_LOGS") {
            Serial.println("[BRIDGE] Receiving log upload from STM32...");
            uploadLogs();
        } else if (cmd.startsWith("HEARTBEAT")) {
            forwardHeartbeat(cmd);
        } else if (cmd.startsWith("LOOKUP_EMPLOYEE")) {
            lookupEmployee(cmd);
        }
    }
    delay(10);
}

bool fetchAndSend() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[BRIDGE] WiFi not connected");
        return false;
    }

    HTTPClient http;
    http.begin(PI_SEQUENCE_URL);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[BRIDGE] HTTP error: %d\n", httpCode);
        http.end();
        return false;
    }

    // Read response body — handles both Content-Length and chunked responses
    uint8_t payload[MAX_PAYLOAD];
    int payloadLen = 0;
    int contentLen = http.getSize();  // -1 if chunked/unknown
    WiFiClient *stream = http.getStreamPtr();

    if (contentLen > 0 && contentLen <= MAX_PAYLOAD) {
        // Known length — read exactly that many bytes
        payloadLen = stream->readBytes(payload, contentLen);
    } else {
        // Unknown/chunked — read until closed or buffer full
        uint32_t deadline = millis() + 3000;
        while (millis() < deadline && payloadLen < MAX_PAYLOAD) {
            if (stream->available()) {
                payload[payloadLen++] = (uint8_t)stream->read();
            } else if (!http.connected()) {
                break;
            }
        }
    }
    http.end();

    if (payloadLen < META_SIZE + SHA256_SIZE + STEP_SIZE) {
        Serial.printf("[BRIDGE] Response too short: %d bytes\n", payloadLen);
        return false;
    }

    // Pi output: meta(20B) + blob + sha256(meta+blob, 32B) — split them here
    int blobLen = payloadLen - META_SIZE - SHA256_SIZE;
    if (blobLen % STEP_SIZE != 0) {
        Serial.printf("[BRIDGE] Blob length %d is not a multiple of %d — corrupt?\n",
                      blobLen, STEP_SIZE);
        return false;
    }

    Serial.printf("[BRIDGE] %d bytes meta + %d bytes blob (%d steps) + 32 bytes hash\n",
                  META_SIZE, blobLen, blobLen / STEP_SIZE);

    // Frame: [ 0xAA | BLOB_LEN | meta(20B) | blob(blobLen B) | hash(32B) | 0x55 ]
    // BLOB_LEN covers blob only — STM32 uses it to locate hash start.
    // Meta and hash are forwarded as-is from the Pi .bin file.
    // 12ms inter-byte delay — STM32 scan cycle is HAL_Delay(10).
    uint8_t b;
    b = START_BYTE;          SerialSTM.write(b); delay(12);
    b = (uint8_t)blobLen;    SerialSTM.write(b); delay(12);
    for (int i = 0; i < payloadLen; i++) {   // sends meta + blob + hash contiguously
        SerialSTM.write(payload[i]);
        delay(12);
    }
    b = END_BYTE;            SerialSTM.write(b); delay(12);

    Serial.printf("[BRIDGE] Frame sent: AA %02X [%d meta + %d blob + 32 hash] 55\n",
                  blobLen, META_SIZE, blobLen);
    return true;
}

void forwardHeartbeat(const String& line) {
    // Parse "HEARTBEAT|tick_ms|state|seq_name|fault"
    // Field indices after splitting on '|': 0=HEARTBEAT 1=tick 2=state 3=seq 4=fault
    int p1 = line.indexOf('|');
    int p2 = line.indexOf('|', p1 + 1);
    int p3 = line.indexOf('|', p2 + 1);
    int p4 = line.indexOf('|', p3 + 1);

    if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) {
        Serial.printf("[BRIDGE] Malformed heartbeat: '%s'\n", line.c_str());
        return;
    }

    String tick    = line.substring(p1 + 1, p2);
    String state   = line.substring(p2 + 1, p3);
    String seqName = line.substring(p3 + 1, p4);
    String fault   = line.substring(p4 + 1);

    Serial.printf("[BRIDGE] HB tick=%s state=%s seq=%s fault=%s\n",
                  tick.c_str(), state.c_str(), seqName.c_str(), fault.c_str());

    if (WiFi.status() != WL_CONNECTED) return;

    String body = "{\"tick\":"  + tick    +
                  ",\"state\":" + state   +
                  ",\"seq\":\"" + seqName + "\"" +
                  ",\"fault\":" + fault   + "}";

    HTTPClient http;
    http.begin(PI_HEARTBEAT_URL);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    if (code != HTTP_CODE_OK) {
        Serial.printf("[BRIDGE] Heartbeat POST failed: HTTP %d\n", code);
    }
    http.end();
}


// POST a JSON body to a URL. Returns true on HTTP 200.
bool postJson(const String& url, const String& body) {
    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    http.end();
    return code == HTTP_CODE_OK;
}

// Retry any log batch that failed to POST in a previous cycle.
void flushPendingLogs() {
    if (pendingLogs.length() == 0) return;
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.println("[BRIDGE] Flushing pending logs...");
    if (postJson(PI_LOG_URL, pendingLogs)) {
        Serial.println("[BRIDGE] Pending logs flushed OK");
        pendingLogs = "";
    } else {
        Serial.println("[BRIDGE] Pending logs flush failed, will retry");
    }
}

void uploadLogs() {
    flushPendingLogs();  // deliver any previously buffered batch first

    String body = "[";
    bool first = true;
    uint32_t deadline = millis() + 5000;  // 5s to receive all lines

    while (millis() < deadline) {
        if (!SerialSTM.available()) continue;

        String line = SerialSTM.readStringUntil('\n');
        line.trim();

        if (line == "LOGS_END") break;
        if (line.length() == 0)  continue;

        // Parse "timestamp_ms,tier,event_code,data"
        int c1 = line.indexOf(',');
        int c2 = line.indexOf(',', c1 + 1);
        int c3 = line.indexOf(',', c2 + 1);
        if (c1 < 0 || c2 < 0 || c3 < 0) {
            Serial.printf("[BRIDGE] Malformed log line: '%s'\n", line.c_str());
            continue;
        }

        if (!first) body += ",";
        body += "{\"ts\":"    + line.substring(0, c1) +
                ",\"tier\":"  + line.substring(c1 + 1, c2) +
                ",\"event\":" + line.substring(c2 + 1, c3) +
                ",\"data\":"  + line.substring(c3 + 1) + "}";
        first = false;
    }
    body += "]";

    if (first) {
        Serial.println("[BRIDGE] Log upload: no events");
        return;
    }

    if (WiFi.status() != WL_CONNECTED || !postJson(PI_LOG_URL, body)) {
        Serial.println("[BRIDGE] Log upload failed — buffering for retry");
        if (pendingLogs.length() == 0) {
            pendingLogs = body;  // preserve most-recent failed batch
        }
    } else {
        Serial.printf("[BRIDGE] Log upload OK (%d bytes)\n", body.length());
    }
}

void lookupEmployee(const String& line) {
    // Parse "LOOKUP_EMPLOYEE|1001"
    int p = line.indexOf('|');
    if (p < 0) {
        Serial.println("[BRIDGE] Malformed LOOKUP_EMPLOYEE");
        return;
    }
    String empId = line.substring(p + 1);
    empId.trim();

    Serial.printf("[BRIDGE] Looking up employee %s\n", empId.c_str());

    if (WiFi.status() != WL_CONNECTED) {
        // No connection — send a fallback so the display still updates
        SerialSTM.print("EMPLOYEE_NAME|Employee " + empId + "\r\n");
        return;
    }

    HTTPClient http;
    http.begin(String(PI_EMPLOYEE_URL) + "/" + empId);
    int code = http.GET();

    if (code == HTTP_CODE_OK) {
        String payload = http.getString();
        // Extract "name" field without pulling in a JSON library
        int nameStart = payload.indexOf("\"name\":\"");
        if (nameStart >= 0) {
            nameStart += 8;
            int nameEnd = payload.indexOf("\"", nameStart);
            if (nameEnd >= 0) {
                String name = payload.substring(nameStart, nameEnd);
                SerialSTM.print("EMPLOYEE_NAME|" + name + "\r\n");
                Serial.printf("[BRIDGE] Employee %s → %s\n", empId.c_str(), name.c_str());
                http.end();
                return;
            }
        }
    }

    // Lookup failed — fall back to ID so display still shows something
    Serial.printf("[BRIDGE] Employee lookup failed: HTTP %d\n", code);
    SerialSTM.print("EMPLOYEE_NAME|Employee " + empId + "\r\n");
    http.end();
}
