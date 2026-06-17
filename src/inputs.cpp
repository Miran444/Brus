#include "inputs.h"

BrusInputs::BrusInputs() {
    inputState = 0;
    lastInputState = 0;
    revolutionCount = 0;
    lastCounterState = false;
    tempAlarm = false;
    lastReadTime = 0;
    
    // AS5600 angleEncoder bo nastavljen kasneje z setAngleEncoder()
    angleEncoder = nullptr;
    
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
    
    // Inicializacija AS5600 DIR pin (direction polarity)
    // LOW (GND) = values increase clockwise
    // HIGH (VDD) = values increase counterclockwise
    // POMEMBNO: Samo pinMode - dejanska vrednost se nastavi v main.cpp iz Preferences
    pinMode(AS5600_DIR, OUTPUT);
    Serial.println("AS5600 DIR pin inicializiran (vrednost bo nastavljena iz Preferences)");
    
    // Opomba: AS5600 angleEncoder se inicializira kasneje v main.cpp,
    // POTEM ko se iz Preferences naloži as5600DirCW in nastavi DIR pin
    // angleEncoder->begin(&Wire);  // ODKOMENTIRANO - se kliče v main.cpp
    
    // Prvo branje
    update();
    
    // Izpis začetnega stanja ob zagonu
    Serial.println("===== ZAČETNO STANJE VHODOV =====");
    
    S1Mode mode = getS1Mode();
    Serial.print("S1 (Način): ");
    if (mode == MODE_OFF) Serial.println("OFF");
    else if (mode == MODE_MANUAL) Serial.println("ROČNO");
    else if (mode == MODE_AUTO) Serial.println("AVTOMATSKO");
    
    S2Cycles cycles = getS2Cycles();
    Serial.print("S2 (Cikli): ");
    if (cycles == CYCLES_NONE) Serial.println("OFF");
    else if (cycles == CYCLES_1) Serial.println("1 CIKEL");
    else if (cycles == CYCLES_2) Serial.println("2 CIKLA");
    else if (cycles == CYCLES_3) Serial.println("3 CIKLI");
    else if (cycles == CYCLES_4) Serial.println("4 CIKLE");
    else if (cycles == CYCLES_CONTINUOUS) Serial.println("NEPREKINJENO");
    else Serial.println("?");
    
    Serial.print("Tipke: ");
    if (isResetPressed()) Serial.print("RESET ");
    if (getInputBit(IN_S41_DOL)) Serial.print("S41-DOL ");
    if (getInputBit(IN_S42_GOR)) Serial.print("S42-GOR ");
    Serial.println();
    
    Serial.print("Senzorji: ");
    if (isSpindleAtBottom()) Serial.print("SPODAJ ");
    // if (isSpindleTilted()) Serial.print("NAKLON ");  // ZASTARELO - TILT ni več v uporabi
    if (isKnifeIn()) Serial.print("NOŽ-IN ");
    if (isKnifeOut()) Serial.print("NOŽ-VEN ");
    Serial.println();
    
    Serial.print("Raw vrednost: 0x");
    Serial.print(inputState, HEX);
    Serial.print(" (bin: ");
    for (int i = 15; i >= 0; i--) {
        Serial.print((inputState >> i) & 0x01);
        if (i == 8) Serial.print(" ");
    }
    Serial.println(")");
    Serial.println("==================================");
}

