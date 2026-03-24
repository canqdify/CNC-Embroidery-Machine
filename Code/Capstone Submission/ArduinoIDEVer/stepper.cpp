// stepper.cpp
#include "stepper.h"
#include "stepper.pio.h" // the pio header file 
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////
// local prototypes
/////////////////////////////////////////////////////////////////////////////

void pio_handler(void);
void dma_handler(void);
uint32_t *getWords(uint steps);

/////////////////////////////////////////////////////////////////////////////
// library variables
/////////////////////////////////////////////////////////////////////////////

float freq = 1000.0;

volatile bool xPioReady = false;
volatile bool yPioReady = false;

volatile bool xDmaReady = false;
volatile bool yDmaReady = false;

Motor xMotor_s = {0, true, MOTOR_DIR_PIN_X, MOTOR_STEP_PIN_X, MOTOR_PIO, MOTOR_SM_X, 0, MOTOR_DREQ_X};
Motor yMotor_s = {0, true, MOTOR_DIR_PIN_Y, MOTOR_STEP_PIN_Y, MOTOR_PIO, MOTOR_SM_Y, 0, MOTOR_DREQ_Y};

uint32_t *xWords = NULL; //array of words to send
uint32_t *yWords = NULL;

//public pointer variables
volatile bool *xReady = &xPioReady; // public state machine flags
volatile bool *yReady = &yPioReady; // public state machine flags

Motor *xMotor = &xMotor_s; // public motor
Motor *yMotor = &yMotor_s; // public motor

/////////////////////////////////////////////////////////////////////////////
// function implementations
/////////////////////////////////////////////////////////////////////////////

/// @brief sets up step and dir pins as outputs for motor control
void gpioInit(void)
{
    // Use Arduino/GPIO calls to initialize pins
    // set to outputs and initial low
    gpio_init(MOTOR_DIR_PIN_X);
    gpio_init(MOTOR_STEP_PIN_X);
    gpio_init(MOTOR_DIR_PIN_Y);
    gpio_init(MOTOR_STEP_PIN_Y);

    gpio_set_dir(MOTOR_DIR_PIN_X, GPIO_OUT);
    gpio_set_dir(MOTOR_STEP_PIN_X, GPIO_OUT);
    gpio_set_dir(MOTOR_DIR_PIN_Y, GPIO_OUT);
    gpio_set_dir(MOTOR_STEP_PIN_Y, GPIO_OUT);

    gpio_put(MOTOR_DIR_PIN_X, 0);
    gpio_put(MOTOR_STEP_PIN_X, 0);
    gpio_put(MOTOR_DIR_PIN_Y, 0);
    gpio_put(MOTOR_STEP_PIN_Y, 0);
}

/// @brief Inits pio and state machines for both x and y motors, enables them together
void pioInitXY(void)
{
    // add programs to the PIO and init the state machines with the pio_init helpers
    uint x_offset = pio_add_program(MOTOR_PIO, &xMove_program);
    xMove_program_init(MOTOR_PIO, MOTOR_SM_X, x_offset, MOTOR_STEP_PIN_X, freq);

    uint y_offset = pio_add_program(MOTOR_PIO, &yMove_program);
    yMove_program_init(MOTOR_PIO, MOTOR_SM_Y, y_offset, MOTOR_STEP_PIN_Y, freq);

    //enable xy pios together
    uint sm_mask = (1u << MOTOR_SM_X) | (1u << MOTOR_SM_Y);
    pio_enable_sm_mask_in_sync(MOTOR_PIO, sm_mask);

    //enable interrupts together
    // the pio_program uses irq wait 0 for x and irq wait 1 for y (as in the pio)
    pio_set_irq0_source_mask_enabled(MOTOR_PIO, BIT(pis_interrupt0) | BIT(pis_interrupt1), true);
    irq_set_exclusive_handler(PIO0_IRQ_0, pio_handler);
    irq_set_enabled(PIO0_IRQ_0, true);

    xPioReady = true;
    yPioReady = true;
}

/// @brief Clears pio irq flag, fires every time 32 bits have moved through osr
void pio_handler(void)
{
    // check which interrupt pending
    if (pio_interrupt_get(MOTOR_PIO, MOTOR_SM_X))
    {
        pio_interrupt_clear(MOTOR_PIO, MOTOR_SM_X);
        xPioReady = xDmaReady; // only sets ready flag if dma is done transfer
    }
    if (pio_interrupt_get(MOTOR_PIO, MOTOR_SM_Y))
    {
        pio_interrupt_clear(MOTOR_PIO, MOTOR_SM_Y);
        yPioReady = yDmaReady; // only sets ready flag if dma is done transfer
    }
}

