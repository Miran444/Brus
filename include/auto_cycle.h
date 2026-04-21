#ifndef AUTO_CYCLE_H
#define AUTO_CYCLE_H

#include <Arduino.h>
#include "config.h"
#include "inputs.h"
#include "outputs.h"

class AutoCycle {
private:
    BrusInputs* inputs;
    BrusOutputs* outputs;
    
    // Stanje cikla
    CycleState state;
    uint8_t targetCycles;           // Število ciklov za izvesti (iz S2)
    uint8_t completedCycles;        // Število že izvedenih ciklov
    bool continuousMode;            // Ali je aktiven neprekinjeni način
    
    // Merjenje obratov
    unsigned long targetRevolutions;    // Število obratov za vrnitev (spust)
    unsigned long startRevolutions;     // Začetno stanje števca ob začetku faze
    
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
    void processStateStarting();
    void processStateDown();
    void processStateUp();
    void processStateComplete();
    void processStateError();
    
public:
    AutoCycle(BrusInputs* inp, BrusOutputs* out);
    
    void begin();
    void update();
    
    // Kontrola cikla
    void start(uint8_t cycles);     // Začni cikel z določenim številom
    void startContinuous();         // Začni neprekinjeni cikel
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
