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

### Korak 1a: Preinitialize Event za Page 0
1. Desni klik na **page0** → **Event**
2. **Preinitialize Event** → vnesite:
```
// Nastavi vse ročne gumbe na neaktivne (siva barva texta)
bBrus.pco=54970
bPnev.pco=54970
bGor.pco=54970
bDol.pco=54970
```

**OPOMBA:** ESP32 bo ob zagonu preveril nastavitve in ustrezno aktiviral gumbe v MODE_MANUAL.

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

**Text za začetni kot:**
1. Toolbox → **Text**
2. Position: **X=10, Y=140, W=100, H=35**
3. Properties:
   - **objname**: `tAngleStart`
   - **txt**: `0.0°`
   - **font**: size 2
   - **pco**: 2016 (zelena)
   - **bco**: 16448
   - **xcen**: 1
   - **ycen**: 1

**Label "Start:"**
1. Toolbox → **Text**
2. Position: **X=10, Y=120, W=100, H=20**
3. Properties:
   - **txt**: `Start:`
   - **font**: size 1
   - **pco**: 50712 (svetlo siva)
   - **bco**: 16448

**Text za končni kot:**
1. Toolbox → **Text**
2. Position: **X=120, Y=140, W=100, H=35**
3. Properties:
   - **objname**: `tAngleStop`
   - **txt**: `0.0°`
   - **font**: size 2
   - **pco**: 1024 (modra)
   - **bco**: 16448
   - **xcen**: 1
   - **ycen**: 1

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
ESP32 bo preko kode spremenil barvo texta: `bBrus.pco=0` (aktiven - črna) ali `bBrus.pco=54970` (neaktiven - siva).

**Barve texta za buttone:**
- **pco=0** - Črna (aktiven)
- **pco=54970** - Siva (neaktiven)

---

### Touch Eventi za ročno upravljanje:

**bGor (ID=17) - Vreteno GOR:**
1. Desni klik na **bGor** → **Event**
2. Nastavite **Send Component ID** na **enabled**
3. V **Touch Press Event** pustite prazno (ESP32 bo prejel ID=17)
4. V **Touch Release Event** pustite prazno (ESP32 bo prejel ID=17)

**bDol (ID=18) - Vreteno DOL:**
1. Desni klik na **bDol** → **Event**
2. Nastavite **Send Component ID** na **enabled**
3. V **Touch Press Event** pustite prazno (ESP32 bo prejel ID=18)
4. V **Touch Release Event** pustite prazno (ESP32 bo prejel ID=18)

**bBrus (ID=19) - Motor kamna (toggle):**
1. Desni klik na **bBrus** → **Event**
2. Nastavite **Send Component ID** na **enabled**
3. V **Touch Press Event** pustite prazno (ESP32 bo prejel ID=19 ob vsakem pritisku)

**bPnev (ID=20) - Ventil noža (toggle):**
1. Desni klik na **bPnev** → **Event**
2. Nastavite **Send Component ID** na **enabled**
3. V **Touch Press Event** pustite prazno (ESP32 bo prejel ID=20 ob vsakem pritisku)

**Delovanje:**
- **bGor/bDol**: Med držanjem gumba se motor vretena premika, ob spustitvi se ustavi. Trenutni kot se prikazuje v `tAngle`.
- **bBrus/bPnev**: Toggle gumba - enkrat pritisnemo = vklopi, ponovno pritisnemo = izklopi. Barva ozadja se spremeni (zelena=aktiven, siva=neaktiven).

---

### Korak 8a: Button bSettings za prehod na Page 1 (nastavitve)

**Button SETTINGS:**
1. Toolbox → **Button**
2. Position: **X=420, Y=10, W=60, H=40**
3. Properties:
   - **objname**: `bSettings`
   - **id**: 16
   - **txt**: `⚙`  (ali "SET")
   - **font**: size 3
   - **pco**: 65535
   - **bco**: 1024 (modra)

