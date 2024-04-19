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
 * @file    main.c
 * @brief   ADC demo application
 * @details Continuously monitors the ADC channels
 */

/***** Includes *****/
#include <stdio.h>
#include <stdint.h>
#include <mxc.h>
#include "example_config.h"



/***** Globals *****/
//#define COLLECT_DATA

#ifdef USE_INTERRUPTS
#define OST_CLOCK_SOURCE MXC_TMR_8K_CLK // \ref mxc_tmr_clock_t
#define OST_FREQ 1 // (Hz)
#define OST_TIMER MXC_TMR5 // Can be MXC_TMR0 through MXC_TMR5

#define FREQ 10 // (Hz)
#define DUTY_CYCLE 50 // (%)

volatile unsigned int adc_done = 0;
volatile unsigned int adc_val = 0;
volatile unsigned int beat = 0;
#endif

unsigned int sensor_buf[4096] = {};//buffer to hold sensor values
unsigned int buf_index = 0;
int avg = 0;

/***** Functions *****/
void OneshotTimer();

#ifdef USE_INTERRUPTS
void OneshotTimerHandler()
{
    // Clear interrupt
    MXC_TMR_ClearFlags(OST_TIMER);
    adc_done++;
    // Clear interrupt
    if (MXC_TMR5->wkfl & MXC_F_TMR_WKFL_A) {
        MXC_TMR5->wkfl = MXC_F_TMR_WKFL_A;

        //printf("Oneshot timer expired!\n");
    }
    if (adc_done < 2) OneshotTimer();//Restarting timer
}

void OneshotTimer()
{
    for (int i = 0; i < 5000; i++) {}
    //Button debounce

    // Declare variables
    mxc_tmr_cfg_t tmr;
    uint32_t periodTicks = MXC_TMR_GetPeriod(OST_TIMER, OST_CLOCK_SOURCE, 1, OST_FREQ);
    /*
    Steps for configuring a timer for PWM mode:
    1. Disable the timer
    2. Set the prescale value
    3  Configure the timer for continuous mode
    4. Set polarity, timer parameters
    5. Enable Timer
    */

    MXC_TMR_Shutdown(OST_TIMER);

    tmr.pres = TMR_PRES_1;
    tmr.mode = TMR_MODE_ONESHOT;
    tmr.bitMode = TMR_BIT_MODE_32;
    tmr.clock = OST_CLOCK_SOURCE;
    tmr.cmp_cnt = periodTicks; //SystemCoreClock*(1/interval_time);
    tmr.pol = 0;

    if (MXC_TMR_Init(OST_TIMER, &tmr, true) != E_NO_ERROR) {
        printf("Failed one-shot timer Initialization.\n");
        return;
    }

    MXC_TMR_EnableInt(OST_TIMER);

    // Enable wkup source in Poower seq register
    MXC_LP_EnableTimerWakeup(OST_TIMER);
    // Enable Timer wake-up source
    MXC_TMR_EnableWakeup(OST_TIMER, &tmr);

    //printf("Oneshot timer started.\n\n");
    LED_Toggle(LED_RED);//Turning on red led

    MXC_TMR_Start(OST_TIMER);
}

volatile int buttonPressed;
void buttonHandler(void *pb)
{
    MXC_NVIC_SetVector(TMR5_IRQn, OneshotTimerHandler);
    NVIC_EnableIRQ(TMR5_IRQn);

    buttonPressed = 1;
}

void setTrigger(int waitForTrigger)
{
    int tmp;

    buttonPressed = 0;

    if (waitForTrigger) {
        while (!buttonPressed) {}
    }

    // Debounce the button press.
    for (tmp = 0; tmp < 0x80000; tmp++) {
        __NOP();
    }
    adc_done = FALSE;
}

void adc_complete_cb(void *req, int error)
{
    //printf("conversion\n");
    adc_done = 1;
    return;
}

void ADC_IRQHandler(void)
{
    if (MXC_ADC->intr & (MXC_F_ADC_INTR_LO_LIMIT_IF /*| MXC_F_ADC_INTR_HI_LIMIT_IF*/)){
        MXC_ADC->intr |= MXC_F_ADC_INTR_LO_LIMIT_IF;//Clearing flags
        //MXC_ADC->intr |= MXC_F_ADC_INTR_HI_LIMIT_IF;

        beat++;
    }
    adc_val = MXC_ADC->data;
    MXC_ADC->intr |= MXC_F_ADC_INTR_DONE_IF;//Clearing ADC done flag
}
#endif

void convert(){
    MXC_ADC->ctrl |= MXC_F_ADC_CTRL_START;//Starting ADC converison. Interrupt will be called once done
    return;
}


