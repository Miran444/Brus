# NEXTION HMI - Podrobna navodila za BRUS projekt

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

### Korak 3: Prikaz kota - velika številka (desno)
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

### Korak 4: Gauge prikaz kota (levo zgoraj)
1. Toolbox → **Gauge** → povlecite na zaslon
2. Position: **X=10, Y=80, W=200, H=200**
3. Properties:
   - **objname**: `nAngle`
   - **sta**: 0 (start angle)
   - **end**: 30 (end angle - 30°)
   - **wid**: 180 (velikost)
   - **pco**: 2016 (zelena - kazalec)
   - **bco**: 0 (črna)

### Korak 5: Prikaz ciklov (levo spodaj)
1. Toolbox → **Text**
2. Position: **X=10, Y=290, W=150, H=40**
3. Properties:
   - **objname**: `tCycles`
   - **txt**: `0/0`
   - **font**: size 3
   - **pco**: 65535
   - **bco**: 16448

### Korak 6: Label "Cikli:" (nad tCycles)
1. Toolbox → **Text**
2. Position: **X=10, Y=260, W=150, H=25**
3. Properties:
   - **objname**: `tLabel1`
   - **txt**: `Cikli:`
   - **font**: size 1
   - **pco**: 50712 (svetlo siva)
   - **bco**: 16448

### Korak 7: Prikaz obratov
1. Toolbox → **Number**
2. Position: **X=170, Y=290, W=120, H=40**
3. Properties:
   - **objname**: `nRev`
   - **val**: 0
   - **font**: size 3
   - **pco**: 65535
   - **bco**: 16448
   - **format**: 0 (decimal)
   - **lenth**: 5 (število mest)

### Korak 8: Label "Obrati:" (nad nRev)
1. Toolbox → **Text**
2. Position: **X=170, Y=260, W=120, H=25**
3. Properties:
   - **objname**: `tLabel2`
   - **txt**: `Obrati:`
   - **font**: size 1
   - **pco**: 50712
   - **bco**: 16448

### Korak 9: Status vretena
1. Toolbox → **Text**
2. Position: **X=300, Y=290, W=170, H=40**
3. Properties:
   - **objname**: `tSpindle`
   - **txt**: `STOP`
   - **font**: size 3
   - **pco**: 63488 (rdeča)
   - **bco**: 16448
   - **xcen**: 1
   - **ycen**: 1

### Korak 10: Progress bar - hitrost vretena
1. Toolbox → **Progress Bar**
2. Position: **X=300, Y=250, W=170, H=20**
3. Properties:
   - **objname**: `jSpeed`
   - **val**: 0
   - **maxval**: 100
   - **pco**: 1024 (modra)
   - **bco**: 0

### Korak 11: Ikone statusov (spodaj desno)

**Ikona motorja kamna:**
1. Najprej dodajte 2 slike v Picture Library:
   - Motor OFF (npr. siva ikona)
   - Motor ON (npr. zelena ikona)
2. Toolbox → **Picture**
3. Position: **X=250, Y=210, W=50, H=50**
4. Properties:
   - **objname**: `picMotor`
   - **pic**: 0 (OFF slika)

**Label motorja:**
1. Toolbox → **Text**
2. Position: **X=240, Y=265, W=70, H=15**
3. Properties:
   - **txt**: `Motor`
   - **font**: size 0
   - **pco**: 50712

**Ikona črpalke:**
1. Toolbox → **Picture**
2. Position: **X=320, Y=210, W=50, H=50**
3. Properties:
   - **objname**: `picPump`
   - **pic**: 0

**Label črpalke:**
1. Toolbox → **Text**
2. Position: **X=310, Y=265, W=70, H=15**
3. Properties:
   - **txt**: `Črpalka`
   - **font**: size 0
   - **pco**: 50712

**Ikona noža:**
1. Toolbox → **Picture**
2. Position: **X=390, Y=210, W=50, H=50**
3. Properties:
   - **objname**: `picKnife`
   - **pic**: 0

**Label noža:**
1. Toolbox → **Text**
2. Position: **X=380, Y=265, W=70, H=15**
3. Properties:
   - **txt**: `Nož`
   - **font**: size 0
   - **pco**: 50712

### Korak 12: Alarm prikaz (center spodaj)
1. Toolbox → **Text**
2. Position: **X=10, Y=230, W=460, H=20**
3. Properties:
   - **objname**: `tAlarm`
   - **txt**: `ALARM`
   - **font**: size 2
   - **pco**: 63488 (rdeča)
   - **bco**: 0
   - **xcen**: 1
   - **vis**: 0 (neviden na začetku)

---

## 3. IKONE - Enostavna rešitev

Če nimate pripravljenih ikon:

### Možnost 1: Uporaba teksta namesto ikon
Zamenjajte Picture objekte s Text objekti:
- **picMotor** → Text z "M" (manjši font)
- **picPump** → Text z "P"
- **picKnife** → Text z "N"
- Spreminjate barvo teksta: pco=50712 (OFF), pco=2016 (ON)

### Možnost 2: Dodajanje preprostih ikon
1. Tools → **Picture Library**
2. Add → Dodajte 2 png slike (64x64px):
   - 0: Siva ikona (OFF)
   - 1: Zelena ikona (ON)
3. Uporabite v Picture objektih

---

## 4. TESTIRANJE

### Preizkus pošiljanja iz ESP32:
V Serial Monitoru bi morali videti podatke, Nextion pa jih bo prikazal.

### Debug v Nextion Editorju:
1. Kliknite **Debug** (zgoraj desno)
2. V Command line vnesite testne komande:
   ```
   tMode.txt="AUTO"
   tAngle.txt="23.5°"
   nAngle.val=23
   nRev.val=42
   tCycles.txt="3/7"
   ```

---

## 5. COMPILE IN EXPORT

1. **Debug** → Preverite če vse deluje
2. **File → Compile** → Počakajte
3. **File → TFT File Output** → Shranite .tft datoteko
4. Kopirajte .tft na microSD kartico
5. Vstavite v Nextion display
6. Vklopite - display bo avtomatsko naložil .tft

---

## 6. POVEZAVA S ESP32

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

## TROUBLESHOOTING

**Nextion ne sprejema podatkov:**
- Preverite baudrate (mora biti 115200)
- Preverite TX/RX križanje
- Preverite napajanje (Nextion potrebuje 5V, ne 3.3V)

**Tekst se ne posodobi:**
- Preverite imena objektov (case-sensitive!)
- Debug v Serial Monitor ESP32

**Slike ne delujejo:**
- Preverite Picture Library (slike morajo biti manjše od 4096 bajtov vsaka)

---

Srečno s kreacijo HMI! Če boste imeli težave pri kateremkoli koraku, mi sporočite.
