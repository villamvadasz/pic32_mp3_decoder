#ifndef _VS1053_H_
#define _VS1053_H_

	#include "k_stdtype.h"
	
	extern uint8 vs1053_IsDataRequested(void);
	extern uint8 vs1053_WriteStream_nb(uint8 *ptr, uint16 size);
	extern uint8 vs1053_getState(void);
	extern void vs1053_SetVolume(unsigned char leftchannel, unsigned char rightchannel);
	extern void vc1054_recovery(void);
	
	extern void init_vs1053(void);
	extern void do_vs1053(void);
	extern void isr_vs1053_1ms(void);

#endif
