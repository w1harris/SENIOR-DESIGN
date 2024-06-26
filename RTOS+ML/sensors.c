#include "sensors.h"
#include <mxc.h>
#include <stdlib.h>
#include "max9867.h"
#include "ble.h"
#include <string.h>
#include "tasks.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

volatile unsigned int beats;//Variable to keep track of heart beats
volatile unsigned int CMic_Val;//Variable to store current ADC reading for CMic
volatile unsigned int ECG_Val;//Stores current ecg reading
unsigned int prev = 0;//Previous ecg value

extern volatile uint8_t ECG_ON;

mxc_i2c_req_t reqMaster;//Controlling I2C master registers
IMU_ctrl_reg imuSetting = {  //Holds current IMU settings
    IMU_Accel_Range2G, IMU_Gyro_Range245, IMU_Mag_Range4Gauss, //Setting default values
    IMU_AccelGyro_ODR14, IMU_Gyro_LPower, IMU_Mag_LPower
};

void initI2C(){
    MXC_I2C_Init(I2C_MASTER, 1, 0);
    
    //MXC_I2C_SetFrequency(I2C_MASTER, I2C_FREQ);Already set in Init function (100kHz)
    return;
}

/*void codec_init(void)
{
    if (max9867_init(MXC_I2C1, CODEC_MCLOCK, 1) != E_NO_ERROR)
        blink_halt("Error initializing MAX9867 CODEC");

    if (max9867_enable_playback(1) != E_NO_ERROR)
        blink_halt("Error enabling playback path");

    if (max9867_playback_volume(-6, -6) != E_NO_ERROR)
        blink_halt("Error setting playback volume");

    if (max9867_enable_record(1) != E_NO_ERROR)
        blink_halt("Error enabling record path");

    if (max9867_adc_level(-12, -12) != E_NO_ERROR)
        blink_halt("Error setting ADC level");

    if (max9867_linein_gain(-6, -6) != E_NO_ERROR)
        blink_halt("Error setting Line-In gain");
    else
        printf("Codec initialized successfully \n");
}*/

void initIMU(){
    uint8_t rx_buf[1] = {0};//Receive buffer

    reqMaster.i2c = I2C_MASTER;
    reqMaster.addr = IMU_AccelGyro_ADDR;
    reqMaster.restart = 0;

    //Checking for existance of IMU modules
    if (!readIMU(WHO_AM_I, rx_buf)){
        if(rx_buf[0] != IMU_AccelGyro_WHOAMI){
            printf("Accel & Gyro I2C ERROR\n");
        }
    } else printf("Accel & Gyro I2C ERROR\n");

    reqMaster.addr = IMU_Mag_ADDR;
    if (!readIMU(WHO_AM_I, rx_buf)){
        if(rx_buf[0] != IMU_Mag_WHOAMI){
            printf("Magnetometer I2C ERROR\n");
        }
    } else printf("Magnetometer I2C ERROR\n");

    //Configuring IMU control registers
    reqMaster.addr = IMU_AccelGyro_ADDR;
    readIMU(CTRL_REG6_XL, rx_buf);
    writeIMU(CTRL_REG6_XL, *rx_buf | imuSetting.accelRange);//Setting accelerometer mode

    readIMU(CTRL_REG1_G, rx_buf);
    writeIMU(CTRL_REG1_G, *rx_buf | imuSetting.gyroRange);//Setting gyroscope mode

    //Low power mode
    readIMU(CTRL_REG3_G, rx_buf);
    writeIMU(CTRL_REG3_G, *rx_buf | imuSetting.gyroPower);//Enabling gyroscope low power mode

    //Setting accelerometer & gyroscope ODR
    readIMU(CTRL_REG1_G, rx_buf);
    writeIMU(CTRL_REG1_G, *rx_buf | imuSetting.accelgyroODR);//Setting ODR to 14.9Hz

    reqMaster.addr = IMU_Mag_ADDR;//Changing to Magnetometer to edit regs
    readIMU(CTRL_REG2_M, rx_buf);
    writeIMU(CTRL_REG2_M, *rx_buf | imuSetting.magRange);//Setting Magnetometer mode

    readIMU(CTRL_REG1_M, rx_buf);
    writeIMU(CTRL_REG1_M, *rx_buf | imuSetting.magPower);//Setting Magnetometer performance mode

}

int readIMU(uint8_t reg, uint8_t *data){
    reqMaster.tx_buf = &reg;
    reqMaster.tx_len = 1;
    reqMaster.rx_buf = data;
    reqMaster.rx_len = 1;

    return MXC_I2C_MasterTransaction(&reqMaster);//Sending back return code from I2C communication
}

