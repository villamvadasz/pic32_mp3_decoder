#ifndef _C_EEP_MANAGER_H_
#define _C_EEP_MANAGER_H_

	#define EEPROM_START_ADDR	0x0000
	#define EEPROM_SIZE			0x0800

	//Use 0xFF to disable version check
	//#define EEPROM_VERSION 0xFF

	#define EEPROM_VERSION 0x04
	//#define EEPROM_INVALIDATE_OLD

	typedef enum _EepManager_Item {
		EepManager_Items_FirstItem = 0,
		EepManager_Items_app_mp3_volume_eeprom,
		EepManager_Items_app_playFileTrigger,
		EepManager_Items_LastItem,
	} EepManager_Item;

	extern uint8 app_mp3_volume_eeprom;
	extern uint32 app_playFileTrigger;

#endif
