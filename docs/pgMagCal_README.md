# Stran pgMagCal - Kalibracija magneta AS5600

## Opis

Stran **pgMagCal** (page8) omogoča enostavno kalibracijo razdalje magneta od AS5600 senzorja.

## Elementi strani

### Text elementi:
- **tCalStatus** - Glavni status kalibracije
  - Format: "Razdalja OPTIMALNA!" / "Magnet PREMOČEN - oddaljite!" / "Magnet PREŠIBEK - približajte!"
  - Barve ozadja: Zelena (2016) = OK, Rdeča (63488) = napaka, Rumena (65504) = opozorilo

- **tAGC** - AGC vrednost
  - Format: "64" (število 0-128)
  - Optimalno območje: 32-96, cilj ~64

- **tMagStatus** - Status magneta iz Status registra
  - Format: "OPTIMAL" / "PREMOČEN" / "PREŠIBEK" / "NI ZAZNAN"
  - Barve teksta: Zelena = OK, Rdeča = napaka, Rumena = opozorilo

### Progress bar:
- **jAGC** - Vizualni prikaz AGC vrednosti
  - Območje: 0-100 (pretvorjeno iz AGC 0-128)
  - Barve: Zelena (32-96), Rdeča (<32), Rumena (>96)

### Gumbi:
- **bRefresh** (id=12) - Ročna osvežitev vrednosti
  - Event: "#PRESS:12"
  
- **bPage8_Back** - Vrnitev na page 4 (pgSettings)
  - Event: "#PAGE:4" (avtomatsko)

## Eventi

### Vstop na stran
```
Event: "#PAGE:8"
Handler: handlePageChange(8)
```

### Pritisk na bRefresh
```
Event: "#PRESS:12"
Handler: handleTouchPress(12)
```

## Implementacija

### 1. Avtomatska osvežitev (vsakih 0.5s)
```cpp
// V loop()
if (currentPage == 8) {
    if (currentMillis - lastMagnetCalUpdate >= 500) {
        updateMagnetCalibrationDisplay();
        lastMagnetCalUpdate = currentMillis;
    }
}
```

### 2. Funkcija za posodabljanje
```cpp
void updateMagnetCalibrationDisplay() {
    // Preveri prisotnost senzorja
    if (!angleSensor.isSensorPresent()) {
        // Prikaz napake
        return;
    }
    
    // Preberi AGC in Status
    uint8_t agc = angleSensor.getAGC();
    uint8_t status = angleSensor.getMagnetStatus();
    
    // Posodobi AGC prikaz
    display.setText("tAGC", String(agc));
    display.setProgress("jAGC", (agc * 100) / 128);
    
    // Posodobi status magneta (iz Status registra)
    if (status & 0x08) {
        display.setText("tMagStatus", "PREMOČEN");
        display.sendRawCommand("tMagStatus.pco=63488");
    } else if (status & 0x10) {
        display.setText("tMagStatus", "PREŠIBEK");
        display.sendRawCommand("tMagStatus.pco=65504");
    } else if (status & 0x20) {
        display.setText("tMagStatus", "OPTIMAL");
        display.sendRawCommand("tMagStatus.pco=2016");
    }
    
    // Posodobi glavni status (iz AGC vrednosti)
    if (agc < 32) {
        display.setText("tCalStatus", "Magnet PREMOČEN - oddaljite!");
        display.sendRawCommand("tCalStatus.bco=63488");
        display.sendRawCommand("jAGC.pco=63488");
    } else if (agc <= 96) {
        display.setText("tCalStatus", "Razdalja OPTIMALNA!");
        display.sendRawCommand("tCalStatus.bco=2016");
        display.sendRawCommand("jAGC.pco=2016");
    } else {
        display.setText("tCalStatus", "Magnet PREŠIBEK - približajte!");
        display.sendRawCommand("tCalStatus.bco=65504");
        display.sendRawCommand("jAGC.pco=65504");
    }
}
```

## AGC Register (pri 3.3V)

- **Območje**: 0-128
- **Optimalno**: 32-96 (sredina ~64)
- **< 32**: Magnet preblizu (premočen) - oddaljite magnet
- **32-96**: Optimalno območje za robustno delovanje
- **> 96**: Magnet predaleč (prešibek) - približajte magnet

## Status Register

- **Bit 3 (0x08)**: MH - Magnet too strong
- **Bit 4 (0x10)**: ML - Magnet too weak
- **Bit 5 (0x20)**: MD - Magnet detected

## Postopek kalibracije

1. Odprite stran pgMagCal iz pgSettings
2. Spremljajte AGC vrednost in status magneta
3. Premikajte magnet navzgor/navzdol ali bližje/dlje
4. Cilj: AGC vrednost ~64, status "OPTIMAL"
5. Ko dosežete optimalno območje (32-96), magnet pritrdite
6. Pritisnite "NAZAJ" za vrnitev na pgSettings

## Opombe

- **Brez shranjevanja**: Kalibracija je fizična - ni potrebno shraniti v NVS
- **Avtomatsko osvežanje**: Vsake 0.5 sekunde (500ms)
- **Toleranca**: Območje 32-96 zagotavlja robustno delovanje
- **Vpliv na natančnost**: Slabo kalibriran magnet lahko povzroči:
  - Manj natančne meritve kota
  - Večji šum v meritvah
  - Nestabilnost pri mejnih kotih

## Debug izpis

```
[PAGE] Preklop na stran 8
  -> pgMagCal (kalibracija magneta AS5600)
  -> Stran aktivna: AGC vrednost se osvežuje vsakih 0.5s
  -> Optimalno območje: AGC 32-96 (cilj ~64)

[bRefresh] Ročna osvežitev kalibracije magneta
```