int writeIMU(uint8_t reg, uint8_t value){
    uint8_t tx_buf[2] = {reg, value};//transmit buffer. Send IMU reg address then value to write to address

    reqMaster.tx_buf = tx_buf;
    reqMaster.tx_len = sizeof(tx_buf);
    reqMaster.rx_len = 0;
    
    return MXC_I2C_MasterTransaction(&reqMaster);//Sending back return code from I2C communication
}

void getIMU(uint8_t magnetometer){
    char *data = calloc(50, sizeof(char));//Allocating space

    reqMaster.addr = IMU_AccelGyro_ADDR;
    int rx_len = (ZAXIS_G+1) * 2;
    uint8_t tx_buf[1] = {OUT_X_G};
    uint8_t *rx_buf = calloc(rx_len, sizeof(uint8_t));//rx buffer is ZAXIS * 2 because imu output addrs have 2, 8 bit regs
    uint8_t *buffer_base = rx_buf;

    reqMaster.tx_buf = tx_buf;
    reqMaster.tx_len = 1;
    reqMaster.rx_buf = rx_buf;
    reqMaster.rx_len = rx_len;
    
    if(!MXC_I2C_MasterTransaction(&reqMaster)){//IMU will automatically cycle through the Accel & Gyro output registers. All register values are stored in rx_buf
        float combined_buf[(rx_len/2) + 1];//Creating a combined buffer to hold the 16 bit register values

        for (int i = 0; i < rx_len/2; i++){//Assembling combined buffer
            combined_buf[i] = *rx_buf;//Storing LSBs
            rx_buf++;//Incrementing pointer
            combined_buf[i] = combined_buf[i] + (*rx_buf << 8);//Left shifting the MSBs
            rx_buf++;//Incrementing pointer
        }

        //Formatting output
        float mult = 0;
        switch(imuSetting.accelRange){
            case IMU_Accel_Range2G:
                mult = IMU_Accel_MG_LSB_2G;
                break;
            case IMU_Accel_Range4G:
                mult = IMU_Accel_MG_LSB_4G;
                break;
            case IMU_Accel_Range8G:
                mult = IMU_Accel_MG_LSB_8G;
                break;
            case IMU_Accel_Range16G:
                mult= IMU_Accel_MG_LSB_16G;
                break;
        }
        mult /= 1000;
        for (int i = 0; i < 3; i++) combined_buf[i] *= mult;

        switch(imuSetting.gyroRange){
            case IMU_Gyro_Range245:
                mult = IMU_Gyro_DPS_DIGIT_245DPS;
                break;
            case IMU_Gyro_Range500:
                mult = IMU_Gyro_DPS_DIGIT_500DPS;
                break;
            case IMU_Gyro_Range2000:
                mult = IMU_Gyro_DPS_DIGIT_2000DPS;
                break;
        }
        mult /= 1000;
        for (int i = 3; i < 6; i++) combined_buf[i] *= mult;
        //Done formatting Accel and Gyro output

        //Printing
        /*printf("---Gyroscope readings---\nX: %.2f dps\nY: %.2f dps\nZ: %.2f dps\n---Accelerometer readings---\nX: %.2f m/s^2\nY: %.2f m/s^2\nZ: %.2f m/s^2\n", 
        (double)combined_buf[0], (double)combined_buf[1],
        (double)combined_buf[2], (double)combined_buf[3], (double)combined_buf[4], (double)combined_buf[5]);
        */
        //Formatting string
        sprintf(data, "G: %.2f %.2f %.2f A: %.2f %.2f %.2f ", (double)combined_buf[0], (double)combined_buf[1], 
        (double)combined_buf[2], (double)combined_buf[3], (double)combined_buf[4], (double)combined_buf[5]);
        send_data(data);//Sending  Gyro + Accel data over ble

        if (magnetometer){//Magnetometer output register reads

            rx_buf = buffer_base;//Resetting buffer pointer
            reqMaster.addr = IMU_Mag_ADDR;//Changing IMU device address

            //Xaxis
            readIMU(OUT_X_L_M, rx_buf++);//Reading Xaxis address and shfting rx buffer
            readIMU(OUT_X_L_M+1, rx_buf++);//Reading Xaxis MSB address and shifting rx buffer

            //Yaxis
            readIMU(OUT_Y_L_M, rx_buf++);//Reading Yaxis address and shfting rx buffer
            readIMU(OUT_Y_L_M+1, rx_buf++);//Reading Yaxis MSB address and shifting rx buffer

            //Zaxis
            readIMU(OUT_Z_L_M, rx_buf++);//Reading Zaxis address and shfting rx buffer
            readIMU(OUT_Z_L_M+1, rx_buf++);//Reading Zaxis MSB address and shifting rx buffer
            

            rx_buf = buffer_base;//Resetting buffer pointer
            for (int i = 0; i < 3; i++){//Assembling combined buffer. Reusing the first 3 indexes so Gyro data getting overwritten
                combined_buf[i] = *rx_buf;//Storing LSBs
                rx_buf++;//Incrementing pointer
                combined_buf[i] = combined_buf[i] + (*rx_buf << 8);//Left shifting the MSBs
            }

            //Formatting output
            switch(imuSetting.gyroRange){
                case IMU_Mag_Range4Gauss:
                    mult = IMU_Mag_MGAUSS_4GAUSS;
                    break;
                case IMU_Mag_Range8Gauss:
                    mult = IMU_Mag_MGAUSS_8GAUSS;
                    break;
                case IMU_Mag_Range12Gauss:
                    mult = IMU_Mag_MGAUSS_12GAUSS;
                    break;
                case IMU_Mag_Range16Gauss:
                    mult = IMU_Mag_MGAUSS_16GAUSS;
                    break;
            }
            mult /= 1000;
            for (int i = 0; i < 3; i++) combined_buf[i] *= mult;
            //Done formatting magnetometer output

            //Printing
            /*printf("---Magnetometer readings---\nX: %.2f gauss\nY: %.2f gauss\nZ: %.2f gauss\n", 
            (double)combined_buf[0], (double)combined_buf[1], (double)combined_buf[3]);
            */
            memset(data, 0, 50);//Clearing data
            sprintf(data, "M: %.2f %.2f %.2fZ", (double)combined_buf[0], (double)combined_buf[1], (double)combined_buf[3]);
            send_data(data);//Transmitting mag data over ble
        }
        rx_buf = buffer_base; 
    } else printf("ERROR getting data\n");

    free(rx_buf);//Freeing buffer memory
    free(data);
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
    MXC_ADC->ctrl |= MXC_ADC_CH_4 << MXC_F_ADC_CTRL_CH_SEL_POS;//Selecting AIN4
    MXC_ADC->ctrl &= ~MXC_F_ADC_CTRL_SCALE;//No scale
    MXC_ADC->ctrl &= ~MXC_F_ADC_CTRL_REF_SCALE;//No scale
    MXC_ADC->ctrl &= ~MXC_F_ADC_CTRL_DATA_ALIGN;//LSB data alignment
    MXC_ADC->intr |= MXC_F_ADC_INTR_DONE_IF;//Clearing ADC done interrupt flag
    MXC_ADC->intr |= MXC_F_ADC_INTR_DONE_IE;//Enabling ADC done interrupt
    
    /* Set up LIMIT3 to monitor low trip points */
    while (MXC_ADC->status & (MXC_F_ADC_STATUS_ACTIVE | MXC_F_ADC_STATUS_AFE_PWR_UP_ACTIVE)) {}
    MXC_ADC_SetMonitorChannel(MXC_ADC_MONITOR_3, ADC_CHANNEL);
    MXC_ADC_SetMonitorHighThreshold(MXC_ADC_MONITOR_3, 0);
    MXC_ADC_SetMonitorLowThreshold(MXC_ADC_MONITOR_3, 485);
    MXC_ADC_EnableMonitor(MXC_ADC_MONITOR_3);

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

    MXC_GPIO_OutSet(MXC_GPIO0, MXC_GPIO_PIN_8);//Setting SDN
    return;
}

