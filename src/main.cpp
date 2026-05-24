#include <Arduino.h>
#include <Preferences.h>
#include "inputs.h"
#include "outputs.h"
#include "auto_cycle.h"
#include "nextion.h"
#include "as5600.h"
#include "config.h"

// Globalni objekti
BrusInputs inputs;
BrusOutputs outputs;
AS5600 angleSensor;
NextionDisplay display;
AutoCycle autoCycle(&inputs, &outputs, &angleSensor, &display);
Preferences preferences;

// Časovniki
unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL = 1000; // Izpis vsako sekundo

// Stanje sistema
S1Mode lastMode = MODE_OFF;
bool autoModeActive = false;

// Stanje toggle gumbov (manual control)
bool brusActive = false;     // bBrus - motor kamna
bool pnevActive = false;     // bPnev - ventil noža
bool bGorPressed = false;    // bGor - vreteno gor (pritisnjeno)
bool bDolPressed = false;    // bDol - vreteno dol (pritisnjeno)

// Shranjeni koti (v stopinjah)
float savedAngleStart = 0.0;
float savedAngleStop = 0.0;
bool anglesConfigured = false;  // Ali sta kota nastavljena

// Kalibrirani min/max koti iz referenčnega hoda (že z 0.5° toleranco)
float calibratedMinAngle = 0.0;  // Minimalni kot
float calibratedMaxAngle = 0.0;  // Maksimalni kot
bool anglesCalibrated = false;   // Ali je bila opravljena kalibracija

// Hitrosti vretena (v procentih 50-100%)
uint8_t speedZacetni = 90;  // Faza 1: Start do Start-2.0°
uint8_t speedSredina = 75;  // Faza 2: Start-2.0° do Stop+2.0°
uint8_t speedKoncni = 85;   // Faza 3: Stop+2.0° do Stop
uint8_t speedRocno = 80;    // Hitrost ročnega pomika

// Referenčne vrednosti
float revPerAngle = 0.0;    // Število obratov na stopinjo (iz kalibracije)

// ===== PAGE MANAGEMENT - NOVA STRUKTURA =====
// page0: pgIntro (startup, 5s timeout)
// page1: pgModeOFF (osnovna stran)
// page2: pgModeMAN (ročni način)
// page3: pgModeAUTO (avtomatski način)
// page4: pgSettings (nastavitve menu)
// page5: pgRef (referenčni hod)
// page6: pgSpeed (hitrosti)
// page7: pgAngle (koti)
uint8_t currentPage = 0;  // Trenutna stran
unsigned long introStartTime = 0;
bool introShown = false;
S1Mode lastS1Mode = MODE_OFF;

// Page7 (pgAngle) - Nastavitev kotov
enum AngleSettingMode {
  ANGLE_IDLE = 0,      // Ni aktivno
  ANGLE_SET_START = 1, // Nastavljamo začetni kot
  ANGLE_SET_STOP = 2   // Nastavljamo končni kot
};
AngleSettingMode angleSettingMode = ANGLE_IDLE;
float tempAngleStart = 0.0;  // Začasni koti pred shranjevanjem
float tempAngleStop = 0.0;
bool anglesChanged = false;  // Ali so se koti spremenili (za aktivacijo bSave)

// Page5 (pgRef) - Referenčni hod
enum ReferenceState {
  REF_IDLE = 0,           // Ni aktivno
  REF_FIND_MIN_DOWN = 1,  // Iščemo najnižjo točko (navzdol do S43 aktiven)
  REF_FIND_MIN_UP = 2,    // Gremo gor do S43 neaktiven + 0.5°
  REF_FIND_MAX = 3        // Iščemo maksimalni kot (navzgor do S46)
};
ReferenceState refState = REF_IDLE;
float measuredMinAngle = 0.0;
float measuredMaxAngle = 0.0;
float startMeasurementAngle = 0.0;  // Kot ko S43 postane neaktiven
uint32_t refStartTime = 0;
uint32_t refRevolutions = 0;
uint32_t lastRevCount = 0;
#define ANGLE_TOLERANCE 0.5  // Toleranca 0.5° od S43/S46 stikal

// Deklaracije funkcij
void handleTouchPress(uint8_t componentId);
void handleTouchRelease(uint8_t componentId);
void handlePageChange(uint8_t newPage);
void handleStringData(const String& data);
void loadAnglesFromPreferences();
void saveAnglesToPreferences();
void clearAnglesFromPreferences();
void saveSpeedsToPreferences();
uint8_t speedToPWM(uint8_t percent);
void startReferenceRun();
void updateReferenceRun();
void finishReferenceRun();
void updatePage1AngleDisplay();
void updateBNastaviKoteText();
void updateAutoModeReadiness();

// ===== NEXTION CALLBACK FUNKCIJE =====
void onNextionTouch(uint8_t pageId, uint8_t componentId, bool isPress) {
  if (isPress) {
    handleTouchPress(componentId);
  } else {
    handleTouchRelease(componentId);
  }
}

void onNextionPageChange(uint8_t pageId) {
  handlePageChange(pageId);
}

void onNextionString(const String& data) {
  handleStringData(data);
}

void onNextionNumeric(int32_t value) {
  Serial.print("[NEXTION] Numeric value: ");
  Serial.println(value);
}

void setup() {
  // Inicializacija Serial komunikacije
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=================================");
  Serial.println("  BRUS - KNECHT WEMA USK 200/E  ");
  Serial.println("  ESP32-S3 Kontroler v1.0       ");
  Serial.println("=================================");
  
  // Inicializacija izhodov (NAJPREJ - varno stanje)
  outputs.begin();
  
  // Inicializacija vhodov
  inputs.begin();
  
  // Poveži inputs z outputs za kontrolo cilindra
  outputs.setInputs(&inputs);
  
  // Inicializacija avtomatskega cikla
  autoCycle.begin();
  
  // Inicializacija AS5600 senzorja
  Wire.begin(I2C_SDA, I2C_SCL);
  if (angleSensor.begin(&Wire)) {
    Serial.println("AS5600 senzor inicializiran");
  } else {
    Serial.println("NAPAKA: AS5600 senzor ni najden!");
  }
  
  // Inicializacija Nextion displaya
  display.begin();
  
  // Registriraj callback funkcije
  display.onTouch(onNextionTouch);
  display.onPageChange(onNextionPageChange);
  display.onString(onNextionString);
  display.onNumeric(onNextionNumeric);
  Serial.println("[MAIN] Nextion callbacks registered");
  
  // Nastavi začetno stanje gumbov
  display.setBrusState(false);
  display.setPnevState(false);
  
  // Naloži shranjene kote iz NVS
  loadAnglesFromPreferences();
  
  // Nastavi začetno stran - pgIntro (page0)
  // ZAČASNO IZKLJUČENO zaradi debugginga - gremo direktno na page1
  // display.showPage(1);
  // currentPage = 1;
  // introShown = false;
  // Serial.println("Začetna stran: pgModeOFF (page1)");
  
  Serial.println("Sistem pripravljen!");
  // Serial.println("Intro stran prikazana - preklop na pgModeOFF čez 5s...");
  
  // SIMULATOR MODE: če AS5600 ni prisoten
  if (!angleSensor.isSensorPresent()) {
    Serial.println("========================================");
    Serial.println("AS5600 SIMULATOR MODE aktiven!");
    Serial.println("Ukazi za testiranje:");
    Serial.println("  angle=XX  - nastavi simuliran kot (0-360)");
    Serial.println("  angle=45  - primer: nastavi na 45°");
    Serial.println("");
    Serial.println("SERIAL MONITOR NASTAVITVE:");
    Serial.println("  - Baud: 115200");
    Serial.println("  - Line ending: Newline (NL) ali Both NL & CR");
    Serial.println("========================================");
  }
  
  Serial.println();
}

