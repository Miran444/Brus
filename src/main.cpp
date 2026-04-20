#include <Arduino.h>
#include "inputs.h"
#include "outputs.h"
#include "config.h"

// Globalni objekti
BrusInputs inputs;
BrusOutputs outputs;

// Časovniki
unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL = 1000; // Izpis vsako sekundo

void setup() {
  // Inicializacija Serial komunikacije
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=================================");
  Serial.println("  BRUS - KNECHT WEMA USK 200/E  ");
  Serial.println("  ESP32-S3 Kontroler v1.0       ");
  Serial.println("=================================");
  
  // Inicializacija izhodov (NAJPREJ - varno stanje)
  outputs.begin();
  
  // Inicializacija vhodov
  inputs.begin();
  
  Serial.println("Sistem pripravljen!");
  Serial.println();
}

void loop() {
  // Posodobi vhode
  inputs.update();
  
  // Trenutni način delovanja
  S1Mode mode = inputs.getS1Mode();
  
  // ===== ROČNI NAČIN =====
  if (mode == MODE_MANUAL) {
    // Kontrola vretena s tipkama S41/S42
    if (inputs.isS41DownPressed()) {
      outputs.moveSpindleDown(SPINDLE_SPEED_MEDIUM);
    } 
    else if (inputs.isS42UpPressed()) {
      outputs.moveSpindleUp(SPINDLE_SPEED_MEDIUM);
    } 
    else {
      // Če nobena tipka ni pritisnjena, ustavi vreteno
      if (outputs.isSpindleMoving()) {
        outputs.stopSpindle();
      }
    }
  }
  
  // ===== AVTOMATSKI NAČIN =====
  else if (mode == MODE_AUTO) {
    // TODO: Implementacija avtomatskega cikla
    // - Preberi število ciklov iz S2
    // - Izvajaj sekvenco: vklop motorja, črpalke, premik noža, itd.
  }
  
  // ===== NAČIN OFF =====
  else if (mode == MODE_OFF) {
    // V OFF načinu vse ustavi
    if (outputs.isGrindingMotorOn() || outputs.isWaterPumpOn() || 
        outputs.isKnifePusherOn() || outputs.isSpindleMoving()) {
      outputs.emergencyStop();
    }
  }
  
  // ===== RESET TIPKA =====
  if (inputs.isResetPressed()) {
    outputs.emergencyStop();
    inputs.resetRevolutions();
    Serial.println("*** RESET ***");
  }
  
  // ===== TEMPERATURA ALARM =====
  if (inputs.isTempAlarm()) {
    outputs.emergencyStop();
    Serial.println("!!! TEMPERATURA ALARM - SISTEM USTAVLJEN !!!");
  }
  
  // ===== PERIODIČEN DEBUG IZPIS =====
  if (millis() - lastPrintTime > PRINT_INTERVAL) {
    lastPrintTime = millis();
    
    Serial.print("Mode: ");
    switch(mode) {
      case MODE_OFF:    Serial.print("OFF     "); break;
      case MODE_MANUAL: Serial.print("MANUAL  "); break;
      case MODE_AUTO:   Serial.print("AUTO    "); break;
    }
    
    Serial.print("| Cycles: ");
    S2Cycles cycles = inputs.getS2Cycles();
    if (cycles == CYCLES_CONTINUOUS) {
      Serial.print("CONT");
    } else {
      Serial.print(cycles);
      Serial.print("   ");
    }
    
    Serial.print(" | Rev: ");
    Serial.print(inputs.getRevolutions());
    Serial.print("  ");
    
    // Prikaz aktivnih komponent
    if (outputs.isGrindingMotorOn()) Serial.print("[MOTOR] ");
    if (outputs.isWaterPumpOn()) Serial.print("[PUMP] ");
    if (outputs.isKnifePusherOn()) Serial.print("[KNIFE] ");
    if (outputs.isSpindleMoving()) {
      Serial.print("[SPINDLE ");
      Serial.print(outputs.getSpindleDirection() == SPINDLE_UP ? "UP" : "DN");
      Serial.print(":"); 
      Serial.print(outputs.getSpindleSpeed());
      Serial.print("] ");
    }
    
    // Senzorji in alarmi
    if (inputs.isSpindleTilted()) Serial.print("[TILT<10°] ");
    if (inputs.isTempAlarm()) Serial.print("[TEMP!] ");
    
    Serial.println();
  }
  
  delay(10); // Kratka zakasnitev za stabilnost
}