int main(void)
{
    printf("\n******************** ADC Example ********************\n");
    printf("\nADC readings are taken on ADC channel %d every 10ms\n", ADC_CHANNEL);
    printf("and are subsequently printed to the terminal.\n\n");
        
    MXC_ADC->ctrl &= ~MXC_F_ADC_CTRL_CLK_EN;//Disabling clock
    MXC_GCR->pclkdiv |= MXC_F_GCR_PCLKDIV_ADCFRQ;//Setting ADC clock freq to 3,333,333(See user manual for options)
    MXC_GCR->pclkdis0 &= ~MXC_F_GCR_PCLKDIS0_ADC;//Clearing peripheral clock disable reg
    MXC_ADC->ctrl |= MXC_F_ADC_CTRL_CLK_EN;//Enabling clock
    MXC_ADC->intr |= MXC_F_ADC_INTR_REF_READY_IF;//Clearing ADC ref ready interrupt flag
    MXC_ADC->intr |= MXC_F_ADC_INTR_REF_READY_IE;//Enabling ref ready interrupt flag
    MXC_ADC->ctrl &= ~MXC_F_ADC_CTRL_REF_SEL;//Setting 1.22V internal reference
    MXC_ADC->ctrl |= MXC_F_ADC_CTRL_PWR;//Turning adc on
    MXC_ADC->ctrl |= MXC_F_ADC_CTRL_REFBUF_PWR;//Turning on internal ref buffer
    while(!(MXC_ADC->intr & MXC_F_ADC_INTR_REF_READY_IF)){}//Waiting for internal reference to power on
    MXC_ADC->intr |= MXC_F_ADC_INTR_REF_READY_IF;//Clearing ADC ref ready interrupt flag
    
    //Conversion init
    MXC_ADC->ctrl |= MXC_S_ADC_CTRL_CH_SEL_AIN3;//Selecting AIN3
    MXC_ADC->ctrl &= ~MXC_F_ADC_CTRL_SCALE;//No scale
    MXC_ADC->ctrl &= ~MXC_F_ADC_CTRL_REF_SCALE;//No scale
    MXC_ADC->ctrl &= ~MXC_F_ADC_CTRL_DATA_ALIGN;//LSB data alignment
    MXC_ADC->intr |= MXC_F_ADC_INTR_DONE_IF;//Clearing ADC done interrupt flags
    MXC_ADC->intr |= MXC_F_ADC_INTR_DONE_IE;//Enabling ADC done interrupt

    /* Set up LIMIT0 to monitor high and low trip points */
    while (MXC_ADC->status & (MXC_F_ADC_STATUS_ACTIVE | MXC_F_ADC_STATUS_AFE_PWR_UP_ACTIVE)) {}
    MXC_ADC_SetMonitorChannel(MXC_ADC_MONITOR_3, ADC_CHANNEL);
    MXC_ADC_SetMonitorHighThreshold(MXC_ADC_MONITOR_3, 0);
    MXC_ADC_SetMonitorLowThreshold(MXC_ADC_MONITOR_3, 250);
    MXC_ADC_EnableMonitor(MXC_ADC_MONITOR_3);
    
#ifdef USE_INTERRUPTS
    NVIC_EnableIRQ(ADC_IRQn);
#endif

    mxc_gpio_cfg_t leads_off;
    leads_off.port = MXC_GPIO0;
    leads_off.mask = MXC_GPIO_PIN_5;
    leads_off.pad = MXC_GPIO_PAD_NONE;
    leads_off.func = MXC_GPIO_FUNC_IN;
    MXC_GPIO_Config(&leads_off);//Init LO-
    leads_off.mask = MXC_GPIO_PIN_6;
    MXC_GPIO_Config(&leads_off);//Init LO+

    mxc_gpio_cfg_t SDN;
    SDN.port = MXC_GPIO0;
    SDN.mask = MXC_GPIO_PIN_8;
    SDN.pad = MXC_GPIO_PAD_NONE;
    SDN.func = MXC_GPIO_FUNC_OUT;
    SDN.vssel = MXC_GPIO_VSSEL_VDDIOH;
    SDN.drvstr = MXC_GPIO_DRVSTR_0;
    MXC_GPIO_Config(&SDN);//Init SDN(Allows for low power shutdown mode when not in use)

    MXC_GPIO_OutSet(MXC_GPIO0, MXC_GPIO_PIN_8);

    PB_RegisterCallback(1, buttonHandler);
    while (1) {//Starting program loop

    #ifdef COLLECT_DATA
        if (buf_index){//Data is in buffer
            buf_index = 0;
            printf("---COUGH---\n");
            while(sensor_buf[buf_index]){
                printf("%d\n", sensor_buf[buf_index++]);
            }
            printf("---END---\n");
            
            //Clearing buffer
            buf_index = 0;
            for (int i = 0; i < sizeof(sensor_buf)/sizeof(unsigned int); i++){
                sensor_buf[i] = 0;
            }
        }
    #else

    #endif

        /* Convert channel 0 */
#ifdef USE_INTERRUPTS
    #ifdef COLLECT_DATA
        setTrigger(1);//Waiting for button press
        OneshotTimer();//Starting timer
        while (adc_done < 2){//Trying to get about 2 seconds of sample data
            convert();//Starting conversion
            MXC_Delay(MSEC(1));//Delay to allow for oneshot interrupt to intervene
            sensor_buf[buf_index++] = adc_val;//Saving data
        }
        LED_Toggle(LED_RED);
    #else
        convert();//Starting conversion
        printf("%d\n", adc_val);//Printing value

        //Leads detection
        if (MXC_GPIO_InGet(MXC_GPIO0, MXC_GPIO_PIN_5)){//LO-
            printf("LO- not connected\n");
        } else if (MXC_GPIO_InGet(MXC_GPIO0, MXC_GPIO_PIN_6)){//LO+
            printf("LO+ not connected\n");
        }

        MXC_Delay(MSEC(15));//Runs every 15ms
    #endif
        
    #endif//USE_INTERRUPTS
        /*if (beat){
            beat = 0;
            printf("1 beat detected\n");
        }*/
        
        /* Determine if programmable limits on AIN0 were exceeded */
        /*if (MXC_ADC_GetFlags() & (MXC_F_ADC_INTR_LO_LIMIT_IF)) {
            printf(" %s Limit on AIN3 ",
                   (MXC_ADC_GetFlags() & MXC_F_ADC_INTR_LO_LIMIT_IF) ? "Low" : "High");
            MXC_ADC_ClearFlags(MXC_F_ADC_INTR_LO_LIMIT_IF | MXC_F_ADC_INTR_HI_LIMIT_IF);
        } else {
            printf("                   ");
        }*/

        //printf("\n");

        /* Delay for 1/4 second before next reading */
        //MXC_TMR_Delay(MXC_TMR0, MSEC(10));
    }
}
