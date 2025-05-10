#include <string.h>
#include <stdlib.h>
#include "app.h"

#include "mal.h"
#include "k_stdtype.h"
#include "tmr.h"
#include "mp3.h"
#include "sleep.h"
#include "ff.h"
#include "vs1053.h"
#include "eep_manager.h"
#include "c_eep_manager.h"
#include "fatfs.h"

//#define APP_DEBUG_PLAY_FIRST_FILE_ALWAYS

#define APP_FAILSAFE_TIMER 5000
#define APP_MAX_FILE_NUMBER_SEARCHING 5000
#define APP_FAILSAFE_COUNTER_LIMIT	10

#define BUTTON_NEXT_TRIS TRISBbits.TRISB3
#define BUTTON_NEXT_PORT PORTBbits.RB3
#define BUTTON_NEXT_CNP CNPUEbits.CNPUE5

#define BUTTON_PLAY_PAUSE_TRIS TRISBbits.TRISB2
#define BUTTON_PLAY_PAUSE_PORT PORTBbits.RB2
#define BUTTON_PLAY_PAUSE_CNP CNPUEbits.CNPUE4

#define BUTTON_PREV_TRIS TRISBbits.TRISB1
#define BUTTON_PREV_PORT PORTBbits.RB1
#define BUTTON_PREV_CNP CNPUEbits.CNPUE3

#define BUTTON_VOLUME_TRIS TRISBbits.TRISB4
#define BUTTON_VOLUME_PORT PORTBbits.RB4
#define BUTTON_VOLUME_CNP CNPUEbits.CNPUE6

#define APP_BUTTON_DEBOUNCE_TIME 50
#define APP_VOLUME_CHANGE 8

typedef enum _App_States {
	App_State_Init = 0,
	App_State_TriggerPause,
	App_State_Pause,
	App_State_TriggerResume,
	App_State_Play,
	App_State_Playing,
	App_State_PlayTriggerFailsafe,
	App_State_TriggerNext,
	App_State_TriggerPrev,
} App_State;

typedef enum _App_States_FS {
	App_State_FS_Init = 0,
	App_State_FS_WaitTrigger,
	App_State_FS_CheckDisk,
	App_State_FS_OpenDir,
	App_State_FS_Iterate,
	App_State_FS_CloseDir,
} App_State_FS;

typedef struct _App_Button {
	uint8 currentState;
	uint8 prevState;
	uint8 configuration;
	uint16 debounceCnt;
	uint8 wasPressed;
} App_Button;

App_State app_state = App_State_Init;
App_State_FS app_state_fs = App_State_FS_Init;
uint8 do_app_1ms = 0;
App_Button app_next_button;
App_Button app_play_button;
App_Button app_prev_button;
App_Button app_volume_button;

uint8 app_next_item_button_event = 0;
uint8 app_prev_item_button_event = 0;
uint8 app_volume_up_button_event = 0;
uint8 app_volume_down_button_event = 0;
uint8 app_play_pause_button_event = 0;

uint32 app_playFilePosition = 0;
uint8 app_volume = 0;
Timer app_failsafe_timer;
uint32 app_failsafe_counter = 0;

char app_fileNameFound[64];
uint8 app_fileNameFound_updated = 0;
uint8 app_fileNameFound_direction = 1;//1-forward 0-backward search
DIR app_dir_fs;
uint32 app_dir_fs_lastPositionCurrent = 0;
uint32 app_dir_fs_failsafeExitCounter = APP_MAX_FILE_NUMBER_SEARCHING;

void app_init_button(App_Button *button, uint8 currentState, uint8 configuration);
void app_process_button(App_Button *button, uint8 currentState);
uint8 app_was_button_pressed(App_Button *button);
void app_search_next_file(void);
void app_search_prev_file(void);

