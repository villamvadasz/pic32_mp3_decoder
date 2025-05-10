#include "eep_manager.h"
#include "c_eep_manager.h"

#include <stdio.h>
#include "app.h"

uint8 app_mp3_volume_eeprom = 0x0;
const uint8 default_app_mp3_volume_eeprom = 0x80;
extern uint32 app_playFilePosition;
const uint32 default_app_playFilePosition = 0;

EepManager_ItemTable eepManager_ItemTable[EepManager_Items_LastItem] = {
	{EepManager_Items_app_mp3_volume_eeprom, 	sizeof(app_mp3_volume_eeprom), 	&app_mp3_volume_eeprom,	(void *)&default_app_mp3_volume_eeprom,		EEP_USE_CRC,	EEP_NO_BACKUP},
	{EepManager_Items_app_playFileTrigger, 		sizeof(app_playFilePosition), 	&app_playFilePosition,	(void *)&default_app_playFilePosition,		EEP_USE_CRC,	EEP_NO_BACKUP},
};