// ===== OBDELAVA NEXTION STRING EVENTS =====
void handleStringData(const String& data) {
  if (data.length() == 0) return;
  
  // Preveri če je PAGE: event
  if (data.startsWith("PAGE:")) {
    // Format: "PAGE:X" kjer je X hex številka strani (0-7)
    String pageHex = data.substring(5);
    uint8_t newPage = (uint8_t)strtol(pageHex.c_str(), NULL, 16);
    Serial.print("[PAGE] Event: ");
    Serial.println(newPage);
    handlePageChange(newPage);
    return;
  }
  
  // Preveri če je ID14: event (hRocno slider na page2)
  if (data.startsWith("ID14:")) {
    String valueStr = data.substring(5);
    uint8_t value = valueStr.toInt();
    if (value < 50) value = 50;
    if (value > 100) value = 100;
    speedRocno = value;
    Serial.print("[hRocno] Hitrost ročno: ");
    Serial.print(speedRocno);
    Serial.println("%");
    saveSpeedsToPreferences();
    return;
  }
  
  // Preveri če je ID2: event (hZacetni slider na page6)
  if (data.startsWith("ID2:")) {
    String valueStr = data.substring(4);
    uint8_t value = valueStr.toInt();
    if (value < 50) value = 50;
    if (value > 100) value = 100;
    speedZacetni = value;
    Serial.print("[hZacetni] Hitrost začetni: ");
    Serial.print(speedZacetni);
    Serial.println("%");
    saveSpeedsToPreferences();
    return;
  }
  
  // Preveri če je ID6: event (hSredina slider na page6)
  if (data.startsWith("ID6:")) {
    String valueStr = data.substring(4);
    uint8_t value = valueStr.toInt();
    if (value < 50) value = 50;
    if (value > 100) value = 100;
    speedSredina = value;
    Serial.print("[hSredina] Hitrost sredina: ");
    Serial.print(speedSredina);
    Serial.println("%");
    saveSpeedsToPreferences();
    return;
  }
  
  // Preveri če je ID8: event (hKoncni slider na page6)
  if (data.startsWith("ID8:")) {
    String valueStr = data.substring(4);
    uint8_t value = valueStr.toInt();
    if (value < 50) value = 50;
    if (value > 100) value = 100;
    speedKoncni = value;
    Serial.print("[hKoncni] Hitrost končni: ");
    Serial.print(speedKoncni);
    Serial.println("%");
    saveSpeedsToPreferences();
    return;
  }
  
  // Parsing kotov iz page7 (pgAngle) - format: "ID5:357;ID6:80"
  float start, stop;
  if (display.parseAngleSettings(data, start, stop)) {
    // Validacija: Začetni kot mora biti večji od končnega
    if (start <= stop) {
      Serial.println("[ANGLE] NAPAKA: Začetni kot mora biti večji od končnega!");
      Serial.print("  Start: ");
      Serial.print(start, 1);
      Serial.print("°, Stop: ");
      Serial.print(stop, 1);
      Serial.println("°");
      display.setText("tStatus_pg1", "NAPAKA: Začetni > Končni!");
      return;
    }
    
    // Koti uspešno parsirani
    tempAngleStart = start;
    tempAngleStop = stop;
    anglesChanged = true;
    
    Serial.print("[ANGLE] Nastavljeni: Start=");
    Serial.print(tempAngleStart, 1);
    Serial.print("°, Stop=");
    Serial.print(tempAngleStop, 1);
    Serial.println("°");
    display.setText("tStatus_pg1", "Koti OK - pritisnite Save!");
    
    // Posodobi prikaz
    updatePage1AngleDisplay();
    
    // Aktiviraj bSave gumb
    display.setButtonState("bSave", true);
  }
}

