#ifndef OUTPUTS_H
#define OUTPUTS_H

#include <Arduino.h>
#include "config.h"

class BrusOutputs {
private:
    // Stanje izhodov
    bool grindingMotorOn;
    bool knifePusherOn;
    bool waterPumpOn;
    
    // Motor vretena
    uint8_t spindleSpeed;        // 0-255
    SpindleDirection spindleDir;
    bool spindleMoving;
    
    // PWM setup
    void setupPWM();
    
public:
    BrusOutputs();
    void begin();
    
    // ===== KONTROLA RELEJEV =====
    void setGrindingMotor(bool state);   // Trifazni motor brusnega kamna
    void setKnifePusher(bool state);     // Pnevmatski ventil za nož
    void setWaterPump(bool state);       // Črpalka vode
    
    bool isGrindingMotorOn() { return grindingMotorOn; }
    bool isKnifePusherOn() { return knifePusherOn; }
    bool isWaterPumpOn() { return waterPumpOn; }
    
    // ===== KONTROLA MOTORJA VRETENA (MD13S) =====
    void setSpindleSpeed(uint8_t speed); // 0-255
    void setSpindleDirection(SpindleDirection dir);
    void moveSpindleUp(uint8_t speed = SPINDLE_SPEED_MEDIUM);
    void moveSpindleDown(uint8_t speed = SPINDLE_SPEED_MEDIUM);
    void stopSpindle();
    
    uint8_t getSpindleSpeed() { return spindleSpeed; }
    SpindleDirection getSpindleDirection() { return spindleDir; }
    bool isSpindleMoving() { return spindleMoving; }
    
    // ===== VARNOST =====
    void emergencyStop();        // Izklop vseh izhodov
    void safeState();            // Varno stanje (npr. ob zagonu)
    
    // ===== DEBUG =====
    void printOutputState();
};

#endif // OUTPUTS_H
