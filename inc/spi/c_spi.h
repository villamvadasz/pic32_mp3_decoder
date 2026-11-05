#ifndef _C_SPI_H_
#define _C_SPI_H_

	#error You have included an example config of c_spi.h

	#include "mal.h"

	#define	SPI_PORT_SYNC()	_sync()

	#define SPI_BRG_CONFIG 8

	#define	SPI_CKP_CONFIG_DEFAULT 1		//Idle state for clock is a high level; active state is a low level
	//#define	SPI_CKP_CONFIG_DEFAULT 0	//Idle state for clock is a low level; active state is a high level

	#define SPI_CKE_CONFIG_DEFAULT	1		//Serial output data changes on transition from active clock state to Idle clock state. see CKP
	//#define SPI_CKE_CONFIG_DEFAULT 0		//Serial output data changes on transition from Idle clock state to active clock state. see CKP

	#define SPI_SCK_TRIS	TRISGbits.TRISG6
	#define SPI_SDI_TRIS	TRISGbits.TRISG7
	#define SPI_SDO_TRIS	TRISGbits.TRISG8

#endif
