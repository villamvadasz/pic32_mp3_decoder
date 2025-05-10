#include <string.h>
#include <stdio.h>
#include "mp3.h"
#include "c_mp3.h"

#include "fatfs.h"
#include "ff.h"
#include "tokenize.h"
#ifdef MP3_USE_VS1053
	#include "vs1053.h"
#endif
#ifdef MP3_USE_HELIX
	#include "sound_pwm.h"
	#include "mp3dec.h"
#endif
#include "k_stdtype.h"
#include "SDcard.h"
#include "tmr.h"
#include "mal.h"

#ifdef MP3_USE_VS1053
	#define MP3_FILE_READ_BUFFERSIZE 512
	#define MP3_FILE_READ_BUFFERCOUNT 2
	#define MP3_VS1053_WRITE_SIZE 32
#endif

typedef enum _MP3_States {
	MP3_State_Start = 0,
	MP3_State_Idle,
	MP3_State_OpenFile,
	MP3_State_FillBuffer,
	#ifdef MP3_USE_HELIX
		MP3_State_FindSyncWord,
		MP3_State_DecodeMP3,
	#endif
	MP3_State_CloseFile,
} MP3_States;

MP3_States MP3_State = MP3_State_Start;
#ifdef MP3_USE_VS1053
	//SDCard -> SPI -> FAT -> mp3FileReadBuffer1/2 -> SPI -> Buffer -> VS1053
	uint8 mp3FileReadBuffer[MP3_FILE_READ_BUFFERCOUNT][MP3_FILE_READ_BUFFERSIZE]; //(sizeof(mp3FileReadBuffer1) / sizeof(*mp3FileReadBuffer1))
	uint16 mp3FileReadBuffer_Fillness[MP3_FILE_READ_BUFFERCOUNT];
	uint8 mp3FileReadBuffer_Select = 0;
	uint16 mp3FileReadBuffer_Pos = 0;
	sint8 vs1053_markBufferAsEmpty = -1;
	uint8 mp3MarkEof = 0;
	uint8 mp3PlayFile_trigger = 0;
	uint8 mp3stopPlaying = 0;
	uint8 mp3pausePlaying = 0;
	uint8 mp3IsPlaying = 0;
#endif
FIL fmp3;
char mp3FileName[32];
uint8 app_drv = 0; //FATFS_USE_SDCARD

#ifdef MP3_USE_HELIX
	HMP3Decoder mp3Decoder;
	MP3FrameInfo mp3FrameInfoA;
	#define READBUF_SIZE (1940 * 1)
	BYTE readBuf[READBUF_SIZE];
	BYTE *readPtr;
	INT16 outBufA[2 * 1152];
	sint8 outBuf_SoundFeeder = -1;
	INT bytesLeft;
#endif
Timer mp3_failsafe_timer;
volatile uint32 do_mp3_1ms = 0;

void do_mp3_task_1ms(void);

void init_mp3(void) {
	#ifdef MP3_USE_HELIX
		mp3Decoder = MP3InitDecoder();
	#endif
	init_timer(&mp3_failsafe_timer);
}

void do_mp3(void) {
	if (do_mp3_1ms) {
		do_mp3_1ms = 0;
		do_mp3_task_1ms();
	}
#ifdef MP3_USE_VS1053
	if (fatfs_isDiskReady()) {
		if (vs1053_getState() == 1) {//Not sending last buffer
			if (vs1053_markBufferAsEmpty != -1) {//Safe to mark buffer as empty since data is consumed
				mp3FileReadBuffer_Fillness[(uint8)vs1053_markBufferAsEmpty] = 0;
				vs1053_markBufferAsEmpty = -1;
			}
			if (vs1053_IsDataRequested() != 0) {//Data is requested
				//Check if there is any data in the buffer
				if (mp3FileReadBuffer_Select < MP3_FILE_READ_BUFFERCOUNT) {
					uint16 charsInCurrentBuffer = mp3FileReadBuffer_Fillness[mp3FileReadBuffer_Select];
					if (charsInCurrentBuffer != 0) {
						//Check how much chars are in the curernt buffer
						if (charsInCurrentBuffer >= mp3FileReadBuffer_Pos) {
							uint16 maxItemInBuffer = charsInCurrentBuffer - mp3FileReadBuffer_Pos;
							if (maxItemInBuffer > MP3_VS1053_WRITE_SIZE) {//If data is requested, 32 byte reception are garantied
								maxItemInBuffer = MP3_VS1053_WRITE_SIZE;
							}
							vs1053_WriteStream_nb(mp3FileReadBuffer[mp3FileReadBuffer_Select] + mp3FileReadBuffer_Pos, maxItemInBuffer);
							mp3FileReadBuffer_Pos += maxItemInBuffer; 
							if (mp3FileReadBuffer_Pos >= charsInCurrentBuffer) {
								//Mark buffer as empty since every byte was sent
								vs1053_markBufferAsEmpty = mp3FileReadBuffer_Select;
								//Select next buffer
								mp3FileReadBuffer_Select++;
								mp3FileReadBuffer_Pos = 0;								
								if (mp3FileReadBuffer_Select >= MP3_FILE_READ_BUFFERCOUNT) {
									mp3FileReadBuffer_Select = 0;
								}
							}
						} else {
							//Impossible?
						}
					} else {
						//Buffer underrun, or nothing to play
					}
				} else {
					//Implausible buffer state
					mp3FileReadBuffer_Select = 0;
				}
			} else {
				//vs1052 internal 2048 byte buffer is full
			}
		} else {
			//Still sending last buffer
		}
	} else {
		//wait until SD Card gets initialized
	}
#endif
}

