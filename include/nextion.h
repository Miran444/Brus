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
    unsigned long lastRevolutions;
    bool lastMotorState;
    bool lastPumpState;
    bool lastKnifeState;
    bool lastSpindleMoving;
    
    // Helper funkcije
    void sendCommand(const char* cmd);
    void endCommand();
    
public:
    NextionDisplay();
    
    void begin();
    void update(unsigned long currentMillis);
    
    // Pošiljanje podatkov na display
    void setMode(const char* mode);           // "OFF", "MANUAL", "AUTO"
    void setCycles(uint8_t current, uint8_t target);
    void setAngle(float angle);
    void setRevolutions(unsigned long rev);
    void setMotorStatus(bool grinding, bool pump, bool knife);
    void setSpindleStatus(bool moving, bool directionUp, uint8_t speed);
    void setAlarm(const char* message);
    void clearAlarm();
    
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
