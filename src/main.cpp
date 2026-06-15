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
unsigned long lastMagnetCalUpdate = 0;
const unsigned long MAGNET_CAL_UPDATE_INTERVAL = 300;  // Osvežitev kalibracije magneta vsakih 0.3s (debug)

// Power monitoring - Detekcija napajanja iz usmernika
bool powerPresent = false;          // Ali je napajanje iz usmernika prisotno
float powerVoltage = 0.0;           // Napetost na ADC pinu (V)
unsigned long lastPowerCheck = 0;   // Časovnik za periodično preverjanje

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

// AS5600 nastavitve (pgMagCal - page8)
bool as5600DirCW = true;     // true=CW (clockwise), false=CCW (counterclockwise)
float as5600AngleOffset = 0.0;  // Offset za prikazovanje kota (nastavi z bSetZero)

// ===== PAGE MANAGEMENT - NOVA STRUKTURA =====
// page0: pgIntro (startup, 5s timeout)
// page1: pgModeOFF (osnovna stran)
// page2: pgModeMAN (ročni način)
// page3: pgModeAUTO (avtomatski način)
// page4: pgSettings (nastavitve menu)
// page5: pgRef (referenčni hod)
// page6: pgSpeed (hitrosti)
// page7: pgAngle (koti)
// page8: pgMagCal (kalibracija magneta AS5600)
uint8_t currentPage = 0;  // Trenutna stran
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

// Začasne spremenljivke za parsing ID5/ID6 iz bSave gumba
static int16_t receivedID5 = -1;  // xStopAngle × 10
static int16_t receivedID6 = -1;  // xStartAngle × 10

// Flags za opozorila
bool autoModeWarningShown = false;  // Za prikaz NAPAKA: Koti niso nastavljeni samo enkrat
bool safeModeStatusShown = false;   // Za prikaz safe mode statusa samo enkrat
bool powerStatusShown = false;      // Za prikaz power monitoring statusa samo enkrat

// Zadnje prikazano stanje za debug izpis
static S1Mode lastPrintedMode = MODE_OFF;
static S2Cycles lastPrintedCycles = CYCLES_NONE;
static unsigned long lastPrintedRevolutions = 0;
static bool lastPrintedMotor = false;
static bool lastPrintedPump = false;
static bool lastPrintedKnife = false;
static bool lastPrintedSpindle = false;
static SpindleDirection lastPrintedDir = SPINDLE_DOWN;
static uint8_t lastPrintedSpeed = 0;

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
void updateMagnetCalibrationDisplay(bool forceUpdate = false);

// ===== POWER MONITORING FUNCTIONS =====
/**
 * @brief Preberi napetost na ADC pinu za detekcijo napajanja
 * @return Napetost v voltih
 */
