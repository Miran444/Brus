#include "outputs.h"
#include "inputs.h"

BrusOutputs::BrusOutputs() {
    grindingMotorOn = false;
    knifePusherOn = false;
    waterPumpOn = false;
    spindleSpeed = 0;
    spindleDir = SPINDLE_DOWN;
    spindleMoving = false;
    knifeCylinderState = KNIFE_STOPPED;
    knifeMoveStartTime = 0;
    knifeCylinderError = false;
    knifeCylinderErrorMsg = "";
    inputs = nullptr;
}

void BrusOutputs::begin() {
    // Inicializacija digitalnih izhodov za releje
    pinMode(OUT_MOTOR_KAMEN, OUTPUT);
    pinMode(OUT_VENTIL_NOZ_IN, OUTPUT);
    pinMode(OUT_VENTIL_NOZ_OUT, OUTPUT);
    
    // Inicializacija pinov za motor vretena
    pinMode(OUT_SPINDLE_DIR, OUTPUT);
    pinMode(OUT_SPINDLE_PWM, OUTPUT);
    
    // Nastavi vse v varno stanje
    safeState();
    
    // Inicializacija PWM za motor vretena
    setupPWM();
    
    Serial.println("BrusOutputs inicializiran - vsi izhodi v varnem stanju");
}