**Touch Press Event za bSettings:**
1. Desni klik na **bSettings** → **Event**
2. Nastavite **Send Component ID** na **enabled** (ID=16)
3. V **Touch Press Event** pustite prazno (ESP32 bo prejel ID=16 in preklopil na page1)

**OPOMBA:** Gumb bSettings je aktiven samo v MANUAL načinu. ESP32 avtomatsko upravlja njegovo aktivacijo.

---

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

## 3. PAGE 1 - NASTAVITVE KOTOV

Page 1 je namenjena nastavitvi začetnega in končnega kota za avtomatsko delovanje.  
**Dostop**: S pritiskom na gumb **bSettings** na Page 0 (samo v MANUAL načinu).

**POMEMBNO - Smer delovanja:**
- **Začetni kot (StartAngle)** mora biti VEČJI od končnega kota (EndAngle)
- Primer: StartAngle = 28.0°, EndAngle = 5.0°
- **Avtomatski cikel**: Začetni → Končni → Začetni (vreteno gre DOL → GOR → DOL)
- Vreteno se spušča od višjega kota (npr. 28°) proti nižjemu kotu (npr. 5°)

### Korak 1: Status prikaz za Page 1
1. Toolbox → **Text** → povlecite na zaslon
2. Position: **X=10, Y=10, W=460, H=50**
3. Properties:
   - **objname**: `tStatus_pg1`
   - **id**: 21 (Component ID)
   - **txt**: `Vnesi kote ali uporabi Nastavi gumb`
   - **font**: size 2
   - **pco** (text color): 65535 (bela)
   - **bco** (background): 1024 (modra)
   - **xcen**: 1 (center horizontalno)
   - **ycen**: 1 (center vertikalno)

**OPOMBA:** tStatus_pg1 prikazuje informativna sporočila (max 50 znakov):
- "Vnesi kote ali uporabi Nastavi gumb" - začetno stanje
- "Nastavi START kot (Gor/Dol tipke)" - med nastavljanjem
- "Koti OK - pritisnite bSave!" - po uspešnem vnosu
- "NAPAKA: Start > Stop!" - napaka pri validaciji
- "Koti shranjeni!" - uspešno shranjevanje

---

### Dve možnosti nastavitve kotov:

---

### VARIANTA 1: Ročni vnos kotov (Xfloat objekti)

**Xfloat za vnos začetnega kota:**
1. Toolbox → **Xfloat**
2. Position: **X=50, Y=80, W=150, H=60**
3. Properties:
   - **objname**: `xStartAngle`
   - **id**: 5 (Component ID)
   - **vvs0**: 1 (omogoči numeric keyboard)
   - **vvs1**: 1 (ena decimalna mesta)
   - **minval**: 50 (min 5.0°)
   - **maxval**: 300 (max 30.0° * 10)
   - **val**: 280 (privzeto 28.0° - ZAČETNI kot je večji!)

**Xfloat za vnos končnega kota:**
1. Toolbox → **Xfloat**
2. Position: **X=250, Y=80, W=150, H=60**
3. Properties:
   - **objname**: `xEndAngle`
   - **id**: 6
   - **vvs0**: 1
   - **vvs1**: 1
   - **minval**: 50 (min 5.0°)
   - **maxval**: 300 (max 30.0°)
   - **val**: 50 (privzeto 5.0° - KONČNI kot je manjši!)

**Touch Release Event za xStartAngle in xEndAngle:**
```
// Pošlji podatke na ESP32
covx xStartAngle.val,vaStartAngle.txt,0,0
covx xEndAngle.val,vaEndAngle.txt,0,0
prints "ID5:",4
prints vaStartAngle.txt,4
prints ";ID6:",5
prints vaEndAngle.txt,4
printh ff ff ff
```

**OPOMBA:** Potrebujete začasne string variable:
- **vaStartAngle** (String Variable)
- **vaEndAngle** (String Variable)

---

### VARIANTA 2: Nastavitev s fizičnimi tipkami (bNastaviKote)

