#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "stepper.h"
// #include "hardware/pio.h" 
#include "../build/stepper/stepper.pio.h" // built in stepper cmake
#include "hardware/dma.h"
#include "hardware/irq.h"

/////////////////////////////////////////////////////////////////////////////
// local prototypes
/////////////////////////////////////////////////////////////////////////////

void pio_handler(void);
void dma_handler(void);
uint32_t *getWords(uint steps);

/////////////////////////////////////////////////////////////////////////////
// library variables
/////////////////////////////////////////////////////////////////////////////

//TODO: confirm this number
float freq = 1000.0; 
                     

volatile bool xPioReady = false;
volatile bool yPioReady = false;
// volatile bool drPioReady = false;

volatile bool xDmaReady = false;
volatile bool yDmaReady = false;

Motor xMotor_s = {0, true, MOTOR_DIR_PIN_X, MOTOR_STEP_PIN_X, MOTOR_PIO, MOTOR_SM_X, 0, MOTOR_DREQ_X};
Motor yMotor_s = {0, true, MOTOR_DIR_PIN_Y, MOTOR_STEP_PIN_Y, MOTOR_PIO, MOTOR_SM_Y, 0, MOTOR_DREQ_Y};

uint32_t *xWords; //array of words to send
uint32_t *yWords;

//public pointer variables
volatile bool *xReady = &xPioReady; // public state machine flags
volatile bool *yReady = &yPioReady; // public state machine flags

Motor *xMotor = &xMotor_s; // public motor
Motor *yMotor = &yMotor_s; // public motor

/////////////////////////////////////////////////////////////////////////////
// constants
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// function implementations
/////////////////////////////////////////////////////////////////////////////

/// @brief sets up step, dir, and AWO pins as outputs for motor control
void gpioInit(void)
{
    gpio_init_mask(
        BIT(MOTOR_DIR_PIN_X)  | 
        BIT(MOTOR_STEP_PIN_X) | 
        BIT(MOTOR_DIR_PIN_Y)  | 
        BIT(MOTOR_STEP_PIN_Y) | 
        BIT(MOTOR_AWO_PIN)
    );

    gpio_set_dir_out_masked(
        BIT(MOTOR_DIR_PIN_X)  | 
        BIT(MOTOR_STEP_PIN_X) | 
        BIT(MOTOR_DIR_PIN_Y)  | 
        BIT(MOTOR_STEP_PIN_Y) | 
        BIT(MOTOR_AWO_PIN)
    );
}

/// @brief Inits pio and state machines for both x and y motors, enables them together
void pioInitXY(void)
{
    //set up x pio
    xMove_program_init(
        MOTOR_PIO, 
        MOTOR_SM_X, 
        pio_add_program(MOTOR_PIO, &xMove_program), 
        MOTOR_STEP_PIN_X, 
        freq
    );
    
    //set up y pio
    yMove_program_init(
        MOTOR_PIO, 
        MOTOR_SM_Y, 
        pio_add_program(MOTOR_PIO, &yMove_program), 
        MOTOR_STEP_PIN_Y, 
        freq
    );

    //enable xy pios together
    pio_enable_sm_mask_in_sync(MOTOR_PIO, BIT(MOTOR_SM_X) | BIT(MOTOR_SM_Y));

    //enable interrupts together
    pio_set_irq0_source_mask_enabled(MOTOR_PIO, BIT(pis_interrupt0) | BIT(pis_interrupt1), true);
    irq_set_exclusive_handler(PIO0_IRQ_0, pio_handler);
    irq_set_enabled(PIO0_IRQ_0, true);

    xPioReady = true;
    yPioReady = true;
}

