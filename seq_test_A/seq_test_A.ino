#include <HardwareSerial.h>

HardwareSerial SerialSTM(1);   // UART1

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 UART1 LOOPBACK TEST ===");

  SerialSTM.begin(115200, SERIAL_8N1, 16, 17);  // RX=16, TX=17
}

void loop() {
  const uint8_t test_frame[] = {0xAA, 0x0A, 0x01, 0xE8, 0x03, 0x00, 0x00, 0x02, 0xF4, 0x01, 0x00, 0x00, 0x55};

  SerialSTM.write(test_frame, sizeof(test_frame));
  Serial.println("Sent full 13-byte frame on UART1");
  for (int i = 0; i < sizeof(test_frame); i++) {
    Serial.printf("%02X ", test_frame[i]);
  }
Serial.println();
  SerialSTM.flush();

  Serial.println("Sent test frame on UART1 TX (GPIO17)");

  // Read back what was looped (if you short GPIO17 TX → GPIO16 RX)
  while (SerialSTM.available()) {
    uint8_t b = SerialSTM.read();
    Serial.printf("Loopback received: 0x%02X\n", b);
  }

  delay(2000);
}