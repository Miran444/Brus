#ifndef OUTPUTS_H
#define OUTPUTS_H

#include <Arduino.h>
#include "config.h"

// Forward deklaracija
class BrusInputs;

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
    
    // Kontrola cilindra noža
    enum KnifeCylinderState {
        KNIFE_STOPPED = 0,       // Cilinder miren
        KNIFE_MOVING_IN = 1,     // Premikanje NOTER
        KNIFE_MOVING_OUT = 2     // Premikanje VEN
    };
    KnifeCylinderState knifeCylinderState;
    unsigned long knifeMoveStartTime;
    const unsigned long KNIFE_SAFETY_TIMEOUT = 10000; // 10 sekund
    bool knifeCylinderError;
    String knifeCylinderErrorMsg;
    BrusInputs* inputs;  // Pointer na inputs za branje senzorjev
    
    // PWM setup
    void setupPWM();
    
    // Privatne funkcije za cilinder
    void moveKnifeIn();
    void moveKnifeOut();
    void stopKnifeCylinder();
    
public:
    BrusOutputs();
    void begin();
    void setInputs(BrusInputs* inputsPtr) { inputs = inputsPtr; }
    void update();  // Periodično posodabljanje stanja cilindra
    
    // ===== KONTROLA RELEJEV =====
    void setGrindingMotor(bool state);   // Trifazni motor brusnega kamna (+ črpalka)
    void setKnifePusher(bool state);     // Pnevmatski ventil za nož (začne/ustavi oscilacijo)
    void setWaterPump(bool state);       // Črpalka vode
    
    bool isGrindingMotorOn() { return grindingMotorOn; }
    bool isKnifePusherOn() { return knifePusherOn; }
    bool isWaterPumpOn() { return waterPumpOn; }
    bool hasKnifeCylinderError() { return knifeCylinderError; }
    String getKnifeCylinderError() { return knifeCylinderErrorMsg; }
    
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
