#include <xgpio.h>
#include <stdio.h>
#include <stdlib.h>
#include <sleep.h>
#include "xparameters.h"
#include "xaxidma.h"
#include "xil_cache.h"

#define GPIO_DEVICE_ID XPAR_AXI_GPIO_0_DEVICE_ID  // Define the GPIO device ID
#define DMA_DEV_ID            XPAR_AXI_DMA_0_DEVICE_ID
#define DDR_BASE_ADDR         XPAR_PS7_DDR_0_S_AXI_BASEADDR //0x00100000
#define MEM_BASE_ADDR         (DDR_BASE_ADDR + 0x00000000)
#define TX_BUFFER_BASE        (MEM_BASE_ADDR + 0x01000000)
#define RX_BUFFER_BASE        (MEM_BASE_ADDR + 0x03000000)
#define MAX_PKT_LEN           0x8000 //was 0x20 for 32 change to 0x30 to check 48 byte (include key)
#define POLL_TIMEOUT_COUNTER  1000000U // for 28/03/2024 not in use!
#define TEST_START_VALUE      0x00

XAxiDma AxiDma;
XAxiDma_Config *CfgPtr;
int Status;
int TimeOut = POLL_TIMEOUT_COUNTER;
u8 *TxBufferPtr;
u8 *RxBufferPtr;
XGpio Gpio;


void util_source_buffer(u8 *TxBufferPtr) {
    u8 Value = TEST_START_VALUE;  // Initialize the Value variable
    for (int Index = 0; Index < (int)MAX_PKT_LEN; Index++) {
        TxBufferPtr[Index] = Value;
        Value = (Value + 1) & 0xFF;
    }
}

void print_tx_buffer(u8 *buffer) {
    for (int Index = 0; Index < (int)MAX_PKT_LEN; Index++) {
        printf("%02X ", buffer[Index]);  // Print data in hexadecimal format
    }
    printf("\n\r");
}

void print_rx_buffer(u8 *buffer) {
    for (int Index = 0; Index < ((int) MAX_PKT_LEN-16); Index++) {
        printf("%02X ", buffer[Index]);  // Print data in hexadecimal format
    }
    printf("\n\r");
}




int main() {
	init_platform();

	int status_gpio;
    u32 data;

    // Initialize the GPIO driver
	status_gpio = XGpio_Initialize(&Gpio, GPIO_DEVICE_ID);
	if (status_gpio != XST_SUCCESS) {
		printf("GPIO Initialization Failed\r\n");
		return XST_FAILURE;
	}
	printf("GPIO Initialization Success\r\n");

	// Set the direction for GPIO2 (input)
	//XGpio_SetDataDirection(&Gpio, 2, 0xFFFFFFFF);
	//printf("GPIO SetDataDirection Success\r\n");



	Xil_DCacheDisable();
	Xil_ICacheDisable();

    CfgPtr = XAxiDma_LookupConfig(DMA_DEV_ID);
    if (!CfgPtr) {
        printf("No config found for %d\r\n", DMA_DEV_ID);  // Use printf for consistency
        return XST_FAILURE;
    }

    Status = XAxiDma_CfgInitialize(&AxiDma, CfgPtr);
    if (Status != XST_SUCCESS) {
        printf("Initialization failed %d\r\n", Status);
        return XST_FAILURE;
    }

	// Disable interrupts, we use polling mode

	XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK,
			    XAXIDMA_DEVICE_TO_DMA);
	XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK,
			    XAXIDMA_DMA_TO_DEVICE);

    TxBufferPtr = (u8 *)TX_BUFFER_BASE;
    RxBufferPtr = (u8 *)RX_BUFFER_BASE;

    // Flush the buffers before the DMA transfer, in case the Data Cache is enabled
    //Xil_DCacheFlushRange((UINTPTR)TxBufferPtr, MAX_PKT_LEN);
    //Xil_DCacheFlushRange((UINTPTR)RxBufferPtr, MAX_PKT_LEN);

    util_source_buffer(TxBufferPtr);
    //printf("TxBufferPtr DATA: \n\r");
    //print_tx_buffer(TxBufferPtr);

    Status = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)TxBufferPtr,
                                     MAX_PKT_LEN, XAXIDMA_DMA_TO_DEVICE);

    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // added 28/03/2024
    while(XAxiDma_Busy(&AxiDma, XAXIDMA_DMA_TO_DEVICE));

    // device to dma
    Status = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)RxBufferPtr,
                                     (MAX_PKT_LEN), XAXIDMA_DEVICE_TO_DMA);

    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    while(XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA));

    // Invalidate the cache after the DMA transfer to ensure the CPU reads the updated data from memory
    //Xil_DCacheInvalidateRange((UINTPTR)RxBufferPtr, MAX_PKT_LEN);

    printf("RxBufferPtr DATA: \n\r");
    print_rx_buffer(RxBufferPtr);

    //while (1) {
    //	// Read the value of en_decrypt from GPIO2
    //	data = XGpio_DiscreteRead(&Gpio, 1);
    //    printf("GPIO Read Data: %d\r\n", data);
    //   // Check the value of en_decrypt
    //    if (data & 0x01)
     //   {
     //   	printf("en_decrypt is on\r\n");
     //   }
     //   else
     //   {
     //   	printf("en_decrypt is off\r\n");
     //   }

     //   usleep(5000000);
    //}

    cleanup_platform();
    return 0;
}


