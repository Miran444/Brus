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
    // Nastavitev pinov za bit-banging (ne uporabljamo SPI hardware)
    pinMode(SPI_LD, OUTPUT);
    pinMode(SPI_CLK, OUTPUT);
    pinMode(SPI_MISO, INPUT);
    
    // Initial state
    digitalWrite(SPI_CLK, LOW);
    digitalWrite(SPI_LD, HIGH);  // HIGH = shift mode ready
    
    pinMode(TEMP_ALARM, INPUT_PULLUP);  // Pull-up za TOK (active-low signal)
    
    Serial.println("BrusInputs inicializiran - Bit-banging mode");
    
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
    
    // 1. Load pulse: LD LOW -> HIGH (naloži paralelne vhode v shift register)
    digitalWrite(SPI_LD, LOW);
    delayMicroseconds(5);  // Load pulse width
    digitalWrite(SPI_LD, HIGH);
    delayMicroseconds(2);  // Setup time
    
    // 2. Shift 16 bitov ven (bit-banging)
    for (int i = 0; i < 16; i++) {
        // Preberi trenutni bit na MISO (MSB first)
        if (digitalRead(SPI_MISO)) {
            data |= (1 << (15 - i));  // MSB first: bit 15, 14, 13, ... 0
        }
        
        // CLK pulse: LOW -> HIGH -> LOW
        digitalWrite(SPI_CLK, HIGH);
        delayMicroseconds(1);
        digitalWrite(SPI_CLK, LOW);
        delayMicroseconds(1);
    }
    
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
    // Feed watchdog
    yield();
    
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
    
    // Temperatura alarm (TOK signal je active-low: LOW=alarm, HIGH=OK)
    tempAlarm = !digitalRead(TEMP_ALARM);
    
    // ===== DEBUG: Izpis vhodov vsaki 2 sekundi =====
    static unsigned long lastDebugPrint = 0;
    if (millis() - lastDebugPrint >= 2000) {
        lastDebugPrint = millis();
        Serial.print("[INPUTS DEBUG] 16-bit: ");
        for (int i = 15; i >= 0; i--) {
            Serial.print((inputState >> i) & 0x01);
            if (i == 8) Serial.print(" ");  // Presledek med byte1 in byte2
        }
        Serial.print(" (0x");
        Serial.print(inputState, HEX);
        Serial.print(") | Bit0=");
        Serial.print(getInputBit(0));
        Serial.print(" | IN_S43(bit0)=");
        Serial.println(getInputBit(IN_S43_SAFETY));
    }
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
    // Beremo vse pozicije S2 (nova konfiguracija po testiranju)
    bool continuous = getInputDebounced(IN_S2_NEPREKINJEN);  // IN10
    bool cycles4 = getInputDebounced(IN_S2_4_CIKLE);         // IN11
    bool cycles3 = getInputDebounced(IN_S2_3_CIKLI);         // IN12
    bool cycles2 = getInputDebounced(IN_S2_2_CIKLA);         // IN13
    bool cycles1 = getInputDebounced(IN_S2_1_CIKEL);         // IN14
    
    // Dekodiranje - samo ena pozicija je aktivna
    if (continuous) return CYCLES_CONTINUOUS;
    if (cycles4) return CYCLES_4;
    if (cycles3) return CYCLES_3;
    if (cycles2) return CYCLES_2;
    if (cycles1) return CYCLES_1;
    
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

bool BrusInputs::isSpindleAtTop() {
    // S46 - končno stikalo pri MAX kotu (IN_USER vhod)
    return getInputBit(IN_USER);
}

bool BrusInputs::isKnifeIn() {
    // Senzor končnega položaja noža NOTER (IN)
    return getInputBit(IN_KNIFE_IN);
}

bool BrusInputs::isKnifeOut() {
    // Senzor končnega položaja noža VEN (OUT)
    return getInputBit(IN_KNIFE_OUT);
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
