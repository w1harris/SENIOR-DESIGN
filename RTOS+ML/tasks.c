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
#include "ML.h"

//FREERTOS Configs
extern unsigned int disable_tickless;
extern TaskHandle_t cmd_task_id;
//ECG
#define ADC_WAIT ((portTickType)15)//Used to make the ADC task wait 15 ticks
extern volatile unsigned int beats;
unsigned int oldBeat;
//Console UART
#define CMD_LINE_BUF_SIZE 80
#define OUTPUT_BUF_SIZE 512
#define UARTx_IRQn UART0_IRQn
//LEDS
extern SemaphoreHandle_t xGPIOmutex;
extern mxc_uart_regs_t *ConsoleUART;

void vTask0(void *pvParameters){
    TickType_t xLastWakeTime;
    unsigned int x = LED_OFF;

    /* Get task start time */
    xLastWakeTime = xTaskGetTickCount();

    while (1) {
        /* Protect hardware access with mutex
     *
     * Note: This is not strictly necessary, since MXC_GPIO_SetOutVal() is implemented with bit-band
     * access, which is inherently task-safe. However, for other drivers, this would be required.
     *
     */
        if (xSemaphoreTake(xGPIOmutex, portMAX_DELAY) == pdTRUE) {
            if (x == LED_OFF) {
                x = LED_ON;
            } else {
                x = LED_OFF;
            }
            /* Return the mutex after we have modified the hardware state */
            xSemaphoreGive(xGPIOmutex);
        }
        /* Wait 1 second until next run */
        vTaskDelayUntil(&xLastWakeTime, configTICK_RATE_HZ);
    }
}

void vTask1(void *pvParameters){
    TickType_t xLastWakeTime;
    unsigned int x = LED_ON;

    /* Get task start time */
    xLastWakeTime = xTaskGetTickCount();

    while (1) {
        /* Protect hardware access with mutex
     *
     * Note: This is not strictly necessary, since MXC_GPIO_SetOutVal() is implemented with bit-band
     * access, which is inherently task-safe. However, for other drivers, this would be required.
     *
     */
        if (xSemaphoreTake(xGPIOmutex, portMAX_DELAY) == pdTRUE) {
            if (x == LED_OFF) {
                LED_On(0);
                x = LED_ON;
            } else {
                LED_Off(0);
                x = LED_OFF;
            }
            /* Return the mutex after we have modified the hardware state */
            xSemaphoreGive(xGPIOmutex);
        }
        /* Wait 1 second until next run */
        vTaskDelayUntil(&xLastWakeTime, configTICK_RATE_HZ);
    }
}

void vTickTockTask(void *pvParameters){
    TickType_t ticks = 0;
    TickType_t xLastWakeTime;

    /* Get task start time */
    xLastWakeTime = xTaskGetTickCount();

    while (1) {
        ticks = xTaskGetTickCount();
        printf("Uptime is 0x%08x (%u seconds), tickless-idle is %s\n", ticks,
               ticks / configTICK_RATE_HZ, disable_tickless ? "disabled" : "ENABLED");
        vTaskDelayUntil(&xLastWakeTime, (configTICK_RATE_HZ * 10));
    }
}

void vIMUTask(void *pvParameters){
    TickType_t lastWakeTime = xTaskGetTickCount();//Storing current time

    while(1){
        getIMU(TRUE);
        vTaskDelayUntil(&lastWakeTime, configTICK_RATE_HZ/2);//Waiting 500 ticks
    }

}

void vADCTask(void *pvParameters){
    TickType_t lastWakeTime = xTaskGetTickCount();//Storing current time
    TickType_t ticks = 0;

    while(1){
        convertADC();
        ticks = xTaskGetTickCount();//Gets total number of ticks since system initialization
        if (oldBeat != beats){
            printf("Total beats: %d\n", beats);
            printf("Current bpm: %d\n", 60*beats/(ticks/configTICK_RATE_HZ));
            oldBeat = beats;
        }
        xTaskDelayUntil(&lastWakeTime, ADC_WAIT);//Delaying this task for adcFrequency(10 ticks)
    }
}

