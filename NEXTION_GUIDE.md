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

## 4. PAGE 3 - REFERENČNI HOD

Ta stran se uporablja za avtomatsko merjenje mejnih kotov in kalibracijo.  
**Dostop**: S pritiskom na gumb **bRef** (id=15) na Page 1 (stalno aktiven).

### Globalne spremenljivke na Page 0 (Variables):
Ustvarite v **Tools → Variables** (na page0):
1. **vaMinAngle** - Number, id=22 (min kot × 10, npr. 50 = 5.0°)
2. **vaMaxAngle** - Number, id=23 (max kot × 10, npr. 280 = 28.0°)
3. **vaAngleStart** - Number (začetni kot za delo × 10)
4. **vaAngleStop** - Number (končni kot za delo × 10)

---

### Korak 1: Prikaz trenutnega kota
1. Toolbox → **Text**
2. Position: **X=50, Y=80, W=180, H=80**
3. Properties:
   - **objname**: `tActualAngle`
   - **id**: 14
   - **txt**: `0.0`
   - **font**: velik font (size 5-6)
   - **pco**: 65535 (bela)
   - **bco**: 16448 (temno siva)
   - **xcen**: 1
   - **ycen**: 1

**POMEMBNO:** Ta vrednost se **stalno osvežuje** med referenčnim hodom!

### Korak 2: Prikaz minimalnega kota
1. Toolbox → **Text**
2. Position: **X=250, Y=80, W=120, H=40**
3. Properties:
   - **objname**: `tMinAngle`
   - **id**: 15
   - **txt**: `0.0`
   - **font**: size 3
   - **pco**: 2016 (zelena)
   - **bco**: 16448

### Korak 3: Prikaz maksimalnega kota
1. Toolbox → **Text**
2. Position: **X=250, Y=130, W=120, H=40**
3. Properties:
   - **objname**: `tMaxAngle`
   - **id**: 16
   - **txt**: `0.0`
   - **font**: size 3
   - **pco**: 2016 (zelena)
   - **bco**: 16448

### Korak 4: Prikaz hitrosti motorja (za page4)
1. Toolbox → **Text**
2. Position: **X=50, Y=180, W=100, H=30**
3. Properties:
   - **objname**: `tMSpeed`
   - **id**: 17
   - **txt**: `0`
   - **font**: size 2

### Korak 5: Število obratov
1. Toolbox → **Text**
2. Position: **X=50, Y=220, W=100, H=30**
3. Properties:
   - **objname**: `tMRev`
   - **id**: 18
   - **txt**: `0`
   - **font**: size 2
   - **pco**: 65535

**Opis:** Prikazuje število obratov motorja (S45 števec) od tMinAngle do tMaxAngle.

### Korak 6: Obrati na stopinjo
1. Toolbox → **Text**
2. Position: **X=180, Y=220, W=120, H=30**
3. Properties:
   - **objname**: `tRevPerAngle`
   - **id**: 19
   - **txt**: `0.0`
   - **font**: size 2
   - **pco**: 65535

**Opis:** Izračunani obrati motorja za eno stopinjo kota (obrati / (maxAngle - minAngle)).

### Korak 7: Čas referenčnega hoda
1. Toolbox → **Text**
2. Position: **X=320, Y=220, W=120, H=30**
3. Properties:
   - **objname**: `tTime`
   - **id**: 20
   - **txt**: `0.0`
   - **font**: size 2
   - **pco**: 65535

**Opis:** Čas potreben za hod od minAngle do maxAngle (format: sekunde z 1 decimalno mesto, npr. "12.5").

### Korak 8: Button za nazaj
1. Toolbox → **Button**
2. Properties:
   - **objname**: `bBackFromRef`
   - **txt**: `◀ NAZAJ`
   - **Touch Release Event**: `page 1`

### Korak 9: Button za začetek referenčnega hoda
1. Toolbox → **Button**
2. Position: **X=150, Y=260, W=180, H=60**
3. Properties:
   - **objname**: `bRefStart`
   - **id**: 3 (Component ID)
   - **txt**: `START`
   - **font**: size 3
   - **pco**: 65535 (bela)
   - **bco**: 2016 (zelena)

**Touch Press Event za bRefStart:**
- Nastavite **Send Component ID** na **enabled** (ID=3)
- ESP32 bo prejel Touch Press Event in začel referenčni hod

