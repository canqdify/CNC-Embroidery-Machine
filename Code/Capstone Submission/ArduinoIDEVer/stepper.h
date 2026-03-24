// stepper.h
#pragma once
#ifndef stepper_h
#define stepper_h

/////////////////////////////////////////////////////////////////////////////
// Author:        Claire Neilson
// Descr:         Controls stepper motor using pico pio
/////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

#ifndef LED_DELAY_MS
#define LED_DELAY_MS 250
#endif

#include "hardware/pio.h" 

// useful math macros 
#define BIT(n)  (1u<<(n)) // displays number as bit in corresponding position
#define STEP(n) (100 * n) // input distance in mm, converts to number of steps, halfstepping 1 mm = 100 steps

//for my sanity
#define MOTOR_PIO pio0

#define MOTOR_STEP_PIN_X 0 // pico w pin 1
#define MOTOR_DIR_PIN_X 1 // pico w pin2
#define MOTOR_SM_X 0
#define MOTOR_DREQ_X DREQ_PIO0_TX0 //pio0 sm0 tx fifo

#define MOTOR_STEP_PIN_Y 2 // pico w pin 4
#define MOTOR_DIR_PIN_Y 3 // pico w pin 5
#define MOTOR_SM_Y 1
#define MOTOR_DREQ_Y DREQ_PIO0_TX1 //pio0 sm1 tx fifo

typedef struct Motor Motor;
struct Motor 
{
    uint steps;
    bool dir;
    uint gpio_pin_dir;
    uint gpio_pin_step;
    PIO pio;
    uint pio_sm;
    uint dma_chan;
    uint dma_dreq;
};

volatile extern bool *xReady;
volatile extern bool *yReady;

extern Motor *xMotor;
extern Motor *yMotor;

/////////////////////////////////////////////////////////////////////////////
// Library Prototypes
/////////////////////////////////////////////////////////////////////////////

void gpioInit(void);
void pioInitXY(void); 
void moveMotor(Motor *motor);
void dmaInitXY(void);

void abort(void);

int pico_led_init(void);
void pico_set_led(bool led_on);

/////////////////////////////////////////////////////////////////////////////
// Hidden Helpers (local to implementation only)
/////////////////////////////////////////////////////////////////////////////
#endif