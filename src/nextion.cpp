#include "nextion.h"

NextionDisplay::NextionDisplay() {
    serial = nullptr;
    lastUpdateTime = 0;
    
    // Inicializacija bufferjev
    lastMode = "";
    lastCycles = 0;
    lastCompletedCycles = 0;
    lastAngle = -999.0;
    lastStatus = "";
    lastBrusState = false;
    lastPnevState = false;
    lastSpindleMoving = false;
    lastAngleStart = -999.0;
    lastAngleStop = -999.0;
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
        
        lastAngle = angle;
    }
}

void NextionDisplay::setAngleRange(float angleStart, float angleStop) {
    if (abs(angleStart - lastAngleStart) > 0.1) {
        char buffer[16];
        sprintf(buffer, "%.1f", angleStart);
        setText("xAngleStart", buffer);
        lastAngleStart = angleStart;
    }
    
    if (abs(angleStop - lastAngleStop) > 0.1) {
        char buffer[16];
        sprintf(buffer, "%.1f", angleStop);
        setText("xAngleStop", buffer);
        lastAngleStop = angleStop;
    }
}

void NextionDisplay::setStatus(const char* status) {
    if (String(status) != lastStatus) {
        setText("tStatus", status);
        lastStatus = String(status);
        
        // Spremeni barvo ozadja glede na stanje
        if (strcmp(status, "Stop") == 0) {
            sendCommand("tStatus.bco=63488");  // Rdeča
        } else if (strcmp(status, "Pripravljen") == 0) {
            sendCommand("tStatus.bco=1024");   // Modra
        } else if (strcmp(status, "Run") == 0) {
            sendCommand("tStatus.bco=2016");   // Zelena
        } else {
            // Vse ostalo (vključno z alarm sporočili) = rdeča
            sendCommand("tStatus.bco=63488");  // Rdeča
        }
    }
}

void NextionDisplay::setButtonState(const char* button, bool enabled) {
    // Omogoči/onemogoči button (za ročni način)
    serial->print("tsw ");
    serial->print(button);
    serial->print(",");
    serial->print(enabled ? "1" : "0");
    endCommand();
}

void NextionDisplay::setBrusState(bool active) {
    if (active != lastBrusState) {
        // Spremeni barvo buttona (pco = text color)
        if (active) {
            sendCommand("bBrus.bco=2016");  // Zelena ko je aktiven
        } else {
            sendCommand("bBrus.bco=50712"); // Siva ko ni aktiven
        }
        lastBrusState = active;
    }
}

void NextionDisplay::setPnevState(bool active) {
    if (active != lastPnevState) {
        // Spremeni barvo buttona
        if (active) {
            sendCommand("bPnev.bco=2016");  // Zelena ko je aktiven
        } else {
            sendCommand("bPnev.bco=50712"); // Siva ko ni aktiven
        }
        lastPnevState = active;
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

String NextionDisplay::readString() {
    // Prebere custom string iz Nextiona
    // Format: ASCII znaki do 0xFF 0xFF 0xFF
    String result = "";
    unsigned long timeout = millis() + 1000;  // 1 sekunda timeout
    int ffCount = 0;
    
    while (millis() < timeout) {
        if (serial->available()) {
            uint8_t c = serial->read();
            
            if (c == 0xFF) {
                ffCount++;
                if (ffCount >= 3) {
                    break;  // Konec stringa
                }
            } else {
                ffCount = 0;
                result += (char)c;
            }
        }
    }
    
    return result;
}

bool NextionDisplay::parseAngleSettings(String data, float &startAngle, float &endAngle) {
    // Parsira string formata "ID5:357;ID6:80"
    // ID5:357 = 35.7°, ID6:80 = 8.0°
    
    Serial.print("Parsing string: ");
    Serial.println(data);
    
    int id5Pos = data.indexOf("ID5:");
    int id6Pos = data.indexOf("ID6:");
    
    if (id5Pos == -1 || id6Pos == -1) {
        Serial.println("ERROR: Invalid format - missing ID5 or ID6");
        return false;
    }
    
    // Najdi separatorje
    int semicolonPos = data.indexOf(";", id5Pos);
    
    if (semicolonPos == -1) {
        Serial.println("ERROR: Missing semicolon separator");
        return false;
    }
    
    // Ekstrahiraj vrednosti
    String startStr = data.substring(id5Pos + 4, semicolonPos);  // "ID5:" = 4 znaki
    String endStr = data.substring(id6Pos + 4);  // "ID6:" = 4 znaki
    
    // Odstrani morebitne neželene znake na koncu
    endStr.trim();
    
    // Konvertiraj v float (vrednost je *10)
    int startVal = startStr.toInt();
    int endVal = endStr.toInt();
    
    startAngle = startVal / 10.0;
    endAngle = endVal / 10.0;
    
    Serial.print("Parsed: Start=");
    Serial.print(startAngle);
    Serial.print("°, End=");
    Serial.print(endAngle);
    Serial.println("°");
    
    return true;
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
