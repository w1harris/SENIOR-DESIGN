#define bleUART_PORT (2) //BLE chip will connect to UART2
#define UART_BAUD 9600 //Defining the baud rate to be used
#define BUFF_SIZE 256 //Size of the tx & rx buffers

//Function to initialize UART communication with BLE chip
void initBLE();
//Function to issue commands to BLE
void exeCommand(char *command);

void DMA_Handler(void);
void UART_Handler(void);
void readCallback(mxc_uart_req_t *req, int error);
int uart_bluetooth(int classification);
int send_data(char *data);