#include "as5600.h"

AS5600::AS5600() {
    wire = nullptr;
    i2cAddress = AS5600_ADDRESS;
    currentAngle = 0.0;
    rawAngle = 0;
    tiltDetected = false;
    lastTiltState = false;
    sensorPresent = false;
    magnetStatus = 0;
    simulatedAngle = 45.0;  // Default simulirana vrednost
}

bool AS5600::begin(TwoWire* i2c) {
    wire = i2c;
    
    // Preveri če je senzor prisoten
    wire->beginTransmission(i2cAddress);
    uint8_t error = wire->endTransmission();
    
    if (error == 0) {
        sensorPresent = true;
        Serial.println("AS5600 magnetic encoder najden!");
        
        // Prvi odčitek
        update();
        printStatus();
        
        return true;
    } else {
        sensorPresent = false;
        Serial.println("OPOZORILO: AS5600 ni najden na I2C vodilu!");
        return false;
    }
}

uint8_t AS5600::readRegister8(uint8_t reg) {
    wire->beginTransmission(i2cAddress);
    wire->write(reg);
    wire->endTransmission();
    
    wire->requestFrom(i2cAddress, (uint8_t)1);
    if (wire->available()) {
        return wire->read();
    }
    return 0;
}

uint16_t AS5600::readRegister16(uint8_t regH, uint8_t regL) {
    // POMEMBNO: Preberi oba byte-a v ENI I2C transakciji!
    // Če preberemo v dveh ločenih transakcijah, se lahko register posodobi vmes
    // in dobimo "glitch" (HIGH byte od enega odčitka, LOW byte od drugega)
    
    wire->beginTransmission(i2cAddress);
    wire->write(regH);  // Nastavi naslov na HIGH byte
    wire->endTransmission(false);  // false = repeated start (ne spusti vodila)
    
    // Preberi 2 byte-a naenkrat (HIGH in LOW)
    wire->requestFrom(i2cAddress, (uint8_t)2);
    
    if (wire->available() >= 2) {
        uint8_t highByte = wire->read();
        uint8_t lowByte = wire->read();
        return (highByte << 8) | lowByte;
    }
    
    return 0;  // Napaka pri branju
}

uint16_t AS5600::readRawAngle() {
    return readRegister16(AS5600_RAW_ANGLE_H, AS5600_RAW_ANGLE_L);
}

void AS5600::update() {
    if (!sensorPresent) {
        // Simulator mode - uporabi simuliran kot
        currentAngle = simulatedAngle;
        
        // Detekcija naklona za simuliran kot (uporablja surovi kot brez offseta)
        if (!lastTiltState) {
            if (currentAngle <= TILT_ANGLE_THRESHOLD) {
                tiltDetected = true;
                lastTiltState = true;
            }
        } else {
            if (currentAngle > (TILT_ANGLE_THRESHOLD + ANGLE_HYSTERESIS)) {
                tiltDetected = false;
                lastTiltState = false;
            } else {
                tiltDetected = true;
            }
        }
        return;
    }
    
    // Preberi surovi kot
    rawAngle = readRawAngle();
    
    // Pretvori v stopinje (12-bit = 4096 korakov za 360°)
    currentAngle = (rawAngle * 360.0) / 4096.0;
    
    // Preveri status magneta
    magnetStatus = readRegister8(AS5600_STATUS);
    
    // Detekcija naklona z histerezo (uporablja surovi kot brez offseta)
    if (!lastTiltState) {
        // Če še ni bil detektiran naklon, preveri če smo pod pragom
        if (currentAngle <= TILT_ANGLE_THRESHOLD) {
            tiltDetected = true;
            lastTiltState = true;
        }
    } else {
        // Če je bil že detektiran, potrebujemo histereza (kot mora biti > prag + histereza)
        if (currentAngle > (TILT_ANGLE_THRESHOLD + ANGLE_HYSTERESIS)) {
            tiltDetected = false;
            lastTiltState = false;
        } else {
            tiltDetected = true;
        }
    }
}

// Opomba: calibrateZero() in setAngleOffset() so odstranjene.
// Offset se sedaj upravlja eksterno (as5600AngleOffset v main.cpp)

float AS5600::getAngle() {
    return currentAngle;
}

float AS5600::getCalibratedAngle(float offset) {
    // Izračuna kalibriran kot z upoštevanjem offseta
    float calibrated = currentAngle - offset;
    
    // Normaliziraj na 0-360°
    if (calibrated < 0) {
        calibrated += 360.0;
    } else if (calibrated >= 360.0) {
        calibrated -= 360.0;
    }
    
    return calibrated;
}

bool AS5600::isTiltAngleReached() {
    return tiltDetected;
}

bool AS5600::isMagnetDetected() {
    // Status register: bit 5 = MD (magnet detected)
    return (magnetStatus & 0x20) != 0;
}

uint16_t AS5600::getMagnetStrength() {
    return readRegister16(AS5600_MAGNITUDE_H, AS5600_MAGNITUDE_L);
}

uint8_t AS5600::getAGC() {
    if (!sensorPresent) return 0;
    return readRegister8(AS5600_AGC);
}

uint8_t AS5600::getMagnetStatus() {
    if (!sensorPresent) return 0;
    return readRegister8(AS5600_STATUS);
}

bool AS5600::isMagnetOptimal() {
    if (!sensorPresent) return false;
    
    // Preveri status register
    if (!(magnetStatus & 0x20)) return false;  // Magnet ni zaznan
    if (magnetStatus & 0x08) return false;      // Magnet premočen
    if (magnetStatus & 0x10) return false;      // Magnet prešibek
    
    // Preveri AGC vrednost - optimalno je okoli 64 (sredina 0-128 @ 3.3V)
    uint8_t agc = getAGC();
    // Toleranca: 48-80 (64 ± 16)
    return (agc >= 48 && agc <= 80);
}

void AS5600::printStatus() {
    Serial.println("--- AS5600 STATUS ---");
    Serial.print("Prisoten: ");
    Serial.println(sensorPresent ? "DA" : "NE");
    
    if (!sensorPresent) return;
    
    Serial.print("Magnet: ");
    if (magnetStatus & 0x08) {
        Serial.println("PREMOCEN");  // MH - bit 3, magnet too strong
    } else if (magnetStatus & 0x10) {
        Serial.println("PRESIBEK");  // ML - bit 4, magnet too weak
    } else if (magnetStatus & 0x20) {
        Serial.println("OK");  // MD - bit 5, magnet detected
    } else {
        Serial.println("NI ZAZNAN");
    }
    
    uint8_t agc = getAGC();
    Serial.print("AGC: ");
    Serial.print(agc);
    Serial.print("/128 [");
    if (agc < 48) {
        Serial.print("PREMOCEN - oddaljite magnet");
    } else if (agc > 80) {
        Serial.print("PRESIBEK - priblizajte magnet");
    } else {
        Serial.print("OPTIMAL");
    }
    Serial.println("]");
    
    Serial.print("Moč signala: ");
    Serial.println(getMagnetStrength());
    
    Serial.print("Surova vrednost: ");
    Serial.print(rawAngle);
    Serial.print(" (");
    Serial.print(currentAngle, 2);
    Serial.println("°)");
    
    Serial.print("Tilt (<");
    Serial.print(TILT_ANGLE_THRESHOLD, 1);
    Serial.print("°): ");
    Serial.println(tiltDetected ? "DA" : "NE");
    
    Serial.println("-------------------");
}
