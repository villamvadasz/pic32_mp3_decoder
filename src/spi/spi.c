#include <xc.h>
#include <stdio.h>
#include <string.h>
#include "spi.h"
#include "c_spi.h"

#include "k_stdtype.h"
#include "tmr.h"
#include "mal.h"
#include "c_main.h"

SPIStateEnum spi_StateEnum = SPI_READY_STATE;
unsigned char *spi_bufferTxTemp = NULL;
unsigned char *spi_bufferRxTemp = NULL;
unsigned int spi_sizeTemp = 0;
unsigned int spi_sizeTotalTemp = 0;
volatile unsigned int spi_user = 0;
unsigned int spi_State = 0;
Timer spi_FailsafeTimeout;
unsigned char spi_bufferRxTemp_dummy;
unsigned char spi_bufferTxTemp_dummy;
unsigned char spi_bufferRxTemp_dummy2[1024];
unsigned char spi_bufferTxTemp_dummy2[1024];

void init_spi(void) {
	SPI_SDO_TRIS = 0;
	SPI_PORT_SYNC();
	#ifdef SPI_SDI_TRIS
		SPI_SDI_TRIS = 1;
		SPI_PORT_SYNC();
	#endif
	SPI_SCK_TRIS = 0;
	SPI_PORT_SYNC();
	
	IEC1CLR = _IEC1_SPI2EIE_MASK | _IEC1_SPI2RXIE_MASK | _IEC1_SPI2TXIE_MASK;
	SPI2CON = 0;
	{
		//Errata: Read buffer only if bit is set
		if (SPI2STATbits.SPIRBF) {
			volatile unsigned char tempRead = SPI2BUF;
			tempRead;
		}
	}
	IFS1CLR = _IFS1_SPI2EIF_MASK | _IFS1_SPI2RXIF_MASK | _IFS1_SPI2TXIF_MASK;

	#if defined(__32MX470F512H__)
		IPC8bits.SPI2IP = 3;
		IPC8bits.SPI2IS = 3;
	#else
		IPC7bits.SPI2IP = 3;
		IPC7bits.SPI2IS = 3;
	#endif
	
	SPI2BRG = SPI_BRG_CONFIG_DEFAULT;
	SPI2STATbits.SPIROV = 0;
	SPI2CONbits.CKP	= SPI_CKP_CONFIG_DEFAULT;
	SPI2CONbits.CKE	= SPI_CKE_CONFIG_DEFAULT;

	SPI2CONbits.MSTEN = 1;
	SPI2CONbits.ON = 1;
	SPI_PORT_SYNC();

#if defined(__32MX470F512H__)
	IPC8bits.SPI2IP = 3;
	IPC8bits.SPI2IS = 3;
#else
	IPC7bits.SPI2IP = 3;
	IPC7bits.SPI2IS = 3;
#endif

	//DMA
	DMACONbits.ON = 0;
	
	//DMA SPI RX

	DCH0ECONbits.CHSIRQ = _SPI2_RX_IRQ;
	DCH0ECONbits.SIRQEN = 0;

	DCH0SSA = KVA_TO_PA(&SPI2BUF);
	DCH0SSIZ = 1;
	DCH0CSIZ = 1;
	
	DCH0CONbits.CHAEN = 0;
	DCH0CONbits.CHEN = 0;

	//DMA SPI TX

	DCH1ECONbits.CHSIRQ = _SPI2_TX_IRQ;
	DCH1ECONbits.SIRQEN = 0;

	DCH1DSA = KVA_TO_PA(&SPI2BUF);
	DCH1DSIZ = 1;
	DCH1CSIZ = 1;
	
	DCH1CONbits.CHAEN = 0;
	DCH1CONbits.CHEN = 0;
	
	DMACONbits.ON = 1;

	init_timer(&spi_FailsafeTimeout);
	
	spi_bufferTxTemp_dummy = 0xFF;
	spi_bufferRxTemp_dummy = 0xFF;

	memset(spi_bufferTxTemp_dummy2, 0xFF, 1024);
	memset(spi_bufferRxTemp_dummy2, 0xFF, 1024);
}

