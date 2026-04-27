/**
 * Primer validacije nastavitev ob zagonu in upravljanja načinov delovanja
 * 
 * Funkcionalnost:
 * 1. Ob zagonu preveri če so izvedeni referenčni hod (tMinAngle, tMaxAngle)
 * 2. Preveri če so nastavljeni začetni in končni koti (xAngleStart, xAngleStop)
 * 3. V MODE_MANUAL omogoči ročne gumbe
 * 4. MODE_AUTO dovoli samo če so vse nastavitve OK
 * 
 * Page 0 Preinitialize Event:
 *   bBrus.pco=54970
 *   bPnev.pco=54970
 *   bGor.pco=54970
 *   bDol.pco=54970
 */

#include <Arduino.h>
#include <Preferences.h>
#include "nextion.h"
#include "inputs.h"
#include "config.h"

// Globalni objekti
NextionDisplay display;
Preferences preferences;
BrusInputs inputs;

// Sistemske nastavitve
struct SystemSettings {
    float minAngle;       // Minimalni referenčni kot
    float maxAngle;       // Maksimalni referenčni kot
    float angleStart;     // Začetni kot za delo
    float angleStop;      // Končni kot za delo
    bool referenceValid;  // Ali je referenčni hod izveden
    bool anglesValid;     // Ali so koti nastavljeni
};

SystemSettings settings;
S1Mode currentMode = MODE_OFF;

void loadSettings() {
    preferences.begin("brus", false);
    
    settings.minAngle = preferences.getFloat("minAngle", 0.0);
    settings.maxAngle = preferences.getFloat("maxAngle", 0.0);
    settings.angleStart = preferences.getFloat("angleStart", 0.0);
    settings.angleStop = preferences.getFloat("angleStop", 0.0);
    
    preferences.end();
    
    // Preveri validnost
    settings.referenceValid = (settings.minAngle > 0 && settings.maxAngle > 0 && 
                               settings.minAngle < settings.maxAngle);
    settings.anglesValid = (settings.angleStart > 0 && settings.angleStop > 0 && 
                           settings.angleStart < settings.angleStop);
    
    Serial.println("=== NALOŽENE NASTAVITVE ===");
    Serial.printf("Min kot: %.1f°\n", settings.minAngle);
    Serial.printf("Max kot: %.1f°\n", settings.maxAngle);
    Serial.printf("Start kot: %.1f°\n", settings.angleStart);
    Serial.printf("Stop kot: %.1f°\n", settings.angleStop);
    Serial.printf("Referenca veljavna: %s\n", settings.referenceValid ? "DA" : "NE");
    Serial.printf("Koti veljavni: %s\n", settings.anglesValid ? "DA" : "NE");
    Serial.println();
}

void updateNextionSettings() {
    // Posodobi globalne spremenljivke v Nextion
    display.setGlobalVariable("tMinAngle", (int32_t)(settings.minAngle * 10));
    display.setGlobalVariable("tMaxAngle", (int32_t)(settings.maxAngle * 10));
    
    // Posodobi lokalne spremenljivke za prikaz
    display.setAngleRange(settings.angleStart, settings.angleStop);
}

bool validateSettings() {
    // Preveri če so vse nastavitve OK za delovanje
    
    if (!settings.referenceValid) {
        Serial.println("OPOZORILO: Referenčni hod ni izveden!");
        display.setStatus("Izvedite referencni hod !");
        return false;
    }
    
    if (!settings.anglesValid) {
        Serial.println("OPOZORILO: Začetni/končni koti niso nastavljeni!");
        display.setStatus("Nastavite zacetni in koncni kot !");
        return false;
    }
    
    // Vse OK
    display.setStatus("Pripravljen");
    return true;
}

