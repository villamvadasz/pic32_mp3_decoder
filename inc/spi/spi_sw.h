#ifndef _SPI_SW_H_
#define _SPI_SW_H_

	#include "k_stdtype.h"

	typedef enum _SPIStateEnum {
		SPI_READY_STATE,
		SPI_BUSY_STATE,
		SPI_ERROR_STATE
	} SPIStateEnum;

	extern uint8 spi_sw_lock(unsigned char user);
	extern uint8 spi_sw_unlock(unsigned char user);

	extern void spi_sw_reconfigure(unsigned char user, uint8 SMP, uint8 CKP, uint8 CKE, uint32 BRG);

	extern SPIStateEnum spi_sw_readWrite_asynch(unsigned char user, unsigned char *bufferOut, unsigned char *bufferIn, uint32 size);
	extern SPIStateEnum spi_sw_get_state(void);

	extern SPIStateEnum spi_sw_readWrite_synch(unsigned char user, unsigned char *bufferOut, unsigned char *bufferIn, uint32 size);

	extern uint16 spi_sw_calculate_BRG(uint32 spi_clk);

	extern void init_spi_sw(void);
	extern void deinit_spi_sw(void);
	extern void do_spi_sw(void);
	extern void isr_spi_sw_1ms(void);

#endif