void handleTouchPress(uint8_t componentId) {
  Serial.print("Touch Press: ");
  Serial.println(componentId);
  
  // ===== PAGE 2 - pgModeMAN (ROČNI NAČIN) =====
  if (currentPage == 2) {
    switch(componentId) {
      case 4: { // bGor - začni premik gor
        Serial.println("[bGor] Vreteno GOR - START");
        bGorPressed = true;
        uint8_t pwmSpeed = speedToPWM(speedRocno);
        outputs.moveSpindleUp(pwmSpeed);
        break;
      }
        
      case 5: { // bDol - začni premik dol
        Serial.println("[bDol] Vreteno DOL - START");
        bDolPressed = true;
        uint8_t pwmSpeed = speedToPWM(speedRocno);
        outputs.moveSpindleDown(pwmSpeed);
        break;
      }
        
      case 6:  // bBrus - toggle motor kamna
        brusActive = !brusActive;
        outputs.setGrindingMotor(brusActive);
        display.setBrusState(brusActive);
        Serial.print("[bBrus] Motor kamna: ");
        Serial.println(brusActive ? "ON" : "OFF");
        break;
        
      case 7:  // bPnev - toggle ventil noža
        pnevActive = !pnevActive;
        outputs.setKnifePusher(pnevActive);
        display.setPnevState(pnevActive);
        Serial.print("[bPnev] Ventil noža: ");
        Serial.println(pnevActive ? "ON" : "OFF");
        break;
        
      default:
        Serial.print("Neznan gumb ID na page2: ");
        Serial.println(componentId);
        break;
    }
  }
  // ===== PAGE 3 - pgModeAUTO =====
  else if (currentPage == 3) {
    switch(componentId) {
      case 9: { // bStart - začni avtomatski cikel
        // Preveri pogoje
        if (!anglesCalibrated) {
          Serial.println("[bStart] NAPAKA: Ni kalibracije!");
          display.setText("tStatus_pg3", "NAPAKA: Ni kalibracije!");
          return;
        }
        if (!anglesConfigured) {
          Serial.println("[bStart] NAPAKA: Ni nastavljenih kotov!");
          display.setText("tStatus_pg3", "NAPAKA: Ni kotov!");
          return;
        }
        
        // Preveri trenutni kot
        float currentAngle = angleSensor.getCalibratedAngle();
        if (currentAngle < calibratedMinAngle || currentAngle > calibratedMaxAngle) {
          Serial.println("[bStart] NAPAKA: Kot izven mej!");
          display.setText("tStatus_pg3", "NAPAKA: Pozicioniraj vreteno!");
          return;
        }
        
        S2Cycles cycles = inputs.getS2Cycles();
        if (cycles == CYCLES_NONE) {
          Serial.println("[bStart] NAPAKA: S2 ni nastavljen!");
          display.setText("tStatus_pg3", "NAPAKA: Nastavi S2!");
          return;
        }
        
        // Vse OK - začni avtomatski cikel
        Serial.println("========================================");
        Serial.println("[bStart] START AVTOMATSKEGA CIKLA");
        Serial.println("========================================");
        
        autoCycle.start(cycles, savedAngleStart, savedAngleStop,
                       calibratedMinAngle, calibratedMaxAngle, anglesCalibrated,
                       speedZacetni, speedSredina, speedKoncni, revPerAngle);
        autoModeActive = true;
        
        display.setText("tStatus_pg3", "AUTO cikel zagnan!");
        break;
      }
        
      default:
        Serial.print("Neznan gumb ID na page3: ");
        Serial.println(componentId);
        break;
    }
  }
  // ===== PAGE 4 - pgSettings (MENU) =====
  // Gumbi (bRef, bSpeed, bNastaviKote) že v GUI odpirajo strani,
  // zato Touch Press handler ni potreben
  
  // ===== PAGE 5 - pgRef (REFERENČNI HOD) =====
  else if (currentPage == 5) {
    switch(componentId) {
      case 3:  // bRefStart - začni referenčni hod
        if (refState == REF_IDLE) {
          Serial.println("[bRefStart] Začetek referenčnega hoda");
          startReferenceRun();
        } else {
          Serial.println("[bRefStart] Referenčni hod že teče!");
        }
        break;
        
      default:
        Serial.print("Neznan gumb ID na page5: ");
        Serial.println(componentId);
        break;
    }
  }
  // ===== PAGE 7 - pgAngle (NASTAVITEV KOTOV) =====
  else if (currentPage == 7) {
    switch(componentId) {
      case 7:  // bNastaviKote - začni/nadaljuj nastavitev
        if (angleSettingMode == ANGLE_IDLE) {
          // Začni nastavitev začetnega kota
          angleSettingMode = ANGLE_SET_START;
          tempAngleStart = angleSensor.getCalibratedAngle();
          Serial.println("[bNastaviKote] Nastavljanje ZAČETNEGA kota - uporabite tipke S42(Gor)/S41(Dol)");
          display.setText("bNastaviKote", "Zacetni");
          updatePage1AngleDisplay();
        }
        else if (angleSettingMode == ANGLE_SET_START) {
          // Shrani začetni kot in začni nastavitev končnega
          tempAngleStart = angleSensor.getCalibratedAngle();
          angleSettingMode = ANGLE_SET_STOP;
          anglesChanged = true;
          tempAngleStop = angleSensor.getCalibratedAngle();
          Serial.print("[bNastaviKote] Začetni kot nastavljen: ");
          Serial.print(tempAngleStart, 1);
          Serial.println("°");
          Serial.println("[bNastaviKote] Nastavljanje KONČNEGA kota - uporabite tipke S42(Gor)/S41(Dol)");
          display.setText("bNastaviKote", "Koncni");
          updatePage1AngleDisplay();
          display.setButtonState("bSave", true);
        }
        else if (angleSettingMode == ANGLE_SET_STOP) {
          // Shrani končni kot in resetiraj
          tempAngleStop = angleSensor.getCalibratedAngle();
          
          // Validacija: Začetni kot mora biti večji od končnega
          if (tempAngleStart <= tempAngleStop) {
            Serial.println("[bNastaviKote] NAPAKA: Začetni kot mora biti večji od končnega!");
            Serial.print("  Start: ");
            Serial.print(tempAngleStart, 1);
            Serial.print("°, Stop: ");
            Serial.print(tempAngleStop, 1);
            Serial.println("°");
            angleSettingMode = ANGLE_IDLE;
            display.setText("bNastaviKote", "Nastavi");
            return;
          }
          
          // Preveri ali so koti znotraj kalibriranih limitov
          if (anglesCalibrated) {
            if (tempAngleStart > calibratedMaxAngle) {
              Serial.print("[bNastaviKote] NAPAKA: Začetni kot ");
              Serial.print(tempAngleStart, 1);
              Serial.print("° presega max limit ");
              Serial.print(calibratedMaxAngle, 1);
              Serial.println("°");
              angleSettingMode = ANGLE_IDLE;
              display.setText("bNastaviKote", "Nastavi");
              return;
            }
            if (tempAngleStop < calibratedMinAngle) {
              Serial.print("[bNastaviKote] NAPAKA: Končni kot ");
              Serial.print(tempAngleStop, 1);
              Serial.print("° presega min limit ");
              Serial.print(calibratedMinAngle, 1);
              Serial.println("°");
              angleSettingMode = ANGLE_IDLE;
              display.setText("bNastaviKote", "Nastavi");
              return;
            }
          } else {
            Serial.println("[bNastaviKote] OPOZORILO: Koti niso preverjeni - kalibracija ni opravljena!");
          }
          
          angleSettingMode = ANGLE_IDLE;
          anglesChanged = true;
          Serial.print("[bNastaviKote] Končni kot nastavljen: ");
          Serial.print(tempAngleStop, 1);
          Serial.println("°");
          Serial.println("[bNastaviKote] Nastavitev končana - pritisnite bSave za shranitev");
          display.setText("bNastaviKote", "Nastavi");
          updatePage1AngleDisplay();
        }
        break;
        
      case 8:   // bSave - shrani kote
        savedAngleStart = tempAngleStart;
        savedAngleStop = tempAngleStop;
        saveAnglesToPreferences();
        anglesConfigured = true;
        anglesChanged = false;
        
        Serial.println("[bSave] Koti shranjeni v NVS");
        Serial.print("  Start: ");
        Serial.print(savedAngleStart, 1);
        Serial.println("°");
        Serial.print("  Stop: ");
        Serial.print(savedAngleStop, 1);
        Serial.println("°");
        
        // Posodobi globalne spremenljivke na pgModeOFF (format × 10)
        display.setNumber("pgModeOFF.gStartAngle", (int32_t)(savedAngleStart * 10));
        display.setNumber("pgModeOFF.gStopAngle", (int32_t)(savedAngleStop * 10));
        
        // Deaktiviraj bSave
        display.setButtonState("bSave", false);
        angleSettingMode = ANGLE_IDLE;
        updateBNastaviKoteText();
        break;
        
      default:
        Serial.print("Neznan gumb ID na page7: ");
        Serial.println(componentId);
        break;
    }
  }
  // ===== PAGE 1 - pgModeOFF =====
  else if (currentPage == 1) {
    switch(componentId) {
      case 20:  // bSettings - preklopi na page4 (pgSettings)
        Serial.println("[bSettings] Preklop na Page 4 - Nastavitve");
        display.showPage(4);
        break;
        
      default:
        Serial.print("Neznan gumb ID na page1: ");
        Serial.println(componentId);
        break;
    }
  }
  // ===== PAGE 3 - pgModeAUTO =====
  else if (currentPage == 3) {
    switch(componentId) {
      case 9: { // bStart - začni avtomatski cikel
        // Preveri pogoje
        if (!anglesCalibrated) {
          Serial.println("[bStart] NAPAKA: Ni kalibracije!");
          display.setText("tStatus_pg3", "NAPAKA: Ni kalibracije!");
          return;
        }
        if (!anglesConfigured) {
          Serial.println("[bStart] NAPAKA: Ni nastavljenih kotov!");
          display.setText("tStatus_pg3", "NAPAKA: Ni kotov!");
          return;
        }
        
        // Preveri trenutni kot
        float currentAngle = angleSensor.getCalibratedAngle();
        if (currentAngle < calibratedMinAngle || currentAngle > calibratedMaxAngle) {
          Serial.println("[bStart] NAPAKA: Kot izven mej!");
          display.setText("tStatus_pg3", "NAPAKA: Pozicioniraj vreteno!");
          return;
        }
        
        S2Cycles cycles = inputs.getS2Cycles();
        if (cycles == CYCLES_NONE) {
          Serial.println("[bStart] NAPAKA: S2 ni nastavljen!");
          display.setText("tStatus_pg3", "NAPAKA: Nastavi S2!");
          return;
        }
        
        // Vse OK - začni avtomatski cikel
        Serial.println("========================================");
        Serial.println("[bStart] START AVTOMATSKEGA CIKLA");
        Serial.println("========================================");
        
        autoCycle.start(cycles, savedAngleStart, savedAngleStop,
                       calibratedMinAngle, calibratedMaxAngle, anglesCalibrated,
                       speedZacetni, speedSredina, speedKoncni, revPerAngle);
        autoModeActive = true;
        
        display.setText("tStatus_pg3", "AUTO cikel zagnan!");
        break;
      }
        
      default:
        Serial.print("Neznan gumb ID na page3: ");
        Serial.println(componentId);
        break;
    }
  }
  // ===== PAGE 4 - pgSettings =====
  // Navigacijski meni brez Touch Press handlerjev (GUI pošlje PAGE: event)
}

