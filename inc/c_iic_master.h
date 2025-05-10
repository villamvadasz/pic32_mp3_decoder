#ifndef _C_IIC_MASTER_H_
#define _C_IIC_MASTER_H_

	#define IIC_PORT_SYNC() 	_sync()

	#define IIC_BIT_RATE_NOPS 100

	#define IIC_MAL_CONFIG() ;

	#define IIC_SCL_PORT	PORTDbits.RD10
	#define IIC_SCL_TRIS	TRISDbits.TRISD10
	#define IIC_SCL_LAT		LATDbits.LATD10

	#define IIC_SDA_PORT	PORTDbits.RD9
	#define IIC_SDA_TRIS	TRISDbits.TRISD9
	#define IIC_SDA_LAT		LATDbits.LATD9

	//#define IIC_HELPER_TRIS	TRISBbits.TRISB10
	//#define IIC_HELPER_LAT	LATBbits.LATB10
	
#endif
