#include <mxc.h>
#include <stdlib.h>
#include "ble.h"
#include <string.h>
#include <strings.h>

extern mxc_gpio_cfg_t bleEN;

mxc_uart_regs_t *bleUART = MXC_UART_GET_UART(bleUART_PORT);
mxc_uart_req_t reqUART;

uint8_t tx_buf[BUFF_SIZE];
uint8_t tx_pos = 0;//Stores current positon in transmit buffer
uint8_t rx_buf[BUFF_SIZE];
uint8_t rx_pos = 0;//Stores current position in receive buffer

void initBLE(){
    MXC_GPIO_OutSet(bleEN.port, bleEN.mask);//Toggling the GPIO pin to high to turn on ble device
    
    //Clearing buffers
    memset(tx_buf, 0x0, BUFF_SIZE);
    memset(rx_buf, 0x0, BUFF_SIZE);

    strcpy(tx_buf, "AT\r\n");

    reqUART.uart = bleUART;
    reqUART.rxData = rx_buf;
    reqUART.rxLen = BUFF_SIZE;
    reqUART.txData = tx_buf;
    reqUART.txLen = 4;//Transmitting 4 characters at the start
    tx_pos = reqUART.txLen;//Shifting tx buffer position
    reqUART.callback = NULL; //No callback
    
    MXC_UART_Transaction(&reqUART);
    if (!strcasecmp(rx_buf, "OK\r\n")) printf("Pairing successful\n");
    else printf("Pairing ERROR\n");

    rx_pos = 4;//Received 4 characters back
}

void exeCommand(char *command){
    uint8_t overflow = 0;
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

//UART2 ISR
void UART2_IRQHandler(void){

}