/// @brief Clears pio irq flag, fires every time 32 bits have moved through osr
void pio_handler(void)
{
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
    if (motor->steps < 1) return; //why bother with everything if I don't need to

    if(motor == &xMotor_s) //is there a better way to chec?
    {
        if(!(xPioReady && xDmaReady)) return; // sanity check, this may bite me in the ass

        xWords = getWords(motor->steps);
        dma_channel_set_read_addr(motor->dma_chan, xWords, false); //not yet
        
        //clear ready flags, stuff is happening
        xPioReady = false;
        xDmaReady = false;
    }

    if(motor == &yMotor_s)
    {
        if(!(yPioReady && yDmaReady)) return; // sanity check, this may bite me in the ass
        yWords = getWords(motor->steps);
        dma_channel_set_read_addr(motor->dma_chan, yWords, false); //not yet
        
        //clear ready flags, stuff is happening
        yPioReady = false;
        yDmaReady = false;
    }

    dma_channel_set_trans_count(motor->dma_chan, (motor->steps / 32) + 1, true); //and GO
}

/// @brief  Init DMA
/// dma stuff https://github.com/raspberrypi/pico-examples/blob/master/dma
void dmaInitXY(void)
{
    // default config (the parts I care about)
    // read incr: true
    // write incr: false
    // dreq: DREQ_FORCE
    // data size: 32

    //x init
    xMotor_s.dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cx = dma_channel_get_default_config(xMotor_s.dma_chan);
    channel_config_set_dreq(&cx, xMotor_s.dma_dreq);
    
    dma_channel_configure(
        xMotor_s.dma_chan,
        &cx,
        &pio0_hw->txf[xMotor_s.pio_sm],
        NULL,
        0,
        false
    );

    //y init
    yMotor_s.dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cy = dma_channel_get_default_config(yMotor_s.dma_chan);
    channel_config_set_dreq(&cy, yMotor_s.dma_dreq);

    dma_channel_configure(
        yMotor_s.dma_chan,
        &cy,
        &pio0_hw->txf[yMotor_s.pio_sm],
        NULL,
        0,
        false);

    //dma irq init, sharing irq0 for both
    dma_set_irq0_channel_mask_enabled(xMotor_s.dma_chan | yMotor_s.dma_chan, true);

    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    xDmaReady = true;
    yDmaReady = true;
}

/// @brief clears flag for dma motor channel x or y, and frees allocated memory of words arr
void dma_handler(void) 
{
    if (dma_hw->ints0 & BIT(xMotor_s.dma_chan))
    {
        dma_hw->ints0 |= BIT(xMotor_s.dma_chan);
        free(xWords); // everything has been sent, deallocate memory
        xDmaReady = true; // dma done transfer, waits until pio is done
    }

    if(dma_hw->ints0 & BIT(yMotor_s.dma_chan))
    {
        dma_hw->ints0 |= BIT(yMotor_s.dma_chan);
        free(yWords); // everything has been sent, deallocate memory
        yDmaReady = true; // dma done transfer, waits until pio is done
    }
}

/// @brief creates array of words for transfer 
/// @param steps 
/// @return reference to array of delay values for 1 stitch
uint32_t *getWords(uint steps)
{
    int s_a;
    int s_b;
    float prev = 0;
    float num;

    uint32_t *arr = malloc((steps) * sizeof(uint32_t)); 
    if (arr == NULL) panic_unsupported(); //run screaming for the hills (end execution with msg unsupported)

    // get displacement sections a, b, and c
    s_a = steps / 4; // sa = sc
    s_b = s_a * 2;

    //ensure sections are balanced and that the correct number of steps are executed
    if (steps % 4 == 1) s_b += 1;
    else if (steps % 4 == 2) s_a += 1;
    else if (steps % 4 == 3)
    {
        s_a += 1;
        s_b += 1;
    }

    /**************************************************************
        Must include math.h, update CMake accordingly
    ***************************************************************/
   
    /* 
        in case I want to do this on multiple lines
        a = (-9*steps)/(4*pow(pow(STITCH_RATE, -1)))
        b = (9*steps)/(2*pow(STITCH_RATE, -1))
        c = (-5/4)*steps -i
        num = (-1*(b) + sqrt(pow((b),2)-4*(a)*(c)))/(2*(a)) 
    */

    // fill array with delay values
    for(int i = 0; i < steps; i++)
    {
        if (i < s_a)
            num = (float)(sqrt((4 * pow(pow(STITCH_RATE, -1), 2) * i) / 9 * steps)) - prev;
        else if (i < s_a + s_b)
            num = (float)(((2 * pow(STITCH_RATE, -1) * i) / (3 * steps)) + (steps / 4)) - prev;
        else // deeply ugly quadratic
            num = (float)(-1 * ((9 * steps) / (2 * pow(STITCH_RATE, -1))) + sqrt(pow(((9 * steps) / (2 * pow(STITCH_RATE, -1))), 2) - 4 * ((-9 * steps) / (4 * pow(pow(STITCH_RATE, -1)))) * ((-5 / 4) * steps - i))) / (2 * ((-9 * steps) / (4 * pow(pow(STITCH_RATE, -1))))) - prev;

        prev = num;
        arr[x] = (int)(round(num / 0.000005)) - 2;
        arr[x] = arr[x] > 255 ? 255 : arr[x]; // Caps delay at 8-bit value
    }

    return arr;
}