**Button bNastaviKote:**
1. Toolbox → **Button**
2. Position: **X=50, Y=200, W=150, H=60**
3. Properties:
   - **objname**: `bNastaviKote`
   - **id**: 19
   - **txt**: `Nastavi`
   - **font**: size 2
   - **pco**: 65535
   - **bco**: 1024 (modra)

**Touch Press Event za bNastaviKote:**
1. Desni klik na **bNastaviKote** → **Event**
2. Nastavite **Send Component ID** na **enabled** (ID=19)
3. ESP32 bo avtomatsko spreminjal besedilo gumba:
   - **"Nastavi"** - začetno stanje
   - **"Začetni"** - nastavljanje začetnega kota s tipkami Gor/Dol
   - **"Končni"** - nastavljanje končnega kota s tipkami Gor/Dol

**Delovanje:**
1. **Prvi pritisk**: Aktivira nastavljanje začetnega kota
   - Besedilo se spremeni v "Začetni"
   - Uporabite fizične tipke **S42 (Gor)** in **S41 (Dol)** za premik vretena
   - Trenutni kot se prikazuje v `tAngle` (Page 0 in Page 1)
   - Kot se prikazuje tudi v **xStartAngle** (id=5) v formatu Angle*10 (npr. 280 = 28.0°)
2. **Drugi pritisk**: Shrani začetni kot in aktivira nastavljanje končnega
   - Besedilo se spremeni v "Končni"
   - Ponovno uporabite tipke Gor/Dol za nastavitev končnega kota
   - Kot se prikazuje v **xEndAngle** (id=6) v formatu Angle*10 (npr. 50 = 5.0°)
   - Gumb **bSave** se aktivira (postane klikabilen)
3. **Tretji pritisk**: Shrani končni kot in resetira
   - Besedilo se vrne na "Nastavi"
   - Koti so pripravljeni za shranitev
   - **POMEMBNO**: Začetni kot mora biti večji od končnega (StartAngle > EndAngle)

---

### Button za shranjevanje (bSave):
1. Toolbox → **Button**
2. Position: **X=250, Y=200, W=150, H=60**
3. Properties:
   - **objname**: `bSave`
   - **id**: 7
   - **txt**: `SHRANI`
   - **font**: size 2
   - **pco**: 54970 (siva - neaktiven ob zagonu)
   - **bco**: 2016 (zelena)

**Touch Press Event za bSave:**
1. Desni klik na **bSave** → **Event**
2. V **Touch Press Event** vnesite validacijsko kodo:

```
// Preveri: Začetni kot mora biti večji od končnega!
if(xStartAngle.val<=xEndAngle.val)
{
  // NAPAKA: Začetni <= Končni
  tStatus.txt="NAPAKA: Start > Stop!"
  tStatus.bco=63488
}
else
{
  // Validacija OK - pošlji podatke na ESP32
  // Nastavite Send Component ID na enabled (ID=7)
  // ESP32 bo prejel Touch Press Event z ID=7
}
```

**ALI (če želite dodatno validacijo v Nextion HMI):**

```
// Konvertiraj float v string
covx xStartAngle.val,vaStartAngle.txt,0,0
covx xEndAngle.val,vaEndAngle.txt,0,0

// Validacija: Začetni > Končni
if(xStartAngle.val<=xEndAngle.val)
{
  tStatus.txt="NAPAKA: Start > Stop!"
  tStatus.bco=63488
}
else
{
  // Vrednosti so OK - pošlji na ESP32
  prints "ID5:",4
  prints vaStartAngle.txt,4
  prints ";ID6:",5
  prints vaEndAngle.txt,4
  printh ff ff ff
}
```

**Pomembno:**
- **Začetni kot (StartAngle)** mora biti **večji** od **končnega kota (EndAngle)**
- Primer pravilnih vrednosti: Start = 28.0°, Stop = 5.0° ✓
- Primer napačnih vrednosti: Start = 5.0°, Stop = 28.0° ✗

