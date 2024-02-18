#define bleUART_PORT (2) //BLE chip will connect to UART2
#define UART_BAUD 115200 //Defining the baud rate to be used
#define BUFF_SIZE 256 //Size of the tx & rx buffers

//Function to initialize UART communication with BLE chip
void initBLE();
//Function to issue commands to BLE
void exeCommand(char *command);