float readPowerVoltage() {
  int adcValue = analogRead(POWER_SENSE_PIN);
  float voltage = (adcValue / (float)ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;
  return voltage;
}

/**
 * @brief Preveri prisotnost napajanja iz usmernika
 * @return true če je napajanje prisotno, false sicer
 */
bool checkPowerPresence() {
  powerVoltage = readPowerVoltage();
  powerPresent = (powerVoltage >= POWER_VOLTAGE_THRESHOLD);
  return powerPresent;
}

/**
 * @brief Safe mode zanka - aktivna samo Serial komunikacija
 * Program ostane tukaj dokler se napajanje iz usmernika ne pojavi
 */
void safeModeLoop() {
  Serial.println("========================================");
  Serial.println("VARNOSTNI NAČIN (SAFE MODE)");
  Serial.println("========================================");
  Serial.println("OPOZORILO: Napajanje iz usmernika ni prisotno!");
  Serial.println("Periferne enote (display, vhodi, senzorji) ne delujejo.");
  Serial.println("Program je v varnostnem načinu - aktivna samo Serial komunikacija.");
  Serial.println("Čakam na napajanje iz usmernika...");
  Serial.println("========================================");
  
  while (!checkPowerPresence()) {
    // Feed watchdog
    yield();
    
    // Preveri napajanje vsake 0.5s
    static unsigned long lastCheck = 0;
    unsigned long currentMillis = millis();
    
    if (currentMillis - lastCheck >= 500) {
      lastCheck = currentMillis;
      
      // Pošlji status po Serial samo prvič
      if (!safeModeStatusShown) {
        Serial.print("Napajanje iz usmernika: NI PRISOTNO | ADC napetost: ");
        Serial.print(powerVoltage, 2);
        Serial.println(" V");
        safeModeStatusShown = true;
      }
    }
    
    // Preglej Serial ukaze (angle= simulator mode)
    if (Serial.available()) {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      
      if (cmd.startsWith("angle=")) {
        float angle = cmd.substring(6).toFloat();
        if (angle >= 0 && angle <= 360) {
          Serial.print("OPOZORILO: Simulator mode v safe mode. Kot: ");
          Serial.print(angle, 1);
          Serial.println("° (simulator še ni inicializiran)");
        }
      }
      
      // Očisti buffer
      while (Serial.available()) Serial.read();
    }
    
    delay(50);
  }
  
  // Reset flag za naslednji safe mode
  safeModeStatusShown = false;
  
  Serial.println("========================================");
  Serial.println("Napajanje iz usmernika PRISOTNO!");
  float realVoltage = powerVoltage * 2.0;  // Delilnik napetosti 1:1
  Serial.print("Napajalna napetost: ");
  Serial.print(realVoltage, 2);
  Serial.print(" V (ADC: ");
  Serial.print(powerVoltage, 2);
  Serial.println(" V)");
  Serial.println("Zagon normalnega obratovanja...");
  Serial.println("========================================");
}
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
void updatePage7AngleDisplay();
void updateBNastaviKoteText();
void processSavedAngles();
void updateAutoModeReadiness();
void setXFloatValue(const char* objName, float value);
void resetXFloatCache();  // Resetira cache ob spremembi strani
void setNumberWithLength(const char* objName, int32_t value);  // Number z avtomatskim lenth atributom

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
  
  // ===== POWER MONITORING - ADC inicializacija =====
  pinMode(POWER_SENSE_PIN, INPUT);
  analogReadResolution(12);  // 12-bit ADC (0-4095)
  Serial.println("Power monitoring inicializiran (GPIO1 - ADC1_0)");
  
  // ===== INICIALIZACIJA KOMPONENT (VEDNO, NE GLEDE NA NAPAJANJE) =====
  // Te komponente morajo biti inicializirane pred safe mode loop-om
  
  // Inicializacija izhodov (NAJPREJ - varno stanje)
  outputs.begin();
  
  // Inicializacija vhodov
  inputs.begin();
  
  // Nastavi AS5600 DIR pin iz Preferences (po inputs.begin, ki je že inicializiral pin)
  digitalWrite(AS5600_DIR, as5600DirCW ? LOW : HIGH);
  Serial.print("[SETUP] AS5600 DIR iz Preferences: ");
  Serial.println(as5600DirCW ? "CW" : "CCW");
  
  // POMEMBNO: Počakaj da se AS5600 odzove na DIR spremembo
  // AS5600 potrebuje nekaj časa (~10ms) da spremeni smer števanja
  delay(20);
  
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
  
  // ===== PREVERI PRISOTNOST NAPAJANJA =====
  checkPowerPresence();
  Serial.print("Začetno stanje napajanja: ");
  if (powerPresent) {
    float realVoltage = powerVoltage * 2.0;  // Delilnik napetosti 1:1
    Serial.print("PRISOTNO | Napajalna napetost: ");
    Serial.print(realVoltage, 2);
    Serial.print(" V (ADC: ");
    Serial.print(powerVoltage, 2);
    Serial.println(" V)");
  } else {
    Serial.print("NI PRISOTNO | ADC napetost: ");
    Serial.print(powerVoltage, 2);
    Serial.println(" V)");
    // Če napajanje ni prisotno, pojdi v safe mode
    safeModeLoop();
    
    // Po vrnitvi iz safe mode: display je že poslal startup sekvenco
    // NE kličemo display.begin() (bi poslal nov reset ukaz)
    // Samo počakamo na startup sekvenco in registriramo callbacke
    display.waitForStartup();
    
    // Registriraj callback funkcije
    display.onTouch(onNextionTouch);
    display.onPageChange(onNextionPageChange);
    display.onString(onNextionString);
    display.onNumeric(onNextionNumeric);
    Serial.println("[MAIN] Nextion callbacks registered");
    
    // NE nastavljaj gumbov tukaj - display je na page 0 (intro), gumbi so na page 1
    // Stanje gumbov se bo nastavilo avtomatsko ko se display preklopi na page 1
    
    // Naloži shranjene kote iz NVS
    loadAnglesFromPreferences();
    
    Serial.println("Sistem pripravljen po povrnitvi napajanja!");
    return;  // Preskoči ostalo inicializacijo displaya (je že narejena)
  }
  
  // ===== INICIALIZACIJA NEXTION DISPLAYA (normalno, s reset ukazom) =====
  display.begin();
  
  // Registriraj callback funkcije
  display.onTouch(onNextionTouch);
  display.onPageChange(onNextionPageChange);
  display.onString(onNextionString);
  display.onNumeric(onNextionNumeric);
  Serial.println("[MAIN] Nextion callbacks registered");
  
  // Naloži shranjene kote iz NVS
  loadAnglesFromPreferences();
  
  // Nastavi začetno stran - pgIntro (page0)
  // ZAČASNO IZKLJUČENO zaradi debugginga - gremo direktno na page1
  // display.showPage(1);
  // currentPage = 1;
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
  
  // DIR checkbox event (page8 - pgMagCal)
  if (data.startsWith("DIR:")) {
    int dirValue = data.substring(4).toInt();
    as5600DirCW = (dirValue == 0);  // 0=CW, 1=CCW
    
    // Posodobi DIR pin
    digitalWrite(AS5600_DIR, as5600DirCW ? LOW : HIGH);
    
    // POMEMBNO: Počakaj da se AS5600 odzove na DIR spremembo
    delay(20);
    
    // Shrani v Preferences
    preferences.begin("brus", false);
    preferences.putBool("as5600DirCW", as5600DirCW);
    preferences.end();
    
    Serial.print("[DIR] AS5600 smer nastavljena na: ");
    Serial.println(as5600DirCW ? "CW (clockwise)" : "CCW (counterclockwise)");
    
    // Posodobi prikaz (po delay-u ko je AS5600 že odzval)
    updateMagnetCalibrationDisplay(false);
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
    
    // Posodobi text polje t1 (id=15) na strani 2
    char buffer[8];
    sprintf(buffer, "%d", speedRocno);
    display.setText("t1", buffer);
    
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
  
  // Preveri če je ID6: event - lahko je hSredina (page6) ALI xStartAngle (page7)
  if (data.startsWith("ID6:")) {
    String valueStr = data.substring(4);
    
    // page6 (pgSpeed): ID6 = hSredina slider (hitrost 50-100%)
    if (currentPage == 6) {
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
    
    // page7 (pgAngle): ID6 = xStartAngle (začetni kot × 10)
    if (currentPage == 7) {
      receivedID6 = valueStr.toInt();
      Serial.print("[ID6] xStartAngle prejeto: ");
      Serial.print(receivedID6);
      Serial.print(" (");
      Serial.print(receivedID6 / 10.0, 1);
      Serial.println("°)");
      
      // Če imamo obe vrednosti, obdelaj in shrani
      if (receivedID5 >= 0) {
        processSavedAngles();
      }
      return;
    }
    
    // Če nismo na page6 ali page7, ignoriraj
    Serial.print("[ID6] Neznan kontekst - currentPage: ");
    Serial.println(currentPage);
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
  
  // Parsing kotov iz page7 (pgAngle) - bSave gumb pošlje dva ločena eventa:
  // 1. "#ID5:" + 2 bajta (xStopAngle.val) - končni kot × 10
  // 2. "#ID6:" + 2 bajta (xStartAngle.val) - začetni kot × 10
  
  // Preveri če je ID5: event (xStopAngle na page7)
  if (data.startsWith("ID5:") && currentPage == 7) {
    String valueStr = data.substring(4);
    receivedID5 = valueStr.toInt();
    Serial.print("[ID5] xStopAngle prejeto: ");
    Serial.print(receivedID5);
    Serial.print(" (");
    Serial.print(receivedID5 / 10.0, 1);
    Serial.println("°)");
    
    // Če imamo obe vrednosti, obdelaj in shrani
    if (receivedID6 >= 0) {
      processSavedAngles();
    }
    return;
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
        
      case 7: { // bPnev - toggle ventil noža (samo če je kot v mejah)
        // Preveri ali je kot v mejah (samo če je kalibracija izvedena)
        if (anglesCalibrated) {
          float currentAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
          if (currentAngle >= calibratedMinAngle && currentAngle <= calibratedMaxAngle) {
            pnevActive = !pnevActive;
            outputs.setKnifePusher(pnevActive);
            display.setPnevState(pnevActive);
            Serial.print("[bPnev] Ventil noža: ");
            Serial.println(pnevActive ? "ON" : "OFF");
          } else {
            Serial.print("[bPnev] NAPAKA: Kot izven mej! Trenutni: ");
            Serial.print(currentAngle, 1);
            Serial.print("°, Meje: ");
            Serial.print(calibratedMinAngle, 1);
            Serial.print("-");
            Serial.print(calibratedMaxAngle, 1);
            Serial.println("°");
            display.setText("tStatus_pg2", "NAPAKA: Kot izven mej!");
          }
        } else {
          // Ni kalibracije - dovoli uporabo brez preverjanja
          pnevActive = !pnevActive;
          outputs.setKnifePusher(pnevActive);
          display.setPnevState(pnevActive);
          Serial.print("[bPnev] Ventil noža: ");
          Serial.println(pnevActive ? "ON" : "OFF");
        }
        break;
      }
        
      default:
        Serial.print("Neznan gumb ID na page2: ");
        Serial.println(componentId);
        break;
    }
  }
  // ===== PAGE 3 - pgModeAUTO =====
  else if (currentPage == 3) {
    switch(componentId) {
      case 9: { // bStart - začni/ustavi avtomatski cikel
        // Če je cikel že aktiven, ga ustavi
        if (autoCycle.isRunning()) {
          Serial.println("========================================");
          Serial.println("[bStart] STOP AVTOMATSKEGA CIKLA");
          Serial.println("========================================");
          
          autoCycle.stop();
          autoModeActive = false;
          
          display.setText("bStart", "START");
          display.setText("tStatus_pg3", "Cikel ustavljen");
          
          // Posodobi bStart omogočenost
          updateAutoModeReadiness();
          break;
        }
        
        // Sicer začni nov cikel - preveri pogoje
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
        float currentAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
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
                       speedZacetni, speedSredina, speedKoncni, revPerAngle,
                       as5600AngleOffset);
        autoModeActive = true;
        
        display.setText("bStart", "STOP");
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
      case 3: { // bRefStart - začni/ustavi referenčni hod
        // Če je ref. hod že aktiven, ga ustavi
        if (refState != REF_IDLE) {
          Serial.println("[bRefStart] STOP referenčnega hoda");
          
          // Ustavi motor
          outputs.stopSpindle();
          refState = REF_IDLE;
          
          display.setText("bRefStart", "START");
          display.setText("tStatus_pg5", "Referenčni hod ustavljen");
          
          // Preveri kot za ponovno omogočitev gumba
          float currentAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
          bool angleInRange = (currentAngle >= 0.0 && currentAngle <= 30.0);
          display.setButtonState("bRefStart", angleInRange);
          break;
        }
        
        // Sicer začni nov ref. hod - preveri kot
        float currentAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
        if (currentAngle < 0.0 || currentAngle > 30.0) {
          Serial.print("[bRefStart] NAPAKA: Kot izven mej! Trenutni: ");
          Serial.print(currentAngle, 1);
          Serial.println("° (zahtevano: 0-30°)");
          display.setText("tStatus_pg5", "NAPAKA: Pozicioniraj vreteno med 0-30°!");
          break;
        }
        
        // Vse OK - začni referenčni hod
        Serial.println("[bRefStart] Začetek referenčnega hoda");
        startReferenceRun();
        display.setText("bRefStart", "STOP");
        break;
      }
        
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
          tempAngleStart = angleSensor.getCalibratedAngle(as5600AngleOffset);
          Serial.println("[bNastaviKote] Nastavljanje ZAČETNEGA kota - uporabite tipke S42(Gor)/S41(Dol)");
          display.setText("bNastaviKote", "Zacetni");
          updatePage7AngleDisplay();
        }
        else if (angleSettingMode == ANGLE_SET_START) {
          // Shrani začetni kot in začni nastavitev končnega
          tempAngleStart = angleSensor.getCalibratedAngle(as5600AngleOffset);
          angleSettingMode = ANGLE_SET_STOP;
          anglesChanged = true;
          tempAngleStop = angleSensor.getCalibratedAngle(as5600AngleOffset);
          Serial.print("[bNastaviKote] Začetni kot nastavljen: ");
          Serial.print(tempAngleStart, 1);
          Serial.println("°");
          Serial.println("[bNastaviKote] Nastavljanje KONČNEGA kota - uporabite tipke S42(Gor)/S41(Dol)");
          display.setText("bNastaviKote", "Koncni");
          updatePage7AngleDisplay();
          display.setButtonState("bSave", true);
        }
        else if (angleSettingMode == ANGLE_SET_STOP) {
          // Shrani končni kot in resetiraj
          tempAngleStop = angleSensor.getCalibratedAngle(as5600AngleOffset);
          
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
          updatePage7AngleDisplay();
        }
        break;
        
      // bSave (id=8) ne pošilja press eventa - pošilja vrednosti kotov (#ID5:, #ID6:)
      // Obdelava je v handleStringData
      
      default:
        Serial.print("Neznan gumb ID na page7: ");
        Serial.println(componentId);
        break;
    }
  }
  // ===== PAGE 8 - pgMagCal (KALIBRACIJA MAGNETA) =====
  else if (currentPage == 8) {
    switch(componentId) {
      case 12:  // bSetZero - nastavi trenutni kot kot referenco (0°)
        Serial.println("[bSetZero] Nastavljam trenutni kot kot referenco (0°)");
        
        // Shrani trenutni kot kot offset
        as5600AngleOffset = inputs.getAngleEncoder()->getAngle();
        
        // Shrani v Preferences
        preferences.begin("brus", false);
        preferences.putFloat("as5600Offset", as5600AngleOffset);
        preferences.end();
        
        Serial.print("[bSetZero] Offset nastavljen na: ");
        Serial.print(as5600AngleOffset, 1);
        Serial.println("°");
        
        // Posodobi prikaz - xAngle bo pokazal 0.0°
        updateMagnetCalibrationDisplay(false);
        break;
        
      default:
        Serial.print("Neznan gumb ID na page8: ");
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
        float currentAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
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
                       speedZacetni, speedSredina, speedKoncni, revPerAngle,
                       as5600AngleOffset);
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
  
  // ===== POWER MONITORING - Preverjanje napajanja TAKOJ na začetku =====
  // Preverimo napajanje PRED vsakim update() klicem, da preprečimo lažne alarme
  checkPowerPresence();
  if (!powerPresent) {
    // Napajanje je izpadlo - pojdi v safe mode
    Serial.println("========================================");
    Serial.println("OPOZORILO: Napajanje iz usmernika IZPADLO!");
    Serial.print("ADC napetost: ");
    Serial.print(powerVoltage, 2);
    Serial.println(" V");
    Serial.println("========================================");
    
    // Ustavi vse izhode (varnostno) - samo enkrat
    outputs.emergencyStop();
    autoCycle.stop();
    
    // Pojdi v safe mode zanko - tukaj ostane dokler napajanje ne pride nazaj
    safeModeLoop();
    
    // Ko se vrne iz safe mode:
    // Display je poslal startup sekvenco in je že na page0
    // display.update() bo zaznal startup sekvenco in naredil re-inicializacijo
    Serial.println("========================================");
    Serial.println("Povrnitev napajanja...");
    Serial.println("Čakam na startup sekvenco displaya...");
    Serial.println("========================================");
    
    // POMEMBNO: vrni se na začetek loop() in pusti display.update() 
    // da zazna startup sekvenco in naredi re-inicializacijo
    return;
  }
  
  // ===== STARTUP SEKVENCA - Preveri če je display poslal startup signal =====
  // To se zgodi ob povrnitvi napajanja ko se display ponovno zažene
  if (display.wasStartupDetected()) {
    Serial.println("========================================");
    Serial.println("STARTUP RE-INICIALIZACIJA");
    Serial.println("Display se je ponovno zagnal (povrnitev napajanja)");
    Serial.println("Sistem se obnaša kot nov zagon");
    Serial.println("========================================");
    
    // Resetiraj stanje - stroj se obnaša kot da se je zagnal na novo
    currentPage = 0;
    
    // Počisti startup flag
    display.clearStartupFlag();
    
    // Nadaljuj z normalnim delovanjem
  }
  
  // ===== POWER MONITORING  =====
  if (currentMillis - lastPowerCheck >= POWER_CHECK_INTERVAL) {
    lastPowerCheck = currentMillis;
    
    // Debug output samo prvič
    if (!powerStatusShown) {
      float realVoltage = powerVoltage * 2.0;  // Delilnik napetosti 1:1
      Serial.print("[POWER] Napajanje iz usmernika: OK | Napajalna napetost: ");
      Serial.print(realVoltage, 2);
      Serial.print(" V (ADC: ");
      Serial.print(powerVoltage, 2);
      Serial.println(" V)");
      powerStatusShown = true;
    }
  }
  
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
  
  // TEST: Posodobi AS5600 senzor vsakih 300ms
  static unsigned long lastSensorUpdate = 0;
  static float lastDisplayedAngle = -999.0;
  static unsigned long lastAngleUpdate = 0;
  
  if (currentMillis - lastSensorUpdate >= 300) {
    lastSensorUpdate = currentMillis;
    
    // Preberi senzor
    angleSensor.update();
    
    // Posodobi prikaz kota xAngle na stranih ki ga imajo (1,2,3,5,7)
    if (currentPage == 1 || currentPage == 2 || currentPage == 3 || currentPage == 5 || currentPage == 7) {
      float displayAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
      
      // Vedno posodobi display ob vsakem branju (vsakih 300ms)
      // Serial.print("[TEST 300ms] AS5600 prikazani kot: ");
      // Serial.print(displayAngle, 1);
      // Serial.println("°");
      
      setXFloatValue("xAngle", displayAngle);
      lastDisplayedAngle = displayAngle;
      lastAngleUpdate = currentMillis;
    }
  }
  
  // Posodobi podatke na page2 (pgModeMAN) vsake 0.5s
  static unsigned long lastPage2Update = 0;
  if (currentPage == 2 && (currentMillis - lastPage2Update >= 500)) {
    lastPage2Update = currentMillis;
    
    // Posodobi tStatus_pg2 - status motorja in sporočila
    // Preveri in omogoči/onemogoči bPnev glede na kot
    float currentAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
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
  
  // Posodobi tPomik na page2 (pgModeMAN) in page3 (pgModeAUTO) vsake 0.5s
  static unsigned long lastPageUpdate = 0;
  if ((currentPage == 2 || currentPage == 3) && (currentMillis - lastPageUpdate >= 500)) {
    lastPageUpdate = currentMillis;
    
    // Posodobi tPomik - status motorja vretena (obstaja na page2 in page3)
    if (outputs.isSpindleMoving()) {
      if (outputs.getSpindleDirection() == SPINDLE_UP) {
        display.setText("tPomik", "GOR");
        display.sendRawCommand("tPomik.pco=1024");  // Modra
      } else {
        display.setText("tPomik", "DOL");
        display.sendRawCommand("tPomik.pco=1024");  // Modra
      }
    } else {
      display.setText("tPomik", "STOP");
      display.sendRawCommand("tPomik.pco=63488");  // Rdeča
    }
    
    // Posodobi ostale podatke samo na page3 (pgModeAUTO)
    if (currentPage == 3) {
      // Posodobi tCikli - format "izvedeni/nastavljeni"
      S2Cycles cycles = inputs.getS2Cycles();
      char cikliText[20];
      if (cycles == CYCLES_CONTINUOUS) {
        sprintf(cikliText, "%d/∞", autoCycle.getCompletedCycles());
      } else {
        sprintf(cikliText, "%d/%d", autoCycle.getCompletedCycles(), (int)cycles);
      }
      display.setText("tCikli", cikliText);
    
      // Posodobi tKamen - status brusnega kamna
      if (outputs.isGrindingMotorOn()) {
        display.setText("tKamen", "RUN");
        display.sendRawCommand("tKamen.pco=1024");  // Modra
      } else {
        display.setText("tKamen", "STOP");
        display.sendRawCommand("tKamen.pco=63488");  // Rdeča
      }
      
      // Posodobi tCilinder - status pnevmatskega cilindra
      BrusOutputs::KnifeCylinderState cylinderState = outputs.getKnifeCylinderState();
      if (cylinderState == BrusOutputs::KNIFE_MOVING_OUT) {
        display.setText("tCilinder", "OUT");
        display.sendRawCommand("tCilinder.pco=1024");  // Modra
      } else if (cylinderState == BrusOutputs::KNIFE_MOVING_IN) {
        display.setText("tCilinder", "IN");
        display.sendRawCommand("tCilinder.pco=1024");  // Modra
      } else {
        display.setText("tCilinder", "STOP");
        display.sendRawCommand("tCilinder.pco=63488");  // Rdeča
      }
      
      // Preveri pogoje za bStart in posodobi tStatus_pg3
      updateAutoModeReadiness();
      
      // Posodobi tekst gumba bStart glede na stanje cikla
      if (autoCycle.isRunning()) {
        display.setText("bStart", "STOP");
      } else {
        display.setText("bStart", "START");
      }
    }
  }
  
  // Če smo na page7 (pgAngle) in nastavljamo kote, posodabljaj prikaz v realnem času
  if (currentPage == 7 && angleSettingMode != ANGLE_IDLE) {
    float previousAngle = (angleSettingMode == ANGLE_SET_START) ? tempAngleStart : tempAngleStop;
    
    if (angleSettingMode == ANGLE_SET_START) {
      tempAngleStart = angleSensor.getCalibratedAngle(as5600AngleOffset);
      if (abs(tempAngleStart - previousAngle) > 0.1) {  // Sprememba > 0.1°
        anglesChanged = true;
        display.setButtonState("bSave", true);
      }
    } else if (angleSettingMode == ANGLE_SET_STOP) {
      tempAngleStop = angleSensor.getCalibratedAngle(as5600AngleOffset);
      if (abs(tempAngleStop - previousAngle) > 0.1) {  // Sprememba > 0.1°
        anglesChanged = true;
        display.setButtonState("bSave", true);
      }
    }
    updatePage7AngleDisplay();
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
        setNumberWithLength("nCicleTime", (int32_t)elapsedSeconds);
      }
    } else if (refState == REF_IDLE) {
      // Če ref. hod ni aktiven, posodabljaj omogočenost bRefStart glede na kot
      static unsigned long lastRefButtonUpdate = 0;
      if (currentMillis - lastRefButtonUpdate >= 500) {
        lastRefButtonUpdate = currentMillis;
        float currentAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
        bool angleInRange = (currentAngle >= 0.0 && currentAngle <= 30.0);
        display.setButtonState("bRefStart", angleInRange);
        
        // Posodobi status če je izven mej
        if (!angleInRange) {
          display.setText("tStatus_pg5", "NAPAKA: Pozicioniraj vreteno med 0-30°!");
        }
      }
    }
  }
  
  // Posodobi Nextion display (vključuje obdelavo vseh dogodkov preko callbackov)
  display.update(currentMillis);
  
  // ===== PAGE 8 - Avtomatska osvežitev kalibracije magneta =====
  if (currentPage == 8) {
    if (currentMillis - lastMagnetCalUpdate >= MAGNET_CAL_UPDATE_INTERVAL) {
      updateMagnetCalibrationDisplay(false);
      lastMagnetCalUpdate = currentMillis;
    }
  }
  
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
      // Resetiraj warning flag za naslednji vstop v AUTO način
      autoModeWarningShown = false;
    }
    
    // Ob preklopu V MODE_OFF ustavi vse
    if (mode == MODE_OFF && lastMode != MODE_OFF) {
      if (outputs.isGrindingMotorOn() || outputs.isWaterPumpOn() || 
          outputs.isKnifePusherOn() || outputs.isSpindleMoving()) {
        Serial.println("[MODE_OFF] Preklop iz delovnega načina - ustavljam vse");
        outputs.emergencyStop();
      }
      if (autoCycle.isRunning()) {
        autoCycle.stop();
      }
      autoModeActive = false;
    }
    
    // Ob preklopu v AUTO način
    if (mode == MODE_AUTO && lastMode != MODE_AUTO) {
      // Če nismo na page3 (pgModeAUTO), pojdi na page3
      if (currentPage != 3) {
        Serial.println("[MODE_AUTO] Preklop na page3 (pgModeAUTO)");
        display.showPage(3);
        currentPage = 3;
      }
      // Posodobi bStart gumb glede na pripravljenost
      updateAutoModeReadiness();
    }
    
    lastMode = mode;
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
        tempAngleStart = angleSensor.getCalibratedAngle(as5600AngleOffset);
      } else if (angleSettingMode == ANGLE_SET_STOP) {
        tempAngleStop = angleSensor.getCalibratedAngle(as5600AngleOffset);
      }
    }
    
    // Varnostna preverjanja limitov (če je kalibracija opravljena)
    float currentAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
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
    
    // V ROČNEM načinu motorji in vreteno se upravljajo ročno
    // Črpalka deluje avtomatsko z motorjem brusa (setGrindingMotor)
    
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
        if (!autoModeWarningShown) {
          Serial.println("NAPAKA: Koti niso nastavljeni - uporabite page7!");
          autoModeWarningShown = true;
        }
        // V novem GUI je page3 za start, brez dodatnih sporočil tukaj
      } else {
        autoModeWarningShown = false;  // Resetiraj warning flag
        S2Cycles cycles = inputs.getS2Cycles();
        
        if (cycles == CYCLES_CONTINUOUS) {
          autoCycle.start(0, savedAngleStart, savedAngleStop, 
                         calibratedMinAngle, calibratedMaxAngle, anglesCalibrated,
                         speedZacetni, speedSredina, speedKoncni, revPerAngle,
                         as5600AngleOffset);
          autoModeActive = true;
        } 
        else if (cycles >= CYCLES_2 && cycles <= CYCLES_7) {
          autoCycle.start(cycles, savedAngleStart, savedAngleStop,
                         calibratedMinAngle, calibratedMaxAngle, anglesCalibrated,
                         speedZacetni, speedSredina, speedKoncni, revPerAngle,
                         as5600AngleOffset);
          autoModeActive = true;
        }
        else {
          // S2 ni nastavljen na veljavno pozicijo
          Serial.println("OPOZORILO: Nastavite število ciklov na S2!");
        }
      }
    }
  }
  
  // ===== RESET TIPKA (EMERGENCY STOP) =====
  if (inputs.isResetPressed()) {
    // Emergency stop v vseh načinih
    outputs.emergencyStop();
    autoCycle.stop();
    Serial.println("*** RESET - EMERGENCY STOP ***");
    
    delay(500); // Debounce za tipko
  }
  
  // ===== TEMPERATURA ALARM =====
  if (inputs.isTempAlarm()) {
    outputs.emergencyStop();
    autoCycle.stop();
    Serial.println("!!! TEMPERATURA ALARM - SISTEM USTAVLJEN !!!");
  }
  
  // ===== PREVERJANJE NAPAK CILINDRA NOŽA =====
  if (outputs.hasKnifeCylinderError()) {
    // Cilinder ni dosegel končnega položaja v varnostnem času
    if (pnevActive) {
      pnevActive = false;
      display.setPnevState(false);
      Serial.print("!!! NAPAKA CILINDRA: ");
      Serial.println(outputs.getKnifeCylinderError());
      
      // Prikaži napako na displayu če smo na page2
      if (currentPage == 2) {
        display.setText("tStatus_pg2", outputs.getKnifeCylinderError().c_str());
      }
    }
  }
  
  // ===== PERIODIČEN DEBUG IZPIS - samo ob spremembi =====
  if (millis() - lastPrintTime > PRINT_INTERVAL) {
    lastPrintTime = millis();
    
    // Preveri spremembe
    S2Cycles cycles = inputs.getS2Cycles();
    unsigned long revolutions = inputs.getRevolutions();
    bool motorOn = outputs.isGrindingMotorOn();
    bool pumpOn = outputs.isWaterPumpOn();
    bool knifeOn = outputs.isKnifePusherOn();
    bool spindleMoving = outputs.isSpindleMoving();
    SpindleDirection spindleDir = outputs.getSpindleDirection();
    uint8_t spindleSpeed = outputs.getSpindleSpeed();
    
    bool stateChanged = (mode != lastPrintedMode) ||
                        (cycles != lastPrintedCycles) ||
                        (revolutions != lastPrintedRevolutions) ||
                        (motorOn != lastPrintedMotor) ||
                        (pumpOn != lastPrintedPump) ||
                        (knifeOn != lastPrintedKnife) ||
                        (spindleMoving != lastPrintedSpindle) ||
                        (spindleDir != lastPrintedDir) ||
                        (spindleSpeed != lastPrintedSpeed);
    
    // Izpiši samo ob spremembi
    if (stateChanged) {
      // Serial output
      Serial.print("Mode: ");
      switch(mode) {
        case MODE_OFF:    Serial.print("OFF     "); break;
        case MODE_MANUAL: Serial.print("MANUAL  "); break;
        case MODE_AUTO:   Serial.print("AUTO    "); break;
      }
      
      if (mode == MODE_AUTO && autoCycle.isRunning()) {
        Serial.print("| ");
        autoCycle.printStatus();
      } else {
        Serial.print("| Cycles: ");
        if (cycles == CYCLES_CONTINUOUS) {
          Serial.print("CONT");
        } else {
          Serial.print(cycles);
          Serial.print("   ");
        }
        
        Serial.print(" | Rev: ");
        Serial.print(revolutions);
        Serial.print("  ");
        
        // Prikaz kota iz AS5600 (če je prisoten)
        if (USE_AS5600_FOR_TILT && inputs.getAngleEncoder()->isSensorPresent()) {
          float angle = inputs.getSpindleAngle();
          Serial.print("| Angle: ");
          Serial.print(angle, 1);
          Serial.print("° ");
        }
        
        // Prikaz aktivnih komponent
        if (motorOn) Serial.print("[MOTOR] ");
        if (pumpOn) Serial.print("[PUMP] ");
        if (knifeOn) Serial.print("[KNIFE] ");
        if (spindleMoving) {
          Serial.print("[SPINDLE ");
          Serial.print(spindleDir == SPINDLE_UP ? "UP" : "DN");
          Serial.print(":"); 
          Serial.print(spindleSpeed);
          Serial.print("] ");
        }
        
        // Senzorji in alarmi
        if (inputs.isSpindleTilted()) Serial.print("[TILT<10°] ");
        if (inputs.isTempAlarm()) Serial.print("[TEMP!] ");
        
        Serial.println();
      }
      
      // Shrani zadnje stanje
      lastPrintedMode = mode;
      lastPrintedCycles = cycles;
      lastPrintedRevolutions = revolutions;
      lastPrintedMotor = motorOn;
      lastPrintedPump = pumpOn;
      lastPrintedKnife = knifeOn;
      lastPrintedSpindle = spindleMoving;
      lastPrintedDir = spindleDir;
      lastPrintedSpeed = spindleSpeed;
    }
    
    // Nextion display update - vedno posodobi
    const char* modeStr = (mode == MODE_OFF) ? "OFF" : (mode == MODE_MANUAL) ? "MANUAL" : "AUTO";
    display.setMode(modeStr);
    
    if (mode == MODE_AUTO && autoCycle.isRunning()) {
      display.setCycles(autoCycle.getCompletedCycles(), autoCycle.getTargetCycles());
    } else {
      if (cycles == CYCLES_CONTINUOUS) {
        display.setCycles(0, 99);  // 99 = continuous
      } else {
        display.setCycles(0, cycles);
      }
      
      // OPOMBA: tAngle objekt ne obstaja več na nobeni strani
      // setAngle() funkcija zakomentirana
      // if (USE_AS5600_FOR_TILT && inputs.getAngleEncoder()->isSensorPresent()) {
      //   display.setAngle(inputs.getSpindleAngle());
      // }
    }
    
    // Nextion - število obratov
    // display.setRevolutions(inputs.getRevolutions());
    
    // Nextion - status motorjev
    // display.setMotorStatus(
    //   outputs.isGrindingMotorOn(),
    //   outputs.isWaterPumpOn(),
    //   outputs.isKnifePusherOn()
    // );
    
    // Nextion - status vretena (tPomik) se posodablja zgoraj v page2/page3 sekciji
    
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
  
  // Naloži AS5600 nastavitve
  as5600DirCW = preferences.getBool("as5600DirCW", true);  // Privzeto CW
  as5600AngleOffset = preferences.getFloat("as5600Offset", 0.0);
  
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

void setXFloatValue(const char* objName, float value) {
  // Helper funkcija za pošiljanje xFloat vrednosti z avtomatskim nastavitvijo vvs0
  // vvs0 določa število številk pred decimalno vejico, da se izognemo vodilnim ničlam
  
  // Cache za xFloat objekte - preveri ali se je vrednost spremenila
  struct XFloatCache {
    char name[32];
    float value;
  };
  static XFloatCache cache[10];  // Max 10 različnih xFloat objektov
  static uint8_t cacheSize = 0;
  
  // Posebna vrednost za resetiranje cache (klic iz resetXFloatCache)
  if (objName == nullptr) {
    cacheSize = 0;
    return;
  }
  
  // Išči objekt v cache
  int cacheIndex = -1;
  for (uint8_t i = 0; i < cacheSize; i++) {
    if (strcmp(cache[i].name, objName) == 0) {
      cacheIndex = i;
      break;
    }
  }
  
  // Preveri ali se je vrednost spremenila (>0.1°)
  if (cacheIndex >= 0) {
    if (abs(value - cache[cacheIndex].value) < 0.1) {
      return;  // Vrednost se ni spremenila dovolj - ne pošiljaj
    }
    // Posodobi cache
    cache[cacheIndex].value = value;
  } else {
    // Nov objekt - dodaj v cache
    if (cacheSize < 10) {
      strncpy(cache[cacheSize].name, objName, sizeof(cache[cacheSize].name) - 1);
      cache[cacheSize].name[sizeof(cache[cacheSize].name) - 1] = '\0';
      cache[cacheSize].value = value;
      cacheSize++;
    }
  }
  
  int32_t intValue = (int32_t)(value * 10);  // xFloat format (*10)
  
  // Določi vvs0 glede na velikost vrednosti
  uint8_t vvs0 = 1;  // Privzeto 1 števka (0-9.9)
  
  if (intValue >= 100 && intValue < 1000) {
    vvs0 = 2;  // 2 števki (10.0-99.9)
  } else if (intValue >= 1000) {
    vvs0 = 3;  // 3 števke (100.0-999.9)
  }
  
  // Pošlji vvs0 atribut
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "%s.vvs0=%d", objName, vvs0);
  display.sendRawCommand(cmd);
  
  // Pošlji vrednost
  display.setNumber(objName, intValue);
  
  // Debug izpis
  Serial.print("[xFloat] ");
  Serial.print(objName);
  Serial.print(" = ");
  Serial.print(value, 1);
  Serial.println("°");
}