void do_mp3_task_1ms(void) {
#ifdef MP3_USE_VS1053
	switch (MP3_State) {
		case MP3_State_Start : {
			uint8 x = 0;
			for (x = 0; x < MP3_FILE_READ_BUFFERCOUNT; x++) {
				mp3FileReadBuffer_Fillness[x] = 0;
			}
			mp3FileReadBuffer_Select = 0;
			mp3FileReadBuffer_Pos = 0;
			vs1053_markBufferAsEmpty = -1;
			MP3_State = MP3_State_Idle;
			break;
		}
		case MP3_State_Idle : {
			if (fatfs_isDiskReady()) {
				if (mp3PlayFile_trigger) {
					mp3PlayFile_trigger = 0;
					MP3_State = MP3_State_OpenFile;
				}
			} else {
			}
			break;
		}
		case MP3_State_OpenFile : {
			FRESULT res;
			res = f_open(&fmp3, (const char *)mp3FileName, FA_OPEN_EXISTING | FA_READ);
			if (res == FR_OK) {
				MP3_State = MP3_State_FillBuffer;
			} else {
				MP3_State = MP3_State_CloseFile;
			}
			break;
		}
		case MP3_State_FillBuffer : {
			if (f_eof(&fmp3) || (mp3MarkEof) || (mp3stopPlaying)) {
				//File is ended, nothing to read.
				//Wait until all buffers are written out
				uint8 x = 0;
				uint8 isSomeBufferFull = 0;
				for (x = 0; x < MP3_FILE_READ_BUFFERCOUNT; x++) {
					if (mp3FileReadBuffer_Fillness[x] != 0) {
						isSomeBufferFull = 1;
						break;
					}
				}
				if (isSomeBufferFull == 0) {
					mp3MarkEof = 0;
					mp3stopPlaying = 0;
					MP3_State = MP3_State_CloseFile;
				}
			} else {
				uint8 x = 0;
				if (mp3pausePlaying == 0) {
					for (x = 0; x < MP3_FILE_READ_BUFFERCOUNT; x++) {
						if (mp3FileReadBuffer_Fillness[x] == 0) {
							FRESULT res;
							UINT readBytes = (sizeof(mp3FileReadBuffer[0]) / sizeof(*mp3FileReadBuffer[0]));
							UINT readenBytes = 0;
							res = f_read(&fmp3, mp3FileReadBuffer[x], readBytes, &readenBytes);
							write_timer(&mp3_failsafe_timer, 1500);
							if (res == FR_OK) {
								if (readenBytes != 0) {
									mp3FileReadBuffer_Fillness[x] = readenBytes;
								} else {
									mp3FileReadBuffer_Fillness[x] = 0;
									mp3MarkEof = 1;
									//File eof?, next cycle will see this
								}
							} else {
								mp3FileReadBuffer_Fillness[x] = 0;
								mp3MarkEof = 1;
								//File is ended, nothing to read. Next cycle will notice eof condition
								break;
							}
						}
					}
				}
			}
			if ((read_timer(&mp3_failsafe_timer) == 0) && (mp3pausePlaying == 0)) {
				uint8 x = 0;
				write_timer(&mp3_failsafe_timer, 1500);
				vc1054_recovery();
				mp3MarkEof = 1;
				for (x = 0; x < MP3_FILE_READ_BUFFERCOUNT; x++) {
					mp3FileReadBuffer_Fillness[x] = 0;
				}
				mp3FileReadBuffer_Select = 0;
				mp3FileReadBuffer_Pos = 0;
				vs1053_markBufferAsEmpty = -1;
			}
			break;
		}
		case MP3_State_CloseFile : {
			f_close(&fmp3);
			MP3_State = MP3_State_Start;
			break;
		}
		default : {
			MP3_State = MP3_State_Start;
			break;
		}
	}
#endif
#ifdef MP3_USE_HELIX
	switch (MP3_State) {
		case MP3_State_Start : {
			bytesLeft = 0;
			MP3_State = MP3_State_Idle;
			break;
		}
		case MP3_State_Idle : {
			if (fatfs_isDiskReady()) {
				if (mp3PlayFile_trigger) {
					mp3PlayFile_trigger = 0;
					MP3_State = MP3_State_OpenFile;
				}
			} else {
			}
			break;
		}
		case MP3_State_OpenFile : {
			FRESULT res;
			res = f_open(&fmp3, (const char *)mp3FileName, FA_OPEN_EXISTING | FA_READ);
			if (res == FR_OK) {
				bytesLeft = 0;
				outBuf_SoundFeeder = -1;
				readPtr = readBuf;
				MP3_State = MP3_State_FillBuffer;
			} else {
				MP3_State = MP3_State_CloseFile;
			}
			break;
		}
		case MP3_State_FillBuffer : {
			if (f_eof(&fmp3)) {
				//File is ended, nothing to read.
				//Wait until all buffers are written out
				uint8 isSomeBufferFull = 0;
				if (bytesLeft != 0) {
					isSomeBufferFull = 1;
				}
				if (isSomeBufferFull == 0) {
					MP3_State = MP3_State_CloseFile;
				} else {
					MP3_State = MP3_State_FindSyncWord;
				}
			} else {
				FRESULT res;
				UINT readBytes = READBUF_SIZE - bytesLeft;
				UINT readenBytes = 0;
				if (readBytes != 0)
				{ 
					memmove(readBuf, readPtr, bytesLeft);				
					res = f_read(&fmp3, readBuf + bytesLeft, readBytes, &readenBytes);
					if (res == FR_OK) {
						if (readenBytes != 0) {
							if (readenBytes < READBUF_SIZE - bytesLeft) {
								memset(readBuf + bytesLeft + readenBytes, 0, READBUF_SIZE - bytesLeft - readenBytes);
							}
							bytesLeft += readenBytes;
							readPtr = readBuf;
							MP3_State = MP3_State_FindSyncWord;
						} else {
							//File eof?, next cycle will see this
						}
					} else {
						//File is ended, nothing to read. Next cycle will notice eof condition
						break;
					}
				} else {
					MP3_State = MP3_State_FindSyncWord;
				}
			}
			break;
		}
   		case MP3_State_FindSyncWord : {
			int offset = MP3FindSyncWord(readPtr, bytesLeft);
			if (offset < 0) {
				//no Sync found, drop bytes and get new
				bytesLeft = 0;
				MP3_State = MP3_State_FillBuffer;
			} else if (offset == 0) {
				MP3_State = MP3_State_DecodeMP3;
			} else {
				readPtr += offset;
				bytesLeft -= offset;
				MP3_State = MP3_State_FillBuffer;
			}
			break;
		}
   		case MP3_State_DecodeMP3 : {
			uint8 sounderBufferState = sound_pwm_IsDataRequested();
			if (sounderBufferState != 0) {//A or B is empty inside the dma sound module
				if (MP3Decode(mp3Decoder, &readPtr, &bytesLeft, outBufA, 0) == 0) {//Decoded ok
					MP3GetLastFrameInfo(mp3Decoder, &mp3FrameInfoA);
					sound_pwm_WriteStream(outBufA, (2 * 1152), mp3FrameInfoA.samprate);
				} else {//Unable to decode skip byte
					readPtr++;
					bytesLeft--;
				}
			}
			MP3_State = MP3_State_FillBuffer;
			break;
		}
		case MP3_State_CloseFile : {
			FRESULT res;
			res = f_close(&fmp3);
			MP3_State = MP3_State_Start;
			break;
		}
		default : {
			MP3_State = MP3_State_Start;
			break;
		}
	}
#endif
}

