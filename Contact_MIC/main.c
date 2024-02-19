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
#ifdef USE_INTERRUPTS
volatile unsigned int adc_done = 0;
volatile unsigned int adc_val = 0;
#endif

/***** Functions *****/

#ifdef USE_INTERRUPTS
void adc_complete_cb(void *req, int error)
{
    //printf("conversion\n");
    adc_done = 1;
    return;
}

void ADC_IRQHandler(void)
{
    adc_val = MXC_ADC->data; //Saving ADC value
    MXC_ADC->intr |= MXC_F_ADC_INTR_DONE_IF;//Clearing ADC done flag
}
#endif

void convert(){
    MXC_ADC->ctrl |= MXC_F_ADC_CTRL_START;//Starting ADC converison. Interrupt will be called once done
    return;
}


int main(void)
{
    printf("\n******************** Contact MIC test project ********************\n");
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
    MXC_ADC->ctrl |= 0x3UL << MXC_F_ADC_CTRL_CH_SEL_POS;//Selecting AIN3
    MXC_ADC->ctrl &= ~MXC_F_ADC_CTRL_SCALE;//No scale
    MXC_ADC->ctrl &= ~MXC_F_ADC_CTRL_REF_SCALE;//No scale
    MXC_ADC->ctrl &= ~MXC_F_ADC_CTRL_DATA_ALIGN;//LSB data alignment
    MXC_ADC->intr |= MXC_F_ADC_INTR_DONE_IF;//Clearing ADC done interrupt flag
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

    while (1) {//Starting program loop

        /* Flash LED when starting ADC cycle */
        LED_On(LED_RED);
        MXC_TMR_Delay(MXC_TMR0, MSEC(10));
        LED_Off(LED_RED);

        /* Convert channel 0 */
#ifdef USE_INTERRUPTS
        //adc_done = 0;
        //MXC_ADC_StartConversionAsync(ADC_CHANNEL, adc_complete_cb);
        
        convert();//Starting conversion
        
        //while (!adc_done) {}
#else
        MXC_ADC_StartConversion(ADC_CHANNEL);
#endif

        /* Display results on OLED display, display asterisk if overflow */
        
        printf("%d\n", adc_val);
        
        if (adc_val > 850) printf("High movement detected\n");
        else if (adc_val > 250) printf("Movement detected\n");

        /* Delay for 1/4 second before next reading */
        MXC_TMR_Delay(MXC_TMR0, MSEC(10));
    }
}
