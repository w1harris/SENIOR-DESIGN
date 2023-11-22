#include "sensors.h"
#include <mxc.h>

volatile unsigned int beats;//Variable to keep track of heart beats

void initI2C(){
    MXC_I2C_Init(I2C_MASTER, 1, 0);
    //MXC_I2C_SetFrequency(I2C_MASTER, I2C_FREQ);Already set in Init function (100kHz)
    initIMU();
    return;
}

void initIMU(){
    uint8_t tx_buf[1] = {WHO_AM_I};//Transmit buffer
    uint8_t rx_buf[1] = {0};//Receive buffer

    mxc_i2c_req_t reqMaster;//Controlling I2C master registers

    reqMaster.i2c = I2C_MASTER;
    reqMaster.addr = IMU_AccelGyro_ADDR;
    reqMaster.tx_buf = tx_buf;
    reqMaster.tx_len = sizeof(tx_buf); 
    reqMaster.rx_buf = rx_buf;
    reqMaster.rx_len = 1;
    reqMaster.restart = 1;

    if (!MXC_I2C_MasterTransaction(&reqMaster)){
        if(rx_buf[0] != IMU_AccelGyro_WHOAMI){
            printf("Accel & Gyro I2C ERROR\n");
        }
    }

    reqMaster.addr = IMU_Mag_ADDR;
    if (!MXC_I2C_MasterTransaction(&reqMaster)){
        if(rx_buf[0] != IMU_Mag_WHOAMI){
            printf("Magnometer I2C ERROR\n");
        }
    }
}

void I2C_SCAN(){//Called from CLI-commands.c from I2CScan command
    u_int8_t address;
    int numDevices = 0;

    mxc_i2c_req_t reqMaster;//Controlling I2C master registers
    reqMaster.i2c = I2C_MASTER;
    reqMaster.tx_buf = NULL;
    reqMaster.tx_len = 0;
    reqMaster.rx_buf = NULL;
    reqMaster.rx_len = 0;
    reqMaster.restart = 0;
    reqMaster.callback = NULL;

    for (address = 1; address < 127; address++){
        reqMaster.addr = address;//Updating address
        if ((MXC_I2C_MasterTransaction(&reqMaster)) == 0) {
            printf("Found I2C device at 0x%02X\n", address);
            numDevices++;
        }
    }

    printf("%d total devices found\n", numDevices);
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
    MXC_ADC->ctrl |= 0x3UL << MXC_F_ADC_CTRL_CH_SEL_POS;//Selecting AIN3
    MXC_ADC->ctrl &= ~MXC_F_ADC_CTRL_SCALE;//No scale
    MXC_ADC->ctrl &= ~MXC_F_ADC_CTRL_REF_SCALE;//No scale
    MXC_ADC->ctrl &= ~MXC_F_ADC_CTRL_DATA_ALIGN;//LSB data alignment
    MXC_ADC->intr |= MXC_F_ADC_INTR_DONE_IF;//Clearing ADC done interrupt flag
    MXC_ADC->intr |= MXC_F_ADC_INTR_DONE_IE;//Enabling ADC done interrupt

    /* Set up LIMIT3 to monitor low trip points */
    while (MXC_ADC->status & (MXC_F_ADC_STATUS_ACTIVE | MXC_F_ADC_STATUS_AFE_PWR_UP_ACTIVE)) {}
    MXC_ADC_SetMonitorChannel(MXC_ADC_MONITOR_3, ADC_CHANNEL);
    MXC_ADC_SetMonitorHighThreshold(MXC_ADC_MONITOR_3, 0);
    MXC_ADC_SetMonitorLowThreshold(MXC_ADC_MONITOR_3, 250);
    MXC_ADC_EnableMonitor(MXC_ADC_MONITOR_3);
    return;
}

void convertADC(){
    MXC_ADC->ctrl |= MXC_F_ADC_CTRL_START;//Starting ADC converison. Interrupt will be called once done
    return;
}

//ADC interrupt
void ADC_IRQHandler(void)
{//May want to configure this later to use high and low limit to save CPU time
    if (MXC_ADC->intr & (MXC_F_ADC_INTR_LO_LIMIT_IF /*| MXC_F_ADC_INTR_HI_LIMIT_IF*/)){
        MXC_ADC->intr |= MXC_F_ADC_INTR_LO_LIMIT_IF;//Clearing flags
        beats++;//Incrementing beats
    }
    MXC_ADC->intr |= MXC_F_ADC_INTR_DONE_IF;//Clearing ADC done flag
}