**Razlog:** 
Avtomatski cikel deluje tako, da se vreteno premakne od višjega kota (Start) do nižjega kota (Stop) in nazaj: **Start → Stop → Start**

ESP32 bo dodatno preveril validacijo pred shranjevanjem v NVS za dvojno varnost.

**OPOMBA:** Gumb bSave se aktivira šele ko:
- Uporabite bNastaviKote za nastavitev obeh kotov, ALI
- Spremenite vrednosti v xStartAngle/xEndAngle

**Potrebne spremenljivke:**
- **vaStartAngle** (String Variable, za temp shranjevanje pri Xfloat validaciji)
- **vaEndAngle** (String Variable, za temp shranjevanje pri Xfloat validaciji)
- **vaAngleStart** (Number Variable - globalna, posodobi se avtomatsko, format: kot*10)
- **vaAngleStop** (Number Variable - globalna, posodobi se avtomatsko, format: kot*10)

---

### Button za nazaj:
1. Toolbox → **Button**
2. Position: **X=10, Y=10, W=100, H=40**
3. Properties:
   - **objname**: `bBack`
   - **txt**: `◀ NAZAJ`
   - **Touch Event**: `page 0`

---

## 4. PAGE 3 - REFERENČNI HOD (pageRef)

Ta stran se uporablja za avtomatsko merjenje mejnih kotov in kalibracijo.

### Globalne spremenljivke (Variables):
Ustvarite v **Tools → Variables**:
1. **vaMaxAngle** - Number (max izmerjeni kot, npr. 280 = 28.0°)
2. **vaMinAngle** - Number (min izmerjeni kot, npr. 50 = 5.0°)
3. **vaAngleStart** - Number (začetni kot za delo, npr. 50 = 5.0°)
4. **vaAngleStop** - Number (končni kot za delo, npr. 280 = 28.0°)
5. **tRevPerAngle** - Number (število obratov na stopinjo)
6. **tCycleTime** - Number (čas cikla v sekundah)

### Button za referenčni hod (na Page 1):
1. Toolbox → **Button**
2. Properties:
   - **objname**: `bRef`
   - **id**: 15 (0x0F)
   - **txt**: `REFERENCA`
   - **Touch Release Event**: `page 3`

### Osnovna struktura Page 3:
```
[Label: "REFERENČNI HOD"]
[Text: tRefStatus - prikaz trenutnega stanja]
[Text: tMaxAngleDisplay - prikaz max kota]
[Text: tMinAngleDisplay - prikaz min kota]
[Button: bStartRef - začetek referenčnega hoda]
[Button: bBackFromRef - nazaj na Page 1]
```

**Postopek referenčnega hoda (izvaja se v ESP32):**
1. Uporabnik pritisne "bRef" → ESP32 prejme component ID 15
2. ESP32 zažene avtomatski cikel za merjenje min/max kotov
3. ESP32 pošilja posodobitve na display
4. Ko je končano, ESP32 pošlje vrednosti: tMaxAngle, tMinAngle, tRevPerAngle, tCycleTime
5. Te vrednosti se shranijo v Preferences in v Nextion globalne spremenljivke

---

## 5. POMEMBNE OPOMBE

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

### Validacija vnosov:
Validacija kotov se izvaja direktno v HMI (Touch Event) preden se podatki pošljejo na ESP32.  
To preprečuje neveljavne vrednosti in zmanjša obremenitev ESP32.

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
- **tAngleStart** - Text - Začetni kot za avto način (prikaz z °)
- **tAngleStop** - Text - Končni kot za avto način (prikaz z °)
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
#include <Preferences.h>

// Inicializacija
display.begin();
Preferences preferences;

// Nastavitve prikaza
display.setMode("AUTO");                    // OFF, MANUAL, AUTO
display.setStatus("Run");                   // Stop, Pripravljen, Run
display.setStatus("NAPAKA: Senzor!");       // Za prikaz alarmov (rdeča barva)
display.setCycles(3, 7);                    // trenutni, ciljni (99 = neskončno)
display.setAngle(23.5);                     // Trenutni kot
display.setAngleRange(5.0, 28.0);          // Start in Stop kot
display.setSpindleStatus(true, true, 128);  // premika, smer_gor, hitrost

