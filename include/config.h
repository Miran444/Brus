#ifndef CONFIG_H
#define CONFIG_H

// ===== SPI PINS za SN65HVS882 =====
#define SPI_MISO    36  // GPIO36 - MISO (data out from SN65HVS882)
#define SPI_CLK     37  // GPIO37 - Clock
#define SPI_LD      38  // GPIO38 - Load (Latch/CS)
#define TEMP_ALARM  35  // GPIO35 - TOK (temperature alarm from SN65HVS882)

// ===== SPI NASTAVITVE =====
#define SPI_FREQUENCY   1000000  // 1 MHz
#define SPI_MODE        SPI_MODE0

// ===== MAPPING VHODOV =====
// Prvi SN65HVS882 (IN0-IN7)
#define IN_S1_ROCNO         0   // Stikalo S1 - Ročno
#define IN_S1_AVTOMATSKO    1   // Stikalo S1 - Avtomatsko  
#define IN_RESET            2   // Tipka Reset
#define IN_S41_DOL          3   // Tipka S41 - Vreteno dol
#define IN_S42_GOR          4   // Tipka S42 - Vreteno gor
#define IN_STEVEC_OBRATOV   5   // Števec obratov motorja vretena
#define IN_NAKLON_VRETENA   6   // Stikalo naklona (< 10°)
#define IN_RESERVED_0       7   // Rezerva

// Drugi SN65HVS882 (IN8-IN15) - za S2 preklopnik
#define IN_S2_POS1          8   // S2 pozicija 1 cikel
#define IN_S2_POS2          9   // S2 pozicija 2 cikla
#define IN_S2_POS3          10  // S2 pozicija 3 cikli
#define IN_S2_POS4          11  // S2 pozicija 4 cikli
#define IN_S2_POS5          12  // S2 pozicija 5 ciklov
#define IN_S2_POS6          13  // S2 pozicija 6 ciklov
#define IN_S2_NEPREKINJEN   14  // S2 neprekinjeni cikli
#define IN_RESERVED_1       15  // Rezerva

// ===== ENUMERACIJE =====
enum S1Mode {
    MODE_OFF = 0,
    MODE_MANUAL = 1,
    MODE_AUTO = 2
};

enum S2Cycles {
    CYCLES_NONE = 0,
    CYCLES_1 = 1,
    CYCLES_2 = 2,
    CYCLES_3 = 3,
    CYCLES_4 = 4,
    CYCLES_5 = 5,
    CYCLES_6 = 6,
    CYCLES_CONTINUOUS = 7
};

// ===== DEBOUNCE NASTAVITVE =====
#define DEBOUNCE_DELAY_MS   50  // Anti-bounce za tipke

// ===== GPIO IZHODI =====
// Releji (preko 2N7002 tranzistorjev)
#define OUT_MOTOR_KAMEN     10  // GPIO10 - Kontaktor trifaznega motorja brusnega kamna
#define OUT_VENTIL_NOZ      21  // GPIO21 - Pnevmatski ventil za premik noža
#define OUT_CRPALKA         47  // GPIO47 - Črpalka vode

// Motor vretena - MD13S (Cytron) krmilnik
#define OUT_SPINDLE_DIR     11  // GPIO11 - Smer motorja vretena
#define OUT_SPINDLE_PWM     12  // GPIO12 - PWM za hitrost motorja vretena

// ===== PWM NASTAVITVE =====
#define PWM_FREQUENCY       20000   // 20 kHz za gladko delovanje motorja
#define PWM_RESOLUTION      8       // 8-bit (0-255)
#define PWM_CHANNEL_SPINDLE 0       // LEDC kanal za vreteno

// ===== MOTOR NASTAVITVE =====
#define SPINDLE_SPEED_MIN   0       // Minimalna hitrost (0-255)
#define SPINDLE_SPEED_MAX   255     // Maksimalna hitrost (0-255)
#define SPINDLE_SPEED_SLOW  100     // Počasna hitrost
#define SPINDLE_SPEED_MEDIUM 180    // Srednja hitrost
#define SPINDLE_SPEED_FAST  255     // Hitra hitrost

// Smeri motorja vretena
enum SpindleDirection {
    SPINDLE_DOWN = 0,   // Vreteno se spušča
    SPINDLE_UP = 1      // Vreteno se dviga
};

#endif // CONFIG_H
