/**
 * @file magnet_calibration_page.cpp
 * @brief Primer upravljanja strani pgMagCal (page8) za kalibracijo magneta AS5600
 * 
 * Stran pgMagCal prikazuje:
 * - tCalStatus: Status kalibracije (PREVERJANJE/OPTIMAL/PREMOČEN/PREŠIBEK)
 * - tAGC: AGC vrednost (0-128)
 * - jAGC: Progress bar (0-100, pretvorjeno iz AGC 0-128)
 * - tMagStatus: Status magneta (OPTIMAL/PREMOČEN/PREŠIBEK/NI ZAZNAN)
 * - bRefresh (id=12): Ročna osvežitev
 * - bPage8_Back: Vrnitev na page 4 (pgSettings)
 * 
 * Eventi:
 * - "#PAGE:8" - odprtje strani
 * - "#PRESS:12" - pritisk na bRefresh
 */

#include <Arduino.h>
#include "as5600.h"
#include "nextion.h"

// Globalni objekti (iz main.cpp)
extern AS5600 angleSensor;
extern NextionDisplay display;

// Časovnik za avtomatsko osvežitev
unsigned long lastMagnetCalUpdate = 0;
const unsigned long MAGNET_CAL_UPDATE_INTERVAL = 500;  // Vsake 0.5 sekunde

/**
 * @brief Posodobi prikaz kalibracije magneta
 * 
 * Prebere AGC in Status register ter posodobi vse elemente na strani.
 */
void updateMagnetCalibrationDisplay() {
    // Preveri če je senzor prisoten
    if (!angleSensor.isSensorPresent()) {
        display.setText("tCalStatus", "AS5600 NI ZAZNAN!");
        display.sendRawCommand("tCalStatus.bco=63488");  // Rdeča
        display.setText("tAGC", "--");
        display.setProgress("jAGC", 0);
        display.setText("tMagStatus", "NI ZAZNAN");
        display.sendRawCommand("tMagStatus.pco=63488");  // Rdeča
        return;
    }
    
    // Preberi AGC in Status
    uint8_t agc = angleSensor.getAGC();
    uint8_t status = angleSensor.getMagnetStatus();
    
    // Posodobi AGC številko
    char agcStr[8];
    sprintf(agcStr, "%d", agc);
    display.setText("tAGC", agcStr);
    
    // Posodobi progress bar (AGC je 0-128, progress bar je 0-100)
    // Pretvorba: agc * 100 / 128 = agc * 25 / 32
    uint8_t progressValue = (agc * 100) / 128;
    display.setProgress("jAGC", progressValue);
    
    // Posodobi status magneta iz Status registra
    if (status & 0x08) {
        // Bit 3 - MH: magnet too strong
        display.setText("tMagStatus", "PREMOČEN");
        display.sendRawCommand("tMagStatus.pco=63488");  // Rdeča
    } else if (status & 0x10) {
        // Bit 4 - ML: magnet too weak
        display.setText("tMagStatus", "PREŠIBEK");
        display.sendRawCommand("tMagStatus.pco=63488");  // Rdeča
    } else if (status & 0x20) {
        // Bit 5 - MD: magnet detected
        display.setText("tMagStatus", "OPTIMAL");
        display.sendRawCommand("tMagStatus.pco=31");   // Modra
    } else {
        display.setText("tMagStatus", "NI ZAZNAN");
        display.sendRawCommand("tMagStatus.pco=63488");  // Rdeča
    }
    
    // Posodobi glavni status in barvo progress bara glede na AGC vrednost
    // Optimalno območje: 32-96 (pri 3.3V delovanje)
    if (agc < 32) {
        // AGC prenizkek = magnet preblizu = premočen
        display.setText("tCalStatus", "Magnet PREMOČEN - oddaljite!");
        display.sendRawCommand("tCalStatus.bco=63488");  // Rdeča
        display.sendRawCommand("jAGC.pco=63488");        // Progress bar rdeč
    } else if (agc <= 96) {
        // Optimalno območje
        display.setText("tCalStatus", "Razdalja OPTIMALNA!");
        display.sendRawCommand("tCalStatus.bco=31");   // Zelena
        display.sendRawCommand("jAGC.pco=31");         // Progress bar zelen
    } else {
        // AGC previsok = magnet predaleč = prešibek
        display.setText("tCalStatus", "Magnet PREŠIBEK - približajte!");
        display.sendRawCommand("tCalStatus.bco=63488");  // Rdeča
        display.sendRawCommand("jAGC.pco=63488");        // Progress bar rdeč
    }
}

/**
 * @brief Handler za page event
 * @param pageId ID strani (8 za pgMagCal)
 */
void handlePageChange(uint8_t pageId) {
    if (pageId == 8) {
        // Vstop na stran pgMagCal
        Serial.println("Vstop na stran pgMagCal (page8)");
        
        // Začetno stanje - preverjanje
        display.setText("tCalStatus", "Razdalja magneta: PREVERJANJE...");
        display.sendRawCommand("tCalStatus.bco=1024");  // Modra
        
        // Posodobi prikaz
        updateMagnetCalibrationDisplay();
        
        // Resetiraj časovnik za avtomatsko osvežitev
        lastMagnetCalUpdate = millis();
    }
}

/**
 * @brief Handler za touch event
 * @param pageId ID strani
 * @param componentId ID komponente
 * @param isPress true = press, false = release
 */
void handleTouchEvent(uint8_t pageId, uint8_t componentId, bool isPress) {
    if (pageId == 8 && componentId == 12 && isPress) {
        // bRefresh pritisnjen
        Serial.println("bRefresh pritisnjen - ročna osvežitev");
        updateMagnetCalibrationDisplay();
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== AS5600 Magnet Calibration Page Test ===");
    
    // Inicializacija I2C
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);  // 400kHz
    
    // Inicializacija AS5600
    if (angleSensor.begin(&Wire)) {
        Serial.println("AS5600 inicializiran");
    } else {
        Serial.println("OPOZORILO: AS5600 ni zaznan - simulator mode");
    }
    
    // Inicializacija Nextion
    display.begin();
    display.waitForStartup();
    
    // Registriraj callback funkcije
    display.onPageChange(handlePageChange);
    display.onTouch(handleTouchEvent);
    
    Serial.println("\nPreklopite na stran pgMagCal (page8) za kalibracijo magneta.");
    Serial.println("AGC optimalno območje: 32-96 (cilj ~64)");
    Serial.println("Premikajte magnet dokler ne dosežete optimalnega območja.\n");
}

void loop() {
    unsigned long currentMillis = millis();
    
    // Posodobi Nextion display (procesira evente)
    display.update(currentMillis);
    
    // Posodobi AS5600 (prebere trenutni kot)
    angleSensor.update();
    
    // Avtomatsko osvežitev na strani pgMagCal
    if (display.getCurrentPage() == 8) {
        if (currentMillis - lastMagnetCalUpdate >= MAGNET_CAL_UPDATE_INTERVAL) {
            updateMagnetCalibrationDisplay();
            lastMagnetCalUpdate = currentMillis;
            
            // Debug izpis
            uint8_t agc = angleSensor.getAGC();
            uint8_t status = angleSensor.getMagnetStatus();
            Serial.printf("AGC: %d/128, Status: 0x%02X, Optimal: %s\n", 
                         agc, status, 
                         angleSensor.isMagnetOptimal() ? "YES" : "NO");
        }
    }
    
    delay(10);
}
