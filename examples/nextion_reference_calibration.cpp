/**
 * Primer referenčnega hoda in kalibracije kotov
 * 
 * Funkcionalnost:
 * 1. Uporabnik pritisne "bRef" (ID=15) na Page 1
 * 2. ESP32 zažene avtomatski cikel in izmeri min/max kote
 * 3. Izmerjene vrednosti se shranijo kot tMaxAngle, tMinAngle v Preferences
 * 4. Tudi shranimo tRevPerAngle (obrati/stopinjo) in tCycleTime
 * 5. Te vrednosti se uporabijo za validacijo vnosov na Page 1
 */

#include <Arduino.h>
#include <Preferences.h>
#include "nextion.h"
#include "inputs.h"    // AS5600 sensor
#include "outputs.h"   // Spindle control

// Globalni objekti
NextionDisplay display;
Preferences preferences;
BrusInputs inputs;
BrusOutputs outputs;

// Referenčne vrednosti
struct ReferenceData {
    float maxAngle;      // Maksimalni kot (npr. 28.0°)
    float minAngle;      // Minimalni kot (npr. 5.0°)
    uint16_t revPerAngle; // Število obratov na stopinjo
    uint32_t cycleTime;   // Čas celotnega cikla v ms
};

ReferenceData refData;
bool referenceRunning = false;

void loadReferenceData() {
    preferences.begin("brus", false);
    
    refData.maxAngle = preferences.getFloat("maxAngle", 28.0);
    refData.minAngle = preferences.getFloat("minAngle", 5.0);
    refData.revPerAngle = preferences.getUShort("revPerAngle", 100);
    refData.cycleTime = preferences.getULong("cycleTime", 60000);
    
    preferences.end();
    
    Serial.println("Naloženi referenčni podatki:");
    Serial.printf("  Max kot: %.1f°\n", refData.maxAngle);
    Serial.printf("  Min kot: %.1f°\n", refData.minAngle);
    Serial.printf("  Obrati/°: %d\n", refData.revPerAngle);
    Serial.printf("  Čas cikla: %d s\n", refData.cycleTime / 1000);
    
    // Posodobi globalne spremenljivke v Nextion
    display.setGlobalVariable("vaMaxAngle", (int32_t)(refData.maxAngle * 10));
    display.setGlobalVariable("vaMinAngle", (int32_t)(refData.minAngle * 10));
    display.setGlobalVariable("tRevPerAngle", refData.revPerAngle);
    display.setGlobalVariable("tCycleTime", refData.cycleTime / 1000);
}

void saveReferenceData() {
    preferences.begin("brus", false);
    
    preferences.putFloat("maxAngle", refData.maxAngle);
    preferences.putFloat("minAngle", refData.minAngle);
    preferences.putUShort("revPerAngle", refData.revPerAngle);
    preferences.putULong("cycleTime", refData.cycleTime);
    
    preferences.end();
    
    Serial.println("Referenčni podatki shranjeni!");
    
    // Posodobi globalne spremenljivke v Nextion
    display.setGlobalVariable("vaMaxAngle", (int32_t)(refData.maxAngle * 10));
    display.setGlobalVariable("vaMinAngle", (int32_t)(refData.minAngle * 10));
    display.setGlobalVariable("tRevPerAngle", refData.revPerAngle);
    display.setGlobalVariable("tCycleTime", refData.cycleTime / 1000);
}