void init_app(void) {
	BUTTON_NEXT_CNP = 1;
	BUTTON_PLAY_PAUSE_CNP = 1;
	BUTTON_PREV_CNP = 1;
	BUTTON_VOLUME_CNP = 1;

	BUTTON_NEXT_TRIS = 1;
	BUTTON_PLAY_PAUSE_TRIS = 1;
	BUTTON_PREV_TRIS = 1;
	BUTTON_VOLUME_TRIS = 1;
	
	app_volume = app_mp3_volume_eeprom;
	
	if (BUTTON_NEXT_PORT) {
		app_init_button(&app_next_button, 1, 0);
	} else {
		app_init_button(&app_next_button, 0, 0);
	}
	if (BUTTON_PLAY_PAUSE_PORT) {
		app_init_button(&app_play_button, 1, 0);
	} else {
		app_init_button(&app_play_button, 0, 0);
	}
	if (BUTTON_PREV_PORT) {
		app_init_button(&app_prev_button, 1, 0);
	} else {
		app_init_button(&app_prev_button, 0, 0);
	}
	if (BUTTON_VOLUME_PORT) {
		app_init_button(&app_volume_button, 1, 1);
	} else {
		app_init_button(&app_volume_button, 0, 1);
	}
	init_timer(&app_failsafe_timer);
}

