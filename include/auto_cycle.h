#ifndef AUTO_CYCLE_H
#define AUTO_CYCLE_H

#include <Arduino.h>
#include "config.h"
#include "inputs.h"
#include "outputs.h"
#include "as5600.h"

// Forward declaration
class NextionDisplay;

class AutoCycle {
private:
    BrusInputs* inputs;
    BrusOutputs* outputs;
    AS5600* angleSensor;
    NextionDisplay* display;
    
    // Stanje cikla
    CycleState state;
    uint8_t targetCycles;           // Število ciklov za izvesti (iz S2)
    uint8_t completedCycles;        // Število že izvedenih ciklov
    bool continuousMode;            // Ali je aktiven neprekinjeni način
    
    // Koti
    float angleStart;               // Začetni kot (višji)
    float angleStop;                // Končni kot (nižji)
    float calibratedMin;            // Minimalni varni kot
    float calibratedMax;            // Maksimalni varni kot
    float angleOffset;              // Offset za kot (iz as5600AngleOffset)
    
    // Hitrosti (v procentih 50-100%)
    uint8_t speedZacetni;           // Faza 1: Start do Start-2.0°
    uint8_t speedSredina;           // Faza 2: Start-2.0° do Stop+2.0°
    uint8_t speedKoncni;            // Faza 3: Stop+2.0° do Stop
    
    // Helper funkcije
    uint8_t speedToPWM(uint8_t percent);
    
    // Merjenje obratov
    unsigned long targetRevolutions;    // Število obratov za vrnitev (spust)
    unsigned long startRevolutions;     // Začetno stanje števca ob začetku faze
    float revPerAngle;                  // Referenčno število obratov na stopinjo
    float lastCheckedAngle;             // Zadnji kot pri katerem smo preverili obrate
    unsigned long lastCheckedRevs;      // Obrati pri zadnjem preverjanju
    
    // Časovniki
    unsigned long stateStartTime;       // Čas začetka trenutnega stanja
    unsigned long lastTiltCheckTime;    // Za debounce naklona
    bool tiltReached;                   // Ali smo že dosegli < 10°
    
    // Varnost
    bool errorOccurred;
    String errorMessage;
    
    // State machine funkcije
    void enterState(CycleState newState);
    void processStateIdle();
    void processStateCheckKnife();
    void processStateMoveToStart();
    void processStateStarting();
    void processStateDown();
    void processStateUp();
    void processStateComplete();
    void processStateError();
    
public:
    AutoCycle(BrusInputs* inp, BrusOutputs* out, AS5600* sensor, NextionDisplay* disp);
    
    void begin();
    void update();
    
    // Kontrola cikla
    void start(uint8_t cycles, float angleStart, float angleStop,
               float calibratedMin, float calibratedMax, bool isCalibrated,
               uint8_t speedZ, uint8_t speedS, uint8_t speedK, float revPerAngle,
               float angleOffset = 0.0);
    void stop();                    // Ustavi cikel
    void reset();                   // Reset state machine
    
    // Status
    CycleState getState() { return state; }
    uint8_t getCompletedCycles() { return completedCycles; }
    uint8_t getTargetCycles() { return targetCycles; }
    bool isRunning() { return state != CYCLE_IDLE && state != CYCLE_ERROR; }
    bool hasError() { return errorOccurred; }
    String getErrorMessage() { return errorMessage; }
    
    // Debug
    void printStatus();
};

#endif // AUTO_CYCLE_H
