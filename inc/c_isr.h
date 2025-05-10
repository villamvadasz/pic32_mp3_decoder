#ifndef _C_ISR_H_
#define _C_ISR_H_

	#include "tmr.h"
	#include "usb.h"

	#define ISR_USE_TMR1
	#define ISR_IPLV_TMR1 2

	#define ISR_USE_TMR2
	#define ISR_IPLV_TMR2 2

	//#define ISR_USE_TMR4
	//#define ISR_USE_TMR4_GRBL //isr_grbl_tmr_stepper_delay
	#define ISR_IPLV_TMR4 6
	#define ISR_USE_TMR4_CUSTOM

	//#define ISR_USE_SPI1
	//#define ISR_USE_SPI1_WIFI
	//#define ISR_FNC_SPI1()	isr_spi()

	//#define ISR_USE_SPI2
	//#define ISR_USE_SPI2_WIFI
	//#define ISR_FNC_SPI2()	isr_spi()

	//#define ISR_USE_EINT3_WIFI

	//#define ISR_USE_DMA0
	//#define ISR_USE_DMA1
	//#define ISR_USE_DMA2
	//#define ISR_USE_DMA3

	//#define ISR_USE_UART1
	//#define ISR_USE_UART1_SIO

	//#define ISR_USE_UART2
	//#define ISR_USE_UART2_SIO

	#define ISR_USE_USB
	#define ISR_IPLV_USB 7


#endif