// Buttoni (omogoči/onemogoči)
display.setButtonState("bBrus", true);      // Omogoči button (črna barva texta)
display.setButtonState("bPnev", false);     // Onemogoči button (siva barva texta)
display.setButtonState("bGor", true);
display.setButtonState("bDol", true);

// Ali vse hkrati
display.enableManualButtons(true);          // Omogoči vse ročne gumbe
display.enableManualButtons(false);         // Onemogoči vse ročne gumbe

// Stanje buttonov (vizualni feedback)
display.setBrusState(true);                 // Zelena barva = aktiven
display.setPnevState(false);                // Siva = neaktiven

// Globalne spremenljivke (za validacijo)
display.setGlobalVariable("vaMaxAngle", 280);  // 28.0°
display.setGlobalVariable("vaMinAngle", 50);   // 5.0°
int32_t maxAngle = display.getGlobalVariable("vaMaxAngle");

// Branje touch eventov in custom stringov
if (display.available()) {
    uint8_t buttonId = display.readTouchEvent();
    
    if (buttonId == 7) {  // bSave pritisnjen
        String data = display.readString();  // "ID5:357;ID6:80"
        float startAngle, endAngle;
        
        if (display.parseAngleSettings(data, startAngle, endAngle)) {
            // Shrani v Preferences
            preferences.begin("brus", false);
            preferences.putFloat("angleStart", startAngle);
            preferences.putFloat("angleStop", endAngle);
            preferences.end();
            
            // Posodobi prikaz
            display.setAngleRange(startAngle, endAngle);
        }
    }
    else if (buttonId == 15) {  // bRef - referenčni hod
        // Zaženi avtomatski cikel za kalibracijo
        startReferenceRun();
    }
}

// Nalaganje shranjenih vrednosti ob zagonu
preferences.begin("brus", false);
float angleStart = preferences.getFloat("angleStart", 5.0);
float angleStop = preferences.getFloat("angleStop", 28.0);
float maxAngle = preferences.getFloat("maxAngle", 28.0);
float minAngle = preferences.getFloat("minAngle", 5.0);
preferences.end();

display.setAngleRange(angleStart, angleStop);
display.setGlobalVariable("vaMaxAngle", (int32_t)(maxAngle * 10));
display.setGlobalVariable("vaMinAngle", (int32_t)(minAngle * 10));
// vaAngleStart in vaAngleStop se nastavita avtomatsko v setAngleRange()
```

**Primeri celotne implementacije:** 
- `examples/nextion_angle_settings.cpp` - vnos in shranjevanje kotov
- `examples/nextion_reference_calibration.cpp` - referenčni hod in kalibracija
- `examples/nextion_startup_validation.cpp` - validacija ob zagonu in upravljanje načinov

---

## 11. VALIDACIJA OB ZAGONU

Ob zagonu ESP32 preveri:

1. **Referenčni hod izveden?** (vaMinAngle > 0 && vaMaxAngle > 0)
   - NE → `tStatus.txt = "Izvedite referencni hod !"`
   - MODE_MANUAL deluje, MODE_AUTO NE

2. **Koti nastavljeni?** (vaAngleStart > 0 && vaAngleStop > 0)
   - NE → `tStatus.txt = "Nastavite zacetni in koncni kot !"`
   - MODE_MANUAL deluje, MODE_AUTO NE

3. **Vse OK?**
   - DA → `tStatus.txt = "Pripravljen"`
   - MODE_MANUAL in MODE_AUTO delujeta

### Način delovanja:
- **MODE_OFF**: Vsi ročni gumbi neaktivni (siva barva)
- **MODE_MANUAL**: Ročni gumbi aktivni (črna barva) - deluje VEDNO
- **MODE_AUTO**: Ročni gumbi neaktivni - deluje samo če so reference OK

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
