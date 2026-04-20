#include "outputs.h"

BrusOutputs::BrusOutputs() {
    grindingMotorOn = false;
    knifePusherOn = false;
    waterPumpOn = false;
    spindleSpeed = 0;
    spindleDir = SPINDLE_DOWN;
    spindleMoving = false;
}

void BrusOutputs::begin() {
    // Inicializacija digitalnih izhodov za releje
    pinMode(OUT_MOTOR_KAMEN, OUTPUT);
    pinMode(OUT_VENTIL_NOZ, OUTPUT);
    pinMode(OUT_CRPALKA, OUTPUT);
    
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
    
    if (state) {
        Serial.println(">> Motor brusnega kamna VKLOPLJEN");
    } else {
        Serial.println(">> Motor brusnega kamna IZKLOPLJEN");
    }
}

void BrusOutputs::setKnifePusher(bool state) {
    knifePusherOn = state;
    digitalWrite(OUT_VENTIL_NOZ, state ? HIGH : LOW);
    
    if (state) {
        Serial.println(">> Pnevmatski ventil - NOŽ PREKO KAMNA");
    } else {
        Serial.println(">> Pnevmatski ventil - NOŽ NAZAJ");
    }
}

void BrusOutputs::setWaterPump(bool state) {
    waterPumpOn = state;
    digitalWrite(OUT_CRPALKA, state ? HIGH : LOW);
    
    if (state) {
        Serial.println(">> Črpalka vode VKLOPLJENA");
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
    digitalWrite(OUT_VENTIL_NOZ, LOW);
    digitalWrite(OUT_CRPALKA, LOW);
    digitalWrite(OUT_SPINDLE_DIR, LOW);
    
    grindingMotorOn = false;
    knifePusherOn = false;
    waterPumpOn = false;
    spindleSpeed = 0;
    spindleDir = SPINDLE_DOWN;
    spindleMoving = false;
    
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