void resetXFloatCache() {
  // Resetira cache za xFloat objekte - uporablja se ob spremembi strani
  // da se vrednosti ponovno pošljejo tudi če so enake kot prej
  
  // Kliči setXFloatValue z nullptr da resetira cache
  setXFloatValue(nullptr, 0.0);
  
  // Resetiraj tudi Number cache
  setNumberWithLength(nullptr, 0);
  
  Serial.println("[xFloat] Cache resetiran - ob spremembi strani");
}

void setNumberWithLength(const char* objName, int32_t value) {
  // Helper funkcija za pošiljanje Number vrednosti z avtomatskim nastavitvijo lenth atributa
  // lenth določa število cifer za prikaz (podobno kot vvs0 pri xFloat)
  
  // Cache za Number objekte - preveri ali se je vrednost spremenila
  struct NumberCache {
    char name[32];
    int32_t value;
  };
  static NumberCache cache[5];  // Max 5 različnih Number objektov z lenth
  static uint8_t cacheSize = 0;
  
  // Posebna vrednost za resetiranje cache (klic iz resetXFloatCache)
  if (objName == nullptr) {
    cacheSize = 0;
    return;
  }
  
  // Išči objekt v cache
  int cacheIndex = -1;
  for (uint8_t i = 0; i < cacheSize; i++) {
    if (strcmp(cache[i].name, objName) == 0) {
      cacheIndex = i;
      break;
    }
  }
  
  // Preveri ali se je vrednost spremenila
  if (cacheIndex >= 0) {
    if (value == cache[cacheIndex].value) {
      return;  // Vrednost se ni spremenila - ne pošiljaj
    }
    // Posodobi cache
    cache[cacheIndex].value = value;
  } else {
    // Nov objekt - dodaj v cache
    if (cacheSize < 5) {
      strncpy(cache[cacheSize].name, objName, sizeof(cache[cacheSize].name) - 1);
      cache[cacheSize].name[sizeof(cache[cacheSize].name) - 1] = '\0';
      cache[cacheSize].value = value;
      cacheSize++;
    }
  }
  
  // Določi lenth glede na število cifer
  uint8_t lenth = 1;  // Privzeto 1 cifra (0-9)
  
  if (value >= 10 && value < 100) {
    lenth = 2;  // 2 cifri (10-99)
  } else if (value >= 100 && value < 1000) {
    lenth = 3;  // 3 cifre (100-999)
  } else if (value >= 1000) {
    lenth = 4;  // 4 cifre (1000-9999)
  }
  
  // Pošlji lenth atribut
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "%s.lenth=%d", objName, lenth);
  display.sendRawCommand(cmd);
  
  // Pošlji vrednost
  display.setNumber(objName, value);
  
  // Debug izpis
  Serial.print("[Number] ");
  Serial.print(objName);
  Serial.print(" = ");
  Serial.print(value);
  Serial.print(" (lenth=");
  Serial.print(lenth);
  Serial.println(")");
}

