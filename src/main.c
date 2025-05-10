#include "main.h"
#include "c_main.h"

#include "k_stdtype.h"
#include "mal.h"
#include "version.h"
#include "sleep.h"
#include "tmr.h"
#include "stackmeasure.h"
#include "io.h"
#include "isr.h"
#include "tmr.h"
#include "bootloader_interface.h"

#include "spi_sw.h"
#include "c_app.h"

#include "app.h"
#include "eep_manager.h"
#include "ad.h"
#include "mp3.h"
#include "vs1053.h"
#include "fatfs.h"
#include "SPIcard.h"
#include "usb.h"
#include "dee.h"

#define SW_YEAR 0x2020
#define SW_MONTH 0x02
#define SW_DAY 0x03
#define SW_TYPE TYPE_MP3
#define SW_VERSION 0x0100

SoftwareIdentification softwareIdentification = {SW_YEAR, SW_MONTH, SW_DAY, SW_VERSION, SW_TYPE};
unsigned char VERSION_ID[] = "pic_mp3_1_0_0";
const char VERSION_DATE[]  __attribute__ ((address(0x9D008100))) = __DATE__;
const char VERSION_TIME[]  __attribute__ ((address(0x9D008120))) = __TIME__;
const char VERSION_ID_FIX[]  __attribute__ ((address(0x9D008140))) = "pic_mp3_1_0_0";

volatile unsigned char do_loopCnt = 0;
unsigned long loopCntTemp = 0;
unsigned long loopCnt = 0;
unsigned int tick_count = 0;

unsigned long loopCntHistory[60];
unsigned int loopCntHistoryCnt = 0;

__attribute__(( weak )) void init_stackmeasure(void) {} 
__attribute__(( weak )) void do_stackmeasure(void) {} 


int main (void) {

	init_mal();

	init_stackmeasure();

	init_ad();

	init_dee();
	init_eep_manager();

	init_tmr(); //this should be the first beceaus it clears registered timers.

	init_io();
	init_spi_sw();

	init_mp3();
	init_vs1053();
	init_app();
	init_isr();
	init_fatfs();
	do_loopCnt = 0;

	bootloader_interface_clearRequest();
	
	while (1) {
		if (do_loopCnt) {
			do_loopCnt = 0;
			loopCnt = loopCntTemp; //Risk is here that interrupt corrupts the value, but it is taken
			loopCntHistory[loopCntHistoryCnt] = loopCnt;
			loopCntHistoryCnt++;
			loopCntHistoryCnt %= ((sizeof(loopCntHistory)) / (sizeof(*loopCntHistory)));
			loopCntTemp = 0;
			if (loopCnt < 1000) { //1run/ms
				/*while (1) {
					Nop();
					ERROR_LED = 1;
				}*/
			}
		}

		do_stackmeasure();
		do_ad();
		do_eep_manager();
		do_app();
		do_io();
		do_tmr();
		do_usb();
		do_spi_sw();
		do_mp3();
		do_vs1053();
		do_dee();
		do_fatfs();
		//idle_Request();
		loopCntTemp++;
	}
	return 0;
}

void isr_main_1ms(void) {
	static uint16 loopCntTmr = 0;

	tick_count++;

	loopCntTmr ++;
	if (loopCntTmr == 1000) {
		loopCntTmr = 0;
		do_loopCnt = 1;
	}

	isr_app_1ms();
	isr_io_1ms();
	isr_eep_manager_1ms();
	isr_usb_1ms();
	isr_ad_1ms();
	isr_spi_sw_1ms();
	isr_fatrtc_1ms();
	isr_fatfs_1ms();
	isr_sd_spi_1ms();
	isr_dee_1ms();
	isr_vs1053_1ms();
	isr_mp3_1ms();
}

void isr_main_100us(void) {
	isr_app_100us();
}

void isr_main_custom(void) {
	isr_app_custom();
}

void eepManager_NotifyUserFailedRead(int item, uint8 type) {
}

uint8 backToSleep(void) {
	return 0;
}