void do_app(void) {
	static unsigned char appSingleShoot = 1;
	if (appSingleShoot) {
		appSingleShoot = 0;
		
		vs1053_SetVolume(app_volume, app_volume);
	}
	
	if (do_app_1ms) {
		do_app_1ms = 0;
		{
			uint8 app_volume_mode = 0;

			if (BUTTON_NEXT_PORT) {
				app_process_button(&app_next_button, 1);
			} else {
				app_process_button(&app_next_button, 0);
			}
			if (BUTTON_PLAY_PAUSE_PORT) {
				app_process_button(&app_play_button, 1);
			} else {
				app_process_button(&app_play_button, 0);
			}
			if (BUTTON_PREV_PORT) {
				app_process_button(&app_prev_button, 1);
			} else {
				app_process_button(&app_prev_button, 0);
			}
			if (BUTTON_VOLUME_PORT) {
				app_process_button(&app_volume_button, 1);
			} else {
				app_process_button(&app_volume_button, 0);
			}

			if (app_was_button_pressed(&app_volume_button)) {
				app_volume_mode = 1;
			}
			if (app_was_button_pressed(&app_next_button)) {
				if (app_volume_mode) {
					app_volume_up_button_event = 1;
				} else {
					app_next_item_button_event = 1;
				}
			}
			if (app_was_button_pressed(&app_play_button)) {
				app_play_pause_button_event = 1;
			}
			if (app_was_button_pressed(&app_prev_button)) {
				if (app_volume_mode) {
					app_volume_down_button_event = 1;
				} else {
					app_prev_item_button_event = 1;
				}
			}
		}
		
		{//Move here the search for the next file, and make it independent from every other.
			//Will set file name and updated flag, when flag is 0
			switch (app_state_fs) {
				case App_State_FS_Init : {
					app_state_fs = App_State_FS_WaitTrigger;
					break;
				}
				case App_State_FS_WaitTrigger : {
					if (app_fileNameFound_updated == 0) { //somebody consumed the last found file name
						//so provide a new one
						app_state_fs = App_State_FS_CheckDisk;
					}
					break;
				}
				case App_State_FS_CheckDisk : {
					if (fatfs_isDiskReady()) {
						app_dir_fs_lastPositionCurrent = 0;
						app_dir_fs_failsafeExitCounter = APP_MAX_FILE_NUMBER_SEARCHING;
						app_state_fs = App_State_FS_OpenDir;
					} else {
						//just wait for disk ready
					}
					break;
				}
				case App_State_FS_OpenDir : {
					FRESULT res;
					res = f_opendir(&app_dir_fs, "0:");                       /* Open the directory */
					if (res == FR_OK) {
						app_state_fs = App_State_FS_Iterate;
					} else {
						app_state_fs = App_State_FS_CloseDir;
					}
					break;
				}
				case App_State_FS_Iterate : {
					if (app_dir_fs_failsafeExitCounter != 0) {
						app_dir_fs_failsafeExitCounter--;
						{
							FRESULT res;
							FILINFO fno;

							#ifdef APP_DEBUG_PLAY_FIRST_FILE_ALWAYS
								app_playFilePosition = 0;
							#endif

							memset(fno.fname, 0x00, sizeof(fno.fname));
							res = f_readdir(&app_dir_fs, &fno);                   /* Read a directory item */
							if (res == FR_OK) {
								if (fno.fname[0] == 0) {					/* End of dir */
									app_state_fs = App_State_FS_CloseDir;
								} else if (fno.fattrib & AM_DIR) {                 /* It is a directory */
									//currently subdirs are ignored
								} else {                                   /* It is a file. */
									//Store name for failsafe
									if (
										((fno.fname[9] == 'm') || (fno.fname[9] == 'M')) && 
										((fno.fname[10] == 'p') || (fno.fname[10] == 'P')) && 
										(fno.fname[11] == '3')
									) {
										memset(app_fileNameFound, 0x00, sizeof(app_fileNameFound));
										app_fileNameFound[0] = '0';
										app_fileNameFound[1] = ':';
										strcpy(app_fileNameFound + 2, fno.fname);
									}
									if (app_dir_fs_lastPositionCurrent >= app_playFilePosition) {
										app_fileNameFound_updated = 1;
										app_state_fs = App_State_FS_CloseDir;
									}
									app_dir_fs_lastPositionCurrent++;
								}
							} else {
								app_state_fs = App_State_FS_CloseDir;
							}
						}
					} else {
						app_state_fs = App_State_FS_CloseDir;
					}
					break;
				}
				case App_State_FS_CloseDir : {
					f_closedir(&app_dir_fs);
					app_state_fs = App_State_FS_WaitTrigger;
					break;
				}
				default : {
					app_state_fs = App_State_FS_Init;
					break;
				}
			}
		}
		
		
		{
			switch (app_state) {
				case App_State_Init : {
					write_timer(&app_failsafe_timer, APP_FAILSAFE_TIMER);
					app_state = App_State_Play;
					break;
				}
				case App_State_TriggerPause : {
					if (mp3PauseResumeFile() != 0) {
						app_state = App_State_Pause;
					}
					break;
				}
				case App_State_Pause : {
					if (app_play_pause_button_event) {
						app_state = App_State_TriggerResume;
						app_play_pause_button_event = 0;
					}
					break;
				}
				case App_State_TriggerResume : {
					if (mp3PauseResumeFile() != 0) {
						app_state = App_State_Play;
					}
					break;
				}
				case App_State_Play : {
					if (read_timer(&app_failsafe_timer) != 0) {
						if (app_fileNameFound_updated != 0) {
							if (mp3PlayFile(app_fileNameFound) != 0) {
								if (app_failsafe_counter != 0) {
									app_failsafe_counter--;
								}
								app_state = App_State_Playing;
							}
						}
					} else {
						app_state = App_State_PlayTriggerFailsafe;
					}
					break;
				}
				case App_State_Playing : {
					if (mp3IsPlayFinished()) {
						app_search_next_file();
						write_timer(&app_failsafe_timer, APP_FAILSAFE_TIMER);
						app_state = App_State_Play;
					} else if (app_next_item_button_event) {
						app_state = App_State_TriggerNext;
						app_next_item_button_event = 0;
					} else if (app_prev_item_button_event) {
						app_state = App_State_TriggerPrev;
						app_prev_item_button_event = 0;
					} else if (app_volume_up_button_event) {
						if (app_volume <= (0xFF - APP_VOLUME_CHANGE)) {
							app_volume += APP_VOLUME_CHANGE;
						}
						vs1053_SetVolume(app_volume, app_volume);
						app_mp3_volume_eeprom = app_volume;
						eep_manager_WriteAll_Trigger();
						app_volume_up_button_event = 0;
					} else if (app_volume_down_button_event) {
						if (app_volume >= APP_VOLUME_CHANGE) {
							app_volume -= APP_VOLUME_CHANGE;
						}
						vs1053_SetVolume(app_volume, app_volume);
						app_mp3_volume_eeprom = app_volume;
						eep_manager_WriteAll_Trigger();
						app_volume_down_button_event = 0;
					} else if (app_play_pause_button_event) {
						app_state = App_State_TriggerPause;
						app_play_pause_button_event = 0;
					}
					break;
				}
				case App_State_PlayTriggerFailsafe : {
					app_failsafe_counter++;
					if (app_failsafe_counter >= APP_FAILSAFE_COUNTER_LIMIT) {
						mal_reset();
					} else {
						app_state = App_State_TriggerNext;
					}	
					break;
				}
				case App_State_TriggerNext : {
					if (mp3StopFile() != 0) {
						app_search_next_file();
						write_timer(&app_failsafe_timer, APP_FAILSAFE_TIMER);
						app_state = App_State_Play;
					}
					break;
				}
				case App_State_TriggerPrev : {
					if (mp3StopFile() != 0) {
						app_search_prev_file();
						write_timer(&app_failsafe_timer, APP_FAILSAFE_TIMER);
						app_state = App_State_Play;
					}
					break;
				}
				default : {
					app_state = App_State_Init;
					break;
				}
			}			
		}
	}
}

