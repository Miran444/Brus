#ifndef NEXTION_H
#define NEXTION_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "config.h"

// Nextion Event Types
enum NextionEventType {
    NEXTION_EVENT_NONE = 0,
    NEXTION_EVENT_TOUCH_PRESS = 0x65,
    NEXTION_EVENT_TOUCH_RELEASE = 0x66,
    NEXTION_EVENT_CURRENT_PAGE = 0x66,
    NEXTION_EVENT_TOUCH_COORD = 0x67,
    NEXTION_EVENT_SLEEP = 0x86,
    NEXTION_EVENT_WAKE = 0x87,
    NEXTION_EVENT_STARTUP = 0x88,
    NEXTION_EVENT_NUMERIC = 0x71,
    NEXTION_EVENT_STRING = 0x70,
    NEXTION_EVENT_INVALID_INSTRUCTION = 0x1A  // Invalid Variable name or attribute
};

// Event strukture
struct NextionTouchEvent {
    uint8_t pageId;
    uint8_t componentId;
    bool isPress;  // true = press, false = release
};

struct NextionPageEvent {
    uint8_t pageId;
};

struct NextionStringEvent {
    String data;
};

struct NextionNumericEvent {
    int32_t value;
};

// Callback function types
typedef void (*TouchEventCallback)(uint8_t pageId, uint8_t componentId, bool isPress);
typedef void (*PageEventCallback)(uint8_t pageId);
typedef void (*StringEventCallback)(const String& data);
typedef void (*NumericEventCallback)(int32_t value);

class NextionDisplay {
private:
    HardwareSerial* serial;
    unsigned long lastUpdateTime;
    uint8_t currentPage;  // Trenutna stran Nextion displaya
    
    // Bufferi za preverjanje sprememb (posodobi samo če se vrednost spremeni)
    String lastMode;
    uint8_t lastCycles;
    uint8_t lastCompletedCycles;
    float lastAngle;
    String lastStatus;
    bool lastBrusState;
    bool lastPnevState;
    bool lastSpindleMoving;
    uint8_t lastSpindleSpeed;
    float lastAngleStart;
    float lastAngleStop;
    
    // Callback funkcije
    TouchEventCallback touchCallback;
    PageEventCallback pageCallback;
    StringEventCallback stringCallback;
    NumericEventCallback numericCallback;
    
    // Buffer management
    uint8_t rxBuffer[256];
    uint8_t rxBufferIndex;
    unsigned long lastRxTime;
    uint16_t errorCount;
    uint16_t eventsProcessed;
    
    // Temporary buffer za startup sequence detection
    uint8_t tempBuffer[10];  // Nextion startup: printh 00 00 00 ff ff ff 88 ff ff ff
    uint8_t tempBufferSize;
    bool hasTempData;
    
    // Startup detection flag - signalizira da se je display ponovno zagnal
    bool startupDetected;
    bool displayReady;  // Ali je display pripravljen za sprejemanje ukazov
    
    // Helper funkcije
    void sendCommand(const char* cmd);
    void endCommand();
    void clearBuffer();
    bool readEvent();
    bool processEvent(uint8_t eventType);
    bool processUnifiedEvent();  // Nov unified protokol (0x23)
    bool readTouchEventData(NextionTouchEvent& event);
    bool readStringEventData(String& data);
    bool readNumericEventData(int32_t& value);
    void flushInvalidData();
    bool detectStartupSequence();  // Preveri ali je v bufferju startup sekvenca
    void handleStartupSequence();  // Obdela re-inicializacijo po povrnitvi napajanja
    
public:
    NextionDisplay();
    
    void begin();
    void waitForStartup();  // Počakaj na startup sekvenco brez pošiljanja reset ukaza
    void update(unsigned long currentMillis);
    
    // Callback registracija
    void onTouch(TouchEventCallback callback) { touchCallback = callback; }
    void onPageChange(PageEventCallback callback) { pageCallback = callback; }
    void onString(StringEventCallback callback) { stringCallback = callback; }
    void onNumeric(NumericEventCallback callback) { numericCallback = callback; }
    
    // Page management
    void setCurrentPage(uint8_t page) { currentPage = page; }
    uint8_t getCurrentPage() const { return currentPage; }
    
    // Pošiljanje podatkov na display
    void setMode(const char* mode);           // "OFF", "MANUAL", "AUTO"
    void setStatus(const char* status);       // "Stop", "Pripravljen", "Run", "Alarm"
    void setCycles(uint8_t current, uint8_t target);
    void setAngle(float angle);
    void setAngleRange(float angleStart, float angleStop);
    void setButtonState(const char* button, bool enabled);
    void setBrusState(bool active);
    void setPnevState(bool active);
    void setSpindleStatus(bool moving, bool directionUp, uint8_t speed);
    
    // Pošiljanje komand
    void showPage(uint8_t pageId);            // Preklapljanje med zasloni
    void setText(const char* obj, const char* text);
    void setNumber(const char* obj, int32_t value);
    void setProgress(const char* obj, int32_t value);
    void sendRawCommand(const char* cmd);     // Pošlje raw ukaz (za custom atribute)
    void setGlobalVariable(const char* varName, int32_t value);  // Nastavi globalno spremenljivko
    int32_t getGlobalVariable(const char* varName);  // Preberi globalno spremenljivko
    void enableManualButtons(bool enable);  // Omogoči/onemogoči ročne gumbe
    
    // Status in statistika
    uint16_t getErrorCount() const { return errorCount; }
    uint16_t getEventsProcessed() const { return eventsProcessed; }
    void resetStatistics();
    bool isHealthy() const;  // Preveri ali komunikacija normalno deluje
    
    // Startup detection
    bool wasStartupDetected() const { return startupDetected; }
    void clearStartupFlag() { startupDetected = false; }
    
    // Branje dogodkov (deprecated - uporabi callback sistem)
    bool available();
    uint8_t readTouchEvent();
    uint8_t readTouchReleaseEvent();
    String readString();
    bool parseAngleSettings(String data, float &startAngle, float &endAngle);
    
    // Debug
    void test();
    void printDebugInfo();
};

#endif // NEXTION_H
