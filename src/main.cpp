#include <Arduino.h>
#include "inputs.h"
#include "outputs.h"
#include "auto_cycle.h"
#include "config.h"

// Globalni objekti
BrusInputs inputs;
BrusOutputs outputs;
AutoCycle autoCycle(&inputs, &outputs);

// Časovniki
unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL = 1000; // Izpis vsako sekundo

// Stanje sistema
S1Mode lastMode = MODE_OFF;
bool autoModeActive = false;

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
  
  // Inicializacija avtomatskega cikla
  autoCycle.begin();
  
  Serial.println("Sistem pripravljen!");
  Serial.println("ROČNI NAČIN: Nastavite začetni kot (22-27°) in pritisnite RESET");
  Serial.println();
}

void loop() {
  // Posodobi vhode
  inputs.update();
  
  // Trenutni način delovanja
  S1Mode mode = inputs.getS1Mode();
  
  // Detekcija spremembe načina
  if (mode != lastMode) {
    Serial.print("Sprememba načina: ");
    if (mode == MODE_OFF) Serial.println("OFF");
    else if (mode == MODE_MANUAL) Serial.println("MANUAL");
    else if (mode == MODE_AUTO) Serial.println("AUTO");
    
    // Ob spremembi iz AUTO v karkoli drugega ustavi cikel
    if (lastMode == MODE_AUTO && mode != MODE_AUTO) {
      autoCycle.stop();
      autoModeActive = false;
    }
    
    lastMode = mode;
  }
  
  // ===== NAČIN OFF =====
  if (mode == MODE_OFF) {
    // V OFF načinu vse ustavi
    if (outputs.isGrindingMotorOn() || outputs.isWaterPumpOn() || 
        outputs.isKnifePusherOn() || outputs.isSpindleMoving()) {
      outputs.emergencyStop();
    }
    if (autoCycle.isRunning()) {
      autoCycle.stop();
    }
    autoModeActive = false;
  }
  
  // ===== ROČNI NAČIN =====
  else if (mode == MODE_MANUAL) {
    // Kontrola vretena s tipkama S41/S42 (za nastavitev začetnega kota)
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
    
    // V ROČNEM načinu so ostali izhodi onemogočeni
    if (outputs.isGrindingMotorOn() || outputs.isWaterPumpOn() || outputs.isKnifePusherOn()) {
      outputs.setGrindingMotor(false);
      outputs.setWaterPump(false);
      outputs.setKnifePusher(false);
    }
    
    autoModeActive = false;
  }
  
  // ===== AVTOMATSKI NAČIN =====
  else if (mode == MODE_AUTO) {
    // Posodobi avtomatski cikel
    autoCycle.update();
    
    // Če še ni aktiven, začni cikel glede na S2 nastavitev
    if (!autoModeActive && !autoCycle.isRunning()) {
      S2Cycles cycles = inputs.getS2Cycles();
      
      if (cycles == CYCLES_CONTINUOUS) {
        autoCycle.startContinuous();
        autoModeActive = true;
      } 
      else if (cycles >= CYCLES_1 && cycles <= CYCLES_6) {
        autoCycle.start(cycles);
        autoModeActive = true;
      }
      else {
        // S2 ni nastavljen na veljavno pozicijo
        Serial.println("OPOZORILO: Nastavite število ciklov na S2!");
      }
    }
  }
  
  // ===== RESET TIPKA =====
  if (inputs.isResetPressed()) {
    if (mode == MODE_MANUAL) {
      // V ROČNEM načinu: reset števca obratov (potrditev začetnega kota)
      inputs.resetRevolutions();
      Serial.println("*** RESET - Števec obratov ponastavljen ***");
      Serial.println("Začetni kot potrjen! Preklopite na AUTO za začetek.");
    } else {
      // V ostalih načinih: emergency stop
      outputs.emergencyStop();
      autoCycle.stop();
      Serial.println("*** RESET - EMERGENCY STOP ***");
    }
    
    delay(500); // Debounce za tipko
  }
  
  // ===== TEMPERATURA ALARM =====
  if (inputs.isTempAlarm()) {
    outputs.emergencyStop();
    autoCycle.stop();
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
    
    if (mode == MODE_AUTO && autoCycle.isRunning()) {
      Serial.print("| ");
      autoCycle.printStatus();
    } else {
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
  }
  
  delay(10); // Kratka zakasnitev za stabilnost
}