void handleTouchRelease(uint8_t componentId) {
  Serial.print("Touch Release: ");
  Serial.println(componentId);
  
  // ===== PAGE 2 - pgModeMAN BUTTONS =====
  if (currentPage == 2) {
    switch(componentId) {
      case 4:  // bGor - ustavi premik
        Serial.println("[bGor] Vreteno GOR - STOP");
        bGorPressed = false;
        outputs.stopSpindle();
        break;
        
      case 5:  // bDol - ustavi premik
        Serial.println("[bDol] Vreteno DOL - STOP");
        bDolPressed = false;
        outputs.stopSpindle();
        break;
        
      case 14: { // hRocno - slider za hitrost (Touch Release pošlje "ID14:x")
        // To obdelamo v handleNextionEvents() ko pride "ID14:x"
        Serial.println("[hRocno] Touch Release - čakam na ID14:value");
        break;
      }
        
      // bBrus (6) in bPnev (7) sta toggle gumba - brez Release akcije
      
      default:
        break;
    }
  }
  // ===== PAGE 6 - SPEED SETTINGS =====
  // Sliderji (hZacetni, hSredina, hKoncni) pošljejo text "IDX:Y"
  // To je obdelano v handleNextionEvents()
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Feed watchdog timer
  yield();
  
  // ===== SIMULATOR MODE - Serial ukazi =====
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    // Ukaz: angle=XX
    if (cmd.startsWith("angle=")) {
      float angle = cmd.substring(6).toFloat();
      if (angle >= 0 && angle <= 360) {
        angleSensor.setSimulatedAngle(angle);
        Serial.print("Simuliran kot nastavljen na: ");
        Serial.print(angle, 1);
        Serial.println("°");
      } else {
        Serial.println("NAPAKA: Kot mora biti med 0-360°");
      }
    }
    // Ukaz: page=X - test page change
    else if (cmd.startsWith("page=")) {
      int page = cmd.substring(5).toInt();
      if (page >= 0 && page <= 7) {
        Serial.print("TEST: Preklapljanje na page ");
        Serial.println(page);
        currentPage = page;
        display.showPage(page);
      } else {
        Serial.println("NAPAKA: Page mora biti 0-7");
      }
    }
    // Očisti preostale byte iz bufferja
    while (Serial.available()) Serial.read();
  }
  
  // ===== S1 MODE TRACKING - Avtomatski preklop med pgModeOFF/MAN/AUTO =====
  S1Mode currentS1Mode = inputs.getS1Mode();
  if (currentS1Mode != lastS1Mode) {
    // S1 stikalo se je spremenilo
    Serial.print("[S1] Sprememba: ");
    if (currentS1Mode == MODE_OFF) Serial.println("OFF");
    else if (currentS1Mode == MODE_MANUAL) Serial.println("MANUAL");
    else if (currentS1Mode == MODE_AUTO) Serial.println("AUTO");
    
    // Preklopi na ustrezno stran samo če smo na eni od mode strani (1,2,3)
    if (currentPage >= 1 && currentPage <= 3) {
      if (currentS1Mode == MODE_OFF && currentPage != 1) {
        Serial.println("  -> Preklop na pgModeOFF");
        display.showPage(1);
      } else if (currentS1Mode == MODE_MANUAL && currentPage != 2) {
        Serial.println("  -> Preklop na pgModeMAN");
        display.showPage(2);
      } else if (currentS1Mode == MODE_AUTO && currentPage != 3) {
        Serial.println("  -> Preklop na pgModeAUTO");
        display.showPage(3);
      }
    }
    
    lastS1Mode = currentS1Mode;
  }
  
  // Posodobi vhode
  inputs.update();
  
  // Posodobi izhode (kontrola cilindra)
  outputs.update();
  
  // Posodobi AS5600 senzor
  angleSensor.update();
  
  // Posodobi prikaz kota xAngle na stranih ki ga imajo (1,2,3,5,7) - samo ob spremembi
  if (currentPage == 1 || currentPage == 2 || currentPage == 3 || currentPage == 5 || currentPage == 7) {
    static float lastDisplayedAngle = -999.0;
    static unsigned long lastAngleUpdate = 0;
    float currentAngle = angleSensor.getCalibratedAngle();
    
    // Posodobi če: 1) Sprememba >1.0° ALI 2) Vsake 10s (backup)
    if (abs(currentAngle - lastDisplayedAngle) > 1.0 || 
        (currentMillis - lastAngleUpdate > 10000)) {
      display.setNumber("xAngle", (int32_t)(currentAngle * 10));
      lastDisplayedAngle = currentAngle;
      lastAngleUpdate = currentMillis;
    }
  }
  
  // Posodobi podatke na page2 (pgModeMAN) vsake 0.5s
  static unsigned long lastPage2Update = 0;
  if (currentPage == 2 && (currentMillis - lastPage2Update >= 500)) {
    lastPage2Update = currentMillis;
    
    // Posodobi tPomik - status motorja
    if (outputs.isSpindleMoving()) {
      if (outputs.getSpindleDirection() == SPINDLE_UP) {
        display.setText("tPomik", "GOR");
        // Nastavi zeleno barvo
        Serial2.print("tPomik.pco=2016");  // Zelena
        Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
      } else {
        display.setText("tPomik", "DOL");
        // Nastavi zeleno barvo
        Serial2.print("tPomik.pco=2016");  // Zelena
        Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
      }
    } else {
      display.setText("tPomik", "STOP");
      // Nastavi rdečo barvo
      Serial2.print("tPomik.pco=63488");  // Rdeča
      Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
    }
    
    // Preveri in omogoči/onemogoči bPnev glede na kot
    float currentAngle = angleSensor.getCalibratedAngle();
    if (anglesCalibrated) {
      bool inRange = (currentAngle >= calibratedMinAngle && currentAngle <= calibratedMaxAngle);
      display.setButtonState("bPnev", inRange);
      
      // Posodobi status če je izven mej
      if (currentAngle < calibratedMinAngle) {
        display.setText("tStatus_pg2", "OPOZORILO: Kot < MIN!");
      } else if (currentAngle > calibratedMaxAngle) {
        display.setText("tStatus_pg2", "OPOZORILO: Kot > MAX!");
      } else if (!outputs.isSpindleMoving() && !outputs.isGrindingMotorOn() && !outputs.isKnifePusherOn()) {
        display.setText("tStatus_pg2", "Ročni način - uporabite tipke ali gumbe");
      }
    } else {
      // Ni kalibracije - bPnev vedno omogočen
      display.setButtonState("bPnev", true);
    }
  }
  
  // Posodobi podatke na page3 (pgModeAUTO) vsake 0.5s
  static unsigned long lastPage3Update = 0;
  if (currentPage == 3 && (currentMillis - lastPage3Update >= 500)) {
    lastPage3Update = currentMillis;
    
    // Posodobi tCikli - format "izvedeni/nastavljeni"
    S2Cycles cycles = inputs.getS2Cycles();
    char cikliText[20];
    if (cycles == CYCLES_CONTINUOUS) {
      sprintf(cikliText, "%d/∞", autoCycle.getCompletedCycles());
    } else {
      sprintf(cikliText, "%d/%d", autoCycle.getCompletedCycles(), (int)cycles);
    }
    display.setText("tCikli", cikliText);
    
    // Posodobi tPomik - status motorja vretena
    if (outputs.isSpindleMoving()) {
      if (outputs.getSpindleDirection() == SPINDLE_UP) {
        display.setText("tPomik", "GOR");
        // Nastavi zeleno barvo
        Serial2.print("tPomik.pco=2016");  // Zelena
        Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
      } else {
        display.setText("tPomik", "DOL");
        // Nastavi zeleno barvo
        Serial2.print("tPomik.pco=2016");  // Zelena
        Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
      }
    } else {
      display.setText("tPomik", "STOP");
      // Nastavi rdečo barvo
      Serial2.print("tPomik.pco=63488");  // Rdeča
      Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
    }
    
    // Posodobi tKamen - status brusnega kamna
    if (outputs.isGrindingMotorOn()) {
      display.setText("tKamen", "RUN");
      // Nastavi modro barvo
      Serial2.print("tKamen.pco=1024");  // Modra
      Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
    } else {
      display.setText("tKamen", "STOP");
      // Nastavi rdečo barvo
      Serial2.print("tKamen.pco=63488");  // Rdeča
      Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
    }
    
    // Posodobi tCilinder - status pnevmatskega cilindra
    BrusOutputs::KnifeCylinderState cylinderState = outputs.getKnifeCylinderState();
    if (cylinderState == BrusOutputs::KNIFE_MOVING_OUT) {
      display.setText("tCilinder", "OUT");
      // Nastavi modro barvo
      Serial2.print("tCilinder.pco=1024");  // Modra
      Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
    } else if (cylinderState == BrusOutputs::KNIFE_MOVING_IN) {
      display.setText("tCilinder", "IN");
      // Nastavi modro barvo
      Serial2.print("tCilinder.pco=1024");  // Modra
      Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
    } else {
      display.setText("tCilinder", "STOP");
      // Nastavi rdečo barvo
      Serial2.print("tCilinder.pco=63488");  // Rdeča
      Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
    }
    
    // Preveri pogoje za bStart in posodobi tStatus_pg3
    updateAutoModeReadiness();
  }
  
  // Če smo na page7 (pgAngle) in nastavljamo kote, posodabljaj prikaz v realnem času
  if (currentPage == 7 && angleSettingMode != ANGLE_IDLE) {
    float previousAngle = (angleSettingMode == ANGLE_SET_START) ? tempAngleStart : tempAngleStop;
    
    if (angleSettingMode == ANGLE_SET_START) {
      tempAngleStart = angleSensor.getCalibratedAngle();
      if (abs(tempAngleStart - previousAngle) > 0.1) {  // Sprememba > 0.1°
        anglesChanged = true;
        display.setButtonState("bSave", true);
      }
    } else if (angleSettingMode == ANGLE_SET_STOP) {
      tempAngleStop = angleSensor.getCalibratedAngle();
      if (abs(tempAngleStop - previousAngle) > 0.1) {  // Sprememba > 0.1°
        anglesChanged = true;
        display.setButtonState("bSave", true);
      }
    }
    updatePage1AngleDisplay();
  }
  
  // Če smo na page5 (pgRef) in izvajamo referenčni hod, posodabljaj
  if (currentPage == 5) {
    if (refState == REF_FIND_MIN_DOWN || refState == REF_FIND_MIN_UP || refState == REF_FIND_MAX) {
      updateReferenceRun();
      
      // Posodobi nCicleTime vsako sekundo med izvajanjem
      static unsigned long lastTimeUpdate = 0;
      if (currentMillis - lastTimeUpdate >= 1000) {
        lastTimeUpdate = currentMillis;
        unsigned long elapsedSeconds = (currentMillis - refStartTime) / 1000;
        display.setNumber("nCicleTime", (int32_t)elapsedSeconds);
      }
    }
  }
  
  // Posodobi Nextion display (vključuje obdelavo vseh dogodkov preko callbackov)
  display.update(currentMillis);
  
  // Trenutni način delovanja
  S1Mode mode = inputs.getS1Mode();
  
  // Detekcija spremembe načina
  if (mode != lastMode) {
    Serial.print("Sprememba načina: ");
    if (mode == MODE_OFF) Serial.println("OFF");
    else if (mode == MODE_MANUAL) Serial.println("MANUAL");
    else if (mode == MODE_AUTO) Serial.println("AUTO");
    
    // Ob spremembi iz AUTO v karkoli drugega ustavi cikel
    if (lastMode == MODE_AUTO && mode != MODE_AUTO) {
      autoCycle.stop();
      autoModeActive = false;
    }
    
    // Ob preklopu v AUTO način
    if (mode == MODE_AUTO && lastMode != MODE_AUTO) {
      // Če nismo na page0, pojdi na page0
      if (currentPage != 0) {
        Serial.println("[MODE_AUTO] Preklop na page0");
        display.showPage(0);
        currentPage = 0;
      }
      // Posodobi bSettings gumb glede na pripravljenost
      updateAutoModeReadiness();
    }
    
    lastMode = mode;
  }
  
  // ===== NAČIN OFF =====
  if (mode == MODE_OFF) {
    // V OFF načinu vse ustavi
    if (outputs.isGrindingMotorOn() || outputs.isWaterPumpOn() || 
        outputs.isKnifePusherOn() || outputs.isSpindleMoving()) {
      outputs.emergencyStop();
    }
    if (autoCycle.isRunning()) {
      autoCycle.stop();
    }
    autoModeActive = false;
  }
  
  // ===== ROČNI NAČIN =====
  else if (mode == MODE_MANUAL) {
    // Kontrola vretena - fizične tipke (S41/S42) delujejo na page2 in page7
    // Na page2 (pgModeMAN): fizične tipke IN Nextion gumbi (bGor/bDol)
    // Na page7 (pgAngle): samo fizične tipke (med nastavljanjem kotov)
    
    bool wantMoveDown = false;
    bool wantMoveUp = false;
    
    if (currentPage == 2) {
      // Page2 (pgModeMAN) - ročno upravljanje - delujejo fizične tipke IN Nextion gumbi
      wantMoveDown = inputs.isS41DownPressed() || bDolPressed;
      wantMoveUp = inputs.isS42UpPressed() || bGorPressed;
    }
    else if (currentPage == 7 && (angleSettingMode == ANGLE_SET_START || angleSettingMode == ANGLE_SET_STOP)) {
      // Page7 (pgAngle) - med nastavljanjem kotov - samo fizične tipke
      wantMoveDown = inputs.isS41DownPressed();
      wantMoveUp = inputs.isS42UpPressed();
      
      // Posodobi začasni kot med nastavljanjem
      if (angleSettingMode == ANGLE_SET_START) {
        tempAngleStart = angleSensor.getCalibratedAngle();
      } else if (angleSettingMode == ANGLE_SET_STOP) {
        tempAngleStop = angleSensor.getCalibratedAngle();
      }
    }
    
    // Varnostna preverjanja limitov (če je kalibracija opravljena)
    float currentAngle = angleSensor.getCalibratedAngle();
    if (anglesCalibrated) {
      if (wantMoveUp && currentAngle >= calibratedMaxAngle) {
        outputs.stopSpindle();
        if (currentPage == 1) {
          display.setText("tStatus_pg1", "ALARM: Max limit!");
        } else if (currentPage == 2) {
          display.setText("tStatus_pg2", "ALARM: Max limit!");
        }
        Serial.println("ALARM: Dosežen maksimalni limit!");
        wantMoveUp = false;
      }
      if (wantMoveDown && currentAngle <= calibratedMinAngle) {
        outputs.stopSpindle();
        if (currentPage == 1) {
          display.setText("tStatus_pg1", "ALARM: Min limit!");
        } else if (currentPage == 2) {
          display.setText("tStatus_pg2", "ALARM: Min limit!");
        }
        Serial.println("ALARM: Dosežen minimalni limit!");
        wantMoveDown = false;
      }
    }
    
    if (wantMoveDown && !wantMoveUp) {
      // Premik dol (prioriteta ima DOL, če sta obe pritisnjena)
      if (!outputs.isSpindleMoving() || outputs.getSpindleDirection() != SPINDLE_DOWN) {
        uint8_t pwmSpeed = speedToPWM(speedRocno);
        outputs.moveSpindleDown(pwmSpeed);
      }
    } 
    else if (wantMoveUp && !wantMoveDown) {
      // Premik gor
      if (!outputs.isSpindleMoving() || outputs.getSpindleDirection() != SPINDLE_UP) {
        uint8_t pwmSpeed = speedToPWM(speedRocno);
        outputs.moveSpindleUp(pwmSpeed);
      }
    } 
    else {
      // Nobena kontrola ni aktivna - ustavi vreteno
      if (outputs.isSpindleMoving()) {
        outputs.stopSpindle();
      }
    }
    
    // V ROČNEM načinu motorji preko fizičnih tipk niso dovoljeni
    // (samo preko Nextion gumbov bBrus/bPnev)
    // Črpalka in nož ne smeta biti vklopljena v manual mode
    if (outputs.isWaterPumpOn()) {
      outputs.setWaterPump(false);
    }
    
    autoModeActive = false;
  }
  
  // ===== AVTOMATSKI NAČIN =====
  else if (mode == MODE_AUTO) {
    // Posodobi avtomatski cikel
    autoCycle.update();
    
    // Če še ni aktiven, začni cikel glede na S2 nastavitev
    if (!autoModeActive && !autoCycle.isRunning()) {
      // Preveri če so koti nastavljeni
      if (!anglesConfigured) {
        Serial.println("NAPAKA: Koti niso nastavljeni - uporabite page7!");
        // V novem GUI je page3 za start, brez dodatnih sporočil tukaj
      } else {
        S2Cycles cycles = inputs.getS2Cycles();
        
        if (cycles == CYCLES_CONTINUOUS) {
          autoCycle.start(0, savedAngleStart, savedAngleStop, 
                         calibratedMinAngle, calibratedMaxAngle, anglesCalibrated,
                         speedZacetni, speedSredina, speedKoncni, revPerAngle);
          autoModeActive = true;
        } 
        else if (cycles >= CYCLES_2 && cycles <= CYCLES_7) {
          autoCycle.start(cycles, savedAngleStart, savedAngleStop,
                         calibratedMinAngle, calibratedMaxAngle, anglesCalibrated,
                         speedZacetni, speedSredina, speedKoncni, revPerAngle);
          autoModeActive = true;
        }
        else {
          // S2 ni nastavljen na veljavno pozicijo
          Serial.println("OPOZORILO: Nastavite število ciklov na S2!");
        }
      }
    }
  }
  
  // ===== RESET TIPKA =====
  if (inputs.isResetPressed()) {
    if (mode == MODE_MANUAL) {
      // V ROČNEM načinu: briše shranjene kote
      clearAnglesFromPreferences();
      savedAngleStart = 0.0;
      savedAngleStop = 0.0;
      anglesConfigured = false;
      
      display.setAngleRange(0.0, 0.0);
      
      Serial.println("*** RESET - Koti izbrisani ***");
      Serial.println("Nastavite nove kote z gumbom bSave na displayu.");
    } else {
      // V ostalih načinih: emergency stop
      outputs.emergencyStop();
      autoCycle.stop();
      Serial.println("*** RESET - EMERGENCY STOP ***");
    }
    
    delay(500); // Debounce za tipko
  }
  
  // ===== TEMPERATURA ALARM =====
  if (inputs.isTempAlarm()) {
    outputs.emergencyStop();
    autoCycle.stop();
    Serial.println("!!! TEMPERATURA ALARM - SISTEM USTAVLJEN !!!");
  }
  
  // ===== PERIODIČEN DEBUG IZPIS =====
  if (millis() - lastPrintTime > PRINT_INTERVAL) {
    lastPrintTime = millis();
    
    // Serial output
    Serial.print("Mode: ");
    switch(mode) {
      case MODE_OFF:    Serial.print("OFF     "); break;
      case MODE_MANUAL: Serial.print("MANUAL  "); break;
      case MODE_AUTO:   Serial.print("AUTO    "); break;
    }
    
    // Nextion display update
    const char* modeStr = (mode == MODE_OFF) ? "OFF" : (mode == MODE_MANUAL) ? "MANUAL" : "AUTO";
    display.setMode(modeStr);
    
    if (mode == MODE_AUTO && autoCycle.isRunning()) {
      Serial.print("| ");
      autoCycle.printStatus();
      
      // Nextion - cikli
      display.setCycles(autoCycle.getCompletedCycles(), autoCycle.getTargetCycles());
    } else {
      Serial.print("| Cycles: ");
      S2Cycles cycles = inputs.getS2Cycles();
      if (cycles == CYCLES_CONTINUOUS) {
        Serial.print("CONT");
        display.setCycles(0, 99);  // 99 = continuous
      } else {
        Serial.print(cycles);
        Serial.print("   ");
        display.setCycles(0, cycles);
      }
      
      Serial.print(" | Rev: ");
      Serial.print(inputs.getRevolutions());
      Serial.print("  ");
      
      // Prikaz kota iz AS5600 (če je prisoten)
      if (USE_AS5600_FOR_TILT && inputs.getAngleEncoder()->isSensorPresent()) {
        float angle = inputs.getSpindleAngle();
        Serial.print("| Angle: ");
        Serial.print(angle, 1);
        Serial.print("° ");
        
        // Nextion - kot
        display.setAngle(angle);
      }
      
      // Prikaz aktivnih komponent
      if (outputs.isGrindingMotorOn()) Serial.print("[MOTOR] ");
      if (outputs.isWaterPumpOn()) Serial.print("[PUMP] ");
      if (outputs.isKnifePusherOn()) Serial.print("[KNIFE] ");
      if (outputs.isSpindleMoving()) {
        Serial.print("[SPINDLE ");
        Serial.print(outputs.getSpindleDirection() == SPINDLE_UP ? "UP" : "DN");
        Serial.print(":"); 
        Serial.print(outputs.getSpindleSpeed());
        Serial.print("] ");
      }
      
      // Senzorji in alarmi
      if (inputs.isSpindleTilted()) Serial.print("[TILT<10°] ");
      if (inputs.isTempAlarm()) Serial.print("[TEMP!] ");
      
      Serial.println();
    }
    
    // Nextion - število obratov
    // display.setRevolutions(inputs.getRevolutions());
    
    // Nextion - status motorjev
    // display.setMotorStatus(
    //   outputs.isGrindingMotorOn(),
    //   outputs.isWaterPumpOn(),
    //   outputs.isKnifePusherOn()
    // );
    
    // Nextion - status vretena
    display.setSpindleStatus(
      outputs.isSpindleMoving(),
      outputs.getSpindleDirection() == SPINDLE_UP,
      outputs.getSpindleSpeed()
    );
    
    // Nextion - alarmi
    // if (inputs.isTempAlarm()) {
    //   display.setAlarm("TEMPERATURA ALARM!");
    // } else if (autoCycle.hasError()) {
    //   display.setAlarm(autoCycle.getErrorMessage().c_str());
    // } else {
    //   display.clearAlarm();
    // }
  }
  
  delay(10); // Kratka zakasnitev za stabilnost
}

