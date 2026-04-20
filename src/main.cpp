#include <Arduino.h>

void setup() {
  // Inicializacija Serial komunikacije
  Serial.begin(115200);
  delay(1000);
  
  // Vgrajena LED (prilagodite pin glede na vašo ploščico)
  pinMode(LED_BUILTIN, OUTPUT);
  
  Serial.println("Brus ESP32-S3 projekt zagnan!");
}

void loop() {
  // Utripanje LED
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("LED ON");
  delay(1000);
  
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("LED OFF");
  delay(1000);
}
