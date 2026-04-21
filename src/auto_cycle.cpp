#include "auto_cycle.h"

AutoCycle::AutoCycle(BrusInputs* inp, BrusOutputs* out) {
    inputs = inp;
    outputs = out;
    state = CYCLE_IDLE;
    targetCycles = 0;
    completedCycles = 0;
    continuousMode = false;
    targetRevolutions = 0;
    startRevolutions = 0;
    errorOccurred = false;
    tiltReached = false;
}

void AutoCycle::begin() {
    reset();
    Serial.println("AutoCycle inicializiran");
}

void AutoCycle::enterState(CycleState newState) {
    state = newState;
    stateStartTime = millis();
    
    Serial.print(">>> STATE: ");
    switch(state) {
        case CYCLE_IDLE:        Serial.println("IDLE"); break;
        case CYCLE_STARTING:    Serial.println("STARTING"); break;
        case CYCLE_DOWN:        Serial.println("DOWN - spuščanje vretena"); break;
        case CYCLE_UP:          Serial.println("UP - dvig vretena"); break;
        case CYCLE_COMPLETE:    Serial.println("COMPLETE"); break;
        case CYCLE_ERROR:       Serial.println("ERROR"); break;
    }
}

void AutoCycle::update() {
    // State machine
    switch(state) {
        case CYCLE_IDLE:
            processStateIdle();
            break;
        case CYCLE_STARTING:
            processStateStarting();
            break;
        case CYCLE_DOWN:
            processStateDown();
            break;
        case CYCLE_UP:
            processStateUp();
            break;
        case CYCLE_COMPLETE:
            processStateComplete();
            break;
        case CYCLE_ERROR:
            processStateError();
            break;
    }
}

// ===== STATE: IDLE =====
void AutoCycle::processStateIdle() {
    // Čaka na start ukaz
    // (začne se preko start() funkcije)
}

// ===== STATE: STARTING =====
void AutoCycle::processStateStarting() {
    // Vklop izhodov: motor kamna, črpalka, ventil
    outputs->setGrindingMotor(true);
    outputs->setWaterPump(true);
    outputs->setKnifePusher(true);
    
    // Počakaj nekaj časa da se motor kamna zažene
    if (millis() - stateStartTime > STARTUP_DELAY_MS) {
        // Začni s spustom vretena
        enterState(CYCLE_DOWN);
    }
}

// ===== STATE: DOWN =====
void AutoCycle::processStateDown() {
    // Prvi entry v stanje - začni spust
    if (millis() - stateStartTime < 50) {
        startRevolutions = inputs->getRevolutions();
        tiltReached = false;
        outputs->moveSpindleDown(AUTO_SPINDLE_SPEED);
        Serial.print("Začetek spusta - Rev start: ");
        Serial.println(startRevolutions);
    }
    
    // Preveri ali smo dosegli naklon < 10°
    bool tiltNow = inputs->isSpindleTilted();
    
    // Debounce za zanesljivo detekcijo
    if (tiltNow && !tiltReached) {
        if (millis() - lastTiltCheckTime > TILT_DEBOUNCE_MS) {
            tiltReached = true;
            
            // Zabeleži število obratov pri < 10°
            unsigned long currentRev = inputs->getRevolutions();
            targetRevolutions = currentRev - startRevolutions;
            
            Serial.print("!!! DOSEŽEN NAKLON < 10° | Obrати za spust: ");
            Serial.println(targetRevolutions);
            
            // Ustavi vreteno in začni z dvigom
            outputs->stopSpindle();
            delay(100); // Kratka pavza
            
            enterState(CYCLE_UP);
        }
    } else {
        lastTiltCheckTime = millis();
    }
    
    // Varnost - timeout (npr. če stikalo ne deluje)
    if (millis() - stateStartTime > 30000) {  // 30 sekund
        errorOccurred = true;
        errorMessage = "TIMEOUT - naklon ni dosežen";
        enterState(CYCLE_ERROR);
    }
}

