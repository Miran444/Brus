#include "nextion.h"

NextionDisplay::NextionDisplay() {
    serial = nullptr;
    lastUpdateTime = 0;
    currentPage = 0;  // Začetna stran
    
    // Inicializacija bufferjev
    lastMode = "";
    lastCycles = 0;
    lastCompletedCycles = 0;
    lastAngle = -999.0;
    lastStatus = "";
    lastBrusState = false;
    lastPnevState = false;
    lastSpindleMoving = false;
    lastSpindleSpeed = 255;
    lastAngleStart = -999.0;
    lastAngleStop = -999.0;
    
    // Callback funkcije
    touchCallback = nullptr;
    pageCallback = nullptr;
    stringCallback = nullptr;
    numericCallback = nullptr;
    
    // Buffer management
    rxBufferIndex = 0;
    lastRxTime = 0;
    errorCount = 0;
    eventsProcessed = 0;
    
    // Temp buffer za startup sequence
    tempBufferSize = 0;
    hasTempData = false;
    
    // Startup detection
    startupDetected = false;
    displayReady = false;  // Display še ni pripravljen
}

void NextionDisplay::begin() {
    // Inicializacija UART2 za Nextion
    serial = new HardwareSerial(2);
    serial->begin(NEXTION_BAUD, SERIAL_8N1, NEXTION_RX, NEXTION_TX);
    
    delay(500); // Počakaj da se Nextion inicializira
    
    // Počisti buffer
    clearBuffer();
    
    Serial.println("[NEXTION] Inicializiran na UART2");
    Serial.print("[NEXTION] Baud: ");
    Serial.println(NEXTION_BAUD);
    
    // Reset Nextion display
    // POMEMBNO: Pošlji direktno brez displayReady preverjanja (je še false)
    Serial.println("[NEXTION] Pošiljam reset ukaz...");
    serial->print("rest");
    serial->write(0xFF);
    serial->write(0xFF);
    serial->write(0xFF);
    
    // Počakaj na startup sekvenco: printh 00 00 00 ff ff ff 88 ff ff ff (10 bytov)
    Serial.println("[NEXTION] Čakam na startup sekvenco (10B)...");
    unsigned long timeout = millis() + 3000;  // 3 sekunde timeout
    bool startupReceived = false;
    
    while (millis() < timeout) {
        if (serial->available() >= 10) {
            uint8_t buf[10];
            serial->readBytes(buf, 10);
            
            // Preveri startup sekvenco
            if (buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x00 &&
                buf[3] == 0xFF && buf[4] == 0xFF && buf[5] == 0xFF &&
                buf[6] == 0x88 && buf[7] == 0xFF && buf[8] == 0xFF && buf[9] == 0xFF) {
                Serial.println("[NEXTION] ✓ Startup sekvenca prejeta (10B)");
                startupReceived = true;
                break;
            } else {
                Serial.print("[NEXTION] Nepričakovani podatki: ");
                for (int i = 0; i < 10; i++) {
                    Serial.print("0x");
                    if (buf[i] < 0x10) Serial.print("0");
                    Serial.print(buf[i], HEX);
                    Serial.print(" ");
                }
                Serial.println();
            }
        }
        delay(10);
    }
    
    if (!startupReceived) {
        Serial.println("[NEXTION] ✗ NAPAKA: Startup sekvenca (10B) ni prejeta!");
        errorCount++;
    } else {
        Serial.println("[NEXTION] ✓ Startup uspešen");
    }
    
    // PAGE eventi se obdelujejo preko callback sistema v handleStringData()
    // Ne čakamo tukaj - pustimo da event handler prevzame nadzor
    
    resetStatistics();
    
    Serial.println("[NEXTION] ═══════════════════════════════");
    Serial.print("[NEXTION] Inicializacija ");
    Serial.println(startupReceived ? "USPEŠNA ✓" : "NEUSPEŠNA ✗");
    Serial.println("[NEXTION] PAGE eventi preko callback sistema");
    Serial.println("[NEXTION] ═══════════════════════════════");
    
    // Display je pripravljen šele po prvem PAGE eventu
    displayReady = false;
}