/// @brief programmatic stop, kills all stitching and resets to ready
/// @param state addr of the program's state machine
void abort_stitching(Stitching_State *state) //in progress
{
    //TODO
    // include driver motor when that stuff is setup

    // raise AWO first so motors stop while process is spinning down
    gpio_put(MOTOR_AWO_PIN, true);

    // nobody is ready, clear ready bools... maybe? 
    // xPioReady = yPioReady = xDmaReady = yDmaReady = false; 

    //abort dma transfer    
    dma_set_irq0_channel_mask_enabled(xMotor_s.dma_chan | yMotor_s.dma_chan, false); //disable interrupts to prevent misfires
    
    dma_channel_abort(xMotor_s.dma_chan); // blocks until done
    dma_channel_acknowledge_irq0(xMotor_s.dma_chan);

    dma_channel_abort(yMotor_s.dma_chan);
    dma_channel_acknowledge_irq0(yMotor_s.dma_chan);

    dma_set_irq0_channel_mask_enabled(xMotor_s.dma_chan | yMotor_s.dma_chan, true); // reenable interrupts

    // pio abort
    pio_set_sm_mask_enabled(MOTOR_PIO, BIT(MOTOR_SM_X) | BIT(MOTOR_SM_Y), false); // stops running state machines
    
    // drain fifos, want OSR to be empty
    pio_sm_drain_tx_fifo(MOTOR_PIO, BIT(MOTOR_SM_X));  
    pio_sm_drain_tx_fifo(MOTOR_PIO, BIT(MOTOR_SM_Y));

    //reset pio to known state
    pio_restart_sm_mask(MOTOR_PIO, BIT(MOTOR_SM_X) | BIT(MOTOR_SM_Y)); 

    // restart pios
    pio_enable_sm_mask_in_sync(MOTOR_PIO, BIT(MOTOR_SM_X) | BIT(MOTOR_SM_Y)); // might not need this since clkdivs alreaddy sync'd. Can't hurt

    // clear AWO so motors can move
    gpio_put(MOTOR_AWO_PIN, false);

    // reset ready bools
    xPioReady = yPioReady = xDmaReady = yDmaReady = true; 

    // set state to waiting
    *state = e_ready;
}

/// @brief From pico examples for testing
/// https://github.com/raspberrypi/pico-examples/blob/master/blink/blink.c
/// @return 0 if successful
int pico_led_init(void) {
#if defined(PICO_DEFAULT_LED_PIN)
    // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
    // so we can use normal GPIO functionality to turn the led on and off
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // For Pico W devices we need to initialise the driver etc
    return cyw43_arch_init();
#endif
}

/// @brief blink blink
/// https://github.com/raspberrypi/pico-examples/blob/master/blink/blink.c
/// @param led_on on/off t/f
void pico_set_led(bool led_on) {
#if defined(PICO_DEFAULT_LED_PIN)
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // Ask the wifi "driver" to set the GPIO on or off
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#endif
}