// ===== STATE: UP =====
void AutoCycle::processStateUp() {
    // Prvi entry - začni dvig
    if (millis() - stateStartTime < 50) {
        startRevolutions = inputs->getRevolutions();
        outputs->moveSpindleUp(AUTO_SPINDLE_SPEED);
        Serial.print("Začetek dviga - ciljnih obratov: ");
        Serial.println(targetRevolutions);
    }
    
    // Preveri število obratov
    unsigned long currentRev = inputs->getRevolutions();
    unsigned long completedRev = currentRev - startRevolutions;
    
    // Ali smo dosegli ciljno število obratov?
    if (completedRev >= targetRevolutions) {
        outputs->stopSpindle();
        
        Serial.print("Dvig končan - obratov: ");
        Serial.println(completedRev);
        
        // Povečaj števec izvedenih ciklov
        completedCycles++;
        
        // Preveri ali moramo nadaljevati
        if (continuousMode || completedCycles < targetCycles) {
            // Nadaljuj z novim ciklom
            Serial.print("Cikel ");
            Serial.print(completedCycles);
            Serial.print("/");
            Serial.print(continuousMode ? "∞" : String(targetCycles));
            Serial.println(" končan - začenjam novega");
            
            delay(500); // Kratka pavza med cikli
            enterState(CYCLE_DOWN);
        } else {
            // Vsi cikli končani
            enterState(CYCLE_COMPLETE);
        }
    }
    
    // Varnost - timeout
    if (millis() - stateStartTime > 30000) {
        errorOccurred = true;
        errorMessage = "TIMEOUT - dvig ni končan";
        enterState(CYCLE_ERROR);
    }
}

// ===== STATE: COMPLETE =====
void AutoCycle::processStateComplete() {
    // Cikel končan - izklop izhodov
    outputs->setGrindingMotor(false);
    outputs->setWaterPump(false);
    outputs->setKnifePusher(false);
    outputs->stopSpindle();
    
    Serial.println("*** AVTOMATSKI CIKEL KONČAN ***");
    Serial.print("Izvedenih ciklov: ");
    Serial.println(completedCycles);
    
    // Nazaj v IDLE
    enterState(CYCLE_IDLE);
}

// ===== STATE: ERROR =====
void AutoCycle::processStateError() {
    // Napaka - emergency stop
    outputs->emergencyStop();
    
    Serial.println("!!! NAPAKA V CIKLU !!!");
    Serial.println(errorMessage);
    
    // Ostani v ERROR stanju dokler ni reset
}

// ===== KONTROLA =====
void AutoCycle::start(uint8_t cycles) {
    if (state != CYCLE_IDLE) {
        Serial.println("Cikel že teče!");
        return;
    }
    
    targetCycles = cycles;
    completedCycles = 0;
    continuousMode = false;
    errorOccurred = false;
    
    Serial.print("Začenjam avtomatski cikel - ciklov: ");
    Serial.println(cycles);
    
    enterState(CYCLE_STARTING);
}

void AutoCycle::startContinuous() {
    if (state != CYCLE_IDLE) {
        Serial.println("Cikel že teče!");
        return;
    }
    
    targetCycles = 0;
    completedCycles = 0;
    continuousMode = true;
    errorOccurred = false;
    
    Serial.println("Začenjam NEPREKINJENI avtomatski cikel");
    
    enterState(CYCLE_STARTING);
}

void AutoCycle::stop() {
    Serial.println("Ustavitev avtomatskega cikla");
    
    outputs->stopSpindle();
    outputs->setGrindingMotor(false);
    outputs->setWaterPump(false);
    outputs->setKnifePusher(false);
    
    enterState(CYCLE_IDLE);
}

void AutoCycle::reset() {
    state = CYCLE_IDLE;
    targetCycles = 0;
    completedCycles = 0;
    continuousMode = false;
    targetRevolutions = 0;
    startRevolutions = 0;
    errorOccurred = false;
    errorMessage = "";
    tiltReached = false;
}

// ===== DEBUG =====
void AutoCycle::printStatus() {
    Serial.print("Cycle State: ");
    switch(state) {
        case CYCLE_IDLE:        Serial.print("IDLE"); break;
        case CYCLE_STARTING:    Serial.print("STARTING"); break;
        case CYCLE_DOWN:        Serial.print("DOWN"); break;
        case CYCLE_UP:          Serial.print("UP"); break;
        case CYCLE_COMPLETE:    Serial.print("COMPLETE"); break;
        case CYCLE_ERROR:       Serial.print("ERROR"); break;
    }
    
    Serial.print(" | Cycles: ");
    Serial.print(completedCycles);
    Serial.print("/");
    if (continuousMode) {
        Serial.print("∞");
    } else {
        Serial.print(targetCycles);
    }
    
    if (targetRevolutions > 0) {
        Serial.print(" | Target Rev: ");
        Serial.print(targetRevolutions);
    }
    
    Serial.println();
}