void deinit_sw(void) {
	SPI_SDO_TRIS = 1;
	SPI_PORT_SYNC();
	#ifdef SPI_SDI_TRIS
		SPI_SDI_TRIS = 1;
		SPI_PORT_SYNC();
	#endif
	SPI_SCK_TRIS = 1;
	SPI_PORT_SYNC();
	
	IEC1CLR = _IEC1_SPI2EIE_MASK | _IEC1_SPI2RXIE_MASK | _IEC1_SPI2TXIE_MASK;
	SPI2CON = 0;
}

void do_spi(void) {
	if (spi_StateEnum == SPI_BUSY_STATE) {
		switch (spi_State) {
			case 0 : {
				SPI2STATbits.SPIROV = 0;

				if (SPI2STATbits.SPIRBF)
				{
					volatile unsigned char dummy = SPI2BUF;
					(void)dummy;
				}

				DCH0INT = 0;
				DCH1INT = 0;

				if (spi_bufferRxTemp == NULL) {
					DCH0DSA = KVA_TO_PA(spi_bufferRxTemp_dummy2);
					DCH0DSIZ = spi_sizeTemp;
					DCH0CSIZ = 1;
				} else {
					DCH0DSA = KVA_TO_PA(spi_bufferRxTemp);
					DCH0DSIZ = spi_sizeTemp;
					DCH0CSIZ = 1;
				}
				DCH0ECONbits.SIRQEN = 1;
				DCH0CONbits.CHEN = 1;

				if (spi_bufferTxTemp == NULL) {
					DCH1SSA = KVA_TO_PA(spi_bufferTxTemp_dummy2);
					DCH1SSIZ = spi_sizeTemp;
					DCH1CSIZ = 1;
				} else {
					DCH1SSA = KVA_TO_PA(spi_bufferTxTemp);
					DCH1SSIZ = spi_sizeTemp;
					DCH1CSIZ = 1;
				}
				DCH1ECONbits.SIRQEN = 1;
				DCH1CONbits.CHEN = 1;

	
				DCH1ECONbits.CFORCE = 1;
				write_timer(&spi_FailsafeTimeout, 100);
	
				spi_State = 1;
				break;
			}
			case 1 : {
				if ((DCH0INTbits.CHBCIF) && (DCH1INTbits.CHBCIF) && (SPI2STATbits.SPIBUSY == 0) && (SPI2STATbits.SPITBE) && (SPI2STATbits.SPIRBF == 0)) {//RX and TX completed well
					spi_State = 2;
				} else if ((DCH0INTbits.CHERIF) || (DCH1INTbits.CHERIF)) {
					spi_State = 2;
				} else if ((DCH0INTbits.CHTAIF) || (DCH1INTbits.CHTAIF)) {
					spi_State = 2;
				} else if (read_timer(&spi_FailsafeTimeout) == 0) {
					spi_State = 2;
				}
				break;
			}
			case 2 : {
				DCH0ECONbits.SIRQEN = 0;
				DCH1ECONbits.SIRQEN = 0;

				DCH0CONbits.CHEN = 0;
				DCH1CONbits.CHEN = 0;
				
				spi_sizeTotalTemp -= spi_sizeTemp;
				if (spi_sizeTotalTemp > 0) {
					if (spi_bufferTxTemp != NULL) {
						spi_bufferTxTemp += spi_sizeTemp;
					}
					if (spi_bufferRxTemp != NULL) {
						spi_bufferRxTemp += spi_sizeTemp;
					}
					if (spi_sizeTotalTemp > 256) {
						spi_sizeTemp = 256;
					} else {
						spi_sizeTemp = spi_sizeTotalTemp;
					}
					spi_State = 0;
				} else {
					spi_bufferTxTemp = NULL;
					spi_bufferRxTemp = NULL;
					spi_sizeTemp = 0;
					spi_sizeTotalTemp = 0;
					spi_StateEnum = SPI_READY_STATE;
					spi_State = 0;
				}
				break;
			}
			default : {
				break;
			}		
			
		}
	}
}

