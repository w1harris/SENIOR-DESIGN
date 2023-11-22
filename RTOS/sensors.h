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
#define IMU_Mag_WHOAMI (0x3DU) //Checking magnometer I2C communication

//Register mapping
#define WHO_AM_I (0x0F)
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
#define OUT_X_XL (0x28) //Linear accel X-axis output base addr [1:0]
#define OUT_Y_XL (0x2A) //Linear accel Y-axis output base addr [1:0]
#define OUT_Z_XL (0x2C) //Linear accel Z-axis output base addr [1:0]


void initI2C();//Function to initialize I2C communication
void initIMU();//Function to initialize IMU
void I2C_SCAN();//Function that scans I2C 

void initADC();//Function to initialize ADC
void convertADC();//Function to start ADC conversion