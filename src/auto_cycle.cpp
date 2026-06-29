#include "auto_cycle.h"
#include "nextion.h"

AutoCycle::AutoCycle(BrusInputs* inp, BrusOutputs* out, AS5600* sensor, NextionDisplay* disp) {
    inputs = inp;
    outputs = out;
    angleSensor = sensor;
    display = disp;
    state = CYCLE_IDLE;
    targetCycles = 0;
    completedCycles = 0;
    continuousMode = false;
    targetRevolutions = 0;
    startRevolutions = 0;
    errorOccurred = false;
    tiltReached = false;
    lastSpeedPercent = 255;  // Neveljavna vrednost za prvi prikaz
    angleStart = 0.0;
    angleStop = 0.0;
    calibratedMin = 0.0;
    calibratedMax = 0.0;
    speedZacetni = 90;
    speedSredina = 75;
    speedKoncni = 85;
    revPerAngle = 0.0;
    lastCheckedAngle = 0.0;
    lastCheckedRevs = 0;
}

void AutoCycle::start(uint8_t cycles, float angleStart, float angleStop,
                     float calibratedMin, float calibratedMax, bool isCalibrated,
                     uint8_t speedZ, uint8_t speedS, uint8_t speedK, float revPerAngle,
                     float angleOffset) {
    // Validacija: preveri ali so koti znotraj kalibriranih limitov
    if (isCalibrated && angleStart > 0.0 && angleStop > 0.0) {
        if (angleStart > calibratedMax) {
            Serial.print("ERROR: Start angle ");
            Serial.print(angleStart);
            Serial.print(" presega maksimalni limit ");
            Serial.println(calibratedMax);
            errorMessage = "Start>Max";
            errorOccurred = true;
            enterState(CYCLE_ERROR);
            return;
        }
        if (angleStop < calibratedMin) {
            Serial.print("ERROR: Stop angle ");
            Serial.print(angleStop);
            Serial.print(" presega minimalni limit ");
            Serial.println(calibratedMin);
            errorMessage = "Stop<Min";
            errorOccurred = true;
            enterState(CYCLE_ERROR);
            return;
        }
        Serial.println("[AutoCycle] Koti preverjeni - znotraj limitov");
    } else if (!isCalibrated) {
        Serial.println("[AutoCycle] OPOZORILO: Start brez kalibracije!");
    }
    
    // Shrani parametre
    this->angleStart = angleStart;
    this->angleStop = angleStop;
    this->calibratedMin = calibratedMin;
    this->calibratedMax = calibratedMax;
    this->angleOffset = angleOffset;
    this->speedZacetni = speedZ;
    this->speedSredina = speedS;
    this->speedKoncni = speedK;
    this->revPerAngle = revPerAngle;
    targetCycles = cycles;
    completedCycles = 0;
    continuousMode = (cycles == 0);
    errorOccurred = false;
    tiltReached = false;
    
    Serial.print("[AutoCycle] START - cikli: ");
    Serial.println(cycles == 0 ? "∞" : String(cycles));
    Serial.print("Angle range: ");
    Serial.print(angleStart);
    Serial.print("° -> ");
    Serial.print(angleStop);
    Serial.println("°");
    Serial.print("Speeds: Z=");
    Serial.print(speedZ);
    Serial.print("%, S=");
    Serial.print(speedS);
    Serial.print("%, K=");
    Serial.print(speedK);
    Serial.println("%");
    
    display->setStatus("Preverjam montažo noža...");
    enterState(CYCLE_CHECK_KNIFE);
}

