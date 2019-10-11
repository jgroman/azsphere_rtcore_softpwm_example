/***************************************************************************//**
* @file    main.c
* @version 1.0.0
* @authors Microsoft
* @authors Jaroslav Groman
*
* @par Project Name
*      Azure Sphere Software PWM for RTCore App
*
* @par Description
*    .
*
* @par Target device
*    Azure Sphere MT3620
*
* @par Related hardware
*    Avnet Azure Sphere Starter Kit
*
* @par Code Tested With
*    1. Silicon: Avnet Azure Sphere Starter Kit
*    2. IDE: Visual Studio 2017
*    3. SDK: Azure Sphere SDK Preview
*
* @par Notes
*    .
*
*******************************************************************************/

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "mt3620-baremetal.h"
#include "mt3620-timer.h"
#include "mt3620-timer-user.h"
#include "mt3620-gpio.h"

extern uint32_t StackTop; // &StackTop == end of TCM

#define GPIO_RGBLED_RED     8
#define GPIO0               0
#define COUNTER_PWM_MAX     32


static const int counter_pwm_on[] = {0, 1, 2, 8, 16, 24, 31, 32};
static const int num_pwm_counters = 
                            sizeof(counter_pwm_on) / sizeof(counter_pwm_on[0]);
static const int buttonAGpio = 12;
static const int buttonPressCheckPeriodMs = 10;

static int index_counter_pwm = 0;


static void
handle_irq_pwm_timer(void);

static void 
handle_irq_button_timer(void);

static _Noreturn void 
RTCoreMain(void);

static _Noreturn void 
DefaultExceptionHandler(void);



// ARM DDI0403E.d SB1.5.2-3
// From SB1.5.3, "The Vector table must be naturally aligned to a power of two 
// whose alignment value is greater than or equal to (Number of Exceptions 
// supported x 4), with a minimum alignment of 128 bytes.". The array is 
// aligned in linker.ld, using the dedicated section ".vector_table".

// The exception vector table contains a stack pointer, 15 exception handlers, 
// and an entry for each interrupt.

#define INTERRUPT_COUNT 100 // from datasheet
#define EXCEPTION_COUNT (16 + INTERRUPT_COUNT)
#define INT_TO_EXC(i_) (16 + (i_))

const uintptr_t ExceptionVectorTable[EXCEPTION_COUNT] 
__attribute__((section(".vector_table")))
__attribute__((used)) = {
    [0] = (uintptr_t)&StackTop,                // Main Stack Pointer (MSP)
    [1] = (uintptr_t)RTCoreMain,               // Reset
    [2] = (uintptr_t)DefaultExceptionHandler,  // NMI
    [3] = (uintptr_t)DefaultExceptionHandler,  // HardFault
    [4] = (uintptr_t)DefaultExceptionHandler,  // MPU Fault
    [5] = (uintptr_t)DefaultExceptionHandler,  // Bus Fault
    [6] = (uintptr_t)DefaultExceptionHandler,  // Usage Fault
    [11] = (uintptr_t)DefaultExceptionHandler, // SVCall
    [12] = (uintptr_t)DefaultExceptionHandler, // Debug monitor
    [14] = (uintptr_t)DefaultExceptionHandler, // PendSV
    [15] = (uintptr_t)DefaultExceptionHandler, // SysTick

    [INT_TO_EXC(0)] = (uintptr_t)DefaultExceptionHandler,
    [INT_TO_EXC(1)] = (uintptr_t)Gpt_HandleIrq1,
    [INT_TO_EXC(2)... INT_TO_EXC(INTERRUPT_COUNT - 1)] = 
        (uintptr_t)DefaultExceptionHandler};

static _Noreturn void 
DefaultExceptionHandler(void)
{
    for (;;) {
        // empty.
    }
}


static void
start_pwm_timer(uint32_t counter)
{
    // GPT timer doesn't work for counter values 0 and 1

    if (counter == 0)
    {
        // Skip timer, start IRQ handler
        handle_irq_pwm_timer();
    }
    else if (counter == 1)
    {
        // Wait approx 80 usec
        for (uint32_t i = 0; i < 180; i++)
        {
            // Empty block
        }

        // Start IRQ handler
        handle_irq_pwm_timer();
    }
    else
    {
        // Start timer normally
        Gpt_LaunchTimer32k(TimerGpt0, counter, handle_irq_pwm_timer);
    }
}


static void
handle_irq_pwm_timer(void)
{
    static bool b_is_pwm_gpio_high = true;

    // Calculate this pulse duration counter
    uint32_t counter = (b_is_pwm_gpio_high) ?
        counter_pwm_on[index_counter_pwm] : 
        COUNTER_PWM_MAX - counter_pwm_on[index_counter_pwm];

    if (counter > 0)
    {
        // Do not flip output GPIO if requested pulse duration is 0
        Mt3620_Gpio_Write(GPIO_RGBLED_RED, b_is_pwm_gpio_high);
        Mt3620_Gpio_Write(GPIO0, b_is_pwm_gpio_high);
    }

    b_is_pwm_gpio_high = !b_is_pwm_gpio_high;

    start_pwm_timer(counter);
}

static void 
handle_irq_button_timer(void)
{
    // Assume initial state is high, i.e. button not pressed.
    static bool prevState = true;
    bool newState;
    Mt3620_Gpio_Read(buttonAGpio, &newState);

    if (newState != prevState) 
    {
        bool pressed = !newState;
        if (pressed) 
        {
            // Select next index to PWM table
            index_counter_pwm = (index_counter_pwm + 1) % num_pwm_counters;
            // Restart PWM timer with new data
            start_pwm_timer(counter_pwm_on[index_counter_pwm]);
        }
        prevState = newState;
    }

    Gpt_LaunchTimerMs(TimerGpt1, buttonPressCheckPeriodMs, 
        handle_irq_button_timer);
}

static _Noreturn void 
RTCoreMain(void)
{
    // SCB->VTOR = ExceptionVectorTable
    WriteReg32(SCB_BASE, 0x08, (uint32_t)ExceptionVectorTable);

    Gpt_Init();

    // Block includes GPIO0.
    static const GpioBlock pwm0 = {
        .baseAddr = 0x38010000,
        .type = GpioBlock_PWM,
        .firstPin = 0,
        .pinCount = 4
    };

    Mt3620_Gpio_AddBlock(&pwm0);
    Mt3620_Gpio_ConfigurePinForOutput(GPIO0);

    // Block includes GPIO_RGBLED_RED: GPIO8.
    static const GpioBlock pwm2 = {
        .baseAddr = 0x38030000, 
        .type = GpioBlock_PWM, 
        .firstPin = 8, 
        .pinCount = 4
    };

    Mt3620_Gpio_AddBlock(&pwm2);
    Mt3620_Gpio_ConfigurePinForOutput(GPIO_RGBLED_RED);

    // Block includes buttonAGpio, GPIO12
    static const GpioBlock grp3 = {
        .baseAddr = 0x38040000, 
        .type = GpioBlock_GRP, 
        .firstPin = 12, 
        .pinCount = 4
    };

    Mt3620_Gpio_AddBlock(&grp3);
    Mt3620_Gpio_ConfigurePinForInput(buttonAGpio);

    // Start PWM timer
    start_pwm_timer(counter_pwm_on[index_counter_pwm]);

    // Start button check timer
    Gpt_LaunchTimerMs(TimerGpt1, buttonPressCheckPeriodMs, 
        handle_irq_button_timer);

    for (;;) {
        __asm__("wfi");
    }
}

/* [] END OF FILE */
