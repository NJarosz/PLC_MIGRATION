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
#define MAX_PAYLOAD  250   // max total fetched size (blob + SHA256_SIZE)
#define STEP_SIZE    5     // sizeof(SequenceStep_t): uint8 relay_mask + uint32 duration_ms

HardwareSerial SerialSTM(1);  // UART1

// --- forward declaration ---
bool fetchAndSend();

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

    if (payloadLen < SHA256_SIZE + STEP_SIZE) {
        Serial.printf("[BRIDGE] Response too short: %d bytes\n", payloadLen);
        return false;
    }

    // Pi appends SHA-256(blob) to the .bin file — split them here
    int blobLen = payloadLen - SHA256_SIZE;
    if (blobLen % STEP_SIZE != 0) {
        Serial.printf("[BRIDGE] Blob length %d is not a multiple of %d — corrupt?\n",
                      blobLen, STEP_SIZE);
        return false;
    }

    Serial.printf("[BRIDGE] %d bytes blob (%d steps) + 32 bytes hash\n",
                  blobLen, blobLen / STEP_SIZE);

    // Frame: [ 0xAA | LEN | blob(blobLen bytes) | hash(32 bytes) | 0x55 ]
    // LEN covers blob only — STM32 uses it to know where blob ends and hash begins.
    // 12ms inter-byte delay — STM32 scan cycle is HAL_Delay(10).
    uint8_t b;
    b = START_BYTE;          SerialSTM.write(b); delay(12);
    b = (uint8_t)blobLen;    SerialSTM.write(b); delay(12);
    for (int i = 0; i < payloadLen; i++) {   // sends blob then hash contiguously
        SerialSTM.write(payload[i]);
        delay(12);
    }
    b = END_BYTE;            SerialSTM.write(b); delay(12);

    Serial.printf("[BRIDGE] Frame sent: AA %02X [%d blob + 32 hash] 55\n",
                  blobLen, blobLen);
    return true;
}
