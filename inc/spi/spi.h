#ifndef _SPI_H_
#define _SPI_H_

	#include "k_stdtype.h"

	typedef enum _SPIStateEnum {
		SPI_READY_STATE,
		SPI_BUSY_STATE,
		SPI_ERROR_STATE
	} SPIStateEnum;

	extern unsigned int spi_lock(unsigned int user);
	extern unsigned int spi_unlock(unsigned int user);

	extern void spi_reconfigure(unsigned int user, uint8 SMP, uint8 CKP, uint8 CKE, uint32 BRG);
	extern void spi_reconfigure_mode(unsigned int user, uint8 SMP, uint8 mode, uint32 BRG);

	extern SPIStateEnum spi_readWrite_asynch(unsigned int user, unsigned char *bufferOut, unsigned char *bufferIn, uint32 size);
	extern SPIStateEnum spi_get_state(void);

	extern SPIStateEnum spi_readWrite_synch(unsigned int user, unsigned char *bufferOut, unsigned char *bufferIn, uint32 size);

	extern uint16 spi_calculate_BRG(uint32 spi_clk);

	extern void init_spi(void);
	extern void do_spi(void);
	extern void isr_spi(void);
	extern void isr_spi_1ms(void);
	extern void isr_dma(void);

#endif
