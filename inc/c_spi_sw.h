#ifndef _C_SPI_H_
#define _C_SPI_H_

	#include "mal.h"

	#define	SPI_SW_PORT_SYNC()	_sync()

	#define SPI_SW_USER_VS1053 1
	#define SPI_SW_USER_SDCARD 2
	#define SPI_SW_USER_XTP2046 3
	#define SPI_SW_USER_ILI9341 4
	#define SPI_SW_USER_XC1276 5
	#define SPI_SW_USER_NRF24l01 6

	#define SPI_SW_BRG_CONFIG_DEFAULT 32

	//#define	SPI_SW_CKP_CONFIG_DEFAULT 1		//Idle state for clock is a high level; active state is a low level
	#define	SPI_SW_CKP_CONFIG_DEFAULT 0	//Idle state for clock is a low level; active state is a high level

	//#define SPI_SW_CKE_CONFIG_DEFAULT	1		//Serial output data changes on transition from active clock state to Idle clock state. see CKP
	#define SPI_SW_CKE_CONFIG_DEFAULT 0		//Serial output data changes on transition from Idle clock state to active clock state. see CKP

	#define SPI_SW_SCK_PORT	PORTGbits.RG6
	#define SPI_SW_SCK_LAT		LATGbits.LATG6
	#define SPI_SW_SCK_TRIS	TRISGbits.TRISG6

	#define SPI_SW_SDI_PORT	PORTGbits.RG7
	#define SPI_SW_SDI_LAT		LATGbits.LATG7
	#define SPI_SW_SDI_TRIS	TRISGbits.TRISG7

	#define SPI_SW_SDO_PORT	PORTGbits.RG8
	#define SPI_SW_SDO_LAT		LATGbits.LATG8
	#define SPI_SW_SDO_TRIS	TRISGbits.TRISG8

#endif