/// @brief          Inits array of words, sets read addr, and starts dma stransfer to pio
/// @param motor    Reference to the motor object
void moveMotor(Motor *motor)
{
    if (motor->steps < 1) return; // nothing to do

    if (motor == &xMotor_s)
    {
        if(!(xPioReady && xDmaReady)) return; // sanity check

        xWords = getWords(motor->steps);
        dma_channel_set_read_addr(motor->dma_chan, xWords, false); // set read addr

        //clear ready flags, stuff is happening
        xPioReady = false;
        xDmaReady = false;
    }
    else if (motor == &yMotor_s)
    {
        if(!(yPioReady && yDmaReady)) return; // sanity check
        yWords = getWords(motor->steps);
        dma_channel_set_read_addr(motor->dma_chan, yWords, false); // set read addr

        //clear ready flags, stuff is happening
        yPioReady = false;
        yDmaReady = false;
    }
    else
    {
        // unknown motor pointer
        return;
    }

    // each 32-bit DMA transfer will move 32 bits out of the OSR (each word is 32 bits),
    // original code used (steps / 32) + 1; original PIO program packs 16 step pulses per 32-bit? keep same semantics:
    dma_channel_set_trans_count(motor->dma_chan, (motor->steps / 32) + 1, true); // start transfer
}

/// @brief  
/// dma stuff https://github.com/raspberrypi/pico-examples/blob/master/dma
void dmaInitXY(void)
{
    // x init
    xMotor_s.dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cx = dma_channel_get_default_config(xMotor_s.dma_chan);
    channel_config_set_dreq(&cx, xMotor_s.dma_dreq);
    channel_config_set_read_increment(&cx, true);
    channel_config_set_write_increment(&cx, false);
    channel_config_set_transfer_data_size(&cx, DMA_SIZE_32);

    dma_channel_configure(
        xMotor_s.dma_chan,
        &cx,
        &pio0_hw->txf[xMotor_s.pio_sm], // write address (pio FIFO)
        NULL,
        0,
        false
    );

    // y init
    yMotor_s.dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cy = dma_channel_get_default_config(yMotor_s.dma_chan);
    channel_config_set_dreq(&cy, yMotor_s.dma_dreq);
    channel_config_set_read_increment(&cy, true);
    channel_config_set_write_increment(&cy, false);
    channel_config_set_transfer_data_size(&cy, DMA_SIZE_32);

    dma_channel_configure(
        yMotor_s.dma_chan,
        &cy,
        &pio0_hw->txf[yMotor_s.pio_sm],
        NULL,
        0,
        false);

    //dma irq init, sharing irq0 for both
    dma_set_irq0_channel_mask_enabled((1u << xMotor_s.dma_chan) | (1u << yMotor_s.dma_chan), true);

    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    xDmaReady = true;
    yDmaReady = true;
}

/// @brief clears flag for dma motor channel x or y, and frees allocated memory of words arr
void dma_handler(void) 
{
    uint32_t ints = dma_hw->ints0;
    if (ints & (1u << xMotor_s.dma_chan))
    {
        dma_hw->ints0 = (1u << xMotor_s.dma_chan); // clear interrupt
        if (xWords) free(xWords); // free memory
        xWords = NULL;
        xDmaReady = true; // dma done transfer, waits until pio is done
    }

    if (ints & (1u << yMotor_s.dma_chan))
    {
        dma_hw->ints0 = (1u << yMotor_s.dma_chan);
        if (yWords) free(yWords);
        yWords = NULL;
        yDmaReady = true;
    }
}

/// @brief creates array of words for transfer 
/// @param steps 
/// @return reference to array
uint32_t *getWords(uint steps)
{
    int n = steps / 16; // can only send 32 bits but sending both on and off signal 
    uint32_t *arr = (uint32_t *)malloc((n + 1) * sizeof(uint32_t)); // size is n+1 for remainder 
    if (arr == NULL) {
        // replace panic_unsupported() with Serial error + infinite loop in Arduino environment - this is what's done?
        Serial.println("FATAL: malloc failed in getWords()");
        while (1) tight_loop_contents();
    }

    for (int i = 0; i < n; i++) arr[i] = 0xAAAAAAAA; //0b 1010 1010 etc

    // get remainder and fill last position
    int r = steps % 16;
    int s = 0; // remainder to be converted to binary steps
    for (int j = 0; j < r; j++) s += 2 << (2 * j); 

    arr[n] = (uint32_t)s;
    return arr;
}

/// @brief kills all stitching and resets to ready
void abort(void)
{

}
// dont
/// @brief From pico examples for testing
int pico_led_init(void) {
#if defined(PICO_DEFAULT_LED_PIN)
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
#elif defined(CYW43_WL_GPIO_LED_PIN)
    return cyw43_arch_init();
#else
    return PICO_ERROR_GENERIC;
#endif
}
/// @brief blink blink
void pico_set_led(bool led_on) {
#if defined(PICO_DEFAULT_LED_PIN)
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#endif
}