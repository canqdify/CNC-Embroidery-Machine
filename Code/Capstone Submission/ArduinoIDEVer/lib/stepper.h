#pragma once
#ifndef stepper_h
#define stepper_h

/////////////////////////////////////////////////////////////////////////////
// Author:        Claire Neilson
// Descr:         Controls stepper motor (Arduino-compatible header)
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Macros and Constants
/////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>

// useful math macros 
#define BIT(n)  (1u << (n))
#define STEP(n) (100 * (n))

// -------------------------------------------------------------
// Motor pins — Arduino version
// (You can change these in your .cpp or from the .ino)
// -------------------------------------------------------------
#define MOTOR_STEP_PIN_X 0
#define MOTOR_DIR_PIN_X  1

#define MOTOR_STEP_PIN_Y 2
#define MOTOR_DIR_PIN_Y  3

// -------------------------------------------------------------
// Motor structure (hardware-agnostic)
// -------------------------------------------------------------
typedef struct Motor Motor;
struct Motor 
{
    uint32_t steps;
    bool dir;
    uint8_t gpio_pin_dir;
    uint8_t gpio_pin_step;

    // backend-specific fields (used by the .cpp)
    volatile uint32_t *words;
    volatile bool ready;
};

// -------------------------------------------------------------
// Public globals (defined in .cpp)
// -------------------------------------------------------------
extern volatile bool *xReady;
extern volatile bool *yReady;

extern Motor *xMotor;
extern Motor *yMotor;

/////////////////////////////////////////////////////////////////////////////
// Library Prototypes
/////////////////////////////////////////////////////////////////////////////

void gpioInit(void);
void initTimers(void);        // replaces pioInitXY + dmaInitXY
void moveMotor(Motor *motor);
void abort(void);

// optional LED helpers for Arduino
void pico_set_led(bool led_on);

#endif
