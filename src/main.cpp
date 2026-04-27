#include <Arduino.h>
#include "inputs.h"
#include "outputs.h"
#include "auto_cycle.h"
#include "nextion.h"
#include "as5600.h"
#include "config.h"

// Globalni objekti
BrusInputs inputs;
BrusOutputs outputs;
AutoCycle autoCycle(&inputs, &outputs);
NextionDisplay display;

// Časovniki
unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL = 1000; // Izpis vsako sekundo

// Stanje sistema
S1Mode lastMode = MODE_OFF;
bool autoModeActive = false;

// Stanje toggle gumbov (manual control)
bool brusActive = false;     // bBrus - motor kamna
bool pnevActive = false;     // bPnev - ventil noža
bool bGorPressed = false;    // bGor - vreteno gor (pritisnjeno)
bool bDolPressed = false;    // bDol - vreteno dol (pritisnjeno)

// AS5600 senzor kota
AS5600 angleSensor;

// Deklaracije funkcij
void handleNextionEvents();
void handleTouchPress(uint8_t componentId);
void handleTouchRelease(uint8_t componentId);

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
  
  // Inicializacija AS5600 senzorja
  Wire.begin(I2C_SDA, I2C_SCL);
  if (angleSensor.begin(&Wire)) {
    Serial.println("AS5600 senzor inicializiran");
  } else {
    Serial.println("NAPAKA: AS5600 senzor ni najden!");
  }
  
  // Inicializacija Nextion displaya
  display.begin();
  
  // Nastavi začetno stanje gumbov
  display.setBrusState(false);
  display.setPnevState(false);
  
  Serial.println("Sistem pripravljen!");
  Serial.println("ROČNI NAČIN: Nastavite začetni kot (22-27°) in pritisnite RESET");
  Serial.println();
}

// ===== OBDELAVA NEXTION TOUCH EVENTS =====
void handleNextionEvents() {
  while (display.available()) {
    // Preberi prvi byte da ugotovimo tip dogodka
    uint8_t eventType = Serial2.peek();
    
    if (eventType == 0x65) {
      // Touch Press Event
      uint8_t pressId = display.readTouchEvent();
      if (pressId > 0) {
        handleTouchPress(pressId);
      }
    } 
    else if (eventType == 0x66) {
      // Touch Release Event
      uint8_t releaseId = display.readTouchReleaseEvent();
      if (releaseId > 0) {
        handleTouchRelease(releaseId);
      }
    }
    else {
      // Neznan dogodek - preberi in zavrzi
      Serial2.read();
    }
  }
}

void handleTouchPress(uint8_t componentId) {
  Serial.print("Touch Press: ");
  Serial.println(componentId);
  
  S1Mode mode = inputs.getS1Mode();
  
  // Akcije samo v MANUAL načinu
  if (mode != MODE_MANUAL) {
    Serial.println("Gumbi delujejo samo v MANUAL načinu!");
    return;
  }
  
  switch(componentId) {
    case 17:  // bGor - začni premik gor
      Serial.println("[bGor] Vreteno GOR - START");
      bGorPressed = true;
      outputs.moveSpindleUp(SPINDLE_SPEED_MEDIUM);
      break;
      
    case 18:  // bDol - začni premik dol
      Serial.println("[bDol] Vreteno DOL - START");
      bDolPressed = true;
      outputs.moveSpindleDown(SPINDLE_SPEED_MEDIUM);
      break;
      
    case 19:  // bBrus - toggle motor kamna
      brusActive = !brusActive;
      outputs.setGrindingMotor(brusActive);
      display.setBrusState(brusActive);
      Serial.print("[bBrus] Motor kamna: ");
      Serial.println(brusActive ? "ON" : "OFF");
      break;
      
    case 20:  // bPnev - toggle ventil noža
      pnevActive = !pnevActive;
      outputs.setKnifePusher(pnevActive);
      display.setPnevState(pnevActive);
      Serial.print("[bPnev] Ventil noža: ");
      Serial.println(pnevActive ? "ON" : "OFF");
      break;
      
    default:
      Serial.print("Neznan gumb ID: ");
      Serial.println(componentId);
      break;
  }
}

