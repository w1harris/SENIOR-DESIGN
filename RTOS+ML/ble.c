#include <mxc.h>
#include <stdlib.h>
#include "ble.h"
#include <string.h>
#include <strings.h>

#define DMA//Using DMA for ble uart transmission

extern mxc_gpio_cfg_t bleEN;

uint8_t ble_ON = FALSE;

volatile int DMA_FLAG;

mxc_uart_regs_t *bleUART = MXC_UART_GET_UART(bleUART_PORT);
mxc_uart_req_t reqUART;

uint8_t tx_buf[BUFF_SIZE];
uint8_t tx_pos = 0;//Stores current positon in transmit buffer
uint8_t rx_buf[BUFF_SIZE];
uint8_t rx_pos = 0;//Stores current position in receive buffer

void initBLE(){
    //Interrupts
    #ifdef DMA
        MXC_DMA_Init();
        MXC_DMA_ReleaseChannel(0);
        MXC_NVIC_SetVector(DMA0_IRQn, DMA_Handler);
        NVIC_EnableIRQ(DMA0_IRQn);
    #else
        NVIC_ClearPendingIRQ(MXC_UART_GET_IRQ(bleUART_PORT));
        NVIC_DisableIRQ(MXC_UART_GET_IRQ(bleUART_PORT));
        MXC_NVIC_SetVector(MXC_UART_GET_IRQ(bleUART_PORT), UART_Handler);
        NVIC_EnableIRQ(MXC_UART_GET_IRQ(bleUART_PORT));
    #endif
    //Initializing UART peripheral
    MXC_UART_Init(MXC_UART_GET_UART(2), UART_BAUD, MXC_UART_APB_CLK);

    //Clearing buffers
    memset(tx_buf, 0x0, BUFF_SIZE);
    memset(rx_buf, 0x0, BUFF_SIZE);
    
    strcpy(tx_buf, "AT\r\n");

    reqUART.uart = bleUART;
    reqUART.rxData = rx_buf;
    reqUART.rxLen = 0;
    reqUART.txData = tx_buf;
    reqUART.txLen = 4;//Transmitting 4 characters at the start
    tx_pos = reqUART.txLen;//Shifting tx buffer position
    reqUART.callback = NULL; //No callback

    MXC_UART_Transaction(&reqUART);
    printf("Pairing successful\n");

    ble_ON = TRUE;

    rx_pos = 4;//Received 4 characters back
}

void exeCommand(char *command){
    uint8_t tx_end = tx_pos;//Variable to store the end position of the command in the buffer

    while (*command != '\n'){//Storing command in transmit buffer
        tx_buf[tx_end++] = *command;
        command++;//Incrementing command string

        if (tx_end == BUFF_SIZE - 1) tx_end = 0;//Overflow
    }

    reqUART.rxData = &rx_buf[rx_pos];
    reqUART.txData = &tx_buf[tx_pos];
    reqUART.txLen = tx_end - tx_pos;

    if (tx_end < tx_pos){//Accounts for overflow
        MXC_UART_WriteTXFIFO(bleUART, &tx_buf[tx_pos], (BUFF_SIZE-1) - tx_pos);
    }

    MXC_UART_Transaction(&reqUART);

    //Parsing response
    char *response = malloc(30 * sizeof(uint8_t));//May need to make this larger or loop through rx buf then malloc after you know how long the response is
    char *temp = response;//Stores the base addr of response string
    while(rx_buf[rx_pos] != '\n'){
        *response = rx_buf[rx_pos++];
        response++;
    }
    *response = '\0';//Null terminating char

    response = temp;
    printf("%s\n", response);//Printing response
}

