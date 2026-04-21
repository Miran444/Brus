#include "inputs.h"

BrusInputs::BrusInputs() {
    inputState = 0;
    lastInputState = 0;
    revolutionCount = 0;
    lastCounterState = false;
    tempAlarm = false;
    
    // Inicializacija AS5600
    angleEncoder = new AS5600();
    
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
    
    // Inicializacija I2C za AS5600
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(I2C_FREQUENCY);
    
    // Inicializacija AS5600
    angleEncoder->begin(&Wire);
    
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
    
    // Posodobi AS5600 encoder
    angleEncoder->update();
    
    // Števec obratov - detekcija naraščajočega robu
    bool currentCounter = getInputBit(IN_S45_STEVEC);
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
    // Beremo vse pozicije S2 (nova konfiguracija)
    bool continuous = getInputDebounced(IN_S2_NEPREKINJEN);
    bool cycles7 = getInputDebounced(IN_S2_7_CIKLOV);
    bool cycles6 = getInputDebounced(IN_S2_6_CIKLOV);
    bool cycles5 = getInputDebounced(IN_S2_5_CIKLOV);
    bool cycles4 = getInputDebounced(IN_S2_4_CIKLI);
    bool cycles3 = getInputDebounced(IN_S2_3_CIKLI);
    bool cycles2 = getInputDebounced(IN_S2_2_CIKLA);
    
    // Dekodiranje - samo ena pozicija je aktivna
    if (continuous) return CYCLES_CONTINUOUS;
    if (cycles7) return CYCLES_7;
    if (cycles6) return CYCLES_6;
    if (cycles5) return CYCLES_5;
    if (cycles4) return CYCLES_4;
    if (cycles3) return CYCLES_3;
    if (cycles2) return CYCLES_2;
    
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
    // Izbira vira: AS5600 ali S44 stikalo
    if (USE_AS5600_FOR_TILT && angleEncoder->isSensorPresent()) {
        // Uporabi AS5600 magnetic encoder
        return angleEncoder->isTiltAngleReached();
    } else {
        // Uporabi S44 stikalo kot fallback
        return getInputBit(IN_S44_NAKLON);
    }
}

bool BrusInputs::isSpindleAtBottom() {
    // S43 - varnostno končno stikalo pri ~0° (spodnji položaj)
    return getInputBit(IN_S43_SAFETY);
}

// ===== AS5600 FUNKCIJE =====
float BrusInputs::getSpindleAngle() {
    if (angleEncoder->isSensorPresent()) {
        return angleEncoder->getCalibratedAngle();
    }
    return -1.0; // Napaka
}

void BrusInputs::calibrateAngleZero() {
    if (angleEncoder->isSensorPresent()) {
        angleEncoder->calibrateZero();
    }
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
