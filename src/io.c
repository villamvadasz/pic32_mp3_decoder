#include "io.h"
#include "c_io.h"

#include "mal.h"

void init_io(void) {
	LED_LAT = 0;
	LED_TRIS = 0;
	TRISBbits.TRISB15 = 1; //USB_FAULT

	//TRISBbits.TRISB5 = 0; //VBUSON
	//LATBbits.LATB5 = 1;
}

void do_io(void) {
}

void isr_io_1ms(void) {
}

unsigned char get_led(void) {
	return LED_LAT;
}

void clear_led(void) {
	LED_LAT = 0;
}

void set_led(void) {
	LED_LAT = 1;
}

uint8 io_get_Usb_Fault(void) {
	uint8 result = 0;
	/*if (U1OTGSTATbits.SESVD) {
		result = 1;
	}*/
	if (U1OTGSTATbits.VBUSVD) {
		result = 1;
	}
	return result;
}
