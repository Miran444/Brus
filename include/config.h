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
#define IN_S43_SAFETY       0   // S43 - Varnostno končno stikalo vretena v spodnjem položaju (0°)
#define IN_S42_GOR          1   // S42 - Tipka vreteno gor
#define IN_S41_DOL          2   // S41 - Tipka vreteno dol
#define IN_S45_STEVEC       3   // S45 - Števec obratov motorja vretena
#define IN_S44_NAKLON       4   // S44 - Stikalo naklona (< 10°)
#define IN_S1_ROCNO         5   // S1 - Ročno
#define IN_S1_AVTOMATSKO    6   // S1 - Avtomatsko
#define IN_RESET            7   // Reset tipka

// Drugi SN65HVS882 (IN8-IN15) - S2 preklopnik za cikle
#define IN_S2_NEPREKINJEN   8   // S2 - Neprekinjeni cikli
#define IN_S2_7_CIKLOV      9   // S2 - 7 ciklov
#define IN_S2_6_CIKLOV      10  // S2 - 6 ciklov
#define IN_S2_5_CIKLOV      11  // S2 - 5 ciklov
#define IN_S2_4_CIKLI       12  // S2 - 4 cikli
#define IN_S2_3_CIKLI       13  // S2 - 3 cikli
#define IN_S2_2_CIKLA       14  // S2 - 2 cikla
#define IN_USER             15  // User vhod - rezerva za prihodnost

// ===== ENUMERACIJE =====
enum S1Mode {
    MODE_OFF = 0,
    MODE_MANUAL = 1,
    MODE_AUTO = 2
};

enum S2Cycles {
    CYCLES_NONE = 0,
    CYCLES_2 = 2,
    CYCLES_3 = 3,
    CYCLES_4 = 4,
    CYCLES_5 = 5,
    CYCLES_6 = 6,
    CYCLES_7 = 7,
    CYCLES_CONTINUOUS = 99
};

// ===== DEBOUNCE NASTAVITVE =====
#define DEBOUNCE_DELAY_MS   50  // Anti-bounce za tipke

// ===== I2C PINS za AS5600 Magnetic Encoder =====
#define I2C_SDA         8       // GPIO8 - SDA
#define I2C_SCL         9       // GPIO9 - SCL
#define I2C_FREQUENCY   400000  // 400 kHz (fast mode)

// ===== AS5600 NASTAVITVE =====
#define AS5600_ADDRESS      0x36    // I2C adresa AS5600
#define USE_AS5600_FOR_TILT true    // true = uporabi AS5600, false = uporabi S44 stikalo
#define TILT_ANGLE_THRESHOLD 10.0   // Kot pri katerem se aktivira "tilt" signal (stopinje)
#define ANGLE_HYSTERESIS     0.5    // Histereza za preprečitev tresenja (stopinje)

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

// ===== AVTOMATSKI CIKEL - STANJA =====
enum CycleState {
    CYCLE_IDLE = 0,         // Neaktivno
    CYCLE_STARTING,         // Začetek cikla - vklop izhodov
    CYCLE_DOWN,             // Vreteno se spušča do < 10°
    CYCLE_UP,               // Vreteno se dviga nazaj
    CYCLE_COMPLETE,         // Cikel končan
    CYCLE_ERROR             // Napaka
};

// ===== AVTOMATSKI CIKEL - PARAMETRI =====
#define AUTO_SPINDLE_SPEED      180     // Hitrost vretena v avtomatskem načinu
#define STARTUP_DELAY_MS        1000    // Zakasnitev ob začetku cikla (motor kamna, črpalka)
#define TILT_DEBOUNCE_MS        100     // Debounce za stikalo naklona

#endif // CONFIG_H