void vMLcontTask(void *pvParameters){
    vTaskSuspendAll();//Suspending all tasks to run ML model continuously

    while(1){
        runModel();
    }
}

void vMLTask(void *pvParameters){
    TickType_t lastWakeTime = xTaskGetTickCount();//Storing current time

    while(1){
        runModel();
        //May need to add a delay, currently just want to run the model until something else needs to be run
    }
}

void vCmdLineTask_cb(mxc_uart_req_t *req, int error){
    BaseType_t xHigherPriorityTaskWoken;

    /* Wake the task */
    xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(cmd_task_id, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void vCmdLineTask(void *pvParameters){
    unsigned char tmp;
    unsigned int index; /* Index into buffer */
    unsigned int x;
    int uartReadLen;
    char buffer[CMD_LINE_BUF_SIZE]; /* Buffer for input */
    char output[OUTPUT_BUF_SIZE]; /* Buffer for output */
    BaseType_t xMore;
    mxc_uart_req_t async_read_req;

    memset(buffer, 0, CMD_LINE_BUF_SIZE);
    index = 0;

    /* Register available CLI commands */
    vRegisterCLICommands();

    /* Enable UARTx interrupt */
    NVIC_ClearPendingIRQ(UARTx_IRQn);
    NVIC_DisableIRQ(UARTx_IRQn);
    NVIC_SetPriority(UARTx_IRQn, 1);
    NVIC_EnableIRQ(UARTx_IRQn);

    /* Async read will be used to wake process */
    async_read_req.uart = ConsoleUART;
    async_read_req.rxData = &tmp;
    async_read_req.rxLen = 1;
    async_read_req.txData = NULL;
    async_read_req.txLen = 0;
    async_read_req.callback = vCmdLineTask_cb;

    printf("\nEnter 'help' to view a list of available commands.\n");
    printf("cmd> ");
    fflush(stdout);
    while (1) {
        /* Register async read request */
        if (MXC_UART_TransactionAsync(&async_read_req) != E_NO_ERROR) {
            printf("Error registering async request. Command line unavailable.\n");
            vTaskDelay(portMAX_DELAY);
        }
        /* Hang here until ISR wakes us for a character */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        /* Check that we have a valid character */
        if (async_read_req.rxCnt > 0) {
            /* Process character */
            do {
                if (tmp == 0x08) {
                    /* Backspace */
                    if (index > 0) {
                        index--;
                        printf("\x08 \x08");
                    }
                    fflush(stdout);
                } else if (tmp == 0x03) {
                    /* ^C abort */
                    index = 0;
                    printf("^C");
                    printf("\ncmd> ");
                    fflush(stdout);
                } else if ((tmp == '\r') || (tmp == '\n')) {
                    printf("\r\n");
                    /* Null terminate for safety */
                    buffer[index] = 0x00;
                    /* Evaluate */
                    do {
                        xMore = FreeRTOS_CLIProcessCommand(buffer, output, OUTPUT_BUF_SIZE);
                        /* If xMore == pdTRUE, then output buffer contains no null termination, so
             *  we know it is OUTPUT_BUF_SIZE. If pdFALSE, we can use strlen.
             */
                        for (x = 0; x < (xMore == pdTRUE ? OUTPUT_BUF_SIZE : strlen(output)); x++) {
                            putchar(*(output + x));
                        }
                    } while (xMore != pdFALSE);
                    /* New prompt */
                    index = 0;
                    printf("\ncmd> ");
                    fflush(stdout);
                } else if (index < CMD_LINE_BUF_SIZE) {
                    putchar(tmp);
                    buffer[index++] = tmp;
                    fflush(stdout);
                } else {
                    /* Throw away data and beep terminal */
                    putchar(0x07);
                    fflush(stdout);
                }
                uartReadLen = 1;
                /* If more characters are ready, process them here */
            } while ((MXC_UART_GetRXFIFOAvailable(MXC_UART_GET_UART(CONSOLE_UART)) > 0) &&
                     (MXC_UART_Read(ConsoleUART, (uint8_t *)&tmp, &uartReadLen) == 0));
        }
    }
}