void isr_spi_1ms(void) {
}

void spi_reconfigure(unsigned int user, uint8 SMP, uint8 CKP, uint8 CKE, uint32 BRG) {
	if (user == spi_user) {
		SPI2CONbits.ON = 0;
		SPI_PORT_SYNC();
		SPI2BRG = BRG;
		SPI2CONbits.SMP	= SMP;
		SPI2CONbits.CKP	= CKP;
		SPI2CONbits.CKE	= CKE;
		SPI2CONbits.ON = 1;
		SPI_PORT_SYNC();
	}
}

void spi_reconfigure_mode(unsigned int user, uint8 SMP, uint8 mode, uint32 BRG) {
	
	//SPI mode		CPOL	CPHA	CKP	CKE	SMP
	//Mode 0		0		0		0	1	0
	//Mode 1		0		1		0	0	0
	//Mode 2		1		0		1	1	0
	//Mode 3		1		1		1	0	0
	uint8 CKP = 0;
	uint8 CKE = 0;
	switch (mode) {
		case 0 : {
			CKE = 1;
			CKP = 0;
			break;
		}
		case 1 : {
			CKE = 0;
			CKP = 0;
			break;
		}
		case 2 : {
			CKE = 1;
			CKP = 1;
			break;
		}
		case 3 : {
			CKE = 0;
			CKP = 1;
			break;
		}
	}
	spi_reconfigure(user, SMP, CKP, CKE, BRG);
}

SPIStateEnum spi_readWrite_synch(unsigned int user, unsigned char *bufferOut, unsigned char *bufferIn, uint32 size) {
	SPIStateEnum result = SPI_ERROR_STATE;
	if (spi_StateEnum == SPI_READY_STATE) {
		if (spi_readWrite_asynch(user, bufferOut, bufferIn, size) == SPI_BUSY_STATE) {
			while (1) {
				do_spi();
				result = spi_get_state();
				if (result != SPI_BUSY_STATE) {
					break;
				}
			}
		}
	}
	return result;
}

SPIStateEnum spi_readWrite_asynch(unsigned int user, unsigned char *bufferOut, unsigned char *bufferIn, uint32 size) {
	SPIStateEnum result = SPI_ERROR_STATE;
	if (user == spi_user) {
		if ((spi_StateEnum == SPI_READY_STATE) && (SPI2STATbits.SPITBE) && (size > 0) && (size <= 1024)) {
			spi_bufferTxTemp = bufferOut;
			spi_bufferRxTemp = bufferIn;
			if (size > 256) {
				spi_sizeTotalTemp = size;
				spi_sizeTemp = 256;
			} else {
				spi_sizeTotalTemp = size;
				spi_sizeTemp = size;
			}
			spi_StateEnum = SPI_BUSY_STATE;
			result = SPI_BUSY_STATE;
		}
	}
	return result;
}

uint16 spi_calculate_BRG(uint32 spi_clk) {
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

SPIStateEnum spi_get_state(void) {
	return spi_StateEnum;
}

unsigned int spi_lock(unsigned int user) {
	unsigned int result = 0;
	lock_isr();
	if (spi_user == 0) {
		spi_user = user;
		result = spi_user;
		_sync();
	}
	unlock_isr();
	return result;
}

unsigned int spi_unlock(unsigned int user) {
	unsigned int result = 0;
	lock_isr();
	if (spi_user == user) {
		spi_user = 0;
		_sync();
	}
	result = spi_user;
	unlock_isr();
	return result;
}