void isr_app_1ms(void) {
	do_app_1ms = 1;
}

void isr_app_100us(void) {
}

void isr_app_custom(void) {
}

void app_init_button(App_Button *button, uint8 currentState, uint8 configuration) {
	if (button != NULL) {
		button->currentState = currentState;
		button->prevState = currentState;
		button->debounceCnt = APP_BUTTON_DEBOUNCE_TIME;
		button->configuration = configuration;
	}
}

void app_process_button(App_Button *button, uint8 currentState) {
	if (button != NULL) {
		button->currentState = currentState;
		if (button->currentState != button->prevState) {
			if (button->debounceCnt != 0) {
				button->debounceCnt--;
			} else {
				button->prevState = button->currentState;
				button->debounceCnt = APP_BUTTON_DEBOUNCE_TIME;
				if (button->configuration == 0) { //Simple button with a toggle switch
					button->wasPressed = 1;
				} else if (button->configuration == 1) { //Toggle switch with a toggle switch
					button->wasPressed = !button->wasPressed;
				} else {
				}
			}
		} else {
			button->debounceCnt = APP_BUTTON_DEBOUNCE_TIME;
		}
	}
}

uint8 app_was_button_pressed(App_Button *button) {
	uint8 result = 0;
	if (button != NULL) {
		if (button->configuration == 0) {
			if (button->wasPressed) {
				button->wasPressed = 0;
				result = 1;
			}
		} else if (button->configuration == 1) {
			if (button->wasPressed) {
				result = 1;
			} else {
				result = 0;
			}
		} else {
			result = 0;
		}
	}
	return result;
}

void app_search_next_file(void) {
	if (app_playFilePosition < APP_MAX_FILE_NUMBER_SEARCHING) {
		app_playFilePosition++;
	} else {
		app_playFilePosition = 0;
	}
	eep_manager_WriteAll_Trigger();

	
	app_fileNameFound_direction = 1;
	app_fileNameFound_updated = 0;
}

void app_search_prev_file(void) {
	if (app_playFilePosition != 0) {
		app_playFilePosition--;
	} else {
		app_playFilePosition = APP_MAX_FILE_NUMBER_SEARCHING;
	}
	eep_manager_WriteAll_Trigger();
	
	app_fileNameFound_direction = 0;
	app_fileNameFound_updated = 0;
}