---

### Postopek referenčnega hoda (izvaja se v ESP32):

**Zahteve:**
- Sistem mora biti v **MANUAL načinu** (S1 - Ročno)
- Referenčni hod se NE začne avtomatsko ob prehodu na page3
- Uporabnik mora pritisniti **bRefStart** gumb

**Faza 1A: Iskanje najnižje točke**
1. Vreteno gre navzdol (`moveSpindleDown`)
2. Ko **S43 preklopi na "1"** (aktiven) → ustavimo motor (dosežena najnižja točka)

**Faza 1B: Določitev MIN kota**
1. Obrnemo smer motorja → vreteno gre navzgor (`moveSpindleUp`)
2. Ko **S43 preklopi na "0"** (neaktiven):
   - Začnemo z merjenjem časa: `refStartTime = millis()`
   - Shranimo trenutni kot kot `startMeasurementAngle`
3. Nadaljujemo navzgor še **0.5° od startMeasurementAngle**
4. Ko dosežemo `currentAngle = startMeasurementAngle + 0.5°`:
   - `minAngle = currentAngle` (to je naš minimalni delovni kot)
   - Resetiramo števec obratov S45 (začnemo šteti od minAngle naprej)
   - Display posodobi `tMinAngle`

**Faza 2: Iskanje MAX kota**
1. Nadaljujemo navzgor (motor že teče)
2. Stalno štejemo obrate (S45 - IN_S45_STEVEC)
3. Ko se **S46 končno stikalo** aktivira ("1"):
   - Vreteno je doseglo zgornji položaj
   - Ustavimo motor
   - Ustavimo merjenje časa
   - Izračunamo `maxAngle = currentAngle - 0.5°` (toleranca)
   - Display posodobi `tMaxAngle` in `tMRev`

**POMEMBNO:** 
- **S43** je varnostno končno stikalo (najnižji položaj) - vezano na IN_S43_SAFETY
- **S46** je končno stikalo (najvišji položaj) - vezano na IN_USER
- **Sekvenca**: S43="1"(ustavi)→obrni smer→S43="0"(začni merjenje)→+0.5°(minAngle)→...→S46="1"(maxAngle-0.5°)
- **Toleranca 0.5°**: Zagotavlja, da mejni koti ostanejo znotraj varnih mej (odmik od stikal)
- **Merjenje časa**: Se začne šele ko S43 postane neaktiven (iz najnižje točke navzgor)
- **Merjenje obratov**: Se začne ko določimo minAngle (od 0.5° nad S43 preklopom naprej)

**Faza 3: Izračun in shranjevanje**
1. Izračuna:
   - `revPerAngle = totalRevolutions / (maxAngle - minAngle)`
   - `time = (millis() - startTime) / 1000.0` (v sekundah)
2. Posodobi display:
   - `tRevPerAngle` (format "X.X")
   - `tTime` (format "X.X")
3. Shrani vse vrednosti v Preferences (NVS) za trajno shranjevanje

**Opombe:**
- Med celotnim postopkom je `tActualAngle` stalno osvežen
- Referenčni hod se začne samo s pritiskom na **bRefStart** gumb
- Po končanem referenčnem hodu lahko uporabnik:
  - Pritisne **bBackFromRef** → vrne se na page1
  - Vrednosti min/max se avtomatsko uporabijo na page1 za validacijo kotov
- **Končni stikali S43 in S46**: Omogočata zanesljivo detekcijo mejnih leg
- **Toleranca 0.5°**: Zagotavlja, da se vreteno nikoli ne premakne preveč proti stikalom
- **Merjenje obratov**: Šteje samo pot od minAngle do maxAngle (za natančen revPerAngle)
- **Uporaba v AUTO načinu**: revPerAngle in čas se uporabita za spremljanje poteka in detekcijo napak

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

## PAGE 4 - NASTAVITEV HITROSTI

**Namen:** Nastavitev hitrosti pomika vretena v avtomatskem in ročnem načinu

### Opis avtomatskega brusenja

Avtomatsko brusenje je razdeljeno na tri faze:
1. **Začetni kot do Začetni kot - 2.0°** (npr. od 22° do 20°)
   - Faza približevanja materialu
   - Hitrost: `hZacetni` (privzeto 90%)
   