void convertADC(){
    MXC_ADC->ctrl |= MXC_F_ADC_CTRL_START;//Starting ADC converison. Interrupt will be called once done
    return;
}

//ADC interrupt
void ADC_IRQHandler(void)
{//May want to configure this later to use high and low limit to save CPU time
    /*if (MXC_ADC->intr & (MXC_F_ADC_INTR_LO_LIMIT_IF | MXC_F_ADC_INTR_HI_LIMIT_IF)){
        MXC_ADC->intr |= MXC_F_ADC_INTR_LO_LIMIT_IF;//Clearing flags
        beats++;//Incrementing beats
    }*/
    ECG_Val = MXC_ADC->data;//Storing contact mic value

    if (ECG_Val < prev*.96) beats++;

    prev = ECG_Val;
    MXC_ADC->intr |= MXC_F_ADC_INTR_DONE_IF;//Clearing ADC done flag
}

void DEMO(){
    ECG_ON = TRUE;

    //ECG task
    printf("Starting ECG readings\n");
    xTaskCreate(vADCTask, (const char*)"ADCTask", 4 * configMINIMAL_STACK_SIZE, NULL,tskIDLE_PRIORITY + 2, NULL);

    initIMU();

    printf("Starting IMU readings\n");
    if(!xTaskCreate(vIMUTask, (const char*)"IMUTask", 8 * configMINIMAL_STACK_SIZE, NULL,tskIDLE_PRIORITY + 1, NULL)){//Creating IMUTask
        printf("ERROR creating IMU task\n");
    }
}