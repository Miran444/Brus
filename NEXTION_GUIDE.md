# NEXTION HMI - Navodila za BRUS projekt (Posodobljena verzija)

## 1. OSNOVNA NASTAVITEV

### Zagon Nextion Editorja:
1. Odprite **Nextion Editor**
2. Kliknite **File → New → Nextion Project**
3. Izberite model: **NX4832F035** (4.3" Enhanced)
4. Orientation: **Landscape** (ležeče)
5. Ime: **Brus_HMI**
6. Potrdite

---

## 2. PAGE 0 - GLAVNI ZASLON

### POMEMBNO: Nove komponente v HMI
- **Odstranjen**: gauge "nAngle" (ni več potreben)
- **Premaknjen**: number "nRev" bo na page1 (Nastavitve)
- **Dodano**: Status prikaz, buttoni za ročno upravljanje, prikaz kotov

---

### Korak 1: Nastavitev ozadja
1. Desni klik na **page0** → **Attribute**
2. **bco** (background color): 16448 (temno siva)
3. OK

### Korak 2: Dodajanje MODE prikaza (zgoraj center)
1. Toolbox → **Text** → povlecite na zaslon
2. Position: **X=50, Y=10, W=380, H=60**
3. Properties:
   - **objname**: `tMode`
   - **txt**: `OFF`
   - **font**: izberite velik font (npr. size 5)
   - **pco** (text color): 63488 (rdeča)
   - **bco** (background): 16448
   - **xcen**: 1 (center horizontalno)
   - **ycen**: 1 (center vertikalno)

### Korak 3: Prikaz STATUSA stroja (zgoraj levo)
1. Toolbox → **Text** → povlecite na zaslon
2. Position: **X=10, Y=80, W=150, H=50**
3. Properties:
   - **objname**: `tStatus`
   - **txt**: `Stop`
   - **font**: size 3
   - **pco** (text color): 65535 (bela)
   - **bco** (background): 63488 (rdeča)
   - **xcen**: 1 (center horizontalno)
   - **ycen**: 1 (center vertikalno)

**OPOMBA:** tStatus prikazuje trenutno stanje stroja:
- "Stop" = stroj miruje (rdeča)
- "Pripravljen" = pripravljen za zagon (modra)
- "Run" = stroj deluje (zelena)
- Alarm sporočila = prikaže custom tekst (npr. "NAPAKA: Senzor") z rdečo barvo

### Korak 4: Prikaz kota - velika številka (desno zgoraj)
1. Toolbox → **Text** → povlecite na zaslon
2. Position: **X=250, Y=80, W=230, H=120**
3. Properties:
   - **objname**: `tAngle`
   - **txt**: `0.0°`
   - **font**: zelo velik font (size 6-7)
   - **pco**: 65535 (bela)
   - **bco**: 16448
   - **xcen**: 1
   - **ycen**: 1

### Korak 5: Prikaz nastavljenih kotov (levo spodaj)

**Xfloat za začetni kot:**
1. Toolbox → **Xfloat**
2. Position: **X=10, Y=140, W=100, H=35**
3. Properties:
   - **objname**: `xAngleStart`
   - **vvs1**: 1 (decimalno mesto)
   - **font**: size 2
   - **pco**: 2016 (zelena)
   - **bco**: 16448

**Label "Start:"**
1. Toolbox → **Text**
2. Position: **X=10, Y=120, W=100, H=20**
3. Properties:
   - **txt**: `Start:`
   - **font**: size 1
   - **pco**: 50712 (svetlo siva)
   - **bco**: 16448

**Xfloat za končni kot:**
1. Toolbox → **Xfloat**
2. Position: **X=120, Y=140, W=100, H=35**
3. Properties:
   - **objname**: `xAngleStop`
   - **vvs1**: 1 (decimalno mesto)
   - **font**: size 2
   - **pco**: 1024 (modra)
   - **bco**: 16448

**Label "Stop:"**
1. Toolbox → **Text**
2. Position: **X=120, Y=120, W=100, H=20**
3. Properties:
   - **txt**: `Stop:`
   - **font**: size 1
   - **pco**: 50712
   - **bco**: 16448

### Korak 6: Prikaz ciklov (levo sredina)
1. Toolbox → **Text**
2. Position: **X=10, Y=200, W=150, H=40**
3. Properties:
   - **objname**: `tCycles`
   - **txt**: `0/0`
   - **font**: size 3
   - **pco**: 65535
   - **bco**: 16448

### Korak 7: Label "Cikli:" (nad tCycles)
1. Toolbox → **Text**
2. Position: **X=10, Y=180, W=150, H=20**
3. Properties:
   - **objname**: `tLabel1`
   - **txt**: `Cikli:`
   - **font**: size 1
   - **pco**: 50712 (svetlo siva)
   - **bco**: 16448

### Korak 8: Buttoni za ročno upravljanje (center spodaj)

**Button BRUS (motor kamna):**
1. Toolbox → **Button** → povlecite na zaslon
2. Position: **X=170, Y=200, W=80, H=60**
3. Properties:
   - **objname**: `bBrus`
   - **txt**: `BRUS`
   - **font**: size 2
   - **pco**: 65535
   - **bco**: 50712 (siva - neaktiven)

**Button PNEV (nož):**
1. Toolbox → **Button**
2. Position: **X=260, Y=200, W=80, H=60**
3. Properties:
   - **objname**: `bPnev`
   - **txt**: `NOŽ`
   - **font**: size 2
   - **pco**: 65535
   - **bco**: 50712

**Button GOR (vreteno gor):**
1. Toolbox → **Button**
2. Position: **X=350, Y=200, W=60, H=30**
3. Properties:
   - **objname**: `bGor`
   - **txt**: `▲ GOR`
   - **font**: size 1
   - **pco**: 65535
   - **bco**: 50712

**Button DOL (vreteno dol):**
1. Toolbox → **Button**
2. Position: **X=350, Y=235, W=60, H=30**
3. Properties:
   - **objname**: `bDol`
   - **txt**: `▼ DOL`
   - **font**: size 1
   - **pco**: 65535
   - **bco**: 50712

**OPOMBA:** Vsi buttoni (bBrus, bPnev, bGor, bDol) so aktivni samo v ročnem načinu!  
ESP32 bo preko kode omogočil/onemogočil buttone z ukazom `tsw bBrus,1` (enable) ali `tsw bBrus,0` (disable).

### Korak 9: Button Settings (zgoraj desno)
1. Toolbox → **Button**
2. Position: **X=420, Y=10, W=60, H=40**
3. Properties:
   - **objname**: `bSettings`
   - **txt**: `⚙`  (ali "SET")
   - **font**: size 3
   - **pco**: 65535
   - **bco**: 1024 (modra)

**Touch Event za bSettings:**
1. Desni klik na **bSettings** → **Event**
2. **Touch Release Event** → vnesite:
   ```
   page 1
   ```

### Korak 10: Status vretena (desno sredina)
1. Toolbox → **Text**
2. Position: **X=250, Y=270, W=230, H=40**
3. Properties:
   - **objname**: `tSpindle`
   - **txt**: `STOP`
   - **font**: size 3
   - **pco**: 63488 (rdeča)
   - **bco**: 16448
   - **xcen**: 1
   - **ycen**: 1

### Korak 11: Progress bar - hitrost vretena
1. Toolbox → **Progress Bar**
2. Position: **X=250, Y=250, W=230, H=15**
3. Properties:
   - **objname**: `jSpeed`
   - **val**: 0
   - **maxval**: 100
   - **pco**: 1024 (modra)
   - **bco**: 0

---

## 3. PAGE 1 - NASTAVITVE (opcijsko)

Ko boste ustvarili Page 1, jo lahko uporabite za:
- Nastavitve števila ciklov
- Nastavitve kotov (Start/Stop)
- Prikaz števila obratov (nRev)
- Gumb za vrnitev na Page 0

**Osnovna struktura Page 1:**
1. Kliknite na **Page 1** (ali ustvarite novo)
2. Dodajte button za vrnitev:
   - objname: `bBack`
   - txt: `◀ NAZAJ`
   - Touch Event: `page 0`

---

## 4. POMEMBNE OPOMBE

### Buttoni aktivni samo v ročnem načinu:
ESP32 bo z kodo omogočil/onemogočil buttone:
```cpp
display.setButtonState("bBrus", true);   // Omogoči
display.setButtonState("bGor", false);   // Onemogoči
```

### Črpalka vode:
Črpalka je vezana direktno na motor kamna (hardversko). Ko motor kamna teče, avtomatsko teče tudi črpalka. V kodi ni potrebno posebej upravljati črpalke.

### Touch eventi:
Ko uporabnik pritisne button v HMI, Nextion pošlje Touch Event na ESP32.  
ESP32 ga prebere z `readTouchEvent()` funkcijo in reagira.

---

## 5. TESTIRANJE

### Preizkus pošiljanja iz ESP32:
V Serial Monitoru bi morali videti podatke, Nextion pa jih bo prikazal.

### Debug v Nextion Editorju:
1. Kliknite **Debug** (zgoraj desno)
2. V Command line vnesite testne komande:
   ```
   tMode.txt="AUTO"
   tStatus.txt="Run"
   tStatus.bco=2016
   tAngle.txt="23.5°"
   xAngleStart.txt="5.0"
   xAngleStop.txt="28.0"
   tCycles.txt="3/7"
   ```

---

## 6. COMPILE IN EXPORT

1. **Debug** → Preverite če vse deluje
2. **File → Compile** → Počakajte
3. **File → TFT File Output** → Shranite .tft datoteko
4. Kopirajte .tft na microSD kartico
5. Vstavite v Nextion display
6. Vklopite - display bo avtomatsko naložil .tft

---

## 7. POVEZAVA S ESP32

**Hardware povezave:**
- Nextion TX (modri pin) → ESP32 GPIO18 (RX)
- Nextion RX (rumeni pin) → ESP32 GPIO17 (TX)
- Nextion 5V → 5V napajanje
- Nextion GND → GND

**Nastavitve Nextion:**
1. Page → Events → **Preinit**:
   ```
   baud=115200
   ```

---

## 8. SEZNAM VSEH HMI OBJEKTOV

### Page 0 (Glavni zaslon):
- **tMode** - Text - Način delovanja (OFF/MANUAL/AUTO)
- **tStatus** - Text - Status stroja (Stop/Pripravljen/Run ali alarm sporočilo)
- **tAngle** - Text - Trenutni kot (velika številka)
- **xAngleStart** - Xfloat - Začetni kot za avto način
- **xAngleStop** - Xfloat - Končni kot za avto način
- **tCycles** - Text - Prikaz ciklov (npr. "3/7")
- **tSpindle** - Text - Status vretena (GOR/DOL/STOP)
- **jSpeed** - Progress Bar - Hitrost vretena (0-100%)
- **bBrus** - Button - Vklop motorja kamna (ročni način)
- **bPnev** - Button - Vklop noža (ročni način)
- **bGor** - Button - Pomik vretena gor (ročni način)
- **bDol** - Button - Pomik vretena dol (ročni način)
- **bSettings** - Button - Odpri Page 1 (Nastavitve)

### Page 1 (Nastavitve) - opcijsko:
- **nRev** - Number - Prikaz števila obratov
- **bBack** - Button - Vrnitev na Page 0
- (dodajte po želji)

---

## 9. SEZNAM FUNKCIJ V ESP32 KODI

```cpp
// Inicializacija
display.begin();

// Nastavitve prikaza
display.setMode("AUTO");                    // OFF, MANUAL, AUTO
display.setStatus("Run");                   // Stop, Pripravljen, Run
display.setStatus("NAPAKA: Senzor!");       // Za prikaz alarmov (rdeča barva)
display.setCycles(3, 7);                    // trenutni, ciljni (99 = neskončno)
display.setAngle(23.5);                     // Trenutni kot
display.setAngleRange(5.0, 28.0);          // Start in Stop kot
display.setSpindleStatus(true, true, 128);  // premika, smer_gor, hitrost

// Buttoni (omogoči/onemogoči)
display.setButtonState("bBrus", true);      // Omogoči button
display.setButtonState("bPnev", false);     // Onemogoči button
display.setButtonState("bGor", true);
display.setButtonState("bDol", true);

// Stanje buttonov (vizualni feedback)
display.setBrusState(true);                 // Zelena barva = aktiven
display.setPnevState(false);                // Siva = neaktiven

// Branje touch eventov
if (display.available()) {
    uint8_t buttonId = display.readTouchEvent();
    // Reagiraj na pritisk
}
```

---

## 10. PRIPOROČILA

1. **Teste vedno najprej v Debug načinu** preden kompilirate .tft
2. **Uporabljajte konzistentne barve**:
   - Rdeča (63488) = OFF, Stop, Alarm
   - Zelena (2016) = ON, Run, Auto
   - Modra (1024) = Manual, Pripravljen
   - Siva (50712) = Neaktivno
3. **Ne pošiljajte preveč podatkov** - NextionDisplay avtomatsko preverja če se vrednost spremeni
4. **Touch eventi** - Nextion vrne component ID, ne imena objekta

---

---

## 7. NAPREDNE FUNKCIJE (opcijsko)

### Dodajanje gumbov za ročno upravljanje:
1. Toolbox → **Button**
2. Properties:
   - **Send Component ID**: 1 (omogoči)
   - Touch events bodo poslani na ESP32

### Timer za animacije:
1. Page → **Timer0**
2. Interval: 100ms
3. Events → **Timer Event**: animacijska koda

---

## BARVE (za referenco)

- Črna: 0
- Bela: 65535
- Rdeča: 63488
- Zelena: 2016
- Modra: 1024
- Rumena: 65504
- Temno siva: 16448
- Svetlo siva: 50712

---

## CHANGELOG - Spremembe v HMI

**Verzija 2.0 (trenutna):**
- ❌ Odstranjen: gauge "nAngle" (ni več potreben)
- ❌ Odstranjen: text "tAlarm" (alarmi prikazani v tStatus)
- 📦 Premaknjen: number "nRev" bo na Page 1
- ✅ Dodano: text "tStatus" za status stroja in alarme
- ✅ Dodano: button "bBrus" za motor kamna
- ✅ Dodano: button "bPnev" za nož
- ✅ Dodano: button "bGor" in "bDol" za vreteno
- ✅ Dodano: button "bSettings" za Page 1
- ✅ Dodano: Xfloat "xAngleStart" in "xAngleStop"
- ℹ️ Črpalka vode je vezana direktno na motor kamna (ni potreben ločen izhod)

---

## TROUBLESHOOTING

**Nextion ne sprejema podatkov:**
- Preverite baudrate (mora biti 115200)
- Preverite TX/RX križanje
- Preverite napajanje (Nextion potrebuje 5V, ne 3.3V)

**Tekst se ne posodobi:**
- Preverite imena objektov (case-sensitive!)
- Debug v Serial Monitor ESP32

**Buttoni ne reagirajo:**
- Preverite če ste dodali Touch Event
- V ročnem načinu morajo biti buttoni omogočeni: `tsw bBrus,1`

---

Srečno s kreacijo HMI! Če boste imeli težave pri kateremkoli koraku, mi sporočite.