void NextionDisplay::waitForStartup() {
    // Ta funkcija se uporablja po povrnitvi napajanja ko display
    // že sam pošlje startup sekvenco - NE pošiljamo reset ukaza!
    
    // POMEMBNO: Če serial še ni inicializiran, ga inicializiraj
    if (serial == nullptr) {
        serial = new HardwareSerial(2);
        serial->begin(NEXTION_BAUD, SERIAL_8N1, NEXTION_RX, NEXTION_TX);
        delay(100);  // Počakaj da se UART stabilizira
        Serial.println("[NEXTION] UART2 inicializiran");
    }
    
    Serial.println("[NEXTION] Čakam na startup sekvenco displaya...");
    
    // Počisti buffer
    clearBuffer();
    
    // Počakaj na startup sekvenco v bufferju (max 3 sekunde)
    unsigned long timeout = millis() + 3000;
    bool startupReceived = false;
    
    while (millis() < timeout) {
        if (serial->available() >= 10) {
            uint8_t buf[10];
            serial->readBytes(buf, 10);
            
            // Preveri startup sekvenco
            if (buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x00 &&
                buf[3] == 0xFF && buf[4] == 0xFF && buf[5] == 0xFF &&
                buf[6] == 0x88 && buf[7] == 0xFF && buf[8] == 0xFF && buf[9] == 0xFF) {
                Serial.println("[NEXTION] ✓ Startup sekvenca prejeta (10B)");
                startupReceived = true;
                break;
            } else {
                Serial.print("[NEXTION] Nepričakovani podatki: ");
                for (int i = 0; i < 10; i++) {
                    Serial.print("0x");
                    if (buf[i] < 0x10) Serial.print("0");
                    Serial.print(buf[i], HEX);
                    Serial.print(" ");
                }
                Serial.println();
            }
        }
        delay(10);
    }
    
    if (!startupReceived) {
        Serial.println("[NEXTION] ✗ NAPAKA: Startup sekvenca (10B) ni prejeta!");
        errorCount++;
    }
    
    // Resetiraj statistiko
    resetStatistics();
    
    Serial.println("[NEXTION] ═══════════════════════════════");
    Serial.print("[NEXTION] Re-inicializacija po povrnitvi napajanja ");
    Serial.println(startupReceived ? "USPEŠNA ✓" : "NEUSPEŠNA ✗");
    Serial.println("[NEXTION] Čakam na PAGE eventi...");
    Serial.println("[NEXTION] ═══════════════════════════════");
    
    // Display še ni pripravljen - čakamo na PAGE event
    displayReady = false;
}

void NextionDisplay::sendCommand(const char* cmd) {
    if (!displayReady) {
        return;  // Ignoriraj ukaze dokler display ni pripravljen
    }
    serial->print(cmd);
    endCommand();
}

void NextionDisplay::endCommand() {
    // Nextion protokol zahteva 3x 0xFF na koncu vsake komande
    serial->write(0xFF);
    serial->write(0xFF);
    serial->write(0xFF);
}

// ===== BUFFER MANAGEMENT =====

void NextionDisplay::clearBuffer() {
    while (serial->available()) {
        serial->read();
        yield();
    }
    rxBufferIndex = 0;
}

void NextionDisplay::flushInvalidData() {
    // Počisti buffer če vsebuje neznan format
    int cleared = 0;
    while (serial->available() && cleared < 50) {
        uint8_t b = serial->read();
        Serial.print("[NEXTION] Flush: 0x");
        Serial.println(b, HEX);
        cleared++;
        yield();
    }
    if (cleared > 0) {
        errorCount++;
        Serial.print("[NEXTION] Flushed ");
        Serial.print(cleared);
        Serial.println(" invalid bytes");
    }
}

// ===== EVENT PROCESSING =====

