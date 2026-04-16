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
#define MAX_PAYLOAD  250   // max sequence blob size (50 steps * 5 bytes)
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

    if (payloadLen == 0) {
        Serial.println("[BRIDGE] Empty response from Pi");
        return false;
    }
    if (payloadLen % STEP_SIZE != 0) {
        Serial.printf("[BRIDGE] Payload length %d is not a multiple of %d — corrupt?\n",
                      payloadLen, STEP_SIZE);
        return false;
    }

    Serial.printf("[BRIDGE] %d bytes = %d steps — sending frame to STM32\n",
                  payloadLen, payloadLen / STEP_SIZE);

    // Print hex dump of what we're about to send
    Serial.printf("[BRIDGE] Sending frame (%d bytes): ", payloadLen + 3);
    Serial.printf("%02X %02X ", START_BYTE, (uint8_t)payloadLen);
    for (int i = 0; i < payloadLen; i++) Serial.printf("%02X ", payload[i]);
    Serial.printf("%02X\n", END_BYTE);

    // Frame: [ 0xAA | LEN | payload... | 0x55 ]
    // 12ms inter-byte delay — STM32 scan cycle is HAL_Delay(10), so each byte
    // lands during a separate SupervisorComms_Task call.
    uint8_t b;
    b = START_BYTE;           SerialSTM.write(b); Serial.printf("[TX] 0x%02X\n", b); delay(12);
    b = (uint8_t)payloadLen;  SerialSTM.write(b); Serial.printf("[TX] 0x%02X\n", b); delay(12);
    for (int i = 0; i < payloadLen; i++) {
        b = payload[i];
        SerialSTM.write(b);
        Serial.printf("[TX] 0x%02X\n", b);
        delay(12);
    }
    b = END_BYTE;             SerialSTM.write(b); Serial.printf("[TX] 0x%02X\n", b); delay(12);

    Serial.println("[BRIDGE] Frame send complete");
    return true;
}
