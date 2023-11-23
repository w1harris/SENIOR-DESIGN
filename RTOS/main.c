/******************************************************************************
 * Copyright (C) 2023 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated
 * Products, Inc. shall not be used except as stated in the Maxim Integrated
 * Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all
 * ownership rights.
 *
 ******************************************************************************/

/**
 * @file        main.c
 * @brief       FreeRTOS Example Application.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <mxc.h>
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "portmacro.h"
#include "task.h"
#include "semphr.h"
#include "FreeRTOS_CLI.h"
#include "mxc_device.h"
#include "wut.h"
#include "uart.h"
#include "lp.h"
#include "led.h"
#include "board.h"
#include "sensors.h"
#include "tasks.h"

/* FreeRTOS+CLI */
void vRegisterCLICommands(void);

/* Mutual exclusion (mutex) semaphores */
SemaphoreHandle_t xGPIOmutex;

/* Task IDs */
TaskHandle_t cmd_task_id;

/* Enables/disables tick-less mode */
unsigned int disable_tickless = 1;

/* Stringification macros */
#define STRING(x) STRING_(x)
#define STRING_(x) #x

/* Console ISR selection */
#if (CONSOLE_UART == 0)
#define UARTx_IRQHandler UART0_IRQHandler
#define UARTx_IRQn UART0_IRQn
mxc_gpio_cfg_t uart_cts = { MXC_GPIO0, MXC_GPIO_PIN_2, MXC_GPIO_FUNC_IN, MXC_GPIO_PAD_WEAK_PULL_UP,
                            MXC_GPIO_VSSEL_VDDIOH };
mxc_gpio_cfg_t uart_rts = { MXC_GPIO0, MXC_GPIO_PIN_3, MXC_GPIO_FUNC_OUT, MXC_GPIO_PAD_NONE,
                            MXC_GPIO_VSSEL_VDDIOH };
#else
#error "Please update ISR macro for UART CONSOLE_UART"
#endif
mxc_uart_regs_t *ConsoleUART = MXC_UART_GET_UART(CONSOLE_UART);

mxc_gpio_cfg_t uart_cts_isr;

#define CMD_LINE_BUF_SIZE 80
#define OUTPUT_BUF_SIZE 512

//BLE GPIO
extern mxc_uart_regs_t *bleUART;

mxc_gpio_cfg_t bleboot = {MXC_GPIO0, MXC_GPIO_PIN_19, MXC_GPIO_FUNC_OUT, MXC_GPIO_PAD_WEAK_PULL_UP, MXC_GPIO_VSSEL_VDDIOH};//GPIO pin to control BLE power

//GPIO for 306 esp32
mxc_gpio_cfg_t bleEN = {MXC_GPIO1, MXC_GPIO_PIN_6, MXC_GPIO_FUNC_OUT, MXC_GPIO_PAD_WEAK_PULL_DOWN, MXC_GPIO_VSSEL_VDDIOH};//Enables the ble

/* Defined in freertos_tickless.c */
extern void wutHitSnooze(void);

/***** Functions *****/
/* =| UART0_IRQHandler |======================================
 *
 * This function overrides the weakly-declared interrupt handler
 *  in system_max326xx.c and is needed for asynchronous UART
 *  calls to work properly
 *
 * ===========================================================
 */
void UARTx_IRQHandler(void)
{
    MXC_UART_AsyncHandler(ConsoleUART);
    wutHitSnooze();
}

/* =| WUT_IRQHandler |==========================
 *
 * Interrupt handler for the wake up timer.
 *
 * =======================================================
 */
void WUT_IRQHandler(void)
{
    MXC_WUT_IntClear();
    NVIC_ClearPendingIRQ(WUT_IRQn);
}

/* =| main |==============================================
 *
 * This program demonstrates FreeRTOS tasks, mutexes,
 *  and the FreeRTOS+CLI extension.
 *
 * =======================================================
 */
int main(void)
{
    /* Delay to prevent bricks */
    volatile int i;
    for (i = 0; i < 0xFFFFFF; i++) {}

    /* Setup manual CTS/RTS to lockout console and wake from deep sleep */
    MXC_GPIO_Config(&uart_cts);
    MXC_GPIO_Config(&uart_rts);

    //bleON GPIO
    MXC_UART_Init(bleUART, 115200, 2);//Enabling UART
    
    MXC_GPIO_Config(&bleEN);
    MXC_GPIO_OutClr(bleEN.port, bleEN.mask);//Setting power output to low
    
    MXC_GPIO_Config(&bleboot);
    MXC_GPIO_OutSet(bleboot.port, bleboot.mask);

    /* Enable incoming characters */
    MXC_GPIO_OutClr(uart_rts.port, uart_rts.mask);

    //ADC Setup
    initADC();

    //I2C Master Setup
    initI2C();

    /* Print banner (RTOS scheduler not running) */
    printf("\n-=- %s FreeRTOS (%s) Demo -=-\n", STRING(TARGET), tskKERNEL_VERSION_NUMBER);
    printf("SystemCoreClock = %d\n", SystemCoreClock);

    /* Create mutexes */
    xGPIOmutex = xSemaphoreCreateMutex();
    if (xGPIOmutex == NULL) {
        printf("xSemaphoreCreateMutex failed to create a mutex.\n");
    } else {
        /* Configure task */
        if ((xTaskCreate(vTickTockTask, (const char *)"TickTock", 2 * configMINIMAL_STACK_SIZE,
                         NULL, tskIDLE_PRIORITY + 2, NULL) != pdPASS) ||
            (xTaskCreate(vCmdLineTask, (const char *)"CmdLineTask",
                         configMINIMAL_STACK_SIZE + CMD_LINE_BUF_SIZE + OUTPUT_BUF_SIZE, NULL,
                         tskIDLE_PRIORITY + 1, &cmd_task_id) != pdPASS) ||
            (xTaskCreate(vADCTask, (const char*)"ADCTask", 4 * configMINIMAL_STACK_SIZE, NULL,
                         tskIDLE_PRIORITY + 2, NULL) != pdPASS)) {
            printf("xTaskCreate() failed to create a task.\n");
        } else {
            /* Start scheduler */
            printf("Starting scheduler.\n");
            NVIC_EnableIRQ(ADC_IRQn);//Enabling ADC ISR
            vTaskStartScheduler();
        }
    }

    /* This code is only reached if the scheduler failed to start */
    printf("ERROR: FreeRTOS did not start due to above error!\n");
    while (1) {
        __NOP();
    }

    /* Quiet GCC warnings */
    return -1;
}
