#ifndef INPUTS_H
#define INPUTS_H

#include <Arduino.h>
#include <SPI.h>
#include "config.h"
#include "as5600.h"

class BrusInputs {
private:
    SPIClass* spi;
    uint16_t inputState;        // 16 bitov za oba SN65HVS882
    uint16_t lastInputState;    // Za detekcijo sprememb
    
    // AS5600 magnetic encoder
    AS5600* angleEncoder;
    
    // Debounce za tipke
    unsigned long lastDebounceTime[16];
    bool debouncedState[16];
    
    // Števec obratov
    bool lastCounterState;
    unsigned long revolutionCount;
    
    // Temperatura alarm
    bool tempAlarm;
    
    // Privatne funkcije
    uint16_t readSN65HVS882();
    bool getInputBit(uint8_t bitNumber);
    bool getInputDebounced(uint8_t inputNumber);
    
public:
    BrusInputs();
    void begin();
    void update();
    
    // Branje stikala S1 (Mode)
    S1Mode getS1Mode();
    
    // Branje stikala S2 (Cycles)
    S2Cycles getS2Cycles();
    
    // Branje tipk
    bool isResetPressed();
    bool isS41DownPressed();
    bool isS42UpPressed();
    
    // Branje senzorjev
    bool isSpindleTilted();     // Naklon < 10° (AS5600 ali S44)
    bool isSpindleAtBottom();   // Spodnji položaj ~0° (S43 - varnostno)
    unsigned long getRevolutions(); // Števec obratov
    void resetRevolutions();
    
    // AS5600 funkcije
    AS5600* getAngleEncoder() { return angleEncoder; }
    float getSpindleAngle();    // Vrne trenutni kot vretena
    void calibrateAngleZero();  // Kalibracija začetnega kota
    
    // Temperatura alarm
    bool isTempAlarm();
    
    // Debug
    void printInputState();
    uint16_t getRawInputs() { return inputState; }
};

#endif // INPUTS_H
