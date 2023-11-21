#define ADC_CHANNEL MXC_ADC_CH_3 //ECG uses channel 3 for ADC input

#define I2C_MASTER MXC_I2C1 ///< I2C instance (Featherboard)
#define I2C_FREQ 100000 ///< I2C clock frequency

void initI2C();//Function to initialize I2C communication
void initADC();//Function to initialize ADC
void convertADC();//Function to start ADC conversion