// ===== FUNKCIJE ZA DELO S PREFERENCES (NVS) =====

void loadAnglesFromPreferences() {
  preferences.begin("brus", true); // Read-only mode
  
  savedAngleStart = preferences.getFloat("angleStart", 0.0);
  savedAngleStop = preferences.getFloat("angleStop", 0.0);
  
  // Naloži kalibrirane min/max kote iz referenčnega hoda
  calibratedMinAngle = preferences.getFloat("minAngle", 0.0);
  calibratedMaxAngle = preferences.getFloat("maxAngle", 0.0);
  
  preferences.end();
  
  // Preveri ali sta kota nastavljena
  if (savedAngleStart > 0.0 && savedAngleStop > 0.0) {
    anglesConfigured = true;
    Serial.println("Shranjeni koti naloženi iz NVS:");
    Serial.print("  Start: ");
    Serial.print(savedAngleStart, 1);
    Serial.println("°");
    Serial.print("  Stop: ");
    Serial.print(savedAngleStop, 1);
    Serial.println("°");
    
    // Posodobi display
    display.setAngleRange(savedAngleStart, savedAngleStop);
  } else {
    anglesConfigured = false;
    Serial.println("Koti niso nastavljeni - uporabite bSave za nastavitev");
  }
  
  // Preveri ali je kalibracija opravljena
  if (calibratedMinAngle > 0.0 && calibratedMaxAngle > 0.0 && calibratedMaxAngle > calibratedMinAngle) {
    anglesCalibrated = true;
    Serial.println("Kalibrirani limiti naloženi iz NVS:");
    Serial.print("  Min: ");
    Serial.print(calibratedMinAngle, 1);
    Serial.println("°");
    Serial.print("  Max: ");
    Serial.print(calibratedMaxAngle, 1);
    Serial.println("°");
  } else {
    anglesCalibrated = false;
    Serial.println("OPOZORILO: Kalibracija ni opravljena - uporabite Ref na page3");
  }
  
  // Naloži hitrosti vretena
  speedZacetni = preferences.getUChar("speedZacetni", 90);
  speedSredina = preferences.getUChar("speedSredina", 75);
  speedKoncni = preferences.getUChar("speedKoncni", 85);
  speedRocno = preferences.getUChar("speedRocno", 80);
  
  // Naloži referenčne obrate na stopinjo
  uint32_t revPerAngleScaled = preferences.getUInt("revPerAngle", 0);
  revPerAngle = (float)revPerAngleScaled / 10.0;  // Shranjeno kot × 10
  
  Serial.println("Hitrosti naložene iz NVS:");
  Serial.print("  Začetni: ");
  Serial.print(speedZacetni);
  Serial.println("%");
  Serial.print("  Sredina: ");
  Serial.print(speedSredina);
  Serial.println("%");
  Serial.print("  Končni: ");
  Serial.print(speedKoncni);
  Serial.println("%");
  Serial.print("  Ročno: ");
  Serial.print(speedRocno);
  Serial.println("%");
  
  if (revPerAngle > 0.0) {
    Serial.print("  RevPerAngle: ");
    Serial.print(revPerAngle, 1);
    Serial.println(" obr/°");
  } else {
    Serial.println("  OPOZORILO: RevPerAngle ni nastavljen - uporabite Ref");
  }
}