/**
 * @brief Posodobi prikaz kalibracije magneta na strani pgMagCal (page8)
 * 
 * Prebere AGC in Status register ter posodobi vse elemente:
 * - tCalStatus: Glavni status (PREVERJANJE/OPTIMAL/PREMOČEN/PREŠIBEK)
 * - tAGC: AGC vrednost (0-128)
 * - jAGC: Progress bar (0-100, pretvorjeno iz AGC 0-128)
 * - tMagStatus: Status magneta (OPTIMAL/PREMOČEN/PREŠIBEK/NI ZAZNAN)
 */
void updateMagnetCalibrationDisplay(bool forceUpdate) {
  // Cache za prikaze - pošiljaj samo ob spremembi
  static uint8_t lastAGC = 255;  // Nemogoča vrednost za prvo pošiljanje
  static uint8_t lastStatus = 255;
  static bool lastSensorPresent = true;
  
  // Če je forceUpdate true, resetiraj cache da se vse ponovno pošlje
  if (forceUpdate) {
    lastAGC = 255;
    lastStatus = 255;
    lastSensorPresent = true;
  }

  // Preveri če je senzor prisoten
  bool sensorPresent = angleSensor.isSensorPresent();
  if (!sensorPresent) {
    if (lastSensorPresent) {  // Samo ob spremembi
      display.setText("tStatus_pg8", "AS5600 NI ZAZNAN!");
      display.setText("tAGC", "--");
      display.setProgress("jAGC", 0);
      display.setText("tMagStatus", "NI ZAZNAN");
      display.sendRawCommand("tMagStatus.pco=63488");  // Rdeča
      setXFloatValue("xAngle", 0.0);
      lastSensorPresent = false;
    }
    return;
  }
  lastSensorPresent = true;
  
  // Preberi AGC in Status
  uint8_t agc = angleSensor.getAGC();
  uint8_t status = angleSensor.getMagnetStatus();
  
  // Posodobi AGC samo če se je spremenil
  if (agc != lastAGC) {
    char agcStr[8];
    sprintf(agcStr, "%d", agc);
    display.setText("tAGC", agcStr);
    
    // Posodobi progress bar (AGC je 0-128, progress bar je 0-100)
    uint8_t progressValue = (agc * 100) / 128;
    display.setProgress("jAGC", progressValue);
    
    // Posodobi glavni status glede na AGC vrednost
    // Optimalno območje: 48-80 (64 ± 16 pri 3.3V)
    const char* calStatus = "";
    const char* magStatus = "NEZNAN";
    if (agc < 48) {
      calStatus = "Magnet PREMOCEN - oddaljite!";
      magStatus = "PREMOCEN";
      display.setText("tMagStatus", magStatus);
      display.sendRawCommand("tMagStatus.pco=63488");  // Rdeča
    } else if (agc <= 80) {
      calStatus = "Razdalja OPTIMALNA!";
      magStatus = "OPTIMAL";
      display.setText("tMagStatus", magStatus);
      display.sendRawCommand("tMagStatus.pco=31");   // Modra
    } else {
      calStatus = "Magnet PRESIBEK - priblizajte!";
      magStatus = "PRESIBEK";
      display.setText("tMagStatus", magStatus);
      display.sendRawCommand("tMagStatus.pco=63488");  // Rdeča
    }
    display.setText("tStatus_pg8", calStatus);
    
    lastAGC = agc;
  }
  
  // Posodobi xAngle - prikazuje kot z upoštevanjem offseta
  float rawAngle = angleSensor.getAngle();
  float displayAngle = rawAngle - as5600AngleOffset;
  // Normalizacija na 0-360°
  if (displayAngle < 0) displayAngle += 360.0;
  else if (displayAngle >= 360.0) displayAngle -= 360.0;
  setXFloatValue("xAngle", displayAngle);
  
  // Posodobi status magneta samo če se je status spremenil
  // if (status != lastStatus) {
  //   // POMEMBNO: Preveri bite po prioriteti (MH in ML sta prisotni samo ko je MD=1)
  //   const char* magStatus = "NEZNAN";
  //   if (status & 0x20) {
  //     // Bit 5 (MD) - magnet detected
  //     magStatus = "OPTIMAL";
  //     display.setText("tMagStatus", magStatus);
  //     display.sendRawCommand("tMagStatus.pco=31");   // Modra
  //     if (status & 0x08) {
  //       // Bit 3 (MH) - magnet too strong
  //       magStatus = "PREMOCEN";
  //       display.setText("tMagStatus", magStatus);
  //       display.sendRawCommand("tMagStatus.pco=63488");  // Rdeča
  //     } else if (status & 0x10) {
  //       // Bit 4 (ML) - magnet too weak
  //       magStatus = "PRESIBEK";
  //       display.setText("tMagStatus", magStatus);
  //       display.sendRawCommand("tMagStatus.pco=63488");  // Rdeča
  //     }      
  //   } else {
  //     magStatus = "NI ZAZNAN";
  //     display.setText("tMagStatus", magStatus);
  //     display.sendRawCommand("tMagStatus.pco=63488");  // Rdeča
  //   }

    // if (status & 0x08) {
    //   // Bit 3 (MH) - magnet too strong
    //   magStatus = "PREMOCEN";
    //   display.setText("tMagStatus", magStatus);
    //   display.sendRawCommand("tMagStatus.pco=63488");  // Rdeča
    // } else if (status & 0x10) {
    //   // Bit 4 (ML) - magnet too weak
    //   magStatus = "PRESIBEK";
    //   display.setText("tMagStatus", magStatus);
    //   display.sendRawCommand("tMagStatus.pco=63488");  // Rdeča
    // } else if (status & 0x20) {
    //   // Bit 5 (MD) - magnet detected
    //   magStatus = "OPTIMAL";
    //   display.setText("tMagStatus", magStatus);
    //   display.sendRawCommand("tMagStatus.pco=31");   // Modra
    // } else {
    //   magStatus = "NI ZAZNAN";
    //   display.setText("tMagStatus", magStatus);
    //   display.sendRawCommand("tMagStatus.pco=63488");  // Rdeča
    // }
    
  //   lastStatus = status;
  // }
}

