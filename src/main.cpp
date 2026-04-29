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

// Page1 - Nastavitev kotov
uint8_t currentPage = 0;  // Trenutna stran (0=page0, 1=page1, 3=page3, 4=page4)
enum AngleSettingMode {
  ANGLE_IDLE = 0,      // Ni aktivno
  ANGLE_SET_START = 1, // Nastavljamo začetni kot
  ANGLE_SET_STOP = 2   // Nastavljamo končni kot
};
AngleSettingMode angleSettingMode = ANGLE_IDLE;
float tempAngleStart = 0.0;  // Začasni koti pred shranjevanjem
float tempAngleStop = 0.0;
bool anglesChanged = false;  // Ali so se koti spremenili (za aktivacijo bSave)

// Page3 - Referenčni hod
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
void handleNextionEvents();
void handleTouchPress(uint8_t componentId);
void handleTouchRelease(uint8_t componentId);
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
  
  // Nastavi začetno stanje gumbov
  display.setBrusState(false);
  display.setPnevState(false);
  
  // Naloži shranjene kote iz NVS
  loadAnglesFromPreferences();
  
  // Nastavi začetno stran
  display.showPage(0);
  currentPage = 0;
  
  Serial.println("Sistem pripravljen!");
  Serial.println("ROČNI NAČIN: Za nastavitev kotov pritisnite gumb Settings na displayu");
  Serial.println();
}

// ===== OBDELAVA NEXTION TOUCH EVENTS =====
void handleNextionEvents() {
  while (display.available()) {
    // Preberi prvi byte da ugotovimo tip dogodka
    uint8_t eventType = Serial2.peek();
    
    if (eventType == 0x65) {
      // Touch Press Event
      uint8_t pressId = display.readTouchEvent();
      if (pressId > 0) {
        handleTouchPress(pressId);
      }
    } 
    else if (eventType == 0x66) {
      // Touch Release Event
      uint8_t releaseId = display.readTouchReleaseEvent();
      if (releaseId > 0) {
        handleTouchRelease(releaseId);
      }
    }
    else if (eventType == 0x70) {
      // String Return Event - za parsing kotov iz page1
      String data = display.readString();
      if (data.length() > 0) {
        float start, stop;
        if (display.parseAngleSettings(data, start, stop)) {
          // Validacija: Začetni kot mora biti večji od končnega
          if (start <= stop) {
            Serial.println("NAPAKA: Začetni kot mora biti večji od končnega!");
            Serial.print("  Start: ");
            Serial.print(start, 1);
            Serial.print("°, Stop: ");
            Serial.print(stop, 1);
            Serial.println("°");
            display.setText("tStatus_pg1", "NAPAKA: Start > Stop!");
            return;
          }
          
          // Koti uspešno parsirani iz xStartAngle in xEndAngle
          tempAngleStart = start;
          tempAngleStop = stop;
          anglesChanged = true;
          
          Serial.print("Koti nastavljeni ročno: Start=");
          Serial.print(tempAngleStart, 1);
          Serial.print("°, Stop=");
          Serial.print(tempAngleStop, 1);
          Serial.println("°");
          display.setText("tStatus_pg1", "Koti OK - pritisnite bSave!");
          
          // Posodobi prikaz
          updatePage1AngleDisplay();
          
          // Aktiviraj bSave gumb
          display.setButtonState("bSave", true);
        }
      }
    }
    else {
      // Neznan dogodek - preberi in zavrzi
      Serial2.read();
    }
  }
}