void saveAnglesToPreferences() {
  preferences.begin("brus", false); // Read-write mode
  
  preferences.putFloat("angleStart", savedAngleStart);
  preferences.putFloat("angleStop", savedAngleStop);
  
  preferences.end();
  
  Serial.println("Koti shranjeni v NVS (Non-Volatile Storage)");
}

void clearAnglesFromPreferences() {
  preferences.begin("brus", false); // Read-write mode
  
  preferences.clear(); // Briše vse shranjene vrednosti
  
  preferences.end();
  
  Serial.println("Vsi shranjeni podatki izbrisani iz NVS");
}

void saveSpeedsToPreferences() {
  preferences.begin("brus", false); // Read-write mode
  
  preferences.putUChar("speedZacetni", speedZacetni);
  preferences.putUChar("speedSredina", speedSredina);
  preferences.putUChar("speedKoncni", speedKoncni);
  preferences.putUChar("speedRocno", speedRocno);
  
  preferences.end();
  
  Serial.println("Hitrosti shranjene v NVS");
  Serial.print("  Začetni: ");
  Serial.print(speedZacetni);
  Serial.println("%");
  Serial.print("  Sredina: ");
  Serial.print(speedSredina);
  Serial.println("%");
  Serial.print("  Končni: ");
  Serial.print(speedKoncni);
  Serial.println("%");
  Serial.print("  Ročno: ");
  Serial.print(speedRocno);
  Serial.println("%");
}

// Pretvori hitrost iz procenta (50-100%) v PWM vrednost (128-255)
uint8_t speedToPWM(uint8_t percent) {
  if (percent < 50) percent = 50;
  if (percent > 100) percent = 100;
  
  // 50% = 128 PWM, 100% = 255 PWM
  // Linearna interpolacija: PWM = 128 + (percent - 50) * (255 - 128) / 50
  uint8_t pwm = 128 + ((percent - 50) * 127) / 50;
  
  return pwm;
}

