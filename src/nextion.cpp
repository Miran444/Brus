#include "nextion.h"

NextionDisplay::NextionDisplay() {
    serial = nullptr;
    lastUpdateTime = 0;
    
    // Inicializacija bufferjev
    lastMode = "";
    lastCycles = 0;
    lastCompletedCycles = 0;
    lastAngle = -999.0;
    lastRevolutions = 0;
    lastMotorState = false;
    lastPumpState = false;
    lastKnifeState = false;
    lastSpindleMoving = false;
}

void NextionDisplay::begin() {
    // Inicializacija UART2 za Nextion
    serial = new HardwareSerial(2);
    serial->begin(NEXTION_BAUD, SERIAL_8N1, NEXTION_RX, NEXTION_TX);
    
    delay(500); // Počakaj da se Nextion inicializira
    
    // Reset Nextion display
    sendCommand("rest");
    delay(100);
    
    Serial.println("Nextion Display inicializiran na UART2");
    
    // Test komunikacije
    sendCommand("page 0");  // Pojdi na stran 0
    setText("t0", "BRUS v1.0");
    
    delay(100);
}

void NextionDisplay::sendCommand(const char* cmd) {
    serial->print(cmd);
    endCommand();
}

void NextionDisplay::endCommand() {
    // Nextion protokol zahteva 3x 0xFF na koncu vsake komande
    serial->write(0xFF);
    serial->write(0xFF);
    serial->write(0xFF);
}

void NextionDisplay::setText(const char* obj, const char* text) {
    serial->print(obj);
    serial->print(".txt=\"");
    serial->print(text);
    serial->print("\"");
    endCommand();
}

void NextionDisplay::setNumber(const char* obj, int32_t value) {
    serial->print(obj);
    serial->print(".val=");
    serial->print(value);
    endCommand();
}

void NextionDisplay::setProgress(const char* obj, int32_t value) {
    // Progress bar - vrednost 0-100
    serial->print(obj);
    serial->print(".val=");
    serial->print(value);
    endCommand();
}

void NextionDisplay::showPage(uint8_t pageId) {
    serial->print("page ");
    serial->print(pageId);
    endCommand();
}

// ===== POŠILJANJE PODATKOV =====

void NextionDisplay::setMode(const char* mode) {
    if (String(mode) != lastMode) {
        setText("tMode", mode);  // Tekstovni objekt "tMode" v HMI
        lastMode = String(mode);
        
        // Spremeni barvo glede na način
        if (strcmp(mode, "AUTO") == 0) {
            sendCommand("tMode.pco=2016");  // Zelena
        } else if (strcmp(mode, "MANUAL") == 0) {
            sendCommand("tMode.pco=1024");  // Modra
        } else {
            sendCommand("tMode.pco=33840"); // Rdeča
        }
    }
}

void NextionDisplay::setCycles(uint8_t current, uint8_t target) {
    if (current != lastCompletedCycles || target != lastCycles) {
        char buffer[16];
        
        if (target == 99) {  // Continuous mode
            sprintf(buffer, "%d/∞", current);
        } else {
            sprintf(buffer, "%d/%d", current, target);
        }
        
        setText("tCycles", buffer);
        lastCompletedCycles = current;
        lastCycles = target;
    }
}

void NextionDisplay::setAngle(float angle) {
    if (abs(angle - lastAngle) > 0.1) {  // Posodobi samo če se spremeni za več kot 0.1°
        char buffer[16];
        sprintf(buffer, "%.1f°", angle);
        setText("tAngle", buffer);
        
        // Številčna vrednost za gauge/progress
        setNumber("nAngle", (int32_t)angle);
        
        lastAngle = angle;
    }
}

void NextionDisplay::setRevolutions(unsigned long rev) {
    if (rev != lastRevolutions) {
        setNumber("nRev", (int32_t)rev);
        lastRevolutions = rev;
    }
}

void NextionDisplay::setMotorStatus(bool grinding, bool pump, bool knife) {
    // Ikone ali LED indikatorji (sprememba barve/slike)
    if (grinding != lastMotorState) {
        setNumber("picMotor", grinding ? 1 : 0);  // Slika 0=OFF, 1=ON
        lastMotorState = grinding;
    }
    
    if (pump != lastPumpState) {
        setNumber("picPump", pump ? 1 : 0);
        lastPumpState = pump;
    }
    
    if (knife != lastKnifeState) {
        setNumber("picKnife", knife ? 1 : 0);
        lastKnifeState = knife;
    }
}

void NextionDisplay::setSpindleStatus(bool moving, bool directionUp, uint8_t speed) {
    if (moving != lastSpindleMoving) {
        if (moving) {
            if (directionUp) {
                setText("tSpindle", "▲ GOR");
                sendCommand("tSpindle.pco=2016");  // Zelena
            } else {
                setText("tSpindle", "▼ DOL");
                sendCommand("tSpindle.pco=1024");  // Modra
            }
        } else {
            setText("tSpindle", "STOP");
            sendCommand("tSpindle.pco=33840");  // Rdeča
        }
        lastSpindleMoving = moving;
    }
    
    // Hitrost
    setProgress("jSpeed", (speed * 100) / 255);  // Progress bar 0-100%
}

void NextionDisplay::setAlarm(const char* message) {
    setText("tAlarm", message);
    sendCommand("tAlarm.pco=63488");  // Rdeča barva teksta
    sendCommand("vis tAlarm,1");      // Prikaži alarm
}

void NextionDisplay::clearAlarm() {
    sendCommand("vis tAlarm,0");  // Skrij alarm
}

// ===== BRANJE DOGODKOV =====

bool NextionDisplay::available() {
    return serial->available() > 0;
}

uint8_t NextionDisplay::readTouchEvent() {
    // Preberi Nextion touch event
    // Format: 0x65 + page_id + component_id + event_type + 0xFF 0xFF 0xFF
    
    if (serial->available() >= 7) {
        if (serial->read() == 0x65) {  // Touch event header
            uint8_t pageId = serial->read();
            uint8_t componentId = serial->read();
            uint8_t eventType = serial->read();  // 0x01 = Press, 0x00 = Release
            
            // Preveri končna 3x 0xFF
            if (serial->read() == 0xFF && serial->read() == 0xFF && serial->read() == 0xFF) {
                Serial.print("Touch event - Page: ");
                Serial.print(pageId);
                Serial.print(", Component: ");
                Serial.print(componentId);
                Serial.print(", Type: ");
                Serial.println(eventType);
                
                return componentId;
            }
        }
    }
    
    return 0;
}

void NextionDisplay::update(unsigned long currentMillis) {
    // Periodična posodobitev - ne preobremeniti UART
    if (currentMillis - lastUpdateTime < NEXTION_UPDATE_INTERVAL) {
        return;
    }
    lastUpdateTime = currentMillis;
    
    // Preberi morebitne dogodke iz Nextiona
    while (available()) {
        readTouchEvent();
    }
}

// ===== DEBUG =====

void NextionDisplay::test() {
    Serial.println("Nextion test...");
    
    setText("t0", "TEST OK");
    delay(500);
    
    setNumber("n0", 123);
    delay(500);
    
    Serial.println("Nextion test končan");
}