// Helper funkcija za pretvorbo hitrosti iz % v PWM
uint8_t AutoCycle::speedToPWM(uint8_t percent) {
    if (percent < 50) percent = 50;
    if (percent > 100) percent = 100;
    return 128 + ((percent - 50) * 127) / 50;
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
        case CYCLE_IDLE:            Serial.println("IDLE"); break;
        case CYCLE_CHECK_KNIFE:     Serial.println("CHECK_KNIFE - preveri montažo noža"); break;
        case CYCLE_MOVE_TO_START:   Serial.println("MOVE_TO_START - pomik v začetni kot"); break;
        case CYCLE_STARTING:        Serial.println("STARTING - vklop izhodov"); break;
        case CYCLE_DOWN:            Serial.println("DOWN - spuščanje vretena"); break;
        case CYCLE_UP:              Serial.println("UP - dvig vretena"); break;
        case CYCLE_COMPLETE:        Serial.println("COMPLETE"); break;
        case CYCLE_ERROR:           Serial.println("ERROR"); break;
    }
}

void AutoCycle::update() {
    // State machine
    switch(state) {
        case CYCLE_IDLE:
            processStateIdle();
            break;
        case CYCLE_CHECK_KNIFE:
            processStateCheckKnife();
            break;
        case CYCLE_MOVE_TO_START:
            processStateMoveToStart();
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

// ===== STATE: CHECK_KNIFE =====
void AutoCycle::processStateCheckKnife() {
    // Preveri ali je nož montiran in naslonjen na brusni kamen
    float currentAngle = angleSensor->getCalibratedAngle(angleOffset);
    
    Serial.print("[CHECK_KNIFE] Trenutni kot: ");
    Serial.print(currentAngle);
    Serial.println("°");
    
    // Preveri ali je kot med minAngle in maxAngle
    if (currentAngle < calibratedMin || currentAngle > calibratedMax) {
        Serial.println("[CHECK_KNIFE] NAPAKA: Nož ni montiran ali ni naslonjen na brusni kamen!");
        Serial.print("Trenutni kot ");
        Serial.print(currentAngle);
        Serial.print("° je izven območja [");
        Serial.print(calibratedMin);
        Serial.print("° - ");
        Serial.print(calibratedMax);
        Serial.println("°]");
        
        errorMessage = "Noz ni montiran!";
        errorOccurred = true;
        display->setStatus("NAPAKA: Montiraj nož!");
        enterState(CYCLE_ERROR);
        return;
    }
    
    Serial.println("[CHECK_KNIFE] OK - Nož je montiran");
    
    // Če je kot že blizu startAngle, pojdi direktno na STARTING
    if (abs(currentAngle - angleStart) < KNIFE_MOUNT_TOLERANCE) {
        Serial.println("[CHECK_KNIFE] Že v začetnem položaju");
        display->setStatus("Pripravljam motorje...");
        enterState(CYCLE_STARTING);
    } else {
        // Pomik v začetni kot
        display->setStatus("Pozicioniram vreteno...");
        enterState(CYCLE_MOVE_TO_START);
    }
}

// ===== STATE: MOVE_TO_START =====
void AutoCycle::processStateMoveToStart() {
    float currentAngle = angleSensor->getCalibratedAngle(angleOffset);
    
    // Prvi entry - začni premik (samo enkrat)
    static bool firstEntry = true;
    if (millis() - stateStartTime < 50) {
        if (firstEntry) {
            Serial.print("[MOVE_TO_START] Pomik iz ");
            Serial.print(currentAngle);
            Serial.print("° v ");
            Serial.print(angleStart);
            Serial.println("°");
            
            // Določi smer
            if (currentAngle < angleStart) {
                outputs->moveSpindleUp(150);  // Srednja hitrost za pozicioniranje
            } else {
                outputs->moveSpindleDown(150);
            }
            firstEntry = false;
        }
        return;
    }
    firstEntry = true;  // Reset za naslednji vstop
    
    // Preveri ali smo dosegli cilj
    if (abs(currentAngle - angleStart) < MOVE_TO_START_TOLERANCE) {
        outputs->stopSpindle();
        Serial.print("[MOVE_TO_START] Dosežen začetni kot: ");
        Serial.print(currentAngle);
        Serial.println("°");        display->setStatus("Pripravljam motorje...");        enterState(CYCLE_STARTING);
    }
    
    // Varnostni timeout (30 sekund)
    if (millis() - stateStartTime > 30000) {
        outputs->stopSpindle();
        errorMessage = "Timeout: premik v start";
        errorOccurred = true;
        enterState(CYCLE_ERROR);
    }
}

// ===== STATE: STARTING =====
void AutoCycle::processStateStarting() {
    // Prvi entry - vklopi ventil noža (samo enkrat)
    static bool firstEntry = true;
    if (millis() - stateStartTime < 50) {
        if (firstEntry) {
            Serial.println("[STARTING] Vklop OUT_VENTIL_NOZ");
            outputs->setKnifePusher(true);
            firstEntry = false;
        }
        return;
    }
    firstEntry = true;  // Reset za naslednji vstop
    
    // Po 2 sekundah vklopi motor kamna
    if (millis() - stateStartTime >= STARTUP_DELAY_MS) {
        Serial.println("[STARTING] Vklop OUT_MOTOR_KAMEN");
        outputs->setGrindingMotor(true);
        
        // Začni s spustom vretena
        Serial.println("[STARTING] Začetek pomika vretena DOL");
        char msg[50];
        sprintf(msg, "CIKEL %d: Brusenje...", completedCycles + 1);
        display->setStatus(msg);
        enterState(CYCLE_DOWN);
    }
}

// ===== STATE: DOWN =====
void AutoCycle::processStateDown() {
    // PREVERI NAPAKO CILINDRA - ustavi cikel ob napaki
    if (outputs->hasKnifeCylinderError()) {
        outputs->stopSpindle();
        errorMessage = outputs->getKnifeCylinderError();
        errorOccurred = true;  // KRITIČNO: Nastavi flag za ERROR stanje!
        enterState(CYCLE_ERROR);
        return;
    }
    
    float currentAngle = angleSensor->getCalibratedAngle(angleOffset);
    unsigned long currentRevs = inputs->getRevolutions();
    
    // Prvi entry v stanje - določi fazo glede na trenutni kot (samo enkrat)
    static bool firstEntry = true;
    if (millis() - stateStartTime < 50) {
        if (firstEntry) {
            startRevolutions = currentRevs;
            lastCheckedAngle = currentAngle;
            lastCheckedRevs = currentRevs;
            
            Serial.print("Začetek spusta - trenutni kot: ");
            Serial.println(currentAngle);
            firstEntry = false;
        }
        return;
    }
    firstEntry = true;  // Reset za naslednji vstop
    
    // FAZA 1: Od angleStart do (angleStart - 2°)
    // Hitro brusenje začetnega dela
    if (currentAngle > (angleStart - 2.0)) {
        uint8_t pwm = speedToPWM(speedZacetni);
        outputs->moveSpindleDown(pwm);
        
        // Status na display - Faza 1
        static unsigned long lastDisplayUpdate = 0;
        if (millis() - lastDisplayUpdate > 500) {  // Posodobi vsakih 0.5s
            char msg[50];
            sprintf(msg, "Faza: Spust 1/3");
            display->setStatus(msg);
            // Prikaži hitrost samo ob spremembi
            if (speedZacetni != lastSpeedPercent) {
                display->setNumber("nSpeed", speedZacetni);
                lastSpeedPercent = speedZacetni;
            }
            lastDisplayUpdate = millis();
        }
    }
    // FAZA 2: Od (angleStart - 2°) do (angleStop + 2°)
    // Glavno brusenje s srednjo hitrostjo
    else if (currentAngle > (angleStop + 2.0)) {
        uint8_t pwm = speedToPWM(speedSredina);
        outputs->moveSpindleDown(pwm);
        
        // Status na display - Faza 2
        static unsigned long lastDisplayUpdate = 0;
        if (millis() - lastDisplayUpdate > 500) {
            char msg[50];
            sprintf(msg, "Faza: Spust 2/3");
            display->setStatus(msg);
            // Prikaži hitrost samo ob spremembi
            if (speedSredina != lastSpeedPercent) {
                display->setNumber("nSpeed", speedSredina);
                lastSpeedPercent = speedSredina;
            }
            lastDisplayUpdate = millis();
        }
    }
    // FAZA 3: Od (angleStop + 2°) do angleStop
    // Končno brusenje
    else if (currentAngle > angleStop) {
        uint8_t pwm = speedToPWM(speedKoncni);
        outputs->moveSpindleDown(pwm);
        
        // Status na display - Faza 3
        static unsigned long lastDisplayUpdate = 0;
        if (millis() - lastDisplayUpdate > 500) {
            char msg[50];
            sprintf(msg, "Faza: Spust 3/3");
            display->setStatus(msg);
            // Prikaži hitrost samo ob spremembi
            if (speedKoncni != lastSpeedPercent) {
                display->setNumber("nSpeed", speedKoncni);
                lastSpeedPercent = speedKoncni;
            }
            lastDisplayUpdate = millis();
        }
    }
    // Dosežen angleStop - konec spusta
    else {
        outputs->stopSpindle();
        Serial.print("Spust končan pri kotu: ");
        Serial.println(currentAngle);
        
        // Izpis statistike obratov
        unsigned long totalRevs = currentRevs - startRevolutions;
        float angleRange = angleStart - currentAngle;
        if (angleRange > 0.0) {
            float actualRevPerAngle = (float)totalRevs / angleRange;
            Serial.print("Obratov: ");
            Serial.print(totalRevs);
            Serial.print(" / kot: ");
            Serial.print(angleRange, 1);
            Serial.print("° = ");
            Serial.print(actualRevPerAngle, 1);
            Serial.println(" obr/°");
        }
        
        display->setStatus("Vračam v start...");
        delay(100);  // Kratka pavza
        enterState(CYCLE_UP);
        return;
    }
    
    // Preverjanje obratov na stopinjo (vsako stopinjo)
    if (revPerAngle > 0.0 && (lastCheckedAngle - currentAngle) >= 1.0) {
        unsigned long revsSinceLastCheck = currentRevs - lastCheckedRevs;
        float angleDiff = lastCheckedAngle - currentAngle;
        float actualRevPerAngle = (float)revsSinceLastCheck / angleDiff;
        
        // Dovoljeno odstopanje ±30%
        float minExpected = revPerAngle * 0.7;
        float maxExpected = revPerAngle * 1.3;
        
        if (actualRevPerAngle < minExpected) {
            // Počasnejši pomik - možena zamašitev
            Serial.print("OPOZORILO: Počasen spust! ");
            Serial.print(actualRevPerAngle, 1);
            Serial.print(" obr/° (pričakovano: ");
            Serial.print(revPerAngle, 1);
            Serial.println(" obr/°)");
            
            char msg[50];
            sprintf(msg, "OPOZORILO: Počasen spust!");
            display->setStatus(msg);
        } else if (actualRevPerAngle > maxExpected) {
            // Prehiter pomik - moženo drsenje
            Serial.print("OPOZORILO: Prehiter spust! ");
            Serial.print(actualRevPerAngle, 1);
            Serial.print(" obr/° (pričakovano: ");
            Serial.print(revPerAngle, 1);
            Serial.println(" obr/°)");
        }
        
        // Posodobi zadnje preverjanje
        lastCheckedAngle = currentAngle;
        lastCheckedRevs = currentRevs;
    }
    
    // Varnost - preveri minimalni kot
    if (currentAngle < calibratedMin) {
        errorOccurred = true;
        errorMessage = "ALARM: Presezen min. kot";
        outputs->stopSpindle();
        enterState(CYCLE_ERROR);
        return;
    }
    
    // Varnost - timeout (20 sekund)
    if (millis() - stateStartTime > SPINDLE_MOVE_TIMEOUT) {
        errorOccurred = true;
        errorMessage = "TIMEOUT: Vreteno ni dosegelo cilja!";
        outputs->stopSpindle();
        enterState(CYCLE_ERROR);
    }
}

// ===== STATE: UP =====
void AutoCycle::processStateUp() {
    // PREVERI NAPAKO CILINDRA - ustavi cikel ob napaki
    if (outputs->hasKnifeCylinderError()) {
        outputs->stopSpindle();
        errorMessage = outputs->getKnifeCylinderError();
        errorOccurred = true;  // KRITIČNO: Nastavi flag za ERROR stanje!
        enterState(CYCLE_ERROR);
        return;
    }
    
    float currentAngle = angleSensor->getCalibratedAngle(angleOffset);
    unsigned long currentRevs = inputs->getRevolutions();
    
    // Prvi entry - začni dvig
    if (millis() - stateStartTime < 50) {
        startRevolutions = currentRevs;
        lastCheckedAngle = currentAngle;
        lastCheckedRevs = currentRevs;
        
        Serial.print("Začetek dviga - od: ");
        Serial.print(currentAngle);
        Serial.print(" do: ");
        Serial.println(angleStart);
    }
    
    // FAZA 1 (Obrnjena od DOWN): Od angleStop do (angleStop + 2.0°)
    // Končno brusenje - hitro
    if (currentAngle < (angleStop + 2.0)) {
        uint8_t pwm = speedToPWM(speedKoncni);
        outputs->moveSpindleUp(pwm);
        
        // Status na display - Faza 1
        static unsigned long lastDisplayUpdate = 0;
        if (millis() - lastDisplayUpdate > 500) {
            char msg[50];
            sprintf(msg, "Faza: Dvig 1/3");
            display->setStatus(msg);
            // Prikaži hitrost samo ob spremembi
            if (speedKoncni != lastSpeedPercent) {
                display->setNumber("nSpeed", speedKoncni);
                lastSpeedPercent = speedKoncni;
            }
            lastDisplayUpdate = millis();
        }
    }
    // FAZA 2: Od (angleStop + 2.0°) do (angleStart - 2.0°)
    // Glavno območje - srednja hitrost
    else if (currentAngle < (angleStart - 2.0)) {
        uint8_t pwm = speedToPWM(speedSredina);
        outputs->moveSpindleUp(pwm);
        
        // Status na display - Faza 2
        static unsigned long lastDisplayUpdate = 0;
        if (millis() - lastDisplayUpdate > 500) {
            char msg[50];
            sprintf(msg, "Faza: Dvig 2/3");
            display->setStatus(msg);
            // Prikaži hitrost samo ob spremembi
            if (speedSredina != lastSpeedPercent) {
                display->setNumber("nSpeed", speedSredina);
                lastSpeedPercent = speedSredina;
            }
            lastDisplayUpdate = millis();
        }
    }
    // FAZA 3: Od (angleStart - 2.0°) do angleStart
    // Začetno območje - hitro
    else if (currentAngle < angleStart) {
        uint8_t pwm = speedToPWM(speedZacetni);
        outputs->moveSpindleUp(pwm);
        
        // Status na display - Faza 3
        static unsigned long lastDisplayUpdate = 0;
        if (millis() - lastDisplayUpdate > 500) {
            char msg[50];
            sprintf(msg, "Faza: Dvig 3/3");
            display->setStatus(msg);
            // Prikaži hitrost samo ob spremembi
            if (speedZacetni != lastSpeedPercent) {
                display->setNumber("nSpeed", speedZacetni);
                lastSpeedPercent = speedZacetni;
            }
            lastDisplayUpdate = millis();
        }
    }
    // Dosežen angleStart - konec dviga
    else if (currentAngle >= angleStart) {
        outputs->stopSpindle();
        
        Serial.print("Dvig končan pri kotu: ");
        Serial.println(currentAngle);
        
        // Izpis statistike obratov
        unsigned long totalRevs = currentRevs - startRevolutions;
        float angleRange = currentAngle - angleStop;
        if (angleRange > 0.0) {
            float actualRevPerAngle = (float)totalRevs / angleRange;
            Serial.print("Obratov: ");
            Serial.print(totalRevs);
            Serial.print(" / kot: ");
            Serial.print(angleRange, 1);
            Serial.print("° = ");
            Serial.print(actualRevPerAngle, 1);
            Serial.println(" obr/°");
        }
        
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
        return;
    }
    
    // Preverjanje obratov na stopinjo (vsako stopinjo)
    if (revPerAngle > 0.0 && (currentAngle - lastCheckedAngle) >= 1.0) {
        unsigned long revsSinceLastCheck = currentRevs - lastCheckedRevs;
        float angleDiff = currentAngle - lastCheckedAngle;
        float actualRevPerAngle = (float)revsSinceLastCheck / angleDiff;
        
        // Dovoljeno odstopanje ±30%
        float minExpected = revPerAngle * 0.7;
        float maxExpected = revPerAngle * 1.3;
        
        if (actualRevPerAngle < minExpected) {
            // Počasnejši pomik - možna zamašitev
            Serial.print("OPOZORILO: Počasen dvig! ");
            Serial.print(actualRevPerAngle, 1);
            Serial.print(" obr/° (pričakovano: ");
            Serial.print(revPerAngle, 1);
            Serial.println(" obr/°)");
            
            char msg[50];
            sprintf(msg, "OPOZORILO: Počasen dvig!");
            display->setStatus(msg);
        } else if (actualRevPerAngle > maxExpected) {
            // Prehiter pomik - možno drsenje
            Serial.print("OPOZORILO: Prehiter dvig! ");
            Serial.print(actualRevPerAngle, 1);
            Serial.print(" obr/° (pričakovano: ");
            Serial.print(revPerAngle, 1);
            Serial.println(" obr/°)");
        }
        
        // Posodobi zadnje preverjanje
        lastCheckedAngle = currentAngle;
        lastCheckedRevs = currentRevs;
    }
    
    // Varnost - preveri maksimalni kot
    if (currentAngle > calibratedMax) {
        errorOccurred = true;
        errorMessage = "ALARM: Presežen max. kot";
        outputs->stopSpindle();
        enterState(CYCLE_ERROR);
        return;
    }
    
    // Varnost - timeout (20 sekund)
    if (millis() - stateStartTime > SPINDLE_MOVE_TIMEOUT) {
        errorOccurred = true;
        errorMessage = "TIMEOUT: Vreteno ni doseželo cilja!";
        outputs->stopSpindle();
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
    
    // Status na display
    char msg[50];
    sprintf(msg, "KONČANO - %d ciklov", completedCycles);
    display->setStatus(msg);
    
    // Nazaj v IDLE
    enterState(CYCLE_IDLE);
}

// ===== STATE: ERROR =====
void AutoCycle::processStateError() {
    // Napaka - emergency stop
    // Izvede se samo enkrat ob prvem vstopu v ERROR stanje
    if (millis() - stateStartTime < 50) {
        outputs->emergencyStop();
        
        Serial.println("!!! NAPAKA V CIKLU !!!");
        Serial.println(errorMessage);
        
        // Status na display
        display->setStatus(errorMessage.c_str());
        
        // Gumb bStart naj prikazuje "RESET"
        display->setText("bStart", "RESET");
    }
    
    // Ostani v ERROR stanju dokler uporabnik ne pritisne RESET (bStart)
}

// ===== KONTROLA =====
// Stara verzija start(uint8_t) je odstranjena - nova verzija je na začetku datoteke (linija 16)
// startContinuous() je odstranjen - uporabi start(0, ...) za neprekinjeni cikel

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