void handleTouchRelease(uint8_t componentId) {
  Serial.print("Touch Release: ");
  Serial.println(componentId);
  
  S1Mode mode = inputs.getS1Mode();
  
  // Akcije samo v MANUAL načinu
  if (mode != MODE_MANUAL) {
    return;
  }
  
  switch(componentId) {
    case 17:  // bGor - ustavi premik
      Serial.println("[bGor] Vreteno GOR - STOP");
      bGorPressed = false;
      outputs.stopSpindle();
      break;
      
    case 18:  // bDol - ustavi premik
      Serial.println("[bDol] Vreteno DOL - STOP");
      bDolPressed = false;
      outputs.stopSpindle();
      break;
      
    // bBrus (19) in bPnev (20) sta toggle gumba - brez Release akcije
    
    default:
      break;
  }
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Posodobi vhode
  inputs.update();
  
  // Posodobi AS5600 senzor
  angleSensor.update();
  
  // Posodobi prikaz kota na Nextion zaslonu
  display.setAngle(angleSensor.getCalibratedAngle());
  
  // Posodobi Nextion display
  display.update(currentMillis);
  
  // Obdelaj Touch Events iz Nextiona
  handleNextionEvents();
  
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
    // Kontrola vretena - fizične tipke (S41/S42) in Nextion gumbi (bGor/bDol) delujejo vzporedno
    bool wantMoveDown = inputs.isS41DownPressed() || bDolPressed;
    bool wantMoveUp = inputs.isS42UpPressed() || bGorPressed;
    
    if (wantMoveDown && !wantMoveUp) {
      // Premik dol (prioriteta ima DOL, če sta obe pritisnjena)
      if (!outputs.isSpindleMoving() || outputs.getSpindleDirection() != SPINDLE_DOWN) {
        outputs.moveSpindleDown(SPINDLE_SPEED_MEDIUM);
      }
    } 
    else if (wantMoveUp && !wantMoveDown) {
      // Premik gor
      if (!outputs.isSpindleMoving() || outputs.getSpindleDirection() != SPINDLE_UP) {
        outputs.moveSpindleUp(SPINDLE_SPEED_MEDIUM);
      }
    } 
    else {
      // Nobena kontrola ni aktivna - ustavi vreteno
      if (outputs.isSpindleMoving()) {
        outputs.stopSpindle();
      }
    }
    
    // V ROČNEM načinu motorji preko fizičnih tipk niso dovoljeni
    // (samo preko Nextion gumbov bBrus/bPnev)
    // Črpalka in nož ne smeta biti vklopljena v manual mode
    if (outputs.isWaterPumpOn()) {
      outputs.setWaterPump(false);
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
      else if (cycles >= CYCLES_2 && cycles <= CYCLES_7) {
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
      
      // Če je AS5600 prisoten, kalibriraj tudi encoder
      if (USE_AS5600_FOR_TILT && inputs.getAngleEncoder()->isSensorPresent()) {
        inputs.calibrateAngleZero();
      }
      
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
    
    // Serial output
    Serial.print("Mode: ");
    switch(mode) {
      case MODE_OFF:    Serial.print("OFF     "); break;
      case MODE_MANUAL: Serial.print("MANUAL  "); break;
      case MODE_AUTO:   Serial.print("AUTO    "); break;
    }
    
    // Nextion display update
    const char* modeStr = (mode == MODE_OFF) ? "OFF" : (mode == MODE_MANUAL) ? "MANUAL" : "AUTO";
    display.setMode(modeStr);
    
    if (mode == MODE_AUTO && autoCycle.isRunning()) {
      Serial.print("| ");
      autoCycle.printStatus();
      
      // Nextion - cikli
      display.setCycles(autoCycle.getCompletedCycles(), autoCycle.getTargetCycles());
    } else {
      Serial.print("| Cycles: ");
      S2Cycles cycles = inputs.getS2Cycles();
      if (cycles == CYCLES_CONTINUOUS) {
        Serial.print("CONT");
        display.setCycles(0, 99);  // 99 = continuous
      } else {
        Serial.print(cycles);
        Serial.print("   ");
        display.setCycles(0, cycles);
      }
      
      Serial.print(" | Rev: ");
      Serial.print(inputs.getRevolutions());
      Serial.print("  ");
      
      // Prikaz kota iz AS5600 (če je prisoten)
      if (USE_AS5600_FOR_TILT && inputs.getAngleEncoder()->isSensorPresent()) {
        float angle = inputs.getSpindleAngle();
        Serial.print("| Angle: ");
        Serial.print(angle, 1);
        Serial.print("° ");
        
        // Nextion - kot
        display.setAngle(angle);
      }
      
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
    
    // Nextion - število obratov
    // display.setRevolutions(inputs.getRevolutions());
    
    // Nextion - status motorjev
    // display.setMotorStatus(
    //   outputs.isGrindingMotorOn(),
    //   outputs.isWaterPumpOn(),
    //   outputs.isKnifePusherOn()
    // );
    
    // Nextion - status vretena
    display.setSpindleStatus(
      outputs.isSpindleMoving(),
      outputs.getSpindleDirection() == SPINDLE_UP,
      outputs.getSpindleSpeed()
    );
    
    // Nextion - alarmi
    // if (inputs.isTempAlarm()) {
    //   display.setAlarm("TEMPERATURA ALARM!");
    // } else if (autoCycle.hasError()) {
    //   display.setAlarm(autoCycle.getErrorMessage().c_str());
    // } else {
    //   display.clearAlarm();
    // }
  }
  
  delay(10); // Kratka zakasnitev za stabilnost
}