int send_data(char *data){
    uint8_t TxData[BUFF_SIZE];

    int len = 0;//Getting length of data transmission
    while (*data){
        len++;
        data++;
    }
    data -= len;//Shifting pointer back
    
    memcpy(TxData, data, len);//Copying data

    mxc_uart_req_t write_req;
    write_req.uart = MXC_UART_GET_UART(bleUART_PORT);
    write_req.txData = TxData;
    write_req.txLen = len;
    write_req.rxLen = 0;
    write_req.callback = NULL;

    DMA_FLAG = 1;//Setting flag

    #ifdef DMA
        MXC_UART_TransactionDMA(&write_req);
        while (DMA_FLAG){}//Waiting for flag to clear
    #else
        MXC_UART_Transaction(&write_req);//Transmitting data
    #endif

    return E_NO_ERROR;
}

int uart_bluetooth(int classification)
{
    int error, i, fail = 0;
    uint8_t TxData[BUFF_SIZE];
    uint8_t RxData[BUFF_SIZE];

    printf("\n\n**************** UART Example ******************\n");
    printf("Now we will send the classification through uart to bluetooth.\n");

    printf("\n-->UART Baud \t: %d Hz\n", UART_BAUD);
    printf("\n-->Test Length \t: %d bytes\n", BUFF_SIZE);

    // Initialize the data buffers
    
    TxData[0] = classification;

    memset(RxData, 0x0, BUFF_SIZE);

    MXC_DMA_Init();
    MXC_DMA_ReleaseChannel(0);
    MXC_NVIC_SetVector(DMA0_IRQn, DMA_Handler);
    NVIC_EnableIRQ(DMA0_IRQn);

    // Initialize the UART
    if ((error = MXC_UART_Init(MXC_UART_GET_UART(bleUART_PORT), UART_BAUD, MXC_UART_APB_CLK)) !=
        E_NO_ERROR) {
        printf("-->Error initializing UART: %d\n", error);
        printf("-->Example Failed\n");
        return error;
    }

    if ((error = MXC_UART_Init(MXC_UART_GET_UART(bleUART_PORT), UART_BAUD, MXC_UART_APB_CLK)) !=
        E_NO_ERROR) {
        printf("-->Error initializing UART: %d\n", error);
        printf("-->Example Failed\n");
        return error;
    }

    printf("-->UART Initialized\n\n");

    mxc_uart_req_t read_req;
    read_req.uart = MXC_UART_GET_UART(bleUART_PORT);
    read_req.rxData = RxData;
    read_req.rxLen = BUFF_SIZE;
    read_req.txLen = 0;
    read_req.callback = readCallback;

    mxc_uart_req_t write_req;
    write_req.uart = MXC_UART_GET_UART(bleUART_PORT);
    write_req.txData = TxData;
    write_req.txLen = BUFF_SIZE;
    write_req.rxLen = 0;
    write_req.callback = NULL;

    DMA_FLAG = 1;

    MXC_UART_ClearRXFIFO(MXC_UART_GET_UART(bleUART_PORT));

#ifdef DMA
    error = MXC_UART_TransactionDMA(&read_req);
#else
    error = MXC_UART_TransactionAsync(&read_req);
#endif

if (error != E_NO_ERROR) {
        printf("-->Error starting async read: %d\n", error);
        printf("-->Example Failed\n");
        return error;
    }

    error = MXC_UART_Transaction(&write_req);

    if (error != E_NO_ERROR) {
        printf("-->Error starting sync write: %d\n", error);
        printf("-->Example Failed\n");
        return error;
    }

    if ((error = memcmp(RxData, TxData, BUFF_SIZE)) != 0) {
        printf("-->Error verifying Data: %d\n", error);
        fail++;
    } else {
        printf("-->Data verified\n");
    }

    if (fail != 0) {
        printf("\n-->Example Failed\n");
        return E_FAIL;
    }

    LED_On(LED1); // indicates SUCCESS
    printf("\n-->Example Succeeded\n");
    return E_NO_ERROR;


}

//UART2 ISR
void UART_Handler(void){
    MXC_UART_AsyncHandler(MXC_UART_GET_UART(bleUART_PORT));
}

void DMA_Handler(void)
{
    MXC_DMA_Handler();
    DMA_FLAG = 0;
}

void readCallback(mxc_uart_req_t *req, int error){
    //READ_FLAG = error;
}