void handleTouchPress(uint8_t componentId) {
  Serial.print("Touch Press: ");
  Serial.println(componentId);
  
  S1Mode mode = inputs.getS1Mode();
  
  // ===== PAGE 0 - ROČNO UPRAVLJANJE =====
  if (currentPage == 0) {
    // Akcije samo v MANUAL načinu
    if (mode != MODE_MANUAL) {
      Serial.println("Gumbi delujejo samo v MANUAL načinu!");
      return;
    }
    
    switch(componentId) {
      case 17: { // bGor - začni premik gor
        Serial.println("[bGor] Vreteno GOR - START");
        bGorPressed = true;
        uint8_t pwmSpeed = speedToPWM(speedRocno);
        outputs.moveSpindleUp(pwmSpeed);
        break;
      }
        
      case 18: { // bDol - začni premik dol
        Serial.println("[bDol] Vreteno DOL - START");
        bDolPressed = true;
        uint8_t pwmSpeed = speedToPWM(speedRocno);
        outputs.moveSpindleDown(pwmSpeed);
        break;
      }
        
      case 19:  // bBrus - toggle motor kamna
        brusActive = !brusActive;
        outputs.setGrindingMotor(brusActive);
        display.setBrusState(brusActive);
        Serial.print("[bBrus] Motor kamna: ");
        Serial.println(brusActive ? "ON" : "OFF");
        break;
        
      case 20:  // bPnev - toggle ventil noža
        pnevActive = !pnevActive;
        outputs.setKnifePusher(pnevActive);
        display.setPnevState(pnevActive);
        Serial.print("[bPnev] Ventil noža: ");
        Serial.println(pnevActive ? "ON" : "OFF");
        break;
        
      case 16:  // bSettings
        if (mode == MODE_MANUAL) {
          // V MANUAL načinu: preklop na page1
          Serial.println("[bSettings] Preklop na Page 1 - Nastavitve");
          display.showPage(1);
          currentPage = 1;
          
          // Inicializiraj začasne kote iz shranjenih
          tempAngleStart = savedAngleStart;
          tempAngleStop = savedAngleStop;
          anglesChanged = false;
          angleSettingMode = ANGLE_IDLE;
          
          // Posodobi prikaz na page1
          updatePage1AngleDisplay();
          display.setText("tStatus_pg1", "Vnesi kote ali uporabi Nastavi gumb");
        }
        else if (mode == MODE_AUTO) {
          // V AUTO načinu: START avtomatskega cikla
          // Preveri pogoje
          if (!anglesCalibrated) {
            Serial.println("[bSettings/AUTO] NAPAKA: Ni kalibracije!");
            display.setStatus("NAPAKA: Ni kalibracije!");
            return;
          }
          if (!anglesConfigured) {
            Serial.println("[bSettings/AUTO] NAPAKA: Ni nastavljenih kotov!");
            display.setStatus("NAPAKA: Ni kotov!");
            return;
          }
          
          S2Cycles cycles = inputs.getS2Cycles();
          if (cycles == CYCLES_NONE) {
            Serial.println("[bSettings/AUTO] NAPAKA: S2 ni nastavljen!");
            display.setStatus("NAPAKA: Nastavi S2!");
            return;
          }
          
          // Vse OK - začni avtomatski cikel
          Serial.println("========================================");
          Serial.println("[bSettings/AUTO] START AVTOMATSKEGA CIKLA");
          Serial.println("========================================");
          
          autoCycle.start(cycles, savedAngleStart, savedAngleStop,
                         calibratedMinAngle, calibratedMaxAngle, anglesCalibrated,
                         speedZacetni, speedSredina, speedKoncni, revPerAngle);
          autoModeActive = true;
          
          display.setStatus("AUTO cikel zagnan!");
        }
        break;
        
      default:
        Serial.print("Neznan gumb ID: ");
        Serial.println(componentId);
        break;
    }
  }
  // ===== PAGE 1 - NASTAVITEV KOTOV =====
  else if (currentPage == 1) {
    switch(componentId) {
      case 19:  // bNastaviKote - začni/nadaljuj nastavitev
        if (angleSettingMode == ANGLE_IDLE) {
          // Začni nastavitev začetnega kota
          angleSettingMode = ANGLE_SET_START;
          tempAngleStart = angleSensor.getCalibratedAngle();  // Zacetna vrednost
          Serial.println("[bNastaviKote] Nastavljanje ZACETNEGA kota - uporabite tipke Gor/Dol");
          display.setText("bNastaviKote", "Zacetni");
          display.setText("tStatus_pg1", "Nastavi START kot (Gor/Dol tipke)");
          updatePage1AngleDisplay();  // Prikazi zacetno vrednost
        }
        else if (angleSettingMode == ANGLE_SET_START) {
          // Shrani začetni kot in začni nastavitev končnega
          tempAngleStart = angleSensor.getCalibratedAngle();
          angleSettingMode = ANGLE_SET_STOP;
          anglesChanged = true;
          tempAngleStop = angleSensor.getCalibratedAngle();  // Zacetna vrednost za koncni kot
          Serial.print("[bNastaviKote] Zacetni kot nastavljen: ");
          Serial.print(tempAngleStart, 1);
          Serial.println("°");
          Serial.println("[bNastaviKote] Nastavljanje KONCNEGA kota - uporabite tipke Gor/Dol");
          display.setText("bNastaviKote", "Koncni");
          display.setText("tStatus_pg1", "Nastavi STOP kot (Gor/Dol tipke)");
          
          // Posodobi prikaz
          updatePage1AngleDisplay();
          
          // Aktiviraj bSave gumb
          display.setButtonState("bSave", true);
        }
        else if (angleSettingMode == ANGLE_SET_STOP) {
          // Shrani končni kot in resetiraj
          tempAngleStop = angleSensor.getCalibratedAngle();
          
          // Validacija: Zacetni kot mora biti vecji od koncnega
          if (tempAngleStart <= tempAngleStop) {
            Serial.println("[bNastaviKote] NAPAKA: Zacetni kot mora biti vecji od koncnega!");
            Serial.print("  Start: ");
            Serial.print(tempAngleStart, 1);
            Serial.print("°, Stop: ");
            Serial.print(tempAngleStop, 1);
            Serial.println("°");
            
            // Reset nastavitve
            angleSettingMode = ANGLE_IDLE;
            display.setText("bNastaviKote", "Nastavi");
            display.setText("tStatus_pg1", "NAPAKA: Start > Stop!");
            return;
          }
          
          // Preveri ali so koti znotraj kalibriranih limitov
          if (anglesCalibrated) {
            if (tempAngleStart > calibratedMaxAngle) {
              Serial.print("[bNastaviKote] NAPAKA: Start ");
              Serial.print(tempAngleStart, 1);
              Serial.print("° presega max limit ");
              Serial.print(calibratedMaxAngle, 1);
              Serial.println("°");
              
              angleSettingMode = ANGLE_IDLE;
              display.setText("bNastaviKote", "Nastavi");
              char msg[50];
              sprintf(msg, "NAPAKA: Start>%.1f!", calibratedMaxAngle);
              display.setText("tStatus_pg1", msg);
              return;
            }
            if (tempAngleStop < calibratedMinAngle) {
              Serial.print("[bNastaviKote] NAPAKA: Stop ");
              Serial.print(tempAngleStop, 1);
              Serial.print("° presega min limit ");
              Serial.print(calibratedMinAngle, 1);
              Serial.println("°");
              
              angleSettingMode = ANGLE_IDLE;
              display.setText("bNastaviKote", "Nastavi");
              char msg[50];
              sprintf(msg, "NAPAKA: Stop<%.1f!", calibratedMinAngle);
              display.setText("tStatus_pg1", msg);
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
          display.setText("tStatus_pg1", "Pritisnite bSave za shranitev!");
          
          // Posodobi prikaz
          updatePage1AngleDisplay();
        }
        break;
        
      case 15:  // bRef - preklopi na page3
        Serial.println("[bRef] Preklop na Page 3 - Referenčni hod");
        display.showPage(3);
        currentPage = 3;
        
        // Resetiraj stanje
        refState = REF_IDLE;
        
        // Posodobi začetni prikaz
        display.setText("tActualAngle", "0.0");
        display.setText("tMinAngle", "---");
        display.setText("tMaxAngle", "---");
        display.setText("tMRev", "0");
        display.setText("tRevPerAngle", "---");
        
      case 14:  // bSpeed - preklopi na page4
        Serial.println("[bSpeed] Preklop na Page 4 - Nastavitev hitrosti");
        display.showPage(4);
        currentPage = 4;
        
        // Posodobi prikaz hitrosti na page4
        display.setProgress("hZacetni", speedZacetni);
        display.setProgress("hSredina", speedSredina);
        display.setProgress("hKoncni", speedKoncni);
        display.setProgress("hRocno", speedRocno);
        
        Serial.print("Hitrosti: Zač=");
        Serial.print(speedZacetni);
        Serial.print("%, Sre=");
        Serial.print(speedSredina);
        Serial.print("%, Kon=");
        Serial.print(speedKoncni);
        Serial.print("%, Roč=");
        Serial.print(speedRocno);
        Serial.println("%");
        break;
        display.setText("tTime", "---");
        break;
        
      case 7:   // bSave - shrani kote
        if (anglesChanged) {
          // Validacija: Začetni kot mora biti večji od končnega
          if (tempAngleStart <= tempAngleStop) {
            Serial.println("[bSave] NAPAKA: Začetni kot mora biti večji od končnega!");
            Serial.print("  Start: ");
            Serial.print(tempAngleStart, 1);
            Serial.print("°, Stop: ");
            Serial.print(tempAngleStop, 1);
            Serial.println("°");
            display.setText("tStatus_pg1", "NAPAKA: Start > Stop!");
            return;
          }
          
          // Preveri ali so koti znotraj kalibriranih limitov
          if (anglesCalibrated) {
            if (tempAngleStart > calibratedMaxAngle) {
              Serial.print("[bSave] NAPAKA: Start ");
              Serial.print(tempAngleStart, 1);
              Serial.print("° presega max limit ");
              Serial.print(calibratedMaxAngle, 1);
              Serial.println("°");
              char msg[50];
              sprintf(msg, "NAPAKA: Start>%.1f!", calibratedMaxAngle);
              display.setText("tStatus_pg1", msg);
              return;
            }
            if (tempAngleStop < calibratedMinAngle) {
              Serial.print("[bSave] NAPAKA: Stop ");
              Serial.print(tempAngleStop, 1);
              Serial.print("° presega min limit ");
              Serial.print(calibratedMinAngle, 1);
              Serial.println("°");
              char msg[50];
              sprintf(msg, "NAPAKA: Stop<%.1f!", calibratedMinAngle);
              display.setText("tStatus_pg1", msg);
              return;
            }
            Serial.println("[bSave] Koti preverjeni - znotraj limitov");
          } else {
            Serial.println("[bSave] OPOZORILO: Koti niso preverjeni - kalibracija ni opravljena!");
            display.setText("tStatus_pg1", "Opozorilo: Ni kalibracije!");
          }
          
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
          
          display.setText("tStatus_pg1", "Koti shranjeni!");
          
          // Deaktiviraj bSave gumb
          display.setButtonState("bSave", false);
        }
        break;
        
      default:
        Serial.print("Neznan gumb ID na page1: ");
        Serial.println(componentId);
        break;
    }
  }
  // ===== PAGE 3 - REFERENČNI HOD =====
  else if (currentPage == 3) {
    switch(componentId) {
      case 3:  // bRefStart - začni referenčni hod
        if (refState == REF_IDLE) {
          Serial.println("[bRefStart] Začetek referenčnega hoda");
          startReferenceRun();
        }
        break;
        
      default:
        Serial.print("Neznan gumb ID na page3: ");
        Serial.println(componentId);
        break;
    }
  }
  // ===== PAGE 4 - NASTAVITEV HITROSTI =====
  else if (currentPage == 4) {
    switch(componentId) {
      case 9:  // bPage4_Back - nazaj na page1
        Serial.println("[bPage4_Back] Nazaj na Page 1");
        display.showPage(1);
        currentPage = 1;
        
        // Posodobi prikaz kotov
        updatePage1AngleDisplay();
        display.setText("tStatus_pg1", "Hitrosti shranjene");
        break;
        
      default:
        Serial.print("Neznan gumb ID na page4: ");
        Serial.println(componentId);
        break;
    }
  }
}

void handleTouchRelease(uint8_t componentId) {
  Serial.print("Touch Release: ");
  Serial.println(componentId);
  
  S1Mode mode = inputs.getS1Mode();
  
  // ===== PAGE 0 - MANUAL BUTTONS =====
  if (currentPage == 0 && mode == MODE_MANUAL) {
    switch(componentId) {
      case 17:  // bGor - ustavi premik
        Serial.println("[bGor] Vreteno GOR - STOP");
        bGorPressed = false;
        outputs.stopSpindle();
        break;
        
      case 18:  // bDol - ustavi premik
        Serial.println("[bDol] Vreteno DOL - STOP");
        bDolPressed = false;
        outputs.stopSpindle();
        break;
        
      // bBrus (19) in bPnev (20) sta toggle gumba - brez Release akcije
      
      default:
        break;
    }
  }
  // ===== PAGE 4 - SPEED SETTINGS (Progress Bars) =====
  else if (currentPage == 4) {
    // Progress Bar pošlje Touch Release Event + 1 byte vrednosti
    // Format: 0x66 PageID ComponentID 0x00 0xFF 0xFF 0xFF [VALUE]
    // Preberemo vrednost (% 50-100)
    delay(10);  // Kratka zakasnitev da pride vrednost
    if (Serial2.available() > 0) {
      uint8_t value = Serial2.read();
      
      // Omejitev na 50-100%
      if (value < 50) value = 50;
      if (value > 100) value = 100;
      
      switch(componentId) {
        case 2:  // hZacetni
          speedZacetni = value;
          Serial.print("[hZacetni] Hitrost začetni: ");
          Serial.print(speedZacetni);
          Serial.println("%");
          saveSpeedsToPreferences();
          break;
          
        case 6:  // hSredina
          speedSredina = value;
          Serial.print("[hSredina] Hitrost sredina: ");
          Serial.print(speedSredina);
          Serial.println("%");
          saveSpeedsToPreferences();
          break;
          
        case 8:  // hKoncni
          speedKoncni = value;
          Serial.print("[hKoncni] Hitrost končni: ");
          Serial.print(speedKoncni);
          Serial.println("%");
          saveSpeedsToPreferences();
          break;
          
        case 18:  // hRocno
          speedRocno = value;
          Serial.print("[hRocno] Hitrost ročno: ");
          Serial.print(speedRocno);
          Serial.println("%");
          saveSpeedsToPreferences();
          break;
          
        default:
          Serial.print("Neznan Progress Bar ID: ");
          Serial.println(componentId);
          break;
      }
    }
  }
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Posodobi vhode
  inputs.update();
  
  // Posodobi AS5600 senzor
  angleSensor.update();
  
  // Posodobi prikaz kota na Nextion zaslonu
  display.setAngle(angleSensor.getCalibratedAngle());
  
  // Posodobi podatke na page0 vsake 0.5s
  static unsigned long lastPage0Update = 0;
  if (currentPage == 0 && (currentMillis - lastPage0Update >= 500)) {
    lastPage0Update = currentMillis;
    
    // Posodobi status vretena
    display.setSpindleStatus(
      outputs.isSpindleMoving(),
      outputs.getSpindleDirection() == SPINDLE_UP,
      outputs.getSpindleSpeed()
    );
    
    // Posodobi število ciklov
    S1Mode mode = inputs.getS1Mode();
    if (mode == MODE_AUTO && autoCycle.isRunning()) {
      display.setCycles(autoCycle.getCompletedCycles(), autoCycle.getTargetCycles());
    } else {
      S2Cycles cycles = inputs.getS2Cycles();
      if (cycles == CYCLES_CONTINUOUS) {
        display.setCycles(0, 99);  // 99 = continuous
      } else {
        display.setCycles(0, cycles);
      }
    }
  }
  
  // Če smo na page1 in nastavljamo kote, posodabljaj prikaz v realnem času
  if (currentPage == 1 && angleSettingMode != ANGLE_IDLE) {
    if (angleSettingMode == ANGLE_SET_START) {
      tempAngleStart = angleSensor.getCalibratedAngle();
    } else if (angleSettingMode == ANGLE_SET_STOP) {
      tempAngleStop = angleSensor.getCalibratedAngle();
    }
    updatePage1AngleDisplay();
  }
  
  // Če smo na page3 in izvajamo referenčni hod, posodabljaj
  if (currentPage == 3 && (refState == REF_FIND_MIN_DOWN || refState == REF_FIND_MIN_UP || refState == REF_FIND_MAX)) {
    updateReferenceRun();
  }
  
  // Posodobi Nextion display
  display.update(currentMillis);
  
  // Obdelaj Touch Events iz Nextiona
  handleNextionEvents();
  
  // Trenutni način delovanja
  S1Mode mode = inputs.getS1Mode();
  
  // Detekcija spremembe načina
  if (mode != lastMode) {
    Serial.print("Sprememba načina: ");
    if (mode == MODE_OFF) Serial.println("OFF");
    else if (mode == MODE_MANUAL) Serial.println("MANUAL");
    else if (mode == MODE_AUTO) Serial.println("AUTO");
    
    // Posodobi stanje gumbov glede na način (samo na page0)
    if (currentPage == 0) {
      bool enableButtons = (mode == MODE_MANUAL);
      display.setButtonState("bGor", enableButtons);
      display.setButtonState("bDol", enableButtons);
      display.setButtonState("bBrus", enableButtons);
      display.setButtonState("bPnev", enableButtons);
      display.setButtonState("bSettings", enableButtons);
    }
    
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
    // Kontrola vretena - fizične tipke (S41/S42) delujejo na page0 in page1
    // Na page0: fizične tipke ALI Nextion gumbi (bGor/bDol)
    // Na page1: samo fizične tipke (za nastavitev kotov)
    
    bool wantMoveDown = false;
    bool wantMoveUp = false;
    
    if (currentPage == 0) {
      // Page0 - ročno upravljanje - delujejo fizične tipke IN Nextion gumbi
      wantMoveDown = inputs.isS41DownPressed() || bDolPressed;
      wantMoveUp = inputs.isS42UpPressed() || bGorPressed;
    } 
    else if (currentPage == 1 && (angleSettingMode == ANGLE_SET_START || angleSettingMode == ANGLE_SET_STOP)) {
      // Page1 - med nastavljanjem kotov - samo fizične tipke
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
        if (currentPage == 0) {
          display.setStatus("ALARM: Max limit!");
        } else if (currentPage == 1) {
          display.setText("tStatus_pg1", "ALARM: Max limit!");
        }
        Serial.println("ALARM: Dosežen maksimalni limit!");
        wantMoveUp = false;
      }
      if (wantMoveDown && currentAngle <= calibratedMinAngle) {
        outputs.stopSpindle();
        if (currentPage == 0) {
          display.setStatus("ALARM: Min limit!");
        } else if (currentPage == 1) {
          display.setText("tStatus_pg1", "ALARM: Min limit!");
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
    // Periodično posodobi pripravljenost gumba bSettings na page0
    static unsigned long lastAutoCheck = 0;
    if (currentPage == 0 && (currentMillis - lastAutoCheck > 1000)) {
      updateAutoModeReadiness();
      lastAutoCheck = currentMillis;
    }
    
    // Posodobi avtomatski cikel
    autoCycle.update();
    
    // Če še ni aktiven, začni cikel glede na S2 nastavitev
    if (!autoModeActive && !autoCycle.isRunning()) {
      // Preveri če so koti nastavljeni
      if (!anglesConfigured) {
        Serial.println("NAPAKA: Koti niso nastavljeni - uporabite page1!");
        display.setStatus("NAPAKA: Nastavi kote!");
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
      
      display.setStatus("Koti izbrisani");
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
  // Posodobi prikaz kotov na page1
  display.setAngleRange(tempAngleStart, tempAngleStop);
  
  // Nastavi globalne spremenljivke
  display.setGlobalVariable("vaAngleStart", (int32_t)(tempAngleStart * 10));
  display.setGlobalVariable("vaAngleStop", (int32_t)(tempAngleStop * 10));
  
  // Posodobi tudi Xfloat objekte (id=5 in id=6) - format angle*10 za decimalna mesta
  display.setNumber("xStartAngle", (int32_t)(tempAngleStart * 10));
  display.setNumber("xEndAngle", (int32_t)(tempAngleStop * 10));
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
  // in posodobi gumb bSettings na page0
  
  bool ready = true;
  String status = "";
  
  // Preveri kalibracijo (referenčni hod)
  if (!anglesCalibrated) {
    ready = false;
    status = "Ni kalibracije!";
  }
  // Preveri nastavljene kote
  else if (!anglesConfigured) {
    ready = false;
    status = "Ni kotov!";
  }
  // Preveri število ciklov
  else {
    S2Cycles cycles = inputs.getS2Cycles();
    if (cycles == CYCLES_NONE) {
      ready = false;
      status = "Nastavi S2!";
    }
  }
  
  // Posodobi gumb bSettings (id=16)
  if (ready) {
    // Vse OK - omogoči START
    display.setText("bSettings", "START AUTO");
    // Nastavi barve gumba z uporabo serial objekta
    Serial2.print("bSettings.bco=2016");    // Zelena
    Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
    Serial2.print("bSettings.pco=0");       // Črna
    Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
    display.setButtonState("bSettings", true);    // Omogoči
    display.setStatus("Pripravljen za AUTO");
  } else {
    // Pogoji niso izpolnjeni
    display.setText("bSettings", "Nastavitve");
    Serial2.print("bSettings.bco=50712");   // Siva
    Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
    Serial2.print("bSettings.pco=65535");   // Bela
    Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
    display.setButtonState("bSettings", true);    // Vedno omogočen (za dostop do nastavitev)
    display.setStatus(status.c_str());
  }
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
  
  // Posodobi display
  display.setText("tMinAngle", "---");
  display.setText("tMaxAngle", "---");
  display.setText("tMRev", "0");
  display.setText("tRevPerAngle", "---");
  display.setText("tTime", "---");
  display.setText("tStatus_pg3", "FAZA 1A: Iskanje najnizje tocke\rVreteno gre navzdol do S43...");
}

void updateReferenceRun() {
  float currentAngle = angleSensor.getCalibratedAngle();
  char angleStr[10];
  
  // Posodobi prikaz trenutnega kota (stalno!)
  sprintf(angleStr, "%.1f", currentAngle);
  display.setText("tActualAngle", angleStr);
  
  // === FAZA 1A: ISKANJE NAJNIŽJE TOČKE (DOL DO S43) ===
  if (refState == REF_FIND_MIN_DOWN) {
    // Preveri ali je S43 aktiven (najnižja točka)
    if (inputs.isSpindleAtBottom()) {
      // S43 je aktiven - dosežena najnižja točka!
      outputs.stopSpindle();
      delay(200);  // Kratka zakasnitev pred obratom smeri
      
      Serial.println("S43 aktiven - najnižja točka dosežena, obračam smer...");
      display.setText("tStatus_pg3", "S43 AKTIVEN - najnizja tocka!\rObracam smer motorja...\rFAZA 1B: Vreteno gre gor");
      
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
      display.setText("tStatus_pg3", statusMsg);
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
        
        // Posodobi display
        sprintf(angleStr, "%.1f", measuredMinAngle);
        display.setText("tMinAngle", angleStr);
        
        char statusMsg[200];
        sprintf(statusMsg, "MIN KOT DOLOCEN: %.1f deg\r(0.5 deg nad S43 preklopom)\rFAZA 2: Iskanje MAX kota...\rVreteno gre navzgor do S46", measuredMinAngle);
        display.setText("tStatus_pg3", statusMsg);
        
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
      
      // Posodobi display števila obratov
      char revStr[10];
      sprintf(revStr, "%lu", refRevolutions);
      display.setText("tMRev", revStr);
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
      
      // Posodobi display
      sprintf(angleStr, "%.1f", measuredMaxAngle);
      display.setText("tMaxAngle", angleStr);
      
      char statusMsg[200];
      sprintf(statusMsg, "S46 AKTIVEN!\rMAX KOT DOLOCEN: %.1f deg\r(-0.5 deg toleranca)\rIzracunavam statistiko...", measuredMaxAngle);
      display.setText("tStatus_pg3", statusMsg);
      
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
  
  // Posodobi display
  char buffer[20];
  sprintf(buffer, "%.1f", revPerAngle);
  display.setText("tRevPerAngle", buffer);
  
  sprintf(buffer, "%.1f", elapsedTime);
  display.setText("tTime", buffer);
  
  // Shrani v globalne spremenljivke (format × 10)
  display.setGlobalVariable("vaMinAngle", (int32_t)(measuredMinAngle * 10));
  display.setGlobalVariable("vaMaxAngle", (int32_t)(measuredMaxAngle * 10));
  
  // Shrani v Preferences za trajno shranjevanje
  preferences.begin("brus", false);
  preferences.putFloat("minAngle", measuredMinAngle);
  preferences.putFloat("maxAngle", measuredMaxAngle);
  preferences.putUInt("revPerAngle", (uint32_t)(revPerAngle * 10));  // × 10 za decimalno mesto
  preferences.putUInt("cycleTime", (uint32_t)(elapsedTime * 10));  // × 10 za decimalno mesto
  preferences.end();
  
  Serial.println("Podatki shranjeni v NVS");
  
  // Končni status na displayu
  char statusMsg[200];
  sprintf(statusMsg, "REFERENCNI HOD ZAKLJUCEN!\rRazpon: %.1f deg | Obrati: %lu\rOb/deg: %.1f | Cas: %.1f s\rPodatki shranjeni v NVS", 
          angleRange, refRevolutions, revPerAngle, elapsedTime);
  display.setText("tStatus_pg3", statusMsg);
}
