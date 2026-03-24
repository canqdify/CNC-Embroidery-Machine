// -------------------------------------------------------------
// Arduino PORT of your RP2040 PIO+DMA stepper engine
// -------------------------------------------------------------
// Uses hardware timer interrupts to emulate the PIO engine.
// -------------------------------------------------------------

#include <Arduino.h>

// -------------------------------------------------------------
// Motor structure (same shape as your original)
// -------------------------------------------------------------
struct Motor {
    uint32_t steps;
    bool dir;
    uint8_t dir_pin;
    uint8_t step_pin;
    volatile uint32_t *words;     // pointer to step pattern buffer
    volatile bool ready;
};

// -------------------------------------------------------------
// Pins (change to what your board uses)
// -------------------------------------------------------------
#define MOTOR_DIR_PIN_X   2
#define MOTOR_STEP_PIN_X  3
#define MOTOR_DIR_PIN_Y   4
#define MOTOR_STEP_PIN_Y  5

// -------------------------------------------------------------
// Motors
// -------------------------------------------------------------
Motor xMotor = {0, true, MOTOR_DIR_PIN_X, MOTOR_STEP_PIN_X, nullptr, true};
Motor yMotor = {0, true, MOTOR_DIR_PIN_Y, MOTOR_STEP_PIN_Y, nullptr, true};

// software timing
float freq = 10000.0;

// timers
hw_timer_t *timerX = nullptr;
hw_timer_t *timerY = nullptr;

// per-motor counters
volatile uint32_t xWordIndex = 0;
volatile uint32_t yWordIndex = 0;
volatile uint32_t xBitIndex  = 0;
volatile uint32_t yBitIndex  = 0;

// -------------------------------------------------------------
// Create step pattern (same logic as Pico code)
// -------------------------------------------------------------
uint32_t* getWords(uint32_t steps) {
    int n = steps / 16;
    uint32_t *arr = (uint32_t*) malloc((n + 1) * sizeof(uint32_t));
    if (!arr) return nullptr;

    for (int i = 0; i < n; i++) arr[i] = 0xAAAAAAAA;

    int r = steps % 16;
    int s = 0;
    for (int j = 0; j < r; j++) s += 2 << (2 * j);
    arr[n] = s;

    return arr;
}

// -------------------------------------------------------------
// TIMER ISR for X motor
// -------------------------------------------------------------
void IRAM_ATTR onTimerX() {
    if (!xMotor.words) return;

    uint32_t word = xMotor.words[xWordIndex];
    bool stepState = (word >> xBitIndex) & 0x01;
    digitalWrite(xMotor.step_pin, stepState);

    xBitIndex++;
    if (xBitIndex >= 32) {
        xBitIndex = 0;
        xWordIndex++;

        uint32_t wordCount = (xMotor.steps / 32) + 1;
        if (xWordIndex >= wordCount) {
            free((void*)xMotor.words);
            xMotor.words = nullptr;
            xMotor.ready = true;
            xWordIndex = 0;
        }
    }
}

// -------------------------------------------------------------
// TIMER ISR for Y motor
// -------------------------------------------------------------
void IRAM_ATTR onTimerY() {
    if (!yMotor.words) return;

    uint32_t word = yMotor.words[yWordIndex];
    bool stepState = (word >> yBitIndex) & 0x01;
    digitalWrite(yMotor.step_pin, stepState);

    yBitIndex++;
    if (yBitIndex >= 32) {
        yBitIndex = 0;
        yWordIndex++;

        uint32_t wordCount = (yMotor.steps / 32) + 1;
        if (yWordIndex >= wordCount) {
            free((void*)yMotor.words);
            yMotor.words = nullptr;
            yMotor.ready = true;
            yWordIndex = 0;
        }
    }
}

// -------------------------------------------------------------
// Move a motor (replacement for your DMA start)
// -------------------------------------------------------------
void moveMotor(Motor *m) {
    if (m->steps < 1) return;
    if (!m->ready) return;

    m->ready = false;

    m->words = getWords(m->steps);
    if (!m->words) {
        m->ready = true;
        return;
    }

    if (m == &xMotor) {
        xWordIndex = 0;
        xBitIndex  = 0;
    }
    if (m == &yMotor) {
        yWordIndex = 0;
        yBitIndex  = 0;
    }
}

// -------------------------------------------------------------
// Setup
// -------------------------------------------------------------
void setup() {
    pinMode(xMotor.dir_pin, OUTPUT);
    pinMode(xMotor.step_pin, OUTPUT);
    pinMode(yMotor.dir_pin, OUTPUT);
    pinMode(yMotor.step_pin, OUTPUT);

    digitalWrite(xMotor.dir_pin, xMotor.dir);
    digitalWrite(yMotor.dir_pin, yMotor.dir);

    uint32_t period_us = 1e6 / freq;

    // hardware timers (ESP32 has 4)
    timerX = timerBegin(0, 80, true);      // 1 MHz
    timerAttachInterrupt(timerX, &onTimerX, true);
    timerAlarmWrite(timerX, period_us, true);
    timerAlarmEnable(timerX);

    timerY = timerBegin(1, 80, true);
    timerAttachInterrupt(timerY, &onTimerY, true);
    timerAlarmWrite(timerY, period_us, true);
    timerAlarmEnable(timerY);
}

// -------------------------------------------------------------
// Example main loop
// -------------------------------------------------------------
void loop() {
    if (xMotor.ready) {
        xMotor.steps = 3200;   // example
        moveMotor(&xMotor);
    }

    if (yMotor.ready) {
        yMotor.steps = 1600;   // example
        moveMotor(&yMotor);
    }
}