void isr_mp3_1ms(void) {
	do_mp3_1ms = 1;
}

uint8 mp3PlayFile(char *str) {
	uint8 result = 0;
	if (str != NULL) {
		switch (MP3_State) {
			case MP3_State_Idle : {
				if (mp3PlayFile_trigger == 0) {
					//0:testfile.mp3
					safestrcpy(mp3FileName, str, (sizeof(mp3FileName) / sizeof(*mp3FileName)));
					mp3IsPlaying = 1;
					mp3PlayFile_trigger = 1;
					result = 1;
				}
				break;
			}
			default : {
				break;
			}
		}
	}
	return result;
}

uint8 mp3StopFile(void) {
	uint8 result = 0;
	switch (MP3_State) {
		case MP3_State_Idle : {
			mp3IsPlaying = 0;
			result = 1;
			break;
		}
		case MP3_State_OpenFile :
		case MP3_State_FillBuffer : {
			mp3IsPlaying = 0;
			mp3stopPlaying = 1;
			break;
		}
		default : {
			break;
		}
	}
	return result;
}

uint8 mp3PauseResumeFile(void) {
	uint8 result = 0;
	if (mp3pausePlaying) {
		mp3pausePlaying = 0;
	} else {
		mp3pausePlaying = 1;
	}
	result = 1;
	return result;
}

uint8 mp3IsPlayFinished(void) {
	uint8 result = 0;
	if ((MP3_State == MP3_State_Idle) && (mp3PlayFile_trigger == 0) && (mp3IsPlaying)) {
		result = 1;
	}
	return result;
}
