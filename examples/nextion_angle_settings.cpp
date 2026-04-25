/**
 * Primer uporabe Nextion displaya za branje in shranjevanje nastavitev kotov
 * 
 * HMI Setup (Page 1 - Nastavitve):
 * - Xfloat "xStartAngle" (Component ID=5) - vnos začetnega kota
 * - Xfloat "xEndAngle" (Component ID=6) - vnos končnega kota
 * - Button "bSave" (Component ID=7) - gumb za shranjevanje
 * 
 * Button "bSave" Touch Release Event:
 *   print "ID5:"
 *   print xStartAngle.val
 *   print ";ID6:"
 *   print xEndAngle.val
 *   printh ff ff ff
 */

#include <Arduino.h>
#include <Preferences.h>
#include "nextion.h"

// Globalni objekti
NextionDisplay display;
Preferences preferences;

// Nastavitve kotov
float angleStart = 5.0;   // Privzeta vrednost
float angleStop = 28.0;   // Privzeta vrednost

void loadAngleSettings() {
    // Naloži shranjene vrednosti iz NVS
    preferences.begin("brus", false);  // false = read-write mode
    
    angleStart = preferences.getFloat("angleStart", 5.0);
    angleStop = preferences.getFloat("angleStop", 28.0);
    
    preferences.end();
    
    Serial.print("Naložene nastavitve: Start=");
    Serial.print(angleStart);
    Serial.print("°, Stop=");
    Serial.print(angleStop);
    Serial.println("°");
    
    // Posodobi prikaz na Page 0
    display.setAngleRange(angleStart, angleStop);
}

void saveAngleSettings(float start, float stop) {
    // Shrani vrednosti v NVS (non-volatile storage)
    preferences.begin("brus", false);
    
    preferences.putFloat("angleStart", start);
    preferences.putFloat("angleStop", stop);
    
    preferences.end();
    
    // Posodobi globalne spremenljivke
    angleStart = start;
    angleStop = stop;
    
    Serial.print("Shranjene nastavitve: Start=");
    Serial.print(angleStart);
    Serial.print("°, Stop=");
    Serial.print(angleStop);
    Serial.println("°");
    
    // Posodobi prikaz na Page 0
    display.setAngleRange(angleStart, angleStop);
    
    // Prikaži potrditev uporabniku
    display.setStatus("Shranjeno!");
    delay(1000);
    display.setStatus("Pripravljen");
}

void handleNextionEvents() {
    // Preveri če so na voljo dogodki iz Nextiona
    if (display.available()) {
        uint8_t componentId = display.readTouchEvent();
        
        switch(componentId) {
            case 7:  // Button "bSave" pritisnjen
                Serial.println("Prejem nastavitev kotov...");
                
                // Preberi custom string "ID5:357;ID6:80"
                String data = display.readString();
                
                if (data.length() > 0) {
                    float newStart, newStop;
                    
                    // Parsiraj string
                    if (display.parseAngleSettings(data, newStart, newStop)) {
                        // Validacija vrednosti
                        if (newStart >= 0 && newStart <= 30 && 
                            newStop >= 0 && newStop <= 30 &&
                            newStart < newStop) {
                            
                            // Shrani nove vrednosti
                            saveAngleSettings(newStart, newStop);
                            
                            Serial.println("Nastavitve uspešno shranjene!");
                        } else {
                            Serial.println("ERROR: Neveljavne vrednosti kotov!");
                            display.setStatus("NAPAKA: Vrednosti!");
                            delay(2000);
                            display.setStatus("Pripravljen");
                        }
                    } else {
                        Serial.println("ERROR: Napaka pri parsanju!");
                        display.setStatus("NAPAKA: Format!");
                        delay(2000);
                        display.setStatus("Pripravljen");
                    }
                } else {
                    Serial.println("ERROR: Prazen string!");
                }
                break;
                
            case 5:  // xStartAngle touched (opcijsko)
                Serial.println("xStartAngle touched");
                break;
                
            case 6:  // xEndAngle touched (opcijsko)
                Serial.println("xEndAngle touched");
                break;
                
            default:
                if (componentId > 0) {
                    Serial.print("Unknown component ID: ");
                    Serial.println(componentId);
                }
                break;
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("========================================");
    Serial.println("  Nextion Angle Settings Example");
    Serial.println("========================================");
    
    // Inicializacija Nextion
    display.begin();
    
    // Naloži shranjene nastavitve
    loadAngleSettings();
    
    // Nastavi začetni status
    display.setMode("MANUAL");
    display.setStatus("Pripravljen");
    
    Serial.println("Sistem pripravljen!");
    Serial.println("Odprite Page 1 in spremenite nastavitve kotov.");
    Serial.println();
}

void loop() {
    unsigned long currentMillis = millis();
    
    // Posodobi Nextion display (prebere dogodke)
    display.update(currentMillis);
    
    // Obdelaj Nextion dogodke
    handleNextionEvents();
    
    delay(10);
}
