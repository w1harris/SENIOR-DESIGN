#include <mxc.h>
#include <stdlib.h>
#include "ble.h"
#include <string.h>

extern mxc_gpio_cfg_t bleEN;

mxc_uart_regs_t *bleUART = MXC_UART_GET_UART(bleUART_PORT);
mxc_uart_req_t reqUART;

uint8_t tx_buf[BUFF_SIZE];
uint8_t rx_buf[BUFF_SIZE];

void initBLE(){
    MXC_GPIO_OutSet(bleEN.port, bleEN.mask);//Toggling the GPIO pin to high to turn on ble device
    
    
    //Clearing buffers
    memset(tx_buf, 0x0, BUFF_SIZE);
    memset(rx_buf, 0x0, BUFF_SIZE);

    strcpy(tx_buf, "AT\r\n");

    reqUART.uart = bleUART;
    reqUART.rxData = rx_buf;
    reqUART.rxLen = 4;
    reqUART.txData = tx_buf;
    reqUART.txLen = 4;//Transmitting 4 characters at the start
    reqUART.callback = NULL; //No callback
    
    MXC_UART_Transaction(&reqUART);
    printf("%s", rx_buf);
}

//UART2 ISR
void UART2_IRQHandler(void){

}