void processSavedAngles() {
  // Obdelaj prejete kote iz ID5 in ID6
  float start = receivedID6 / 10.0;  // ID6 = xStartAngle
  float stop = receivedID5 / 10.0;    // ID5 = xStopAngle
  
  // Resetiraj začasne spremenljivke
  receivedID5 = -1;
  receivedID6 = -1;
  
  Serial.print("[ANGLE] Obdelava shranjenih kotov: Start=");
  Serial.print(start, 1);
  Serial.print("°, Stop=");
  Serial.print(stop, 1);
  Serial.println("°");
  
  // Validacija: Začetni kot mora biti večji od končnega
  if (start <= stop) {
    Serial.println("[ANGLE] NAPAKA: Začetni kot mora biti večji od končnega!");
    display.setText("tStatus_pg7", "NAPAKA: Začetni > Končni!");
    return;
  }
  
  // Koti uspešno validirani - shrani v globalne spremenljivke
  savedAngleStart = start;
  savedAngleStop = stop;
  anglesConfigured = true;
  anglesChanged = false;
  
  Serial.println("[ANGLE] Koti shranjeni v globalne spremenljivke");
  
  // Shrani v NVS preferences
  saveAnglesToPreferences();
  Serial.println("[ANGLE] Koti shranjeni v NVS");
  
  // Posodobi globalne spremenljivke na pgModeOFF (format × 10)
  setXFloatValue("pgModeOFF.xStartAngle", savedAngleStart);
  setXFloatValue("pgModeOFF.xStopAngle", savedAngleStop);
  display.setNumber("pgModeOFF.gStartAngle", (int32_t)(savedAngleStart * 10));
  display.setNumber("pgModeOFF.gStopAngle", (int32_t)(savedAngleStop * 10));
  
  // Posodobi prikaz na page7 (če smo še vedno na page7)
  if (currentPage == 7) {
    tempAngleStart = savedAngleStart;
    tempAngleStop = savedAngleStop;
    updatePage7AngleDisplay();
    
    // Deaktiviraj bSave in resetiraj način
    display.setButtonState("bSave", false);
    angleSettingMode = ANGLE_IDLE;
    updateBNastaviKoteText();
    
    // Status
    display.setText("tStatus_pg7", "Koti shranjeni!");
  }
}