void handleModeChange(S1Mode newMode) {
    if (newMode == currentMode) return;
    
    Serial.print("Način spremenjen: ");
    currentMode = newMode;
    
    switch(currentMode) {
        case MODE_OFF:
            Serial.println("OFF");
            display.setMode("OFF");
            display.setStatus("Stop");
            display.enableManualButtons(false);  // Onemogoči vse gumbe
            break;
            
        case MODE_MANUAL:
            Serial.println("MANUAL");
            display.setMode("MANUAL");
            
            // MANUAL način deluje vedno (tudi brez referenc)
            display.enableManualButtons(true);  // Omogoči ročne gumbe
            
            if (!settings.referenceValid) {
                display.setStatus("Izvedite referencni hod !");
            } else if (!settings.anglesValid) {
                display.setStatus("Nastavite zacetni in koncni kot !");
            } else {
                display.setStatus("Pripravljen");
            }
            break;
            
        case MODE_AUTO:
            Serial.println("AUTO");
            
            // AUTO način zahteva veljavne nastavitve
            if (validateSettings()) {
                display.setMode("AUTO");
                display.setStatus("Pripravljen");
                display.enableManualButtons(false);  // V AUTO gumbi niso aktivni
            } else {
                Serial.println("ERROR: Ni mogoče preklopiti v AUTO - manjkajoče nastavitve!");
                // Vrni nazaj v MANUAL
                currentMode = MODE_MANUAL;
                display.setMode("MANUAL");
                display.enableManualButtons(true);
            }
            break;
    }
}

void handleNextionEvents() {
    if (display.available()) {
        uint8_t componentId = display.readTouchEvent();
        
        switch(componentId) {
            case 7:  // bSave - shranjevanje kotov (Page 1)
                Serial.println("Prejem nastavitev kotov...");
                {
                    String data = display.readString();
                    float startAngle, endAngle;
                    
                    if (display.parseAngleSettings(data, startAngle, endAngle)) {
                        // Validacija (že preverjena v HMI)
                        if (startAngle > 0 && endAngle > 0 && startAngle < endAngle) {
                            // Shrani
                            preferences.begin("brus", false);
                            preferences.putFloat("angleStart", startAngle);
                            preferences.putFloat("angleStop", endAngle);
                            preferences.end();
                            
                            // Posodobi
                            settings.angleStart = startAngle;
                            settings.angleStop = endAngle;
                            settings.anglesValid = true;
                            
                            display.setAngleRange(startAngle, endAngle);
                            display.setStatus("Shranjeno!");
                            
                            Serial.printf("Shranjeno: %.1f° - %.1f°\n", startAngle, endAngle);
                            
                            delay(1000);
                            validateSettings();  // Ponovno preveri
                        }
                    }
                }
                break;
                
            case 15:  // bRef - referenčni hod (Page 1 -> Page 3)
                Serial.println("Začetek referenčnega hoda...");
                display.showPage(3);
                delay(500);
                // Tukaj bi klical funkcijo startReferenceRun()
                // Po končanem referenčnem hodu:
                // settings.referenceValid = true;
                // validateSettings();
                break;
                
            case 18:  // bSettings - odpri Page 1
                Serial.println("Odpiranje nastavitev...");
                display.showPage(1);
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
    Serial.println("  BRUS - Startup Validation");
    Serial.println("========================================");
    
    // Inicializacija
    inputs.begin();
    display.begin();
    
    // Naloži nastavitve iz Preferences
    loadSettings();
    
    // Posodobi Nextion
    updateNextionSettings();
    
    // Začetna validacija
    display.setMode("OFF");
    if (!validateSettings()) {
        Serial.println("OPOZORILO: Sistem ni popolnoma konfiguriran!");
        if (!settings.referenceValid) {
            Serial.println("  -> Izvedite referenčni hod (Page 3)");
        }
        if (!settings.anglesValid) {
            Serial.println("  -> Nastavite kote (Page 1)");
        }
    }
    
    Serial.println("\nSistem pripravljen!");
    Serial.println("Preklopite S1 stikalo za izbiro načina.\n");
}

void loop() {
    unsigned long currentMillis = millis();
    
    // Posodobi vhode
    inputs.update(currentMillis);
    
    // Preveri spremembo načina (S1 stikalo)
    S1Mode newMode = inputs.getMode();
    if (newMode != currentMode) {
        handleModeChange(newMode);
    }
    
    // Posodobi prikaz
    display.setAngle(inputs.getAngle());
    display.update(currentMillis);
    
    // Obdelaj dogodke
    handleNextionEvents();
    
    // V MANUAL načinu omogoči gumbe
    static S1Mode lastMode = MODE_OFF;
    if (currentMode == MODE_MANUAL && lastMode != MODE_MANUAL) {
        display.enableManualButtons(true);
        lastMode = MODE_MANUAL;
    } else if (currentMode != MODE_MANUAL && lastMode == MODE_MANUAL) {
        display.enableManualButtons(false);
        lastMode = currentMode;
    }
    
    delay(10);
}
