#ifndef NEXTION_H
#define NEXTION_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "config.h"

// Nextion objekti (imena v HMI Editoru)
// Prilagodite glede na vaš HMI dizajn

class NextionDisplay {
private:
    HardwareSerial* serial;
    unsigned long lastUpdateTime;
    
    // Bufferi za preverjanje sprememb (posodobi samo če se vrednost spremeni)
    String lastMode;
    uint8_t lastCycles;
    uint8_t lastCompletedCycles;
    float lastAngle;
    String lastStatus;
    bool lastBrusState;
    bool lastPnevState;
    bool lastSpindleMoving;
    float lastAngleStart;
    float lastAngleStop;
    
    // Helper funkcije
    void sendCommand(const char* cmd);
    void endCommand();
    
public:
    NextionDisplay();
    
    void begin();
    void update(unsigned long currentMillis);
    
    // Pošiljanje podatkov na display
    void setMode(const char* mode);           // "OFF", "MANUAL", "AUTO"
    void setStatus(const char* status);       // "Stop", "Pripravljen", "Run", "Alarm"
    void setCycles(uint8_t current, uint8_t target);
    void setAngle(float angle);
    void setAngleRange(float angleStart, float angleStop);
    void setButtonState(const char* button, bool enabled);
    void setBrusState(bool active);
    void setPnevState(bool active);
    void setSpindleStatus(bool moving, bool directionUp, uint8_t speed);
    
    // Pošiljanje komand
    void showPage(uint8_t pageId);            // Preklapljanje med zasloni
    void setText(const char* obj, const char* text);
    void setNumber(const char* obj, int32_t value);
    void setProgress(const char* obj, int32_t value);
    
    // Branje dogodkov (touch events)
    bool available();
    uint8_t readTouchEvent();
    
    // Debug
    void test();
};

#endif // NEXTION_H