uint16_t BrusInputs::readSN65HVS882() {
    uint8_t chip1 = 0;  // Prvi čip (zgornji byte - biti 15-8)
    uint8_t chip2 = 0;  // Drugi čip (spodnji byte - biti 7-0)
    
    // Onemogoči interrupts za stabilno SPI branje (prepreči UART motnje)
    noInterrupts();
    
    // 1. Load pulse: LD LOW -> HIGH (naloži paralelne vhode v shift register)
    digitalWrite(SPI_LD, LOW);
    delayMicroseconds(8);   // Load pulse width - optimizirano
    digitalWrite(SPI_LD, HIGH);
    delayMicroseconds(5);   // Setup time
    
    // 2. Preberi PRVI ČIP (8 bitov) - biti 15-8
    // Po LOAD je bit 7 že na MISO
    chip1 |= (1 << 7) * digitalRead(SPI_MISO);  // Bit 7 (MSB)
    
    // CLK HIGH shifta naslednji bit na MISO, nato beremo
    for (int i = 1; i < 8; i++) {
        digitalWrite(SPI_CLK, HIGH);  // Rising edge shifta bit (7-i)
        delayMicroseconds(3);         // Clock high time
        chip1 |= (1 << (7 - i)) * digitalRead(SPI_MISO);  // Preberi bit 6,5,4,3,2,1,0
        digitalWrite(SPI_CLK, LOW);
        delayMicroseconds(2);
    }
    
    // 3. Preberi DRUGI ČIP (8 bitov) - biti 7-0
    for (int i = 0; i < 8; i++) {
        digitalWrite(SPI_CLK, HIGH);  // Rising edge shifta bit
        delayMicroseconds(3);         // Clock high time
        chip2 |= (1 << (7 - i)) * digitalRead(SPI_MISO);  // Preberi bit 7,6,5,4,3,2,1,0
        digitalWrite(SPI_CLK, LOW);
        delayMicroseconds(2);
    }
    
    // Ponovno omogoči interrupts
    interrupts();
    
    // 4. Združi oba čipa v 16-bitno vrednost
    uint16_t data = ((uint16_t)chip1 << 8) | chip2;
    
    // DEBUG: Izpis posameznih čipov
    static uint8_t lastChip1 = 0;
    static uint8_t lastChip2 = 0;
    if (chip1 != lastChip1 || chip2 != lastChip2) {
        Serial.print("[SPI DEBUG] Chip1: 0x");
        Serial.print(chip1, HEX);
        Serial.print(" Chip2: 0x");
        Serial.print(chip2, HEX);
        Serial.print(" => 0x");
        Serial.println(data, HEX);
        lastChip1 = chip1;
        lastChip2 = chip2;
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
    
    // Rate limiting - preberi vhode samo 10x/sekundo
    unsigned long currentTime = millis();
    if (currentTime - lastReadTime >= READ_INTERVAL_MS) {
        lastReadTime = currentTime;
        lastInputState = inputState;
        inputState = readSN65HVS882();
        
        // ===== DEBUG: Izpis vhodov samo ob spremembi =====
        if (inputState != lastInputState) {
            Serial.print("[INPUTS DEBUG] 16-bit: ");
            for (int i = 15; i >= 0; i--) {
                Serial.print((inputState >> i) & 0x01);
                if (i == 8) Serial.print(" ");  // Presledek med byte1 in byte2
            }
            Serial.print(" (0x");
            Serial.print(inputState, HEX);
            Serial.println(")");
        }
    }
    
    // Posodobi AS5600 encoder (če je inicializiran)
    if (angleEncoder != nullptr) {
        angleEncoder->update();
    }
    
    // Števec obratov - detekcija naraščajočega robu
    bool currentCounter = getInputBit(IN_S45_STEVEC);
    if (currentCounter && !lastCounterState) {
        revolutionCount++;
    }
    lastCounterState = currentCounter;
    
    // Temperatura alarm (TOK signal je active-low: LOW=alarm, HIGH=OK)
    tempAlarm = !digitalRead(TEMP_ALARM);
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
// ZASTARELO - TILT funkcionalnost ni več v uporabi
// bool BrusInputs::isSpindleTilted() {
//     // Izbira vira: AS5600 ali S44 stikalo
//     if (USE_AS5600_FOR_TILT && angleEncoder != nullptr && angleEncoder->isSensorPresent()) {
//         // Uporabi AS5600 magnetic encoder
//         return angleEncoder->isTiltAngleReached();
//     } else {
//         // Uporabi S44 stikalo kot fallback
//         return getInputBit(IN_S44_NAKLON);
//     }
// }

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
    if (angleEncoder != nullptr && angleEncoder->isSensorPresent()) {
        return angleEncoder->getAngle();  // Vrne surovi kot brez offseta
    }
    return -1.0; // Napaka
}

// Opomba: calibrateAngleZero() je odstranjena.
// Offset (as5600AngleOffset) se upravlja v main.cpp in je shranjen v Preferences.

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