void updatePage1AngleDisplay() {
  // Posodobi prikaz kotov na page7 (pgAngle)
  // xStartAngle (id=6), xStopAngle (id=5) - Xfloat format (*10)
  display.setNumber("xStartAngle", (int32_t)(tempAngleStart * 10));
  display.setNumber("xStopAngle", (int32_t)(tempAngleStop * 10));
}

void updateBNastaviKoteText() {
  if (angleSettingMode == ANGLE_IDLE) {
    display.setText("bNastaviKote", "Nastavi");
  } else if (angleSettingMode == ANGLE_SET_START) {
    display.setText("bNastaviKote", "Zacetni");
  } else if (angleSettingMode == ANGLE_SET_STOP) {
    display.setText("bNastaviKote", "Koncni");
  }
}

void updateAutoModeReadiness() {
  // Preveri ali so vsi pogoji za avtomatski cikel izpolnjeni
  // in posodobi gumb bStart in status na page3 (pgModeAUTO)
  
  // Funkcija se uporablja samo za page3
  if (currentPage != 3) return;
  
  bool ready = true;
  String status = "";
  
  // Preveri kalibracijo (referenčni hod)
  if (!anglesCalibrated) {
    ready = false;
    status = "Najprej izvedi referenčni hod!";
  }
  // Preveri nastavljene kote
  else if (!anglesConfigured) {
    ready = false;
    status = "Nastavi začetni in končni kot!";
  }
  else {
    // Preveri trenutni kot
    float currentAngle = angleSensor.getCalibratedAngle();
    if (currentAngle < calibratedMinAngle || currentAngle > calibratedMaxAngle) {
      ready = false;
      status = "Pozicioniraj vreteno med MIN in MAX!";
    }
    
    // Preveri število ciklov
    if (ready) {
      S2Cycles cycles = inputs.getS2Cycles();
      if (cycles == CYCLES_NONE) {
        ready = false;
        status = "Nastavi S2 stikalo (cikli)!";
      } else if (autoCycle.isRunning()) {
        ready = false;
        status = "Avtomatski cikel v teku...";
      } else {
        status = "Pripravljeno - pritisnite START";
      }
    }
  }
  
  // Posodobi gumb in status na page3
  display.setButtonState("bStart", ready);
  display.setText("tStatus_pg3", status.c_str());
}

// ===== FUNKCIJE ZA REFERENČNI HOD (PAGE 3) =====

void startReferenceRun() {
  Serial.println("========================================");
  Serial.println("REFERENČNI HOD - START");
  Serial.println("========================================");
  
  // Reset spremenljivk
  measuredMinAngle = 0.0;
  measuredMaxAngle = 0.0;
  startMeasurementAngle = 0.0;
  refRevolutions = 0;
  lastRevCount = inputs.getRevolutions();
  refStartTime = 0;  // Začnemo meriti šele ko S43 postane neaktiven
  
  // Faza 1: Iskanje najnižje točke
  refState = REF_FIND_MIN_DOWN;
  Serial.println("Faza 1: Iskanje najnižje točke - vreteno gre navzdol do S43 aktiven...");
  
  // Začni premik navzdol
  outputs.moveSpindleDown(SPINDLE_SPEED_MEDIUM);
  
  // Posodobi display page5 (pgRef)
  display.setNumber("nObrati", 0);
  display.setNumber("nCicleTime", 0);
  display.setNumber("xRevPerAngle", 0);
  display.setText("tStatus_pg5", "FAZA 1A: Iskanje najnizje tocke\rVreteno gre navzdol do S43...");
}

void updateReferenceRun() {
  float currentAngle = angleSensor.getCalibratedAngle();
  
  // xAngle se že posodablja v loop() za page5
  
  // === FAZA 1A: ISKANJE NAJNIŽJE TOČKE (DOL DO S43) ===
  if (refState == REF_FIND_MIN_DOWN) {
    // Preveri ali je S43 aktiven (najnižja točka)
    if (inputs.isSpindleAtBottom()) {
      // S43 je aktiven - dosežena najnižja točka!
      outputs.stopSpindle();
      delay(200);  // Kratka zakasnitev pred obratom smeri
      
      Serial.println("S43 aktiven - najnižja točka dosežena, obračam smer...");
      display.setText("tStatus_pg5", "S43 AKTIVEN - najnizja tocka!\rObracam smer motorja...\rFAZA 1B: Vreteno gre gor");
      
      // Obrni smer - začni iti gor
      refState = REF_FIND_MIN_UP;
      Serial.println("Faza 1B: Vreteno gre gor - čakam S43 neaktiven + 0.5°...");
      outputs.moveSpindleUp(SPINDLE_SPEED_MEDIUM);
    }
  }
  
  // === FAZA 1B: ISKANJE MIN KOTA (GOR OD S43 + 0.5°) ===
  else if (refState == REF_FIND_MIN_UP) {
    // Čakamo da S43 postane neaktiven
    if (!inputs.isSpindleAtBottom() && startMeasurementAngle == 0.0) {
      // S43 se je preklopil na "0" - začnemo merjenje!
      startMeasurementAngle = currentAngle;
      refStartTime = millis();  // Začni merjenje časa
      
      Serial.print("S43 preklop na 0 - začetek merjenja pri ");
      Serial.print(startMeasurementAngle, 1);
      Serial.println("°");
      
      char statusMsg[200];
      sprintf(statusMsg, "S43 NEAKTIVEN - zacetek merjenja!\rKot: %.1f deg\rCakam +0.5 deg za minAngle...", startMeasurementAngle);
      display.setText("tStatus_pg5", statusMsg);
    }
    
    // Če smo začeli merjenje, preveri ali smo prešli 0.5° od začetnega kota
    if (startMeasurementAngle > 0.0) {
      float deltaNadjenih = currentAngle - startMeasurementAngle;
      
      if (deltaNadjenih >= ANGLE_TOLERANCE) {
        // Prešli smo 0.5° - to je minAngle!
        measuredMinAngle = currentAngle;
        
        Serial.print("MIN kot dosežen: ");
        Serial.print(measuredMinAngle, 1);
        Serial.println("° (0.5° nad S43 preklopom)");
        
        // Posodobi display - xMinAngle je Xfloat format (*10)
        display.setNumber("xMinAngle", (int32_t)(measuredMinAngle * 10));
        
        char statusMsg[200];
        sprintf(statusMsg, "MIN KOT DOLOCEN: %.1f deg\r(0.5 deg nad S43 preklopom)\rFAZA 2: Iskanje MAX kota...\rVreteno gre navzgor do S46", measuredMinAngle);
        display.setText("tStatus_pg5", statusMsg);
        
        // Resetiraj števec obratov - začnemo šteti od tu naprej
        inputs.resetRevolutions();
        lastRevCount = 0;
        refRevolutions = 0;
        
        // Preklopi na fazo 2
        refState = REF_FIND_MAX;
        Serial.println("Faza 2: Iskanje MAX kota - vreteno gre navzgor do S46...");
        // Motor že teče gor, samo nadaljujemo
      }
    }
  }
  
  // === FAZA 2: ISKANJE MAX KOTA ===
  else if (refState == REF_FIND_MAX) {
    // Štej obrate
    uint32_t currentRevCount = inputs.getRevolutions();
    if (currentRevCount > lastRevCount) {
      refRevolutions += (currentRevCount - lastRevCount);
      lastRevCount = currentRevCount;
      
      // Posodobi display števila obratov (nObrati je Number format)
      display.setNumber("nObrati", (int32_t)refRevolutions);
    }
    
    // Preveri ali je doseženo končno stikalo S46
    if (inputs.isSpindleAtTop()) {
      // S46 je aktiven - dosežen MAX kot!
      outputs.stopSpindle();
      
      // Max kot je 0.5° manj od trenutnega
      measuredMaxAngle = currentAngle - ANGLE_TOLERANCE;
      
      Serial.print("S46 aktiven - MAX kot dosežen: ");
      Serial.print(measuredMaxAngle, 1);
      Serial.println("° (-0.5° toleranca)");
      
      // Posodobi display - xMaxAngle je Xfloat format (*10)
      display.setNumber("xMaxAngle", (int32_t)(measuredMaxAngle * 10));
      
      char statusMsg[200];
      sprintf(statusMsg, "S46 AKTIVEN!\rMAX KOT DOLOCEN: %.1f deg\r(-0.5 deg toleranca)\rIzracunavam statistiko...", measuredMaxAngle);
      display.setText("tStatus_pg5", statusMsg);
      
      // Končaj referenčni hod
      finishReferenceRun();
    }
  }
}

