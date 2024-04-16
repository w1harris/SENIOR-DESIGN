#define ADC_CHANNEL MXC_ADC_CH_4
#define USE_INTERRUPTS

#include <stdio.h>
#include <stdint.h>
#include <mxc.h>

// Parameters for Continuous timer
#define CONT_FREQ 1 // (Hz)
#define CONT_TIMER MXC_TMR1 // Can be MXC_TMR0 through MXC_TMR5
#define CONT_CLOCK_SOURCE MXC_TMR_8M_CLK // \ref mxc_tmr_clock_t

/***** Globals *****/
#ifdef USE_INTERRUPTS
volatile unsigned int adc_stop = 0;
volatile unsigned int adc_val = 0;
mxc_tmr_cfg_t tmr;
#endif

#ifdef USE_INTERRUPTS
void adc_complete_cb(void *req, int error)
{
    //printf("conversion\n");
    //adc_done = 1;
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

void initADC(){
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
    MXC_ADC->ctrl |= MXC_S_ADC_CTRL_CH_SEL_AIN4;//Selecting AIN3
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
    //MXC_ADC_EnableMonitor(MXC_ADC_MONITOR_3);

    #ifdef USE_INTERRUPTS
        NVIC_EnableIRQ(ADC_IRQn);
    #endif

    initTMR();
}

void initTMR(){
    
    uint32_t periodTicks = MXC_TMR_GetPeriod(CONT_TIMER, CONT_CLOCK_SOURCE, 128, CONT_FREQ);

    /*
    Steps for configuring a timer for PWM mode:
    1. Disable the timer
    2. Set the prescale value
    3  Configure the timer for continuous mode
    4. Set polarity, timer parameters
    5. Enable Timer
    */

    MXC_TMR_Shutdown(CONT_TIMER);

    tmr.pres = TMR_PRES_128;
    tmr.mode = TMR_MODE_CONTINUOUS;
    tmr.bitMode = TMR_BIT_MODE_16B;
    tmr.clock = CONT_CLOCK_SOURCE;
    tmr.cmp_cnt = periodTicks; //SystemCoreClock*(1/interval_time);
    tmr.pol = 0;

    if (MXC_TMR_Init(CONT_TIMER, &tmr, true) != E_NO_ERROR) {
        printf("Failed Continuous timer Initialization.\n");
        return;
    } else MXC_TMR_Stop(&tmr);
    
    MXC_NVIC_SetVector(TMR1_IRQn, ContinuousTimerHandler);
    NVIC_EnableIRQ(TMR1_IRQn);
}

void ContinuousTimerHandler()
{
    // Clear interrupt
    MXC_TMR_ClearFlags(CONT_TIMER);
    LED_Toggle(LED1);
    adc_stop = TRUE;
}