void updatePage7AngleDisplay() {
  // Posodobi prikaz kotov na page7 (pgAngle)
  // xStartAngle (id=6), xStopAngle (id=5) - Xfloat format z avtomatskim vvs0
  setXFloatValue("xStartAngle", tempAngleStart);
  setXFloatValue("xStopAngle", tempAngleStop);
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
    float currentAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
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
  
  // Začni premik navzdol s hitrostjo speedSredina
  uint8_t pwmSpeed = speedToPWM(speedSredina);
  outputs.moveSpindleDown(pwmSpeed);
  
  // Posodobi display page5 (pgRef)
  setNumberWithLength("nObrati", 0);
  setNumberWithLength("nCicleTime", 0);
  display.setNumber("xRevPerAngle", 0);
  display.setText("tStatus_pg5", "FAZA 1A: Iskanje najnizje tocke\rVreteno gre navzdol do S43...");
}

void updateReferenceRun() {
  float currentAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
  
  // Posodobi nObrati med izvajanjem
  setNumberWithLength("nObrati", (int32_t)refRevolutions);
  
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
      uint8_t pwmSpeed = speedToPWM(speedSredina);
      outputs.moveSpindleUp(pwmSpeed);
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
      setNumberWithLength("nObrati", (int32_t)refRevolutions);
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
  setNumberWithLength("nCicleTime", (int32_t)elapsedTime);
  
  // xRevPerAngle - Xfloat format z avtomatskim vvs0
  setXFloatValue("xRevPerAngle", revPerAngle);
  
  // xMinAngle in xMaxAngle - Xfloat format
  setXFloatValue("xMinAngle", measuredMinAngle);
  setXFloatValue("xMaxAngle", measuredMaxAngle);
  
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
  
  // Resetiraj xFloat cache ob spremembi strani
  resetXFloatCache();
  
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
      
      // Najprej pošlji trenutni kot xAngle (ostala xFloat polja se naložijo iz globalnih spr.)
      float displayAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
      setXFloatValue("xAngle", displayAngle);
      
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
      
    case 2: { // pgModeMAN
      Serial.println("  -> pgModeMAN (ročni način)");
      
      // Najprej pošlji trenutni kot xAngle (ostala xFloat polja se naložijo iz globalnih spr.)
      float displayAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
      setXFloatValue("xAngle", displayAngle);
      
      // Naloži hitrost iz preferences
      display.setProgress("hRocno", speedRocno);
      
      // Prikaži vrednost sliderja v text polju t1
      char buffer[8];
      sprintf(buffer, "%d", speedRocno);
      display.setText("t1", buffer);
      
      // Preveri pogoje in nastavi statusno sporočilo
      if (!anglesCalibrated) {
        display.setText("tStatus_pg2", "OPOZORILO: Ni referenčnega hoda!");
      } else {
        display.setText("tStatus_pg2", "Ročni način - uporabite tipke ali gumbe");
      }
      break;
    }
      
    case 3: { // pgModeAUTO
      Serial.println("  -> pgModeAUTO (avtomatski način)");
      
      // Najprej pošlji trenutni kot xAngle
      float displayAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
      setXFloatValue("xAngle", displayAngle);
      
      // Naloži hitrost - uporablja se speedSredina
      setNumberWithLength("nSpeed", (int32_t)(speedSredina));  // Number z avtomatskim lenth
      
      // Inicializiraj tCikli - format "izvedeni/nastavljeni"
      S2Cycles cycles = inputs.getS2Cycles();
      char cikliText[20];
      if (cycles == CYCLES_CONTINUOUS) {
        sprintf(cikliText, "%d/∞", autoCycle.getCompletedCycles());
      } else {
        sprintf(cikliText, "%d/%d", autoCycle.getCompletedCycles(), (int)cycles);
      }
      display.setText("tCikli", cikliText);
      
      // Nastavi tekst gumba bStart glede na stanje cikla
      if (autoCycle.isRunning()) {
        display.setText("bStart", "STOP");
      } else {
        display.setText("bStart", "START");
      }
      
      // Preveri pogoje in nastavi statusno sporočilo ter bStart omogočenost
      updateAutoModeReadiness();
      break;
    }
      
    case 4:  // pgSettings
      Serial.println("  -> pgSettings (menu nastavitev)");
      // Ob vrnitvi iz nastavitvenih strani (5-8) ustavi vse
      if (outputs.isSpindleMoving()) {
        Serial.println("[pgSettings] Vrnitev iz nastavitev - ustavljam vreteno");
        outputs.stopSpindle();
      }
      // Če je bil aktiven referenčni hod, ga prekini
      if (refState != REF_IDLE) {
        Serial.println("[pgSettings] Prekinitev referenčnega hoda");
        refState = REF_IDLE;
      }
      break;
      
    case 5: { // pgRef
      Serial.println("  -> pgRef (referenčni hod)");
      
      // Najprej pošlji trenutni kot xAngle
      float displayAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
      setXFloatValue("xAngle", displayAngle);
      
      // Resetiraj stanje referenčnega hoda - nov začetek
      refState = REF_IDLE;
      
      // Naloži globalne spremenljivke iz pgModeOFF (xMinAngle, xMaxAngle)
      // Te se avtomatsko nalagajo iz globalnih spremenljivk v Nextion GUI
      setXFloatValue("xMinAngle", calibratedMinAngle);
      setXFloatValue("xMaxAngle", calibratedMaxAngle);
      
      // Naloži hitrost iz preferences (speedSredina)
      setNumberWithLength("nSpeed", (int32_t)(speedSredina));  // Number z avtomatskim lenth
      
      // Resetiraj prikaze (samo če ref. hod ni aktiven)
      if (refState == REF_IDLE) {
        setNumberWithLength("nObrati", 0);
        setNumberWithLength("nCicleTime", 0);
        setXFloatValue("xRevPerAngle", revPerAngle);  // Prikaži trenutno vrednost
      }
      
      // Preveri ali je kot v mejah (0-30°) za omogočitev bRefStart
      // Ponovno preberi displayAngle (lahko se je spremenil)
      displayAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
      bool angleInRange = (displayAngle >= 0.0 && displayAngle <= 30.0);
      Serial.print("  -> Trenutni kot: ");
      Serial.println(displayAngle, 1);
      Serial.print("  -> Kot v mejah 0-30°: ");
      Serial.println(angleInRange ? "DA" : "NE");
      // Pošljemo tudi stanje ferState na Serial monitor za debug
      Serial.print("  -> Stanje referenčnega hoda: ");
      Serial.println(refState == REF_IDLE ? "IDLE" : "ACTIVE");
      display.setButtonState("bRefStart", angleInRange && refState == REF_IDLE);
      
      // Nastavi tekst gumba bRefStart glede na stanje
      if (refState == REF_IDLE) {
        display.setText("bRefStart", "START");
        // Statusno sporočilo
        if (!angleInRange) {
          display.setText("tStatus_pg5", "NAPAKA: Pozicioniraj vreteno med 0-30°!");
        } else if (!anglesCalibrated) {
          display.setText("tStatus_pg5", "Pritisnite START za referenčni hod");
        } else {
          display.setText("tStatus_pg5", "OPOZORILO: Referenčni hod bo prepisal obstoječe!");
        }
      } else {
        display.setText("bRefStart", "STOP");
      }
      break;
    }
      
    case 6:  // pgSpeed
      Serial.println("  -> pgSpeed (hitrosti)");
      // Posodobi prikaz hitrosti
      display.setProgress("hZacetni", speedZacetni);
      display.setProgress("hSredina", speedSredina);
      display.setProgress("hKoncni", speedKoncni);
      //display.setProgress("hRocno", speedRocno);
      break;
      
    case 7: { // pgAngle
      Serial.println("  -> pgAngle (nastavitev kotov)");
      
      // Najprej pošlji trenutni kot xAngle
      float displayAngle = angleSensor.getCalibratedAngle(as5600AngleOffset);
      setXFloatValue("xAngle", displayAngle);
      
      // xMinAngle, xMaxAngle, xStartAngle, xStopAngle se naložijo avtomatsko iz globalnih spr.
      
      // Inicializiraj začasne kote
      tempAngleStart = savedAngleStart;
      tempAngleStop = savedAngleStop;
      anglesChanged = false;
      angleSettingMode = ANGLE_IDLE;
      updatePage7AngleDisplay();
      display.setButtonState("bSave", false);
      
      // Status
      display.setText("tStatus_pg7", "Uporabite gumb [Nastavi] za nastavitev kotov");
      break;
    }
    
    case 8: { // pgMagCal (kalibracija magneta)
      Serial.println("  -> pgMagCal (kalibracija magneta AS5600)");
      
      // Začetno stanje - preverjanje
      display.setText("tStatus_pg8", "Razdalja magneta: PREVERJANJE...");
      
      // POMEMBNO: Ponovno nastavi DIR pin iz spremenljivke (za primer da se je sistem resetiral)
      digitalWrite(AS5600_DIR, as5600DirCW ? LOW : HIGH);
      delay(20);  // Počakaj da se AS5600 odzove
      
      // Posodobi DIR checkbox stanje
      display.setNumber("cDir", as5600DirCW ? 0 : 1);  // 0=CW, 1=CCW
      
      // Posodobi prikaz (po delay-u ko je DIR že nastavljen)
      updateMagnetCalibrationDisplay(true);
      
      // Izpiši status senzorja (enkrat ob preklopu)
      angleSensor.printStatus();
      
      // Resetiraj časovnik za avtomatsko osvežitev
      lastMagnetCalUpdate = millis();
      
      Serial.println("  -> Stran aktivna: AGC vrednost se osveža vsakih 3s");
      Serial.println("  -> Optimalno območje: AGC 48-80 (64 ± 16)");
      Serial.print("  -> AS5600 DIR: ");
      Serial.println(as5600DirCW ? "CW (clockwise)" : "CCW (counterclockwise)");
      break;
    }
  }
}