void finishReferenceRun() {
  refState = REF_IDLE;
  
  // Izračunaj statistiko
  float angleRange = measuredMaxAngle - measuredMinAngle;
  float revPerAngle = (refRevolutions > 0) ? (float)refRevolutions / angleRange : 0.0;
  float elapsedTime = (millis() - refStartTime) / 1000.0;  // v sekundah
  
  Serial.println("========================================");
  Serial.println("REFERENČNI HOD - ZAKLJUČEN");
  Serial.println("========================================");
  Serial.print("MIN kot: ");
  Serial.print(measuredMinAngle, 1);
  Serial.println("°");
  Serial.print("MAX kot: ");
  Serial.print(measuredMaxAngle, 1);
  Serial.println("°");
  Serial.print("Razpon: ");
  Serial.print(angleRange, 1);
  Serial.println("°");
  Serial.print("Število obratov: ");
  Serial.println(refRevolutions);
  Serial.print("Obrati/stopinjo: ");
  Serial.println(revPerAngle, 1);
  Serial.print("Čas: ");
  Serial.print(elapsedTime, 1);
  Serial.println(" s");
  Serial.println("========================================");
  
  // Posodobi display na pgRef (page5):
  // nCicleTime - Number format (sekunde)
  display.setNumber("nCicleTime", (int32_t)elapsedTime);
  
  // xRevPerAngle - Xfloat format (*10)
  display.setNumber("xRevPerAngle", (int32_t)(revPerAngle * 10));
  
  // Shrani v globalne spremenljivke na pgModeOFF (format × 10)
  display.setNumber("pgModeOFF.gMinAngle", (int32_t)(measuredMinAngle * 10));
  display.setNumber("pgModeOFF.gMaxAngle", (int32_t)(measuredMaxAngle * 10));
  
  // Shrani v Preferences za trajno shranjevanje
  preferences.begin("brus", false);
  preferences.putFloat("minAngle", measuredMinAngle);
  preferences.putFloat("maxAngle", measuredMaxAngle);
  preferences.putUInt("revPerAngle", (uint32_t)(revPerAngle * 10));  // × 10 za decimalno mesto
  preferences.putUInt("cycleTime", (uint32_t)(elapsedTime * 10));  // × 10 za decimalno mesto
  preferences.end();
  
  // Posodobi globalne spremenljivke v lokalnih spremenljivkah
  calibratedMinAngle = measuredMinAngle;
  calibratedMaxAngle = measuredMaxAngle;
  anglesCalibrated = true;
  
  Serial.println("Podatki shranjeni v NVS");
  
  // Končni status na displayu pgRef
  char statusMsg[200];
  sprintf(statusMsg, "REFERENCNI HOD ZAKLJUCEN!\rRazpon: %.1f deg | Obrati: %lu\rOb/deg: %.1f | Cas: %d s\rPodatki shranjeni v NVS", 
          angleRange, refRevolutions, revPerAngle, (int)elapsedTime);
  display.setText("tStatus_pg5", statusMsg);
}

// ===== PAGE CHANGE HANDLER =====
void handlePageChange(uint8_t newPage) {
  Serial.print("[PAGE] Preklop na stran ");
  Serial.println(newPage);
  
  currentPage = newPage;
  display.setCurrentPage(newPage);  // Posodobi tudi v NextionDisplay
  
  // Posebna logika za vsako stran
  switch(newPage) {
    case 0:  // pgIntro
      Serial.println("  -> pgIntro (startup)");
      // Pošlji podatke v globalne spremenljivke na pgModeOFF
      // Format: Xfloat × 10, Number pa direktno
      display.setNumber("pgModeOFF.gMinAngle", (int32_t)(calibratedMinAngle * 10));
      display.setNumber("pgModeOFF.gMaxAngle", (int32_t)(calibratedMaxAngle * 10));
      display.setNumber("pgModeOFF.gStartAngle", (int32_t)(savedAngleStart * 10));
      display.setNumber("pgModeOFF.gStopAngle", (int32_t)(savedAngleStop * 10));
      display.setNumber("pgModeOFF.gSpeed", (int32_t)(speedRocno));  // Number - brez *10
      
      Serial.println("  -> Podatki poslani v pgModeOFF globalne spremenljivke");
      // pgIntro se avtomatsko preklopi na pgModeOFF
      break;
      
    case 1: { // pgModeOFF
      Serial.println("  -> pgModeOFF (osnovna stran)");
      
      // Preveri pogoje in nastavi statusno sporočilo
      String statusMsg = "";
      
      if (!anglesCalibrated) {
        statusMsg = "Najprej v Nastavitvah izvedite referencni hod!";
      } else if (!anglesConfigured) {
        statusMsg = "Nastavite zacetni in koncni kot!";
      } else {
        statusMsg = "Preklopite stikalo na MAN ali AUTO!";
      }
      
      display.setText("tStatus_pg1", statusMsg.c_str());
      Serial.print("  -> Status: ");
      Serial.println(statusMsg);
      
      // Preveri S1 stikalo in preklopi če je potrebno
      S1Mode mode = inputs.getS1Mode();
      lastS1Mode = mode;
      
      if (mode == MODE_MANUAL) {
        Serial.println("  -> S1 = MANUAL, preklop na pgModeMAN");
        display.showPage(2);
      } else if (mode == MODE_AUTO) {
        Serial.println("  -> S1 = AUTO, preklop na pgModeAUTO");
        display.showPage(3);
      } else {
        Serial.println("  -> S1 = OFF, ostajamo na pgModeOFF");
      }
      break;
    }
      
    case 2:  // pgModeMAN
      Serial.println("  -> pgModeMAN (ročni način)");
      // Naloži hitrost iz preferences
      display.setProgress("hRocno", speedRocno);
      
      // Preveri pogoje in nastavi statusno sporočilo
      if (!anglesCalibrated) {
        display.setText("tStatus_pg2", "OPOZORILO: Ni referenčnega hoda!");
      } else {
        display.setText("tStatus_pg2", "Ročni način - uporabite tipke ali gumbe");
      }
      break;
      
    case 3:  // pgModeAUTO
      Serial.println("  -> pgModeAUTO (avtomatski način)");
      // Naloži hitrost
      display.setNumber("nSpeed", (int32_t)(speedZacetni));  // Number - brez *10
      
      // Preveri pogoje in nastavi statusno sporočilo ter bStart omogočenost
      updateAutoModeReadiness();
      break;
      
    case 4:  // pgSettings
      Serial.println("  -> pgSettings (menu nastavitev)");
      // Samo navigacijski menu, ni aktivnih elementov
      break;
      
    case 5:  // pgRef
      Serial.println("  -> pgRef (referenčni hod)");
      // Resetiraj stanje referenčnega hoda
      refState = REF_IDLE;
      
      // Naloži globalne spremenljivke iz pgModeOFF (xMinAngle, xMaxAngle)
      // Te se avtomatsko nalagajo iz globalnih spremenljivk v Nextion GUI
      
      // Naloži hitrost iz preferences (speedSredina)
      display.setNumber("nSpeed", (int32_t)(speedSredina));  // Number - brez *10
      
      // Resetiraj prikaze
      display.setNumber("nObrati", 0);
      display.setNumber("nCicleTime", 0);
      display.setNumber("xRevPerAngle", 0);  // Xfloat format
      
      // Statusno sporočilo
      if (!anglesCalibrated) {
        display.setText("tStatus_pg5", "Pritisnite START za referenčni hod");
      } else {
        display.setText("tStatus_pg5", "OPOZORILO: Referenčni hod bo prepisu obstoječe!");
      }
      break;
      
    case 6:  // pgSpeed
      Serial.println("  -> pgSpeed (hitrosti)");
      // Posodobi prikaz hitrosti
      display.setProgress("hZacetni", speedZacetni);
      display.setProgress("hSredina", speedSredina);
      display.setProgress("hKoncni", speedKoncni);
      display.setProgress("hRocno", speedRocno);
      break;
      
    case 7:  // pgAngle
      Serial.println("  -> pgAngle (nastavitev kotov)");
      // Inicializiraj začasne kote
      tempAngleStart = savedAngleStart;
      tempAngleStop = savedAngleStop;
      anglesChanged = false;
      angleSettingMode = ANGLE_IDLE;
      updatePage1AngleDisplay();
      display.setButtonState("bSave", false);
      break;
  }
}