void startReferenceRun() {
    Serial.println("=== ZAČETEK REFERENČNEGA HODA ===");
    referenceRunning = true;
    
    display.setStatus("Referenca...");
    
    // Inicializacija
    float measuredMax = 0.0;
    float measuredMin = 90.0;
    unsigned long startRevs = inputs.getRevolutions();
    unsigned long startTime = millis();
    
    // Faza 1: Pomik navzgor do max kota
    Serial.println("Faza 1: Pomik navzgor...");
    outputs.setSpindleSpeed(SPINDLE_UP, 200);
    
    while (inputs.getAngle() < 28.0) {  // Target max angle
        float currentAngle = inputs.getAngle();
        if (currentAngle > measuredMax) {
            measuredMax = currentAngle;
        }
        
        display.setAngle(currentAngle);
        delay(100);
        
        // Timeout zaščita
        if (millis() - startTime > 60000) {
            Serial.println("ERROR: Timeout!");
            outputs.stopSpindle();
            referenceRunning = false;
            display.setStatus("NAPAKA: Timeout!");
            return;
        }
    }
    
    outputs.stopSpindle();
    delay(500);
    
    // Faza 2: Pomik navzdol do min kota
    Serial.println("Faza 2: Pomik navzdol...");
    outputs.setSpindleSpeed(SPINDLE_DOWN, 200);
    
    while (inputs.getAngle() > 5.0) {  // Target min angle
        float currentAngle = inputs.getAngle();
        if (currentAngle < measuredMin) {
            measuredMin = currentAngle;
        }
        
        display.setAngle(currentAngle);
        delay(100);
        
        if (millis() - startTime > 120000) {
            Serial.println("ERROR: Timeout!");
            outputs.stopSpindle();
            referenceRunning = false;
            display.setStatus("NAPAKA: Timeout!");
            return;
        }
    }
    
    // Faza 3: Vrnitev nazaj na max
    Serial.println("Faza 3: Vrnitev...");
    while (inputs.getAngle() < measuredMax - 0.5) {
        display.setAngle(inputs.getAngle());
        delay(100);
    }
    
    outputs.stopSpindle();
    unsigned long endTime = millis();
    unsigned long endRevs = inputs.getRevolutions();
    
    // Izračun podatkov
    refData.maxAngle = measuredMax;
    refData.minAngle = measuredMin;
    refData.cycleTime = endTime - startTime;
    
    float angleRange = measuredMax - measuredMin;
    unsigned long totalRevs = endRevs - startRevs;
    refData.revPerAngle = (uint16_t)(totalRevs / angleRange);
    
    // Shrani
    saveReferenceData();
    
    // Prikaz rezultatov
    Serial.println("=== REFERENČNI HOD KONČAN ===");
    Serial.printf("Max kot: %.1f°\n", refData.maxAngle);
    Serial.printf("Min kot: %.1f°\n", refData.minAngle);
    Serial.printf("Razpon: %.1f°\n", angleRange);
    Serial.printf("Obrati: %d\n", totalRevs);
    Serial.printf("Obrati/°: %d\n", refData.revPerAngle);
    Serial.printf("Čas: %d s\n", refData.cycleTime / 1000);
    
    display.setStatus("Referenca OK");
    referenceRunning = false;
    
    delay(2000);
    display.showPage(1);  // Nazaj na settings page
}

void handleNextionEvents() {
    if (display.available()) {
        uint8_t componentId = display.readTouchEvent();
        
        switch(componentId) {
            case 7:  // bSave - shranjevanje nastavitev kotov
                Serial.println("Prejem nastavitev kotov...");
                {
                    String data = display.readString();
                    float startAngle, endAngle;
                    
                    if (display.parseAngleSettings(data, startAngle, endAngle)) {
                        // Dodatna validacija (že preverjena v HMI, ampak double-check)
                        if (startAngle >= refData.minAngle && 
                            startAngle <= refData.maxAngle &&
                            endAngle >= refData.minAngle && 
                            endAngle <= refData.maxAngle &&
                            startAngle < endAngle) {
                            
                            // Shrani v preferences
                            preferences.begin("brus", false);
                            preferences.putFloat("angleStart", startAngle);
                            preferences.putFloat("angleStop", endAngle);
                            preferences.end();
                            
                            display.setAngleRange(startAngle, endAngle);
                            display.setStatus("Shranjeno!");
                            
                            Serial.printf("Shranjeno: %.1f° - %.1f°\n", startAngle, endAngle);
                        } else {
                            Serial.println("ERROR: Neveljavne vrednosti!");
                            display.setStatus("NAPAKA!");
                        }
                    }
                }
                break;
                
            case 15:  // bRef - začetek referenčnega hoda
                Serial.println("Začetek referenčnega hoda...");
                if (!referenceRunning) {
                    display.showPage(3);  // Pojdi na referenčno stran
                    delay(500);
                    startReferenceRun();
                }
                break;
                
            default:
                if (componentId > 0) {
                    Serial.printf("Touch event ID: %d\n", componentId);
                }
                break;
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("========================================");
    Serial.println("  Nextion Reference Calibration");
    Serial.println("========================================");
    
    // Inicializacija
    inputs.begin();
    outputs.begin();
    display.begin();
    
    // Naloži referenčne podatke
    loadReferenceData();
    
    // Nastavi začetni status
    display.setMode("MANUAL");
    display.setStatus("Pripravljen");
    
    Serial.println("Sistem pripravljen!");
    Serial.println("Za referenčni hod pritisni 'bRef' gumb.");
    Serial.println();
}

void loop() {
    unsigned long currentMillis = millis();
    
    // Posodobi vhode
    inputs.update(currentMillis);
    
    // Posodobi prikaz kota
    display.setAngle(inputs.getAngle());
    
    // Posodobi Nextion
    display.update(currentMillis);
    
    // Obdelaj dogodke
    handleNextionEvents();
    
    delay(10);
}
