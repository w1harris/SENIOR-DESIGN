#include <stdint.h>
#include "mxc.h"

#define ADC_CHANNEL MXC_ADC_CH_3 //ECG uses channel 3 for ADC input

#define I2C_MASTER MXC_I2C1 ///< I2C instance (Featherboard)
#define I2C_FREQ 100000 ///< I2C clock frequency

//IMU DEFINES (Pulled from LSMDS1 github driver)
/** I2C Device Address 8 bit format  if SA0=0 -> 0xD5 if SA0=1 -> 0xD7 **/
#define IMU_I2C_ADD_L      0xD5U
#define IMU_I2C_ADD_H      0xD7U

/** I2C Device Address 8 bit format  if SA0=0 -> 0x3D if SA0=1 -> 0x39 **/
#define MAG_I2C_ADD_L      0x39U
#define MAG_I2C_ADD_H      0x3DU

#define IMU_AccelGyro_ADDR (0x6BU)
#define IMU_Mag_ADDR (0x1E)
#define IMU_AccelGyro_WHOAMI (0x68U) //Checking accelerometer & gyrometer I2C communication
#define IMU_Mag_WHOAMI (0x3DU) //Checking Magnetometer I2C communication

//IMU accelerometer macros
#define IMU_Accel_Range2G  (0b00000000)
#define IMU_Accel_Range4G  (0b00010000)
#define IMU_Accel_Range8G  (0b00011000)
#define IMU_Accel_Range16G (0b00001000)

// Linear Acceleration: mg per LSB
#define IMU_Accel_MG_LSB_2G (0.061F)
#define IMU_Accel_MG_LSB_4G (0.122F)
#define IMU_Accel_MG_LSB_8G (0.244F)
#define IMU_Accel_MG_LSB_16G (0.732F)

//IMU gyroscope macros
#define IMU_Gyro_Range245  (0b00000000)
#define IMU_Gyro_Range500  (0b00001000)
#define IMU_Gyro_Range2000 (0b00011000)
#define IMU_Gyro_LPower    (0b10000000)

// Angular Rate: dps per LSB
#define IMU_Gyro_DPS_DIGIT_245DPS (0.00875F)
#define IMU_Gyro_DPS_DIGIT_500DPS (0.01750F)
#define IMU_Gyro_DPS_DIGIT_2000DPS (0.07000F)

//Accel & Gyro ODR low power modes
#define IMU_AccelGyro_ODR14  (0b00100000)
#define IMU_AccelGyro_ODR59  (0b01000000)
#define IMU_AccelGyro_ODR119 (0b01100000)

//IMU Magnetometer macros
#define IMU_Mag_Range4Gauss  (0b00000000)
#define IMU_Mag_Range8Gauss  (0b00100000)
#define IMU_Mag_Range12Gauss (0b01000000)
#define IMU_Mag_Range16Gauss (0b01100000)
#define IMU_Mag_LPower       (0b00000000)
#define IMU_Mag_MedPerf      (0b00100000)
#define IMU_Mag_HighPerf     (0b01000000)
#define IMU_Mag_UltraPerf    (0b01100000)

// Magnetic Field Strength: gauss range
#define IMU_Mag_MGAUSS_4GAUSS (0.14F)
#define IMU_Mag_MGAUSS_8GAUSS (0.29F)
#define IMU_Mag_MGAUSS_12GAUSS (0.43F)
#define IMU_Mag_MGAUSS_16GAUSS (0.58F)

//-----------Register mapping-------------
//Linear accel & gyro
#define WHO_AM_I (0x0F) //IMU Identifier register
#define CTRL_REG1_G (0x10) //Angular rate sensor Control Register 1.
#define CTRL_REG2_G (0x11) //Angular rate sensor Control Register 2.
#define CTRL_REG3_G (0x12) //Angular rate sensor Control Register 3.
#define CTRL_REG5_XL (0x1F) //Linear acceleration sensor Control Register 5.
#define CTRL_REG6_XL (0x20) //Linear acceleration sensor Control Register 6.
#define CTRL_REG7_XL (0x21) //Linear acceleration sensor Control Register 7.
#define CTRL_REG8 (0x22) //Control register 8(Used to configure serial comm)
#define CTRL_REG9 (0x23) //Control register9(Sleep mode & memory control)
#define CTRL_REG10 (0x24) //Self test register
#define STATUS_REG (0x27) //IMU Status reg

//Magnetometer regs
#define CTRL_REG1_M (0x20)
#define CTRL_REG2_M (0x21)
#define CTRL_REG3_M (0x22)
#define CTRL_REG4_M (0x23)
#define CTRL_REG5_M (0x24)
#define STATUS_REG_M (0x27)

//Output regs
#define OUT_X_G   (0x18) //Angular rate sensor pitch axis (X) angular rate output register. The value is expressed as a 16-bit word in two’s complement.
#define OUT_Y_G   (0x1A) //Angular rate sensor roll axis (Y) angular rate output register. The value is expressed as a 16-bit word in two’s complement.
#define OUT_Z_G   (0x1C) //Angular rate sensor Yaw axis (Z) angular rate output register. The value is expressed as a 16-bit word in two’s complement.
#define OUT_X_XL  (0x28) //Linear acceleration sensor X-axis output register. The value is expressed as a 16-bit word in two’s complement
#define OUT_Y_XL  (0x2A) //Linear acceleration sensor Y-axis output register. The value is expressed as a 16-bit word in two’s complement
#define OUT_Z_XL  (0x2C) //Linear acceleration sensor Z-axis output register. The value is expressed as a 16-bit word in two’s complement.
#define OUT_X_L_M (0x28) //Magnetometer X-axis data output. The value of the magnetic field is expressed as two’s complement.
#define OUT_Y_L_M (0x2A) //Magnetometer Y-axis data output. The value of the magnetic field is expressed as two’s complement.
#define OUT_Z_L_M (0x2C) //Magnetometer Z-axis data output. The value of the magnetic field is expressed as two’s complement.

//Output reg states
#define XAXIS_L (0)
#define YAXIS_L (1)
#define ZAXIS_L (2)
#define XAXIS_G (3)
#define YAXIS_G (4)
#define ZAXIS_G (5)
#define XAXIS_M (6)
#define YAXIS_M (7)
#define ZAXIS_M (8)

typedef struct{
    uint8_t accelRange;    //Accelerometer range (2, 4, 8, 16)
    uint8_t gyroRange;     //Gyroscope range (245, 500, 2000)
    uint8_t magRange;      //Magnetometer range (4,8,12,16)
    uint8_t accelgyroODR;  //Holds accelerometer and gyro ODR setting
    uint8_t gyroPower;     //Specifies the magnetometer power mode
    uint8_t magPower;      //Specifies the magnetometer power mode

} IMU_ctrl_reg;

//Function to initialize I2C communication
void initI2C();
//Function to initialize IMU
void initIMU();
//Function to read data from IMU registers
int readIMU(uint8_t reg, uint8_t *data);
//Function to write data to IMU registers
int writeIMU(uint8_t reg, uint8_t value);
/*Function to get readings from the IMU.
*
* uint8_t magnetometer indicates if we want
* readings from the magnetometer */
void getIMU(uint8_t magnetometer);
//Function that scans I2C BUS
void I2C_SCAN();

//Function to initialize ADC
void initADC();
//Function to start ADC conversion
void convertADC();