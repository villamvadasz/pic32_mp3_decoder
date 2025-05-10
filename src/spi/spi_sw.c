#include <stdio.h>
#include <string.h>
#include "spi_sw.h"
#include "c_spi_sw.h"

#include "k_stdtype.h"
#include "mal.h"
#include "c_main.h"

SPIStateEnum spiStateEnum = SPI_READY_STATE;
unsigned char *bufferTxTemp = NULL;
unsigned char *bufferRxTemp = NULL;
unsigned int spi_sizeTemp = 0;
uint8 spi_default_output = 0;
unsigned char spi_user = 0;

void init_spi_sw(void) {
	SPI_SW_SDO_TRIS = 0;
	SPI_SW_PORT_SYNC();
	#ifdef SPI_SW_SDI_TRIS
		SPI_SW_SDI_TRIS = 1;
		SPI_SW_PORT_SYNC();
	#endif
	SPI_SW_SCK_TRIS = 0;
	SPI_SW_PORT_SYNC();
	
	IEC1CLR = _IEC1_SPI2EIE_MASK | _IEC1_SPI2RXIE_MASK | _IEC1_SPI2TXIE_MASK;
	SPI2CON = 0;
	{
		//Errata: Read buffer only if bit is set
		if (SPI2STATbits.SPIRBF) {
			volatile unsigned char tempRead = SPI2BUF;
			tempRead;
		}
	}
	IFS1CLR = _IEC1_SPI2EIE_MASK | _IEC1_SPI2RXIE_MASK | _IEC1_SPI2TXIE_MASK;

	#if defined(__32MX470F512H__)
		IPC8bits.SPI2IP = 3;
		IPC8bits.SPI2IS = 3;
	#else
		IPC7bits.SPI2IP = 3;
		IPC7bits.SPI2IS = 3;
	#endif
	
	SPI2BRG = SPI_SW_BRG_CONFIG_DEFAULT;
	SPI2STATbits.SPIROV = 0;
	SPI2CONbits.CKP	= SPI_SW_CKP_CONFIG_DEFAULT;
	SPI2CONbits.CKE	= SPI_SW_CKE_CONFIG_DEFAULT;

	SPI2CONbits.MSTEN = 1;
	SPI2CONbits.ON = 1;
	SPI_SW_PORT_SYNC();

#if defined(__32MX470F512H__)
	IPC8bits.SPI2IP = 3;
	IPC8bits.SPI2IS = 3;
#else
	IPC7bits.SPI2IP = 3;
	IPC7bits.SPI2IS = 3;
#endif
}

void deinit_spi_sw(void) {
	SPI_SW_SDO_TRIS = 1;
	SPI_SW_PORT_SYNC();
	#ifdef SPI_SW_SDI_TRIS
		SPI_SW_SDI_TRIS = 1;
		SPI_SW_PORT_SYNC();
	#endif
	SPI_SW_SCK_TRIS = 1;
	SPI_SW_PORT_SYNC();
	
	IEC1CLR = _IEC1_SPI2EIE_MASK | _IEC1_SPI2RXIE_MASK | _IEC1_SPI2TXIE_MASK;
	SPI2CON = 0;
}

void do_spi_sw(void) {
	if (spiStateEnum == SPI_BUSY_STATE) {
		unsigned int j = 0;

		for (j = 0; j < spi_sizeTemp; j++) {
			volatile unsigned char currentBufferOut = spi_default_output;
			volatile unsigned char currentBufferIn = 0;
			if (bufferTxTemp != NULL) {
				currentBufferOut = bufferTxTemp[j];
			}

			lock_isr();//Errata
			SPI2BUF = currentBufferOut;
			unlock_isr();
			while (1) {
				if ((SPI2STATbits.SPITBE) && (SPI2STATbits.SPIRBF)) {
					break;
				}
			}
			IFS1CLR = _IEC1_SPI2EIE_MASK | _IEC1_SPI2RXIE_MASK | _IEC1_SPI2TXIE_MASK;
		
			//Errata: Read buffer only if bit is set
			if (SPI2STATbits.SPIRBF) {
				currentBufferIn = SPI2BUF;
			} else {
			}

			if (bufferRxTemp != NULL) {
				bufferRxTemp[j] = currentBufferIn;
			}
		}
		bufferTxTemp = NULL;
		bufferRxTemp = NULL;
		spi_sizeTemp = 0;
		spiStateEnum = SPI_READY_STATE;
	}
}

void isr_spi_sw_1ms(void) {
}

void spi_sw_reconfigure(unsigned char user, uint8 SMP, uint8 CKP, uint8 CKE, uint32 BRG) {
	if (user == spi_user) {
		SPI2CONbits.ON = 0;
		SPI_SW_PORT_SYNC();
		SPI2BRG = BRG;
		SPI2CONbits.SMP	= SMP;
		SPI2CONbits.CKP	= CKP;
		SPI2CONbits.CKE	= CKE;
		SPI2CONbits.ON = 1;
		SPI_SW_PORT_SYNC();
	}
}

SPIStateEnum spi_sw_readWrite_synch(unsigned char user, unsigned char *bufferOut, unsigned char *bufferIn, uint32 size) {
	SPIStateEnum result = SPI_ERROR_STATE;
	if (spiStateEnum == SPI_READY_STATE) {
		if (spi_sw_readWrite_asynch(user, bufferOut, bufferIn, size) == SPI_BUSY_STATE) {
			while (1) {
				do_spi_sw();
				result = spi_sw_get_state();
				if (result != SPI_BUSY_STATE) {
					break;
				}
			}
		}
	}
	return result;
}

SPIStateEnum spi_sw_readWrite_asynch(unsigned char user, unsigned char *bufferOut, unsigned char *bufferIn, uint32 size) {
	SPIStateEnum result = SPI_ERROR_STATE;
	if (user == spi_user) {
		if (spiStateEnum == SPI_READY_STATE) {
			bufferTxTemp = bufferOut;
			bufferRxTemp = bufferIn;
			spi_sizeTemp = size;
			spiStateEnum = SPI_BUSY_STATE;
			result = SPI_BUSY_STATE;
		}
	}
	return result;
}

uint16 spi_sw_calculate_BRG(uint32 spi_clk) {
	uint32 pb_clk = GetPeripheralClock();
	uint32 brg = 0;

	brg = pb_clk / (2 * spi_clk);

	if(pb_clk % (2 * spi_clk)) {
		brg++;
	}

	if(brg > 0x100) {
		brg = 0x100;
	}

	if(brg) {
		brg--;
	}

	return (uint16) brg;
}

SPIStateEnum spi_sw_get_state(void) {
	return spiStateEnum;
}

uint8 spi_sw_lock(unsigned char user) {
	uint8 result = 0;
	if (spi_user == 0) {
		spi_user = user;
	}
	result = spi_user;
	return result;
}

uint8 spi_sw_unlock(unsigned char user) {
	uint8 result = 0;
	if (spi_user == user) {
		spi_user = 0;
	}
	result = spi_user;
	return result;
}