void BrusOutputs::setupPWM() {
    // Konfiguracija LEDC PWM za ESP32-S3
    ledcSetup(PWM_CHANNEL_SPINDLE, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(OUT_SPINDLE_PWM, PWM_CHANNEL_SPINDLE);
    ledcWrite(PWM_CHANNEL_SPINDLE, 0);  // Start z 0 PWM
}

// ===== KONTROLA RELEJEV =====
void BrusOutputs::setGrindingMotor(bool state) {
    grindingMotorOn = state;
    digitalWrite(OUT_MOTOR_KAMEN, state ? HIGH : LOW);
    
    // Črpalka se avtomatsko vključi z motorjem kamna
    setWaterPump(state);
    
    if (state) {
        Serial.println(">> Motor brusnega kamna VKLOPLJEN (+ črpalka)");
    } else {
        Serial.println(">> Motor brusnega kamna IZKLOPLJEN (+ črpalka)");
    }
}

void BrusOutputs::setKnifePusher(bool state) {
    knifePusherOn = state;
    
    if (state) {
        Serial.println(">> Kontrola cilindra noža ZAČETA - oscilacija NOTER/VEN");
        // Resetiraj napake
        knifeCylinderError = false;
        knifeCylinderErrorMsg = "";
        // Začni z premakom noter
        moveKnifeIn();
    } else {
        Serial.println(">> Kontrola cilindra noža USTAVLJENA");
        stopKnifeCylinder();
    }
}

void BrusOutputs::setWaterPump(bool state) {
    waterPumpOn = state;
    // Črpalka nima več fizičnega izhoda - odstranjena iz hardware-a
    
    if (state) {
        Serial.println(">> Črpalka vode VKLOPLJENA (avtomatsko z motorjem)");
    } else {
        Serial.println(">> Črpalka vode IZKLOPLJENA");
    }
}

// ===== KONTROLA MOTORJA VRETENA =====
void BrusOutputs::setSpindleSpeed(uint8_t speed) {
    // Omeji na veljavne vrednosti
    if (speed > SPINDLE_SPEED_MAX) {
        speed = SPINDLE_SPEED_MAX;
    }
    
    spindleSpeed = speed;
    ledcWrite(PWM_CHANNEL_SPINDLE, speed);
    
    spindleMoving = (speed > 0);
    
    if (!spindleMoving) {
        Serial.println(">> Vreteno USTAVLJENO");
    }
}

void BrusOutputs::setSpindleDirection(SpindleDirection dir) {
    spindleDir = dir;
    digitalWrite(OUT_SPINDLE_DIR, dir == SPINDLE_UP ? HIGH : LOW);
}

void BrusOutputs::moveSpindleUp(uint8_t speed) {
    setSpindleDirection(SPINDLE_UP);
    setSpindleSpeed(speed);
    Serial.print(">> Vreteno GOR, hitrost: ");
    Serial.println(speed);
}

void BrusOutputs::moveSpindleDown(uint8_t speed) {
    setSpindleDirection(SPINDLE_DOWN);
    setSpindleSpeed(speed);
    Serial.print(">> Vreteno DOL, hitrost: ");
    Serial.println(speed);
}

void BrusOutputs::stopSpindle() {
    setSpindleSpeed(0);
}

// ===== KONTROLA CILINDRA NOŽA =====
void BrusOutputs::moveKnifeIn() {
    if (!inputs) return;
    
    // VARNOSTNO PREVERJANJE: Če sta obe stikali aktivirani, ne delaj ničesar (napaka)
    if (inputs->isKnifeIn() && inputs->isKnifeOut()) {
        Serial.println("!!! NAPAKA: Obe končni stikali cilindra sta aktivirani! Ustavljam.");
        knifeCylinderError = true;
        knifeCylinderErrorMsg = "Napaka: Obe končni stikali aktivirani!";
        stopKnifeCylinder();
        knifePusherOn = false;
        return;
    }
    
    // Preveri če je že noter - če ja, začni premik VEN (oscilacija)
    if (inputs->isKnifeIn()) {
        Serial.println("   -> Cilinder: Že na položaju NOTER, začenjam premik VEN");
        // Direktno vklopi ventil za VEN (brez rekurzivnega klica)
        digitalWrite(OUT_VENTIL_NOZ_IN, LOW);
        digitalWrite(OUT_VENTIL_NOZ_OUT, HIGH);
        knifeCylinderState = KNIFE_MOVING_OUT;
        knifeMoveStartTime = millis();
        return;
    }
    
    // Začni premik NOTER
    digitalWrite(OUT_VENTIL_NOZ_IN, HIGH);
    digitalWrite(OUT_VENTIL_NOZ_OUT, LOW);
    knifeCylinderState = KNIFE_MOVING_IN;
    knifeMoveStartTime = millis();
    Serial.println("   -> Cilinder: Premikanje NOTER");
}

void BrusOutputs::moveKnifeOut() {
    if (!inputs) return;
    
    // VARNOSTNO PREVERJANJE: Če sta obe stikali aktivirani, ne delaj ničesar (napaka)
    if (inputs->isKnifeIn() && inputs->isKnifeOut()) {
        Serial.println("!!! NAPAKA: Obe končni stikali cilindra sta aktivirani! Ustavljam.");
        knifeCylinderError = true;
        knifeCylinderErrorMsg = "Napaka: Obe končni stikali aktivirani!";
        stopKnifeCylinder();
        knifePusherOn = false;
        return;
    }
    
    // Preveri če je že ven - če ja, začni premik NOTER (oscilacija)
    if (inputs->isKnifeOut()) {
        Serial.println("   -> Cilinder: Že na položaju VEN, začenjam premik NOTER");
        // Direktno vklopi ventil za NOTER (brez rekurzivnega klica)
        digitalWrite(OUT_VENTIL_NOZ_IN, HIGH);
        digitalWrite(OUT_VENTIL_NOZ_OUT, LOW);
        knifeCylinderState = KNIFE_MOVING_IN;
        knifeMoveStartTime = millis();
        return;
    }
    
    // Začni premik VEN
    digitalWrite(OUT_VENTIL_NOZ_IN, LOW);
    digitalWrite(OUT_VENTIL_NOZ_OUT, HIGH);
    knifeCylinderState = KNIFE_MOVING_OUT;
    knifeMoveStartTime = millis();
    Serial.println("   -> Cilinder: Premikanje VEN");
}

void BrusOutputs::stopKnifeCylinder() {
    digitalWrite(OUT_VENTIL_NOZ_IN, LOW);
    digitalWrite(OUT_VENTIL_NOZ_OUT, LOW);
    knifeCylinderState = KNIFE_STOPPED;
    knifeMoveStartTime = 0;
    Serial.println("   -> Cilinder: USTAVLJEN");
}

void BrusOutputs::update() {
    // Posodabljanje samo če je cilinder aktiven
    if (!knifePusherOn || !inputs) {
        return;
    }
    
    unsigned long currentTime = millis();
    
    // Preveri timeout
    if (knifeCylinderState != KNIFE_STOPPED) {
        if ((currentTime - knifeMoveStartTime) > KNIFE_SAFETY_TIMEOUT) {
            // TIMEOUT!
            knifeCylinderError = true;
            if (knifeCylinderState == KNIFE_MOVING_IN) {
                knifeCylinderErrorMsg = "Varnostni cas cilindra! (NOTER)";
                Serial.println("!!! NAPAKA: Cilinder ni dosegel končnega položaja NOTER v 10s!");
            } else {
                knifeCylinderErrorMsg = "Varnostni cas cilindra! (VEN)";
                Serial.println("!!! NAPAKA: Cilinder ni dosegel končnega položaja VEN v 10s!");
            }
            stopKnifeCylinder();
            knifePusherOn = false;
            return;
        }
    }
    
    // VARNOSTNO PREVERJANJE: Če sta obe stikali aktivirani, ustavi
    if (inputs->isKnifeIn() && inputs->isKnifeOut()) {
        Serial.println("!!! NAPAKA: Obe končni stikali cilindra sta aktivirani! Ustavljam oscilacijo.");
        knifeCylinderError = true;
        knifeCylinderErrorMsg = "Napaka: Obe končni stikali aktivirani!";
        stopKnifeCylinder();
        knifePusherOn = false;
        return;
    }
    
    // Preveri stanje cilindra
    if (knifeCylinderState == KNIFE_MOVING_IN) {
        // Premaknemo se NOTER - čakamo na senzor IN
        if (inputs->isKnifeIn()) {
            Serial.println("   -> Cilinder: Dosežen končni položaj NOTER");
            // Ustavi ventile in pripravi za naslednji premik
            stopKnifeCylinder();
            delay(50);  // Kratka zakasnitev za stabilizacijo
            // Takoj začni premik VEN
            moveKnifeOut();
        }
    }
    else if (knifeCylinderState == KNIFE_MOVING_OUT) {
        // Premaknemo se VEN - čakamo na senzor OUT
        if (inputs->isKnifeOut()) {
            Serial.println("   -> Cilinder: Dosežen končni položaj VEN");
            // Ustavi ventile in pripravi za naslednji premik
            stopKnifeCylinder();
            delay(50);  // Kratka zakasnitev za stabilizacijo
            // Takoj začni premik NOTER
            moveKnifeIn();
        }
    }
    else if (knifeCylinderState == KNIFE_STOPPED && knifePusherOn) {
        // Če je ustavljen, ampak naj bi bil aktiven, začni oscilacijo
        // To se zgodi na začetku ali po napaki
        Serial.println("   -> Cilinder: Ponovno zaganjam oscilacijo");
        moveKnifeIn();
    }
}

// ===== VARNOST =====
void BrusOutputs::emergencyStop() {
    Serial.println("!!! EMERGENCY STOP !!!");
    
    // Izklop VSEH izhodov
    setGrindingMotor(false);
    setKnifePusher(false);
    setWaterPump(false);
    stopSpindle();
}

void BrusOutputs::safeState() {
    // Varno začetno stanje ob zagonu
    digitalWrite(OUT_MOTOR_KAMEN, LOW);
    digitalWrite(OUT_VENTIL_NOZ_IN, LOW);
    digitalWrite(OUT_VENTIL_NOZ_OUT, LOW);
    digitalWrite(OUT_SPINDLE_DIR, LOW);
    
    grindingMotorOn = false;
    knifePusherOn = false;
    waterPumpOn = false;
    spindleSpeed = 0;
    spindleDir = SPINDLE_DOWN;
    spindleMoving = false;
    knifeCylinderState = KNIFE_STOPPED;
    knifeMoveStartTime = 0;
    knifeCylinderError = false;
    knifeCylinderErrorMsg = "";
    
    Serial.println("Varno stanje nastavljeno - vsi izhodi LOW");
}

// ===== DEBUG =====
void BrusOutputs::printOutputState() {
    Serial.println("--- STANJE IZHODOV ---");
    Serial.print("Motor kamna: ");
    Serial.println(grindingMotorOn ? "ON" : "OFF");
    
    Serial.print("Ventil noža: ");
    Serial.println(knifePusherOn ? "AKTIVIRAN" : "OFF");
    
    Serial.print("Črpalka: ");
    Serial.println(waterPumpOn ? "ON" : "OFF");
    
    Serial.print("Vreteno: ");
    if (spindleMoving) {
        Serial.print(spindleDir == SPINDLE_UP ? "GOR" : "DOL");
        Serial.print(", hitrost: ");
        Serial.println(spindleSpeed);
    } else {
        Serial.println("STOP");
    }
    Serial.println("--------------------");
}
