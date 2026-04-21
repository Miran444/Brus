#include "as5600.h"

AS5600::AS5600() {
    wire = nullptr;
    i2cAddress = AS5600_ADDRESS;
    angleOffset = 0.0;
    isCalibrated = false;
    currentAngle = 0.0;
    calibratedAngle = 0.0;
    rawAngle = 0;
    tiltDetected = false;
    lastTiltState = false;
    sensorPresent = false;
    magnetStatus = 0;
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
    uint8_t highByte = readRegister8(regH);
    uint8_t lowByte = readRegister8(regL);
    return (highByte << 8) | lowByte;
}

uint16_t AS5600::readRawAngle() {
    return readRegister16(AS5600_RAW_ANGLE_H, AS5600_RAW_ANGLE_L);
}

void AS5600::update() {
    if (!sensorPresent) return;
    
    // Preberi surovi kot
    rawAngle = readRawAngle();
    
    // Pretvori v stopinje (12-bit = 4096 korakov za 360°)
    currentAngle = (rawAngle * 360.0) / 4096.0;
    
    // Uporabi offset za kalibriran kot
    calibratedAngle = currentAngle - angleOffset;
    
    // Normaliziraj na 0-360°
    if (calibratedAngle < 0) {
        calibratedAngle += 360.0;
    } else if (calibratedAngle >= 360.0) {
        calibratedAngle -= 360.0;
    }
    
    // Preveri status magneta
    magnetStatus = readRegister8(AS5600_STATUS);
    
    // Detekcija naklona z histerezo
    if (!lastTiltState) {
        // Če še ni bil detektiran naklon, preveri če smo pod pragom
        if (calibratedAngle <= TILT_ANGLE_THRESHOLD) {
            tiltDetected = true;
            lastTiltState = true;
        }
    } else {
        // Če je bil že detektiran, potrebujemo histereza (kot mora biti > prag + histereza)
        if (calibratedAngle > (TILT_ANGLE_THRESHOLD + ANGLE_HYSTERESIS)) {
            tiltDetected = false;
            lastTiltState = false;
        } else {
            tiltDetected = true;
        }
    }
}

void AS5600::calibrateZero() {
    update();
    angleOffset = currentAngle;
    isCalibrated = true;
    
    Serial.print("AS5600 kalibriran - offset: ");
    Serial.print(angleOffset, 2);
    Serial.println("°");
}

void AS5600::setAngleOffset(float offset) {
    angleOffset = offset;
    isCalibrated = true;
}

float AS5600::getAngle() {
    return currentAngle;
}

float AS5600::getCalibratedAngle() {
    return calibratedAngle;
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

void AS5600::printStatus() {
    Serial.println("--- AS5600 STATUS ---");
    Serial.print("Prisoten: ");
    Serial.println(sensorPresent ? "DA" : "NE");
    
    if (!sensorPresent) return;
    
    Serial.print("Magnet: ");
    if (magnetStatus & 0x08) {
        Serial.println("PREŠIBEK");
    } else if (magnetStatus & 0x10) {
        Serial.println("PREMOČEN");
    } else if (magnetStatus & 0x20) {
        Serial.println("OK");
    } else {
        Serial.println("NI ZAZNAN");
    }
    
    Serial.print("Moč signala: ");
    Serial.println(getMagnetStrength());
    
    Serial.print("Surova vrednost: ");
    Serial.print(rawAngle);
    Serial.print(" (");
    Serial.print(currentAngle, 2);
    Serial.println("°)");
    
    Serial.print("Kalibriran kot: ");
    Serial.print(calibratedAngle, 2);
    Serial.println("°");
    
    Serial.print("Offset: ");
    Serial.print(angleOffset, 2);
    Serial.println("°");
    
    Serial.print("Tilt (<");
    Serial.print(TILT_ANGLE_THRESHOLD, 1);
    Serial.print("°): ");
    Serial.println(tiltDetected ? "DA" : "NE");
    
    Serial.println("--------------------");
}
