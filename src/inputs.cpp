#include "inputs.h"

BrusInputs::BrusInputs() {
    inputState = 0;
    lastInputState = 0;
    revolutionCount = 0;
    lastCounterState = false;
    tempAlarm = false;
    
    // Inicializacija debounce
    for (int i = 0; i < 16; i++) {
        lastDebounceTime[i] = 0;
        debouncedState[i] = false;
    }
}

void BrusInputs::begin() {
    // Inicializacija SPI
    spi = new SPIClass(HSPI);
    spi->begin(SPI_CLK, SPI_MISO, -1, SPI_LD);  // CLK, MISO, MOSI (ni ga), CS
    
    // Nastavitev pinov
    pinMode(SPI_LD, OUTPUT);
    digitalWrite(SPI_LD, HIGH);  // LD idle HIGH
    
    pinMode(TEMP_ALARM, INPUT);
    
    Serial.println("BrusInputs inicializiran - SPI ready");
    
    // Prvo branje
    update();
}

uint16_t BrusInputs::readSN65HVS882() {
    uint16_t data = 0;
    
    // LD pulse - load data from inputs to shift register
    digitalWrite(SPI_LD, LOW);
    delayMicroseconds(1);
    digitalWrite(SPI_LD, HIGH);
    delayMicroseconds(1);
    
    // Beremo 16 bitov (2x 8-bit čipa v kaskadi)
    spi->beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE));
    
    // Beremo prvi bajt (prvi SN65HVS882)
    uint8_t byte1 = spi->transfer(0x00);
    // Beremo drugi bajt (drugi SN65HVS882)
    uint8_t byte2 = spi->transfer(0x00);
    
    spi->endTransaction();
    
    // Kombiniramo v 16-bitno vrednost
    data = (byte1 << 8) | byte2;
    
    return data;
}

bool BrusInputs::getInputBit(uint8_t bitNumber) {
    if (bitNumber >= 16) return false;
    return (inputState >> bitNumber) & 0x01;
}

bool BrusInputs::getInputDebounced(uint8_t inputNumber) {
    if (inputNumber >= 16) return false;
    
    bool currentState = getInputBit(inputNumber);
    unsigned long currentTime = millis();
    
    // Če se stanje spremenilo, resetiraj debounce timer
    if (currentState != debouncedState[inputNumber]) {
        if ((currentTime - lastDebounceTime[inputNumber]) > DEBOUNCE_DELAY_MS) {
            debouncedState[inputNumber] = currentState;
            lastDebounceTime[inputNumber] = currentTime;
        }
    }
    
    return debouncedState[inputNumber];
}

void BrusInputs::update() {
    // Preberi vhode
    lastInputState = inputState;
    inputState = readSN65HVS882();
    
    // Števec obratov - detekcija naraščajočega robu
    bool currentCounter = getInputBit(IN_STEVEC_OBRATOV);
    if (currentCounter && !lastCounterState) {
        revolutionCount++;
    }
    lastCounterState = currentCounter;
    
    // Temperatura alarm
    tempAlarm = digitalRead(TEMP_ALARM);
}

// ===== STIKALO S1 - MODE =====
S1Mode BrusInputs::getS1Mode() {
    bool manual = getInputDebounced(IN_S1_ROCNO);
    bool automatic = getInputDebounced(IN_S1_AVTOMATSKO);
    
    if (manual && !automatic) {
        return MODE_MANUAL;
    } else if (!manual && automatic) {
        return MODE_AUTO;
    } else {
        return MODE_OFF;  // Obe LOW ali obe HIGH = OFF pozicija
    }
}

// ===== STIKALO S2 - CYCLES =====
S2Cycles BrusInputs::getS2Cycles() {
    // Beremo vse pozicije S2
    bool pos1 = getInputDebounced(IN_S2_POS1);
    bool pos2 = getInputDebounced(IN_S2_POS2);
    bool pos3 = getInputDebounced(IN_S2_POS3);
    bool pos4 = getInputDebounced(IN_S2_POS4);
    bool pos5 = getInputDebounced(IN_S2_POS5);
    bool pos6 = getInputDebounced(IN_S2_POS6);
    bool continuous = getInputDebounced(IN_S2_NEPREKINJEN);
    
    // Dekodiranje - samo ena pozicija je aktivna
    if (pos1) return CYCLES_1;
    if (pos2) return CYCLES_2;
    if (pos3) return CYCLES_3;
    if (pos4) return CYCLES_4;
    if (pos5) return CYCLES_5;
    if (pos6) return CYCLES_6;
    if (continuous) return CYCLES_CONTINUOUS;
    
    return CYCLES_NONE;
}

// ===== TIPKE =====
bool BrusInputs::isResetPressed() {
    return getInputDebounced(IN_RESET);
}

bool BrusInputs::isS41DownPressed() {
    // S41 deluje samo v MANUAL načinu
    if (getS1Mode() == MODE_MANUAL) {
        return getInputDebounced(IN_S41_DOL);
    }
    return false;
}

bool BrusInputs::isS42UpPressed() {
    // S42 deluje samo v MANUAL načinu
    if (getS1Mode() == MODE_MANUAL) {
        return getInputDebounced(IN_S42_GOR);
    }
    return false;
}

// ===== SENZORJI =====
bool BrusInputs::isSpindleTilted() {
    return getInputBit(IN_NAKLON_VRETENA);
}

unsigned long BrusInputs::getRevolutions() {
    return revolutionCount;
}

void BrusInputs::resetRevolutions() {
    revolutionCount = 0;
}

bool BrusInputs::isTempAlarm() {
    return tempAlarm;
}

// ===== DEBUG =====
void BrusInputs::printInputState() {
    Serial.print("Inputs (bin): ");
    for (int i = 15; i >= 0; i--) {
        Serial.print(getInputBit(i) ? "1" : "0");
        if (i == 8) Serial.print(" ");  // Razdeli na 2 bajta
    }
    Serial.print(" | Mode: ");
    
    S1Mode mode = getS1Mode();
    if (mode == MODE_OFF) Serial.print("OFF");
    else if (mode == MODE_MANUAL) Serial.print("MANUAL");
    else if (mode == MODE_AUTO) Serial.print("AUTO");
    
    Serial.print(" | Cycles: ");
    Serial.print(getS2Cycles());
    
    Serial.print(" | Rev: ");
    Serial.print(revolutionCount);
    
    if (tempAlarm) Serial.print(" | TEMP ALARM!");
    
    Serial.println();
}