bool NextionDisplay::detectStartupSequence() {
    // Nextion startup: printh 00 00 00 ff ff ff 88 ff ff ff (10 bytov)
    if (serial->available() < 10) {
        return false;
    }
    
    // Preberi 10 bytov v temp buffer
    uint8_t buf[10];
    size_t bytesRead = serial->readBytes(buf, 10);
    
    if (bytesRead != 10) {
        // Shrani nepopolne byte v temp buffer za kasneje
        tempBufferSize = bytesRead;
        memcpy(tempBuffer, buf, bytesRead);
        hasTempData = true;
        return false;
    }
    
    // Preveri startup sekvenco: 0x00 0x00 0x00 0xFF 0xFF 0xFF 0x88 0xFF 0xFF 0xFF
    bool isStartup = (buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x00 &&
                      buf[3] == 0xFF && buf[4] == 0xFF && buf[5] == 0xFF &&
                      buf[6] == 0x88 && buf[7] == 0xFF && buf[8] == 0xFF && buf[9] == 0xFF);
    
    if (isStartup) {
        Serial.println("[NEXTION] ✓ Startup sekvenca zaznana (10B)");
        hasTempData = false;  // Počisti temp buffer
        startupDetected = true;  // Nastavi flag za main.cpp
        return true;
    }
    
    // Če NI startup sekvenca, shrani byte v temp buffer da jih lahko obdelamo
    tempBufferSize = 10;
    memcpy(tempBuffer, buf, 10);
    hasTempData = true;
    
    Serial.print("[NEXTION] ⚠ Ni startup (shranil 10B): ");
    for (int i = 0; i < 10; i++) {
        Serial.print("0x");
        if (buf[i] < 0x10) Serial.print("0");
        Serial.print(buf[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    
    return false;
}

void NextionDisplay::handleStartupSequence() {
    // Display se je ponovno zagnal (npr. po povrnitvi napajanja)
    // Display je že sam na page0 (intro stran) - NI POTREBNO pošiljati showPage(0)
    
    // Počisti buffer
    clearBuffer();
    
    // Resetiraj statistiko
    resetStatistics();
    
    // Resetiraj bufferirane vrednosti da se ponovno pošljejo
    lastMode = "";
    lastCycles = 0;
    lastCompletedCycles = 0;
    lastAngle = -999.0;
    lastStatus = "";
    lastBrusState = false;
    lastPnevState = false;
    lastSpindleMoving = false;
    lastSpindleSpeed = 255;
    lastAngleStart = -999.0;
    lastAngleStop = -999.0;
    
    currentPage = 0;  // Display je na page0 (intro)
    
    // Display še ni pripravljen za ukaze - čakamo na PAGE event
    displayReady = false;
    
    Serial.println("[NEXTION] ✓ Re-inicializacija končana");
    Serial.println("[NEXTION] Čakam na prvi PAGE event...");
    
    // POMEMBNO: Počakaj da se display popolnoma priprav preden pošiljamo ukaze
    // Display potrebuje nekaj časa po startup sekvenci preden je pripravljen
    delay(100);
    
    // NE kličemo pageCallback(0) - display bo sam poslal PAGE event
    // ko bo pripravljen in preklopil na naslednjo stran
}

bool NextionDisplay::readEvent() {
    // Najprej preveri če imamo shranjene podatke v temp bufferju
    // (ki niso bili startup sekvenca)
    if (hasTempData && tempBufferSize > 0) {
        Serial.print("[NEXTION] Procesiranje temp bufferja (");
        Serial.print(tempBufferSize);
        Serial.println(" bytov)");
        
        // Procesiraj byte iz temp bufferja
        for (uint8_t i = 0; i < tempBufferSize; i++) {
            uint8_t eventType = tempBuffer[i];
            
            // Terminator (0xFF) - preskoči
            if (eventType == 0xFF) {
                continue;
            }
            
            Serial.print("[NEXTION] Temp byte [0x");
            if (eventType < 0x10) Serial.print("0");
            Serial.print(eventType, HEX);
            Serial.println("] - obdelavam kot invalid");
        }
        
        // Počisti temp buffer
        hasTempData = false;
        tempBufferSize = 0;
        errorCount++;
        return false;  // Poročamo da ni bilo validnega eventa
    }
    
    if (!serial->available()) {
        return false;
    }
    
    // POMEMBNO: Če se začne z 0x00, to je LAHKO startup sekvenca
    // Počakaj da je vsaj 10 bytov preden začneš procesirati
    if (serial->peek() == 0x00) {
        // Če ni dovolj bytov, POČAKAJ (ne procesiraj kot invalid event!)
        if (serial->available() < 10) {
            return false;  // Počakaj na več podatkov
        }
        
        // Zdaj imamo dovolj bytov - preveri za startup sekvenco
        if (detectStartupSequence()) {
            handleStartupSequence();
            return true;  // Startup sekvenca je bila obdelana
        }
        // Če ni bila startup sekvenca, nadaljuj z normalnim procesiranjem
        // (detectStartupSequence je že shranil byte v temp buffer)
    }
    
    // Preberi tip dogodka
    uint8_t eventType = serial->peek();
    
    // Terminator (0xFF) - počisti
    if (eventType == 0xFF) {
        serial->read();
        return false;
    }
    
    // Invalid instruction (0x1A) - napaka: poslali smo neveljaven ukaz
    if (eventType == NEXTION_EVENT_INVALID_INSTRUCTION) {
        serial->read();  // Počisti event type
        // Preberi še terminatorje (0xFF 0xFF 0xFF)
        if (serial->available() >= 3) {
            serial->read();
            serial->read();
            serial->read();
        }
        Serial.println("[NEXTION] ⚠️ NAPAKA: Invalid variable/attribute (0x1A)");
        Serial.println("[NEXTION] -> Poslali smo ukaz za neobstoječ objekt ali atribut");
        errorCount++;
        return false;
    }
    
    // Nov unified protokol: začetek z 0x23 ("#")
    if (eventType == 0x23) {
        return processUnifiedEvent();
    }
    
    // Stari Nextion protokol (za backward compatibility)
    return processEvent(eventType);
}

bool NextionDisplay::processUnifiedEvent() {
    // Format: 0x23 [EVENT_NAME] [VALUE_BYTES] 0xFF 0xFF 0xFF
    // Primer: 0x23 "PAGE:" 0x00 0xFF 0xFF 0xFF
    
    serial->read();  // Počisti 0x23
    
    // Preberi ime eventa (do ':')
    String eventName = "";
    unsigned long timeout = millis() + 1000;
    
    while (millis() < timeout) {
        if (serial->available()) {
            uint8_t c = serial->read();
            
            if (c == ':') {
                eventName += (char)c;
                break;  // Konec imena eventa
            } else if (c == 0xFF) {
                // Napaka - ne bi smeli dobiti terminatorja tukaj
                Serial.println("[NEXTION] Unified event error: terminator before ':'");
                return false;
            } else {
                eventName += (char)c;
            }
        }
        yield();
    }
    
    if (!eventName.endsWith(":")) {
        Serial.print("[NEXTION] Unified event error: invalid event name '");
        Serial.print(eventName);
        Serial.println("'");
        return false;
    }
    
    // Preberi vrednost (max 4 byte do 0xFF)
    uint8_t valueBytes[4] = {0, 0, 0, 0};
    int valueCount = 0;
    int ffCount = 0;
    
    timeout = millis() + 1000;
    while (millis() < timeout && valueCount < 4) {
        if (serial->available()) {
            uint8_t c = serial->read();
            
            if (c == 0xFF) {
                ffCount++;
                if (ffCount >= 3) {
                    break;  // Konec eventa
                }
            } else {
                // Če smo že videli 0xFF ampak ni bilo 3x, to je napaka
                if (ffCount > 0) {
                    Serial.println("[NEXTION] Unified event error: invalid terminator sequence");
                    return false;
                }
                valueBytes[valueCount++] = c;
            }
        }
        yield();
    }
    
    if (ffCount < 3) {
        Serial.println("[NEXTION] Unified event error: incomplete terminator");
        return false;
    }
    
    // Debug izpis
    Serial.print("[NEXTION] Unified event: '");
    Serial.print(eventName);
    Serial.print("' value bytes: ");
    for (int i = 0; i < valueCount; i++) {
        Serial.print("0x");
        if (valueBytes[i] < 0x10) Serial.print("0");
        Serial.print(valueBytes[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    
    // Procesiranje glede na ime eventa
    eventsProcessed++;
    
    if (eventName == "PAGE:") {
        // PAGE event - vrednost je številka strani (1 byte)
        if (valueCount >= 1) {
            uint8_t pageId = valueBytes[0];
            Serial.print("[NEXTION] PAGE event: ");
            Serial.println(pageId);
            
            // Display je pripravljen za ukaze šele po prvem PAGE eventu
            if (!displayReady) {
                displayReady = true;
                Serial.println("[NEXTION] ✓ Display pripravljen za ukaze");
            }
            
            // Pošlji SAMO kot string (ne pageCallback) da se ne kliče 2x
            // handleStringData že vsebuje logiko za PAGE eventi
            String pageData = "PAGE:" + String(pageId);
            if (stringCallback) {
                stringCallback(pageData);
            }
            return true;
        }
    }
    else if (eventName == "PRESS:") {
        // Touch press - vrednost je component ID (1 byte)
        if (valueCount >= 1) {
            uint8_t componentId = valueBytes[0];
            Serial.print("[NEXTION] Touch PRESS: component ");
            Serial.println(componentId);
            
            if (touchCallback) {
                touchCallback(currentPage, componentId, true);
            }
            return true;
        }
    }
    else if (eventName == "RELEASE:") {
        // Touch release - vrednost je component ID (1 byte)
        if (valueCount >= 1) {
            uint8_t componentId = valueBytes[0];
            Serial.print("[NEXTION] Touch RELEASE: component ");
            Serial.println(componentId);
            
            if (touchCallback) {
                touchCallback(currentPage, componentId, false);
            }
            return true;
        }
    }
    else if (eventName.startsWith("ID") && eventName.endsWith(":")) {
        // Slider/Number event - format: "IDxx:" vrednost (1-4 byte)
        String idStr = eventName.substring(2, eventName.length() - 1);  // Dobi številko med "ID" in ":"
        
        // Sestavi vrednost (little-endian če več bytev)
        int32_t value = 0;
        for (int i = 0; i < valueCount && i < 4; i++) {
            value |= ((int32_t)valueBytes[i]) << (i * 8);
        }
        
        Serial.print("[NEXTION] Slider/Value event ID");
        Serial.print(idStr);
        Serial.print(": ");
        Serial.println(value);
        
        // Pošlji kot string v obstoječ format "IDxx:value"
        String data = "ID" + idStr + ":" + String(value);
        if (stringCallback) {
            stringCallback(data);
        }
        return true;
    }
    else if (eventName == "DIR:") {
        // DIR checkbox event (pgMagCal - page8)
        if (valueCount >= 1) {
            uint8_t dirValue = valueBytes[0];
            Serial.print("[NEXTION] DIR event: ");
            Serial.println(dirValue);
            
            // Pošlji kot string "DIR:x"
            String data = "DIR:" + String(dirValue);
            if (stringCallback) {
                stringCallback(data);
            }
            return true;
        }
    }
    else {
        Serial.print("[NEXTION] Unknown unified event: '");
        Serial.print(eventName);
        Serial.println("'");
    }
    
    return false;
}

bool NextionDisplay::processEvent(uint8_t eventType) {
    bool success = false;
    
    switch (eventType) {
        case NEXTION_EVENT_TOUCH_PRESS:
        case NEXTION_EVENT_TOUCH_RELEASE: {
            NextionTouchEvent event;
            if (readTouchEventData(event)) {
                eventsProcessed++;
                if (touchCallback) {
                    touchCallback(event.pageId, event.componentId, event.isPress);
                }
                success = true;
            }
            break;
        }
        
        case NEXTION_EVENT_STRING: {
            String data;
            if (readStringEventData(data)) {
                eventsProcessed++;
                if (stringCallback) {
                    stringCallback(data);
                }
                success = true;
            }
            break;
        }
        
        case NEXTION_EVENT_NUMERIC: {
            int32_t value;
            if (readNumericEventData(value)) {
                eventsProcessed++;
                if (numericCallback) {
                    numericCallback(value);
                }
                success = true;
            }
            break;
        }
        
        default:
            // Neznan tip dogodka
            Serial.print("[NEXTION] Unknown event type: 0x");
            Serial.println(eventType, HEX);
            serial->read();  // Počisti prvi byte
            errorCount++;
            break;
    }
    
    if (!success && eventType != 0xFF) {
        errorCount++;
        flushInvalidData();
    }
    
    return success;
}

bool NextionDisplay::readTouchEventData(NextionTouchEvent& event) {
    // Format: 0x65/0x66 [PageID] [ComponentID] [Event] 0xFF 0xFF 0xFF
    if (serial->available() < 7) {
        return false;
    }
    
    uint8_t buffer[7];
    if (serial->readBytes(buffer, 7) != 7) {
        return false;
    }
    
    // Validacija
    if (buffer[4] != 0xFF || buffer[5] != 0xFF || buffer[6] != 0xFF) {
        Serial.println("[NEXTION] Invalid touch event terminator");
        return false;
    }
    
    event.pageId = buffer[1];
    event.componentId = buffer[2];
    event.isPress = (buffer[0] == NEXTION_EVENT_TOUCH_PRESS);
    
    Serial.print("[NEXTION] Touch ");
    Serial.print(event.isPress ? "Press" : "Release");
    Serial.print(" - Page: ");
    Serial.print(event.pageId);
    Serial.print(", Component: ");
    Serial.println(event.componentId);
    
    return true;
}

bool NextionDisplay::readStringEventData(String& data) {
    // Format: 0x70 [ASCII data...] 0xFF 0xFF 0xFF
    serial->read();  // Počisti 0x70
    
    data = "";
    unsigned long timeout = millis() + 1000;
    int ffCount = 0;
    
    while (millis() < timeout) {
        if (serial->available()) {
            uint8_t c = serial->read();
            
            if (c == 0xFF) {
                ffCount++;
                if (ffCount >= 3) {
                    break;
                }
            } else {
                ffCount = 0;
                data += (char)c;
            }
        }
        yield();
    }
    
    if (ffCount < 3) {
        Serial.println("[NEXTION] String event timeout");
        return false;
    }
    
    Serial.print("[NEXTION] String: '");
    Serial.print(data);
    Serial.println("'");
    
    return true;
}

bool NextionDisplay::readNumericEventData(int32_t& value) {
    // Format: 0x71 [4 bytes little-endian] 0xFF 0xFF 0xFF
    if (serial->available() < 8) {
        return false;
    }
    
    uint8_t buffer[8];
    if (serial->readBytes(buffer, 8) != 8) {
        return false;
    }
    
    // Validacija
    if (buffer[0] != 0x71 || buffer[5] != 0xFF || buffer[6] != 0xFF || buffer[7] != 0xFF) {
        Serial.println("[NEXTION] Invalid numeric event format");
        return false;
    }
    
    // Little-endian 32-bit number
    value = buffer[1] | (buffer[2] << 8) | (buffer[3] << 16) | (buffer[4] << 24);
    
    Serial.print("[NEXTION] Numeric: ");
    Serial.println(value);
    
    return true;
}

void NextionDisplay::setText(const char* obj, const char* text) {
    if (!displayReady) {
        return;  // Ignoriraj ukaze dokler display ni pripravljen
    }
    // Serial.print("[NX->] ");
    // Serial.print(obj);
    // Serial.print(".txt=\"");
    // Serial.print(text);
    // Serial.println("\"");
    serial->print(obj);
    serial->print(".txt=\"");
    serial->print(text);
    serial->print("\"");
    endCommand();
}

void NextionDisplay::setNumber(const char* obj, int32_t value) {
    if (!displayReady) {
        return;  // Ignoriraj ukaze dokler display ni pripravljen
    }
    serial->print(obj);
    serial->print(".val=");
    serial->print(value);
    endCommand();
}

void NextionDisplay::setProgress(const char* obj, int32_t value) {
    // Progress bar - vrednost 0-100
    // Serial.print("[NX->] ");
    // Serial.print(obj);
    // Serial.print(".val=");
    // Serial.println(value);
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

void NextionDisplay::sendRawCommand(const char* cmd) {
    // Pošlje custom ukaz (za atribute kot vvs0, ipd.)
    sendCommand(cmd);
}

void NextionDisplay::setGlobalVariable(const char* varName, int32_t value) {
    // Nastavi globalno spremenljivko (Variable v Nextion HMI)
    serial->print(varName);
    serial->print(".val=");
    serial->print(value);
    endCommand();
}

int32_t NextionDisplay::getGlobalVariable(const char* varName) {
    // Preberi globalno spremenljivko
    serial->print("get ");
    serial->print(varName);
    serial->print(".val");
    endCommand();
    
    // Počakaj na odgovor (4 byte number return)
    delay(50);
    if (serial->available() >= 8) {
        if (serial->read() == 0x71) {  // Numeric return header
            uint32_t value = 0;
            value = serial->read();
            value |= serial->read() << 8;
            value |= serial->read() << 16;
            value |= serial->read() << 24;
            
            // Preberi končna 3x 0xFF
            serial->read();
            serial->read();
            serial->read();
            
            return (int32_t)value;
        }
    }
    return -1;  // Error
}

void NextionDisplay::enableManualButtons(bool enable) {
    // Omogoči/onemogoči vse ročne gumbe
    setButtonState("bBrus", enable);
    setButtonState("bPnev", enable);
    setButtonState("bGor", enable);
    setButtonState("bDol", enable);
}

// ===== POŠILJANJE PODATKOV =====

void NextionDisplay::setMode(const char* mode) {
    // OPOMBA: tMode objekt ne obstaja več na nobeni strani
    // Mode se spreminja samo z stikalom S1, ki sproži preklop strani:
    //   - MODE_OFF -> page1 (pgModeOFF)
    //   - MODE_MANUAL -> page2 (pgModeMAN)  
    //   - MODE_AUTO -> page3 (pgModeAUTO)
    // Ta funkcija je ohranjena za backward compatibility, vendar ne dela ničesar
    
    lastMode = String(mode);  // Posodobi samo buffer
}

void NextionDisplay::setCycles(uint8_t current, uint8_t target) {
    if (current != lastCompletedCycles || target != lastCycles) {
        char buffer[16];
        
        if (target == 99) {  // Continuous mode
            sprintf(buffer, "%d/∞", current);
        } else {
            sprintf(buffer, "%d/%d", current, target);
        }
        
        // Pošlji setText samo če smo na page3 (pgModeAUTO), kjer je polje "tCikli"
        if (currentPage == 3) {
            setText("tCikli", buffer);
        }
        
        lastCompletedCycles = current;
        lastCycles = target;
    }
}

// FUNKCIJA ZAKOMENTIRANA - tAngle objekt ne obstaja več na nobeni strani
// Ta funkcija je povzročala 0x1A napake (Invalid variable/attribute)
/*
void NextionDisplay::setAngle(float angle) {
    if (abs(angle - lastAngle) > 0.1) {  // Posodobi samo če se spremeni za več kot 0.1°
        char buffer[16];
        sprintf(buffer, "%.1f°", angle);
        setText("tAngle", buffer);
        
        lastAngle = angle;
    }
}
*/

void NextionDisplay::setAngleRange(float angleStart, float angleStop) {
    if (abs(angleStart - lastAngleStart) > 0.1) {
        char buffer[16];
        sprintf(buffer, "%.1f\xB0", angleStart);  // \xB0 = ° znak
        setText("tAngleStart", buffer);
        lastAngleStart = angleStart;
        
        // Nastavi tudi globalno spremenljivko (int * 10)
        setGlobalVariable("vaAngleStart", (int32_t)(angleStart * 10));
    }
    
    if (abs(angleStop - lastAngleStop) > 0.1) {
        char buffer[16];
        sprintf(buffer, "%.1f\xB0", angleStop);  // \xB0 = ° znak
        setText("tAngleStop", buffer);
        lastAngleStop = angleStop;
        
        // Nastavi tudi globalno spremenljivko (int * 10)
        setGlobalVariable("vaAngleStop", (int32_t)(angleStop * 10));
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
    // Spremeni barvo texta buttona (aktiven/neaktiven)
    serial->print(button);
    serial->print(".pco=");
    serial->print(enabled ? "0" : "54970");  // 0=črna(aktiven), 54970=siva(neaktiven)
    endCommand();
    
    // Omogoči/onemogoči button (touch)
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

// ===== BRANJE DOGODKOV (deprecated - uporabi callback sistem) =====

bool NextionDisplay::available() {
    return serial->available() > 2;  // Minimalno 3 bajti za validen event
}

uint8_t NextionDisplay::readTouchEvent() {
    // DEPRECATED - uporabi callback sistem z onTouch()
    // Ohranjen za backward compatibility
    NextionTouchEvent event;
    if (readTouchEventData(event)) {
        return event.componentId;
    }
    return 0;
}

uint8_t NextionDisplay::readTouchReleaseEvent() {
    // DEPRECATED - uporabi callback sistem z onTouch()
    // Ohranjen za backward compatibility
    NextionTouchEvent event;
    if (readTouchEventData(event)) {
        return event.componentId;
    }
    return 0;
}

String NextionDisplay::readString() {
    // DEPRECATED - uporabi callback sistem z onString()
    // Ohranjen za backward compatibility
    String data;
    readStringEventData(data);
    return data;
}

bool NextionDisplay::parseAngleSettings(String data, float &startAngle, float &endAngle) {
    // DEPRECATED - parsiranje se sedaj izvaja v callback funkciji
    // Ohranjen za backward compatibility
    // Parsira string formata "ID5:357;ID6:80"
    // ID5:357 = 35.7°, ID6:80 = 8.0°
    
    Serial.print("[NEXTION] Parsing angle string: ");
    Serial.println(data);
    
    int id5Pos = data.indexOf("ID5:");
    int id6Pos = data.indexOf("ID6:");
    
    if (id5Pos == -1 || id6Pos == -1) {
        Serial.println("[NEXTION] ERROR: Invalid format - missing ID5 or ID6");
        return false;
    }
    
    // Najdi separatorje
    int semicolonPos = data.indexOf(";", id5Pos);
    
    if (semicolonPos == -1) {
        Serial.println("[NEXTION] ERROR: Missing semicolon separator");
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
    
    Serial.print("[NEXTION] Parsed: Start=");
    Serial.print(startAngle);
    Serial.print("°, End=");
    Serial.print(endAngle);
    Serial.println("°");
    
    return true;
}

void NextionDisplay::update(unsigned long currentMillis) {
    // Procesiranje dogodkov
    const int MAX_EVENTS_PER_UPDATE = 10;
    int eventsThisUpdate = 0;
    
    while (serial->available() && eventsThisUpdate < MAX_EVENTS_PER_UPDATE) {
        if (readEvent()) {
            eventsThisUpdate++;
        }
        yield();
    }
    
    // Preveri health status
    if (!isHealthy() && (currentMillis - lastUpdateTime) > 10000) {
        Serial.print("[NEXTION] WARNING: Error count high: ");
        Serial.println(errorCount);
        lastUpdateTime = currentMillis;
    }
}

// ===== STATUS IN STATISTIKA =====

void NextionDisplay::resetStatistics() {
    errorCount = 0;
    eventsProcessed = 0;
    Serial.println("[NEXTION] Statistics reset");
}

bool NextionDisplay::isHealthy() const {
    // Komunikacija je zdrava če:
    // - Ni preveč napak (max 10% od vseh dogodkov)
    // - Ali manj kot 100 napak absolut no
    if (eventsProcessed == 0) {
        return errorCount < 10;
    }
    
    float errorRate = (float)errorCount / (float)(eventsProcessed + errorCount);
    return errorRate < 0.1 && errorCount < 100;
}

void NextionDisplay::printDebugInfo() {
    Serial.println("===== NEXTION DEBUG INFO =====");
    Serial.print("Events processed: ");
    Serial.println(eventsProcessed);
    Serial.print("Errors: ");
    Serial.println(errorCount);
    Serial.print("Error rate: ");
    if (eventsProcessed > 0) {
        float errorRate = (float)errorCount / (float)(eventsProcessed + errorCount) * 100.0;
        Serial.print(errorRate, 2);
        Serial.println("%");
    } else {
        Serial.println("N/A");
    }
    Serial.print("Health status: ");
    Serial.println(isHealthy() ? "OK" : "ERROR");
    Serial.print("RX buffer available: ");
    Serial.println(serial->available());
    Serial.println("==============================");
}

// ===== DEBUG =====

void NextionDisplay::test() {
    Serial.println("[NEXTION] Running test...");
    
    // setText("t0", "TEST OK");
    // delay(500);
    
    // setNumber("n0", 123);
    delay(500);
    
    Serial.println("[NEXTION] Test complete");
}
