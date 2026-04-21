#ifndef AS5600_H
#define AS5600_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// AS5600 Register adresy
#define AS5600_RAW_ANGLE_H  0x0C
#define AS5600_RAW_ANGLE_L  0x0D
#define AS5600_ANGLE_H      0x0E
#define AS5600_ANGLE_L      0x0F
#define AS5600_STATUS       0x0B
#define AS5600_AGC          0x1A
#define AS5600_MAGNITUDE_H  0x1B
#define AS5600_MAGNITUDE_L  0x1C

class AS5600 {
private:
    TwoWire* wire;
    uint8_t i2cAddress;
    
    // Kalibracija
    float angleOffset;          // Offset za kalibriranje 0° točke
    bool isCalibrated;
    
    // Trenutne vrednosti
    float currentAngle;         // Trenutni kot (0-360°)
    float calibratedAngle;      // Kalibriran kot glede na offset
    uint16_t rawAngle;          // Surova 12-bit vrednost
    
    // Detekcija naklona
    bool tiltDetected;          // Ali je kot < TILT_ANGLE_THRESHOLD
    bool lastTiltState;         // Za histereza
    
    // Status
    bool sensorPresent;
    uint8_t magnetStatus;
    
    // I2C funkcije
    uint16_t readRawAngle();
    uint8_t readRegister8(uint8_t reg);
    uint16_t readRegister16(uint8_t regH, uint8_t regL);
    
public:
    AS5600();
    
    bool begin(TwoWire* i2c = &Wire);
    void update();
    
    // Kalibracija
    void calibrateZero();           // Nastavi trenutni kot kot 0°
    void setAngleOffset(float offset);
    float getAngleOffset() { return angleOffset; }
    
    // Branje kota
    float getAngle();               // Vrne trenutni kot (0-360°)
    float getCalibratedAngle();     // Vrne kalibriran kot
    uint16_t getRawValue() { return rawAngle; }
    
    // Detekcija naklona
    bool isTiltAngleReached();      // Ali je kot < TILT_ANGLE_THRESHOLD
    
    // Status
    bool isSensorPresent() { return sensorPresent; }
    bool isMagnetDetected();        // Ali je magnet prisoten
    uint8_t getMagnetStatus() { return magnetStatus; }
    uint16_t getMagnetStrength();
    
    // Debug
    void printStatus();
};

#endif // AS5600_H
