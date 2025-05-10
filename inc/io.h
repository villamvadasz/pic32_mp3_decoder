#ifndef _IO_H_
#define _IO_H_
		
#include "k_stdtype.h"

extern unsigned char get_led(void);
extern void clear_led(void);
extern void set_led(void);
extern uint8 io_get_Usb_Fault(void);

extern void init_io(void);
extern void do_io(void);
extern void isr_io_1ms(void);

#endif
