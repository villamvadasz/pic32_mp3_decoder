#ifndef _MP3_H_
#define _MP3_H_

	#include "k_stdtype.h"
	
	extern uint8 mp3PlayFile(char *str);
	extern uint8 mp3StopFile(void);
	extern uint8 mp3PauseResumeFile(void);
	extern uint8 mp3IsPlayFinished(void);
	
	extern void init_mp3(void);
	extern void do_mp3(void);
	extern void isr_mp3_1ms(void);

#endif