2. **Začetni kot - 2.0° do Končni kot + 2.0°** (npr. od 20° do 12°)
   - Glavna faza brusenja
   - Hitrost: `hSredina` (privzeto 75%)
   - Počasnejša za večji odjem materiala
   
3. **Končni kot + 2.0° do Končni kot** (npr. od 12° do 10°)
   - Faza zaključevanja
   - Hitrost: `hKoncni` (privzeto 85%)

**Opomba:** Večja kot je hitrost, manj materiala se posname (obrusi).

### Komponente

#### Progress Bar: hZacetni (Začetna hitrost)
- **objname**: `hZacetni`
- **id**: 2
- **Position**: X=50, Y=50, W=300, H=40
- **min val**: 50
- **max val**: 100
- **default val**: 90
- **bco** (background): 33840 (siva)
- **pco** (progress color): 2016 (zelena)
- **Events**:
  - Touch Release Event: ✓ (Send Component ID + 1 byte value)

#### Progress Bar: hSredina (Srednja hitrost)
- **objname**: `hSredina`
- **id**: 6
- **Position**: X=50, Y=120, W=300, H=40
- **min val**: 50
- **max val**: 100
- **default val**: 75
- **bco**: 33840
- **pco**: 2016
- **Events**:
  - Touch Release Event: ✓

#### Progress Bar: hKoncni (Končna hitrost)
- **objname**: `hKoncni`
- **id**: 8
- **Position**: X=50, Y=190, W=300, H=40
- **min val**: 50
- **max val**: 100
- **default val**: 85
- **bco**: 33840
- **pco**: 2016
- **Events**:
  - Touch Release Event: ✓

#### Progress Bar: hRocno (Ročna hitrost)
- **objname**: `hRocno`
- **id**: 18
- **Position**: X=50, Y=260, W=300, H=40
- **min val**: 50
- **max val**: 100
- **default val**: 80
- **bco**: 33840
- **pco**: 2016
- **Events**:
  - Touch Release Event: ✓
- **Namen**: Določa hitrost ročnega pomika v MODE_MANUAL

#### Button: bPage4_Back (Nazaj)
- **objname**: `bPage4_Back`
- **id**: 9
- **Position**: X=370, Y=250, W=100, H=60
- **txt**: `Nazaj`
- **font**: size 3
- **bco**: 1024 (modra)
- **pco**: 65535 (bela)
- **Events**:
  - Touch Press Event: ✓ (Send Component ID)

#### Label Text objekti (opcijsko)
Za prikaz oznak pri slajderjih:
- Text nad `hZacetni`: "Začetna hitrost"
- Text nad `hSredina`: "Srednja hitrost"
- Text nad `hKoncni`: "Končna hitrost"
- Text nad `hRocno`: "Ročna hitrost"

### Navigacija

**Vstop na page4:**
- Iz **page1** → pritisni gumb **bSpeed** (id=14)

**Izhod iz page4:**
- Na **page4** → pritisni gumb **bPage4_Back** (id=9) → nazaj na **page1**

### Delovanje

1. Uporabnik vstopi na page4 preko gumba `bSpeed` na page1
2. Sistem posodobi prikaz vseh 4 progress barov s trenutnimi vrednostmi iz NVS
3. Uporabnik premika slajder (progress bar)
4. Ob spustitvi slajderja:
   - Progress bar pošlje Touch Release Event (0x66) + Component ID + 1 byte vrednost (%)
   - ESP32 prebere vrednost, omeji na 50-100%
   - Shrani v globalno spremenljivko (`speedZacetni`, `speedSredina`, `speedKoncni`, `speedRocno`)
   - Shrani v NVS (Non-Volatile Storage)
5. Pritisnite `bPage4_Back` za vrnitev na page1

### Pomembne opombe

- **Hitrosti so v procentih**: 50% = minimalna hitrost, 100% = maksimalna hitrost
- **PWM pretvorba**: 50% → 128 PWM, 100% → 255 PWM (linearna interpolacija)
- **Samodejno shranjevanje**: Vsaka sprememba se takoj shrani v NVS
- **Ročna hitrost (`hRocno`)**: Uporablja se SAMO v MODE_MANUAL
- **Avtomatske hitrosti**: Uporabljajo se v MODE_AUTO med ciklom

---

Srečno s kreacijo HMI! Če boste imeli težave pri kateremkoli koraku, mi sporočite.
