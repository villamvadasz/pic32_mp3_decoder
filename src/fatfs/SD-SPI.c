/******************************************************************************
 *
 *               Microchip Memory Disk Drive File System
 *
 ******************************************************************************
 * FileName:        SD-SPI.c
 * Dependencies:    SD-SPI.h
 *                  string.h
 *                  FSIO.h
 *                  FSDefs.h
 * Processor:       PIC18/PIC24/dsPIC30/dsPIC33/PIC32
 * Compiler:        C18/C30/C32
 * Company:         Microchip Technology, Inc.
 *
 * Software License Agreement
 *
 * The software supplied herewith by Microchip Technology Incorporated
 * (the "Company") for its PICmicro® Microcontroller is intended and
 * supplied to you, the Company’s customer, for use solely and
 * exclusively on Microchip PICmicro Microcontroller products. The
 * software is owned by the Company and/or its supplier, and is
 * protected under applicable copyright laws. All rights are reserved.
 * Any use in violation of the foregoing restrictions may subject the
 * user to criminal sanctions under applicable laws, as well as to
 * civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN "AS IS" CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
*****************************************************************************
 File Description:

 Change History:
  Rev     Description
  -----   -----------
  1.2.5   Fixed bug in the calculation of the capacity for v1.0 devices
  1.3.0   Improved media initialization sequence, for better card compatibility
          (especially with SDHC cards).
          Implemented SPI optimizations for data transfer rate improvement.
          Added new MDD_SDSPI_AsyncReadTasks() and MDD_SDSPI_AsyncWriteTasks() 
          API functions.  These are non-blocking, state machine based read/write
          handlers capable of considerably improved data throughput, particularly
          for multi-block reads and multi-block writes.
  1.3.2   Modified MDD_SDSPI_AsyncWriteTasks() so pre-erase command only gets
          used for multi-block write scenarios.   
  1.3.4   1) Added support for dsPIC33E & PIC24E controllers.
          2) #include "HardwareProfile.h" is moved up in the order.
          3) "SPI_INTERRUPT_FLAG_ASM" macro has to be defined in "HardwareProfile.h" file
             for PIC18 microcontrollers.Or else an error is generated while building
             the code.
                       "#define SPI_INTERRUPT_FLAG_ASM  PIR1, 3" is removed from SD-SPI.c
          4) Replaced "__C30" usage with "__C30__" .
  1.3.6   1) Modified "FSConfig.h" to "FSconfig.h" in '#include' directive.
          2) Moved 'spiconvalue' variable definition to only C30 usage, as C32
             is not using it.
          3) Modified 'MDD_SDSPI_MediaDetect' function to ensure that CMD0 is sent freshly
             after CS is asserted low. This minimizes the risk of SPI clock pulse master/slave
             syncronization problems.
2015.12.13 Andras Huszti Modified to use own SPI
********************************************************************/
#include <string.h>

#include "k_stdtype.h"
#include "c_fatfs.h"
#include "c_main.h"
#include "GenericTypeDefs.h"
#include "SD-SPI.h"
#ifdef SDSPI_USE_AES
	#include "aes_128.h"
#endif

#include "tmr.h"
#ifdef FATFS_USE_SPI
	#include "spi.h"
	#include "c_spi.h"
#endif
#ifdef FATFS_USE_SPI2
	#include "spi2.h"
	#include "c_spi2.h"
	#define SPI_USER_SDCARD SPI2_USER_SDCARD
	#define spi_calculate_BRG spi2_calculate_BRG
	#define spi_lock spi2_lock
	#define spi_unlock spi2_unlock
	#define spi_reconfigure spi2_reconfigure
	#define spi_readWrite_synch spi2_readWrite_synch
#endif
#ifdef FATFS_USE_SPI3
	#include "spi3.h"
	#include "c_spi3.h"
	#define SPI_USER_SDCARD SPI3_USER_SDCARD
	#define spi_calculate_BRG spi3_calculate_BRG
	#define spi_lock spi3_lock
	#define spi_unlock spi3_unlock
	#define spi_reconfigure spi3_reconfigure
	#define spi_readWrite_synch spi3_readWrite_synch
#endif
#ifdef FATFS_USE_SPI4
	#include "spi4.h"
	#include "c_spi4.h"
	#define SPI_USER_SDCARD SPI4_USER_SDCARD
	#define spi_calculate_BRG spi4_calculate_BRG
	#define spi_lock spi4_lock
	#define spi_unlock spi4_unlock
	#define spi_reconfigure spi4_reconfigure
	#define spi_readWrite_synch spi4_readWrite_synch
#endif
#ifdef FATFS_USE_SPI_SW
	#include "spi_sw.h"
	#include "c_spi_sw.h"
	#define SPI_USER_SDCARD SPI_SW_USER_SDCARD
	#define spi_calculate_BRG spi_sw_calculate_BRG
	#define spi_lock spi_sw_lock
	#define spi_unlock spi_sw_unlock
	#define spi_reconfigure_mode spi_sw_reconfigure_mode
	#define spi_readWrite_synch spi_sw_readWrite_synch
#endif
#include "mal.h"

#define MDD_SDSPI_MEDIADETECT_TIMER 100 //[ms]
#define SD_SPI_USE_BLOCKING_LOW_LEVEL_INIT

typedef enum _SendMMCCmd_Async_Start_states {
	SendMMCCmd_Async_Start_state_0 = 0,
	SendMMCCmd_Async_Start_state_1,
	SendMMCCmd_Async_Start_state_3,
	SendMMCCmd_Async_Start_state_22,
} SendMMCCmd_Async_Start_states;

typedef enum _sd_spi_init_low_states {
	sd_spi_init_low_state_First = 0,
	sd_spi_init_low_state_Slow_1,
	sd_spi_init_low_state_Slow_2,
	sd_spi_init_low_state_Slow_3,
	sd_spi_init_low_state_Slow_4,
	sd_spi_init_low_state_Slow_4_wait,
	sd_spi_init_low_state_try_stop,
	sd_spi_init_low_state_try_stop_wait,
	sd_spi_init_low_state_Init,
	sd_spi_init_low_state_Finish,
	#ifndef SD_SPI_USE_BLOCKING_LOW_LEVEL_INIT
	sd_spi_init_low_state_Finish_wait,
	#endif
} sd_spi_init_low_states;

typedef enum _sd_spi_init_high_states {
	sd_spi_init_high_state_0 = 0,
	sd_spi_init_high_state_1,
	sd_spi_init_high_state_2,
	sd_spi_init_high_state_3,
	sd_spi_init_high_state_31,
	sd_spi_init_high_state_32,
	sd_spi_init_high_state_4,
	sd_spi_init_high_state_5,
} sd_spi_init_high_states;

uint8 do_sd_spi_1ms = 0;
sd_spi_init_low_states sd_spi_init_low_state = sd_spi_init_low_state_First;
sd_spi_init_high_states sd_spi_init_high_state = sd_spi_init_high_state_0;
uint8 sdi_spi_cmd0_retry_cnt = 0;

DWORD sd_spi_finalLBA = 0;
WORD sd_spi_gMediaSectorSize = 0;
BYTE sd_spi_gSDMode = 0;
MEDIA_INFORMATION sd_spi_mediaInformation;
ASYNC_IO sd_spi_ioInfo; //Declared global context, for fast/code efficient access

unsigned char sd_spi_encryptBuffer[512];
unsigned char sd_spi_diskEncryptState = 0;

unsigned int SDSPI_BRG_LOW = 20;
unsigned int SDSPI_BRG_HIGH = 4;
unsigned char sd_spi_bufferOut[10];
unsigned char sd_spi_bufferIn[10];

uint32 sd_spi_MediaDetectStateCnt = MDD_SDSPI_MEDIADETECT_TIMER;//Timer how often detection should run
BYTE sd_spi_MediaDetected = 0;
Timer sd_spi_global_shared_timer;
volatile uint32 sd_spi_runTime = 0;
BYTE SendMMCCmd_Async_Start_cmd;
DWORD SendMMCCmd_Async_Start_address;
MMC_RESPONSE SendMMCCmd_Async_Start_response;
SendMMCCmd_Async_Start_states SendMMCCmd_Async_Start_state = SendMMCCmd_Async_Start_state_0;
volatile uint32 do_sd_spi_fast_polling = 0;

const typMMC_CMD sdmmc_cmdtable[] = {
    // cmd                      crc     response
    {cmdGO_IDLE_STATE,          0x95,   R1,     NODATA},
    {cmdSEND_OP_COND,           0xF9,   R1,     NODATA},
    {cmdSEND_IF_COND,      		0x87,   R7,     NODATA},
    {cmdSEND_CSD,               0xAF,   R1,     MOREDATA},
    {cmdSEND_CID,               0x1B,   R1,     MOREDATA},
    {cmdSTOP_TRANSMISSION,      0xC3,   R1b,    NODATA},
    {cmdSEND_STATUS,            0xAF,   R2,     NODATA},
    {cmdSET_BLOCKLEN,           0xFF,   R1,     NODATA},
    {cmdREAD_SINGLE_BLOCK,      0xFF,   R1,     MOREDATA},
    {cmdREAD_MULTI_BLOCK,       0xFF,   R1,     MOREDATA},
    {cmdWRITE_SINGLE_BLOCK,     0xFF,   R1,     MOREDATA},
    {cmdWRITE_MULTI_BLOCK,      0xFF,   R1,     MOREDATA}, 
    {cmdTAG_SECTOR_START,       0xFF,   R1,     NODATA},
    {cmdTAG_SECTOR_END,         0xFF,   R1,     NODATA},
    {cmdERASE,                  0xDF,   R1b,    NODATA},
    {cmdAPP_CMD,                0x73,   R1,     NODATA},
    {cmdREAD_OCR,               0x25,   R7,     NODATA},
    {cmdCRC_ON_OFF,             0x25,   R1,     NODATA},
    {cmdSD_SEND_OP_COND,        0xFF,   R7,     NODATA}, //Actual response is R3, but has same number of bytes as R7.
    {cmdSET_WR_BLK_ERASE_COUNT, 0xFF,   R1,     NODATA}
};

static void sd_spi_low_level_init(void);
static MMC_RESPONSE SendMMCCmd(BYTE cmd, DWORD address);
static void SendMMCCmd_Async_Start(BYTE cmd, DWORD address);
static uint8 SendMMCCmd_Async_Poll(void);
static MMC_RESPONSE SendMMCCmd_Async_Response(void);
static BYTE MDD_SDSPI_AsyncReadTasks(ASYNC_IO*);
static BYTE MDD_SDSPI_AsyncWriteTasks(ASYNC_IO*);
static unsigned char sd_spi_read_write_spi( unsigned char data_out );
static SPIStateEnum sd_spi_generate_80_clocks_start(void);
static void sd_spi_high_level_init(void);

void init_sd_spi(void) {
	// Turn off the card
	//SD_CD_TRIS = INPUT;            //Card Detect - input
	SD_CS = 1;                     //Initialize Chip Select line
	_sync();
	SD_CS_TRIS = OUTPUT;           //Card Select - output
	_sync();
	//SD_WE_TRIS = INPUT;            //Write Protect - input 
	
	SDSPI_BRG_LOW = spi_calculate_BRG(SPI_FREQUENCY_SLOW);
	SDSPI_BRG_HIGH = spi_calculate_BRG(SPI_FREQUENCY);
	
	init_timer(&sd_spi_global_shared_timer);
	write_timer(&sd_spi_global_shared_timer, 1000);
}

void deinit_sd_spi(void) {
	SD_CS_TRIS = 1;           //Card Select - output
	_sync();
}

void do_sd_spi(void) {
	if ((do_sd_spi_1ms) || (do_sd_spi_fast_polling)) {
		do_sd_spi_1ms = 0;
		sd_spi_runTime = getGlobalTime();
		sd_spi_low_level_init();
		sd_spi_runTime = getGlobalTime() - sd_spi_runTime;
	}
}

void isr_sd_spi_1ms(void) {
	do_sd_spi_1ms = 1;
}

void sd_spi_diskIsEncrypted(void) {
	sd_spi_diskEncryptState = 1;
}

void sd_spi_diskIsDecrypted(void) {
	sd_spi_diskEncryptState = 0;
}

BYTE MDD_SDSPI_MediaDetect(void) {
	return sd_spi_MediaDetected;
}

MEDIA_INFORMATION * MDD_SDSPI_MediaInitialize(void) {
	MEDIA_INFORMATION * result = NULL;
	
	if (sd_spi_mediaInformation.errorCode == MEDIA_NO_ERROR) {
		result = &sd_spi_mediaInformation;
	}
	return result;
}

DWORD MDD_SDSPI_ReadCapacity(void) {
    return (sd_spi_finalLBA);
}

WORD MDD_SDSPI_ReadSectorSize(void) {
    return sd_spi_gMediaSectorSize;
}

BYTE MDD_SDSPI_SectorRead(DWORD sector_addr, BYTE* buffer) {
	BYTE result = FALSE;
    ASYNC_IO info;
    BYTE status;
    
    //Initialize info structure for using the MDD_SDSPI_AsyncReadTasks() function.
    info.wNumBytes = 512;
    info.dwBytesRemaining = 512;
	if (sd_spi_diskEncryptState) {
    	info.pBuffer = sd_spi_encryptBuffer;
	} else {
    	info.pBuffer = buffer;
	}
    info.dwAddress = sector_addr;
    info.bStateVariable = ASYNC_READ_QUEUED;
    
    //Blocking loop, until the state machine finishes reading the sector,
    //or a timeout or other error occurs.  MDD_SDSPI_AsyncReadTasks() will always
    //return either ASYNC_READ_COMPLETE or ASYNC_READ_FAILED eventually 
    //(could take awhile in the case of timeout), so this won't be a totally
    //infinite blocking loop.

	if (spi_lock(SPI_USER_SDCARD) == SPI_USER_SDCARD) {
		{
			unsigned char SMP = 1;
			unsigned char MODE = 0;
			unsigned int BRG = SDSPI_BRG_HIGH;
			spi_reconfigure_mode(SPI_USER_SDCARD, SMP, MODE, BRG);
		}
		while(1) {
			status = MDD_SDSPI_AsyncReadTasks(&info);
			if(status == ASYNC_READ_COMPLETE) {
				if (sd_spi_diskEncryptState) {
					#ifdef SDSPI_USE_AES
						aes_decrypt_ecb(AES_KEY5, sd_spi_encryptBuffer, 512, buffer);
					#endif
				}
				result = TRUE;
				break;
			} else if(status == ASYNC_READ_ERROR) {
				result = FALSE;
				break;
			} 
		}
		spi_unlock(SPI_USER_SDCARD);
	}

	return result;
}

BYTE MDD_SDSPI_SectorWrite(DWORD sector_addr, BYTE* buffer, BYTE allowWriteToZero) {
	BYTE result = FALSE;
    static ASYNC_IO info;
    BYTE status;
    
    if(allowWriteToZero == FALSE)
    {
        if(sector_addr == 0x00000000)
        {
            return FALSE;
        }
    }
    
    //Initialize structure so we write a single sector worth of data.
    info.wNumBytes = 512;
    info.dwBytesRemaining = 512;
	if (sd_spi_diskEncryptState) {
    	info.pBuffer = sd_spi_encryptBuffer;
	} else {
    	info.pBuffer = buffer;
	}
    info.dwAddress = sector_addr;
    info.bStateVariable = ASYNC_WRITE_QUEUED;
	if (sd_spi_diskEncryptState) {
		#ifdef SDSPI_USE_AES
			aes_encrypt_ecb(AES_KEY5, buffer, 512, sd_spi_encryptBuffer);
		#endif
	}
	if (spi_lock(SPI_USER_SDCARD) == SPI_USER_SDCARD) {
		{
			unsigned char SMP = 1;
			unsigned char MODE = 0;
			unsigned int BRG = SDSPI_BRG_HIGH;
			spi_reconfigure_mode(SPI_USER_SDCARD, SMP, MODE, BRG);
		}
		//Repeatedly call the write handler until the operation is complete (or a
		//failure/timeout occurred).
		while(1) {
			status = MDD_SDSPI_AsyncWriteTasks(&info);
			if(status == ASYNC_WRITE_COMPLETE) {
				result = TRUE;
				break;
			} else if(status == ASYNC_WRITE_ERROR) {
				result = FALSE;
				break;
			}
		}
		spi_unlock(SPI_USER_SDCARD);
	}
	return result;
}

BYTE MDD_SDSPI_WriteProtectState(void) {
    return(0);
}

BYTE MDD_SDSPI_ShutdownMedia(void) {
    return 0;
}

static MMC_RESPONSE SendMMCCmd(BYTE cmd, DWORD address) {
	SendMMCCmd_Async_Start(cmd, address);
	while (1) {
		if (SendMMCCmd_Async_Poll() == 1) {
			break;
		}
	}
	return SendMMCCmd_Async_Response();
}

static void SendMMCCmd_Async_Start(BYTE cmd, DWORD address) {
	SendMMCCmd_Async_Start_cmd = cmd;
	SendMMCCmd_Async_Start_address = address;
	SendMMCCmd_Async_Start_state = SendMMCCmd_Async_Start_state_0;
	do_sd_spi_fast_polling = 1;
}

static uint8 SendMMCCmd_Async_Poll(void) {
	static MMC_RESPONSE response;
	static WORD timeout_poll;
	static DWORD longTimeout;
	uint8 result = 0;
	switch (SendMMCCmd_Async_Start_state) {
		case SendMMCCmd_Async_Start_state_0 : {
			CMD_PACKET  CmdPacket;

			memset(&response, 0, sizeof(response));
			timeout_poll = 0;
			longTimeout = 0;
			
			SD_CS = 0;                           //Select card
			_sync();
			// Copy over data
			CmdPacket.cmd        = sdmmc_cmdtable[SendMMCCmd_Async_Start_cmd].CmdCode;
			CmdPacket.address    = SendMMCCmd_Async_Start_address;
			CmdPacket.crc        = sdmmc_cmdtable[SendMMCCmd_Async_Start_cmd].CRC;       // Calc CRC here
			
			CmdPacket.TRANSMIT_BIT = 1;             //Set Tranmission bit
			
			sd_spi_read_write_spi(CmdPacket.cmd);                //Send Command
			sd_spi_read_write_spi(CmdPacket.addr3);              //Most Significant Byte
			sd_spi_read_write_spi(CmdPacket.addr2);
			sd_spi_read_write_spi(CmdPacket.addr1);
			sd_spi_read_write_spi(CmdPacket.addr0);              //Least Significant Byte
			sd_spi_read_write_spi(CmdPacket.crc);                //Send CRC

			//Special handling for CMD12 (STOP_TRANSMISSION).  The very first byte after
			//sending the command packet may contain bogus non-0xFF data.  This 
			//"residual data" byte should not be interpreted as the R1 response byte.
			if(SendMMCCmd_Async_Start_cmd == STOP_TRANSMISSION)
			{
				sd_spi_read_write_spi(0xFF); //Perform dummy read to fetch the residual non R1 byte
			} 

			//Loop until we get a response from the media.  Delay (NCR) could be up 
			//to 8 SPI byte times.  First byte of response is always the equivalent of 
			//the R1 byte, even for R1b, R2, R3, R7 responses.
			timeout_poll = NCR_TIMEOUT;
			SendMMCCmd_Async_Start_state = SendMMCCmd_Async_Start_state_1;
			break;
		}
		case SendMMCCmd_Async_Start_state_1 : {
			response.r1._byte = sd_spi_read_write_spi(0xFF);
			timeout_poll--;
			if ((response.r1._byte == MMC_FLOATING_BUS) && (timeout_poll != 0)) {
				//do it agin
			} else {
				//finished
				//Check if we should read more bytes, depending upon the response type expected.  
				if(sdmmc_cmdtable[SendMMCCmd_Async_Start_cmd].responsetype == R2)
				{
					response.r2._byte1 = response.r1._byte; //We already received the first byte, just make sure it is in the correct location in the struct.
					response.r2._byte0 = sd_spi_read_write_spi(0xFF); //Fetch the second byte of the response.
					SendMMCCmd_Async_Start_state = SendMMCCmd_Async_Start_state_3; 
				}
				else if(sdmmc_cmdtable[SendMMCCmd_Async_Start_cmd].responsetype == R1b)
				{
                    //Keep trying to read from the media, until it signals it is no longer
                    //busy.  It will continuously send 0x00 bytes until it is not busy.
                    //A non-zero value means it is ready for the next command.
                    //The R1b response is received after a CMD12 STOP_TRANSMISSION
                    //command, where the media card may be busy writing its internal buffer
                    //to the flash memory.  This can typically take a few milliseconds, 
                    //with a recommended maximum timeout of 250ms or longer for SD cards.
                    longTimeout = WRITE_TIMEOUT_1MS;
                    SendMMCCmd_Async_Start_state = SendMMCCmd_Async_Start_state_22;
				}
				else if (sdmmc_cmdtable[SendMMCCmd_Async_Start_cmd].responsetype == R7) //also used for response R3 type
				{
					//Fetch the other four bytes of the R3 or R7 response.
					//Note: The SD card argument response field is 32-bit, big endian format.
					//However, the C compiler stores 32-bit values in little endian in RAM.
					//When writing to the _returnVal/argument bytes, make sure the order it 
					//gets stored in is correct.      
					response.r7.bytewise.argument._byte3 = sd_spi_read_write_spi(0xFF);
					response.r7.bytewise.argument._byte2 = sd_spi_read_write_spi(0xFF);
					response.r7.bytewise.argument._byte1 = sd_spi_read_write_spi(0xFF);
					response.r7.bytewise.argument._byte0 = sd_spi_read_write_spi(0xFF);
					SendMMCCmd_Async_Start_state = SendMMCCmd_Async_Start_state_3; 
				} else {
                    SendMMCCmd_Async_Start_state = SendMMCCmd_Async_Start_state_3; 
                }
			}
			break;
		}
		case SendMMCCmd_Async_Start_state_3 : {
			sd_spi_read_write_spi(0xFF);	//Device requires at least 8 clock pulses after 
									//the response has been sent, before if can process
									//the next command.  CS may be high or low.

			// see if we are expecting more data or not
			if(!(sdmmc_cmdtable[SendMMCCmd_Async_Start_cmd].moredataexpected)) {
				SD_CS = 1;
				_sync();
			}

			SendMMCCmd_Async_Start_response = response;

			SendMMCCmd_Async_Start_state = SendMMCCmd_Async_Start_state_0;
			do_sd_spi_fast_polling = 0;
			result = 1;//finished
			break;
		}
		case SendMMCCmd_Async_Start_state_22 : {
			response.r1._byte = sd_spi_read_write_spi(0xFF);
			longTimeout--;
			if (response.r1._byte != 0x00) {
				response.r1._byte = 0x00;
				SendMMCCmd_Async_Start_state = SendMMCCmd_Async_Start_state_3;
			} else if (longTimeout == 0) {
				response.r1._byte = 0xFF;
				SendMMCCmd_Async_Start_state = SendMMCCmd_Async_Start_state_3;
			}

			break;
		}
		default : {
			SendMMCCmd_Async_Start_state = SendMMCCmd_Async_Start_state_0;
			break;
		}
	}

	return result;
}

static MMC_RESPONSE SendMMCCmd_Async_Response(void) {
	return SendMMCCmd_Async_Start_response;
}

static BYTE MDD_SDSPI_AsyncReadTasks(ASYNC_IO* info) {
    BYTE bData;
    MMC_RESPONSE response;
    static WORD sd_spi_blockCounter;
    static DWORD longTimeoutCounter;
    static BOOL SingleBlockRead;
    
    //Check what state we are in, to decide what to do.
    switch(info->bStateVariable)
    {
        case ASYNC_READ_COMPLETE:
            return ASYNC_READ_COMPLETE;
        case ASYNC_READ_QUEUED:
            //Start the read request.  
            
            //Initialize some variables we will use later.
            sd_spi_blockCounter = MEDIA_BLOCK_SIZE; //Counter will be used later for block boundary tracking
            sd_spi_ioInfo = *info; //Get local copy of structure, for quicker access with less code size

            //SDHC cards are addressed on a 512 byte block basis.  This is 1:1 equivalent
            //to LBA addressing.  For standard capacity media, the media is expecting
            //a complete byte address.  Therefore, to convert from the LBA address to the
            //byte address, we need to multiply by 512.
            if (sd_spi_gSDMode == SD_MODE_NORMAL)
            {
                sd_spi_ioInfo.dwAddress <<= 9; //Equivalent to multiply by 512
            }
            if(sd_spi_ioInfo.dwBytesRemaining <= MEDIA_BLOCK_SIZE)
            {
                SingleBlockRead = TRUE;
                response = SendMMCCmd(READ_SINGLE_BLOCK, sd_spi_ioInfo.dwAddress);
            }
            else
            {
                SingleBlockRead = FALSE;
                response = SendMMCCmd(READ_MULTI_BLOCK, sd_spi_ioInfo.dwAddress);
            }
            //Note: SendMMCmd() sends 8 SPI clock cycles after getting the
            //response.  This meets the NAC min timing paramemter, so we don't
            //need additional clocking here.
            
            // Make sure the command was accepted successfully
            if(response.r1._byte != 0x00)
            {
                //Perhaps the card isn't initialized or present.
                info->bStateVariable = ASYNC_READ_ABORT;
                return ASYNC_READ_BUSY; 
            }
            
            //We successfully sent the READ_MULTI_BLOCK command to the media.
            //We now need to keep polling the media until it sends us the data
            //start token byte.
            longTimeoutCounter = NAC_TIMEOUT; //prepare timeout counter for next state
            info->bStateVariable = ASYNC_READ_WAIT_START_TOKEN;
            return ASYNC_READ_BUSY;
        case ASYNC_READ_WAIT_START_TOKEN:
            //In this case, we have already issued the READ_MULTI_BLOCK command
            //to the media, and we need to keep polling the media until it sends
            //us the data start token byte.  This could typically take a 
            //couple/few milliseconds, up to a maximum of 100ms.
            if(longTimeoutCounter != 0x00000000)
            {
                longTimeoutCounter--;
                bData = sd_spi_read_write_spi(0xFF);
                
                if(bData != MMC_FLOATING_BUS)
                {
                    if(bData == DATA_START_TOKEN)
                    {   
                        //We got the start token.  Ready to receive the data
                        //block now.
                        info->bStateVariable = ASYNC_READ_NEW_PACKET_READY;
                        return ASYNC_READ_NEW_PACKET_READY;
                    }
                    else
                    {
                        //We got an unexpected non-0xFF, non-start token byte back?
                        //Some kind of error must have occurred. 
                        info->bStateVariable = ASYNC_READ_ABORT; 
                        return ASYNC_READ_BUSY;
                    }
                }
                else
                {
                    //Media is still busy.  Start token not received yet.
                    return ASYNC_READ_BUSY;
                }
            } 
            else
            {
                //The media didn't respond with the start data token in the timeout
                //interval allowed.  Operation failed.  Abort the operation.
                info->bStateVariable = ASYNC_READ_ABORT; 
                return ASYNC_READ_BUSY;                
            }
            //Should never execute to here
            
        case ASYNC_READ_NEW_PACKET_READY:
            //We have sent the READ_MULTI_BLOCK command and have successfully
            //received the data start token byte.  Therefore, we are ready
            //to receive raw data bytes from the media.
            if(sd_spi_ioInfo.dwBytesRemaining != 0x00000000)
            {
                //Re-update local copy of pointer and number of bytes to read in this
                //call.  These parameters are allowed to change between packets.
                sd_spi_ioInfo.wNumBytes = info->wNumBytes;
           	    sd_spi_ioInfo.pBuffer = info->pBuffer;
           	    
           	    //Update counters for state tracking and loop exit condition tracking.
                sd_spi_ioInfo.dwBytesRemaining -= sd_spi_ioInfo.wNumBytes;
                sd_spi_blockCounter -= sd_spi_ioInfo.wNumBytes;

                //Now read a sd_spi_ioInfo.wNumBytes packet worth of SPI bytes, 
                //and place the received bytes in the user specified pBuffer.
                //This operation directly dictates data thoroughput in the 
                //application, therefore optimized code should be used for each 
                //processor type.
                {
                    //PIC24/dsPIC/PIC32 architecture is efficient with pointers.
                    //Therefore, this code can provide good SPI bus utilization, 
                    //provided the compiler optimization level is 's' or '3'.
                    BYTE* localPointer = sd_spi_ioInfo.pBuffer;
                    WORD localCounter = sd_spi_ioInfo.wNumBytes;
                    spi_readWrite_synch(SPI_USER_SDCARD, NULL, localPointer, localCounter);
                }

                //Check if we have received a multiple of the media block 
                //size (ex: 512 bytes).  If so, the next two bytes are going to 
                //be CRC values, rather than data bytes.  
                if(sd_spi_blockCounter == 0)
                {
                    //Read two bytes to receive the CRC-16 value on the data block.
                    sd_spi_read_write_spi(0xFF);
                    sd_spi_read_write_spi(0xFF);
                    //Following sending of the CRC-16 value, the media may still
                    //need more access time to internally fetch the next block.
                    //Therefore, it will send back 0xFF idle value, until it is
                    //ready.  Then it will send a new data start token, followed
                    //by the next block of useful data.
                    if(sd_spi_ioInfo.dwBytesRemaining != 0x00000000)
                    {
                        info->bStateVariable = ASYNC_READ_WAIT_START_TOKEN;
                    }
                    sd_spi_blockCounter = MEDIA_BLOCK_SIZE;
                    return ASYNC_READ_BUSY;
                }
                    
                return ASYNC_READ_NEW_PACKET_READY;
            }//if(sd_spi_ioInfo.dwBytesRemaining != 0x00000000)
            else
            {
                //We completed the read operation successfully and have returned
                //all data bytes requested.
                //Send CMD12 to let the media know we are finished reading
                //blocks from it, if we sent a multi-block read request earlier.
                if(SingleBlockRead == FALSE)
                {
                    SendMMCCmd(STOP_TRANSMISSION, 0x00000000);
                }
                SD_CS = 1;  //De-select media
                _sync();
                sd_spi_read_write_spi(0xFF);//Send 8 clock cycles
                info->bStateVariable = ASYNC_READ_COMPLETE;
                return ASYNC_READ_COMPLETE;
            }
        case ASYNC_READ_ABORT:
            //If the application firmware wants to cancel a read request.
            info->bStateVariable = ASYNC_READ_ERROR;
            //Send CMD12 to terminate the multi-block read request.
            response = SendMMCCmd(STOP_TRANSMISSION, 0x00000000);
            //Fall through to ASYNC_READ_ERROR/default case.
        case ASYNC_READ_ERROR:
        default:
            //Some error must have happened.
            SD_CS = 1;  //De-select media
            _sync();
            sd_spi_read_write_spi(0xFF);//mSend8ClkCycles
            return ASYNC_READ_ERROR; 
    }//switch(info->stateVariable)    
    
    //Should never get to here.  All pathways should have already returned.
    return ASYNC_READ_ERROR;
}

static BYTE MDD_SDSPI_AsyncWriteTasks(ASYNC_IO* info) {
    static BYTE sd_spi_data_byte;
    static WORD sd_spi_blockCounter;
    static DWORD sd_spi_WriteTimeout;
    static BYTE sd_spi_command;
    DWORD preEraseBlockCount;
    MMC_RESPONSE response;

    
    //Check what state we are in, to decide what to do.
    switch(info->bStateVariable)
    {
        case ASYNC_WRITE_COMPLETE:
            return ASYNC_WRITE_COMPLETE;
        case ASYNC_WRITE_QUEUED:
            //Initiate the write sequence.
            sd_spi_blockCounter = MEDIA_BLOCK_SIZE;    //Initialize counter.  Will be used later for block boundary tracking.

            //Copy input structure into a statically allocated global instance 
            //of the structure, for faster local access of the parameters with 
            //smaller code size.
            sd_spi_ioInfo = *info;

            //Check if we are writing only a single block worth of data, or 
            //multiple blocks worth of data.
            if(sd_spi_ioInfo.dwBytesRemaining <= MEDIA_BLOCK_SIZE)
            {
                sd_spi_command = WRITE_SINGLE_BLOCK;
            }
            else
            {
                sd_spi_command = WRITE_MULTI_BLOCK;
                
                //Compute the number of blocks that we are going to be writing in this multi-block write operation.
                preEraseBlockCount = (sd_spi_ioInfo.dwBytesRemaining >> 9); //Divide by 512 to get the number of blocks to write
                //Always need to erase at least one block.
                if(preEraseBlockCount == 0)
                {
                    preEraseBlockCount++;   
                } 
    
                //Should send CMD55/ACMD23 to let the media know how many blocks it should 
                //pre-erase.  This isn't essential, but it allows for faster multi-block 
                //writes, and probably also reduces flash wear on the media.
                response = SendMMCCmd(APP_CMD, 0x00000000);    //Send CMD55
                if(response.r1._byte == 0x00)   //Check if successful.
                {
                    SendMMCCmd(SET_WR_BLK_ERASE_COUNT , preEraseBlockCount);    //Send ACMD23        
                }
            }

            //The info->dwAddress parameter is the block address.
            //For standard capacity SD cards, the card expects a complete byte address.
            //To convert the block address into a byte address, we multiply by the block size (512).
            //For SDHC (high capacity) cards, the card expects a block address already, so no
            //address cconversion is needed
            if (sd_spi_gSDMode == SD_MODE_NORMAL)  
            {
                sd_spi_ioInfo.dwAddress <<= 9;   //<< 9 multiplies by 512
            }

            //Send the write single or write multi command, with the LBA or byte 
            //address (depeding upon SDHC or standard capacity card)
            response = SendMMCCmd(sd_spi_command, sd_spi_ioInfo.dwAddress);    

            //See if it was accepted
            if(response.r1._byte != 0x00)
            {
                //Perhaps the card isn't initialized or present.
                info->bStateVariable = ASYNC_WRITE_ERROR;
                return ASYNC_WRITE_ERROR; 
            }
            else
            {
                //Card is ready to receive start token and data bytes.
                info->bStateVariable = ASYNC_WRITE_TRANSMIT_PACKET;
            } 
            return ASYNC_WRITE_SEND_PACKET;   

        case ASYNC_WRITE_TRANSMIT_PACKET:
            //Check if we just finished programming a block, or we are starting
            //for the first time.  In this case, we need to send the data start token.
            if(sd_spi_blockCounter == MEDIA_BLOCK_SIZE)
            {
                //Send the correct data start token, based on the type of write we are doing
                if(sd_spi_command == WRITE_MULTI_BLOCK)
                {
					sd_spi_read_write_spi(DATA_START_MULTI_BLOCK_TOKEN);   
                }
                else
                {
                    //Else must be a single block write
                    sd_spi_read_write_spi(DATA_START_TOKEN);   
                }
            } 
               
            //Update local copy of pointer and byte count.  Application firmware
            //is alllowed to change these between calls to this handler function.
            sd_spi_ioInfo.wNumBytes = info->wNumBytes;
            sd_spi_ioInfo.pBuffer = info->pBuffer;
            
            //Keep track of variables for loop/state exit conditions.
            sd_spi_ioInfo.dwBytesRemaining -= sd_spi_ioInfo.wNumBytes;
            sd_spi_blockCounter -= sd_spi_ioInfo.wNumBytes;
            
            //Now send a packet of raw data bytes to the media, over SPI.
            //This code directly impacts data thoroughput in a significant way.  
            //Special care should be used to make sure this code is speed optimized.
            {
                //PIC24/dsPIC/PIC32 architecture is efficient with pointers and 
                //local variables due to the large number of WREGs available.
                //Therefore, this code gives good SPI bus utilization, provided
                //the compiler optimization level is 's' or '3'.
                BYTE* localPointer = sd_spi_ioInfo.pBuffer;    
                WORD localCounter = sd_spi_ioInfo.wNumBytes;
				spi_readWrite_synch(SPI_USER_SDCARD, localPointer, NULL, localCounter);        
            }
 
            //Check if we have finshed sending a 512 byte block.  If so,
            //need to receive 16-bit CRC, and retrieve the data_response token
            if(sd_spi_blockCounter == 0)
            {
                sd_spi_blockCounter = MEDIA_BLOCK_SIZE;    //Re-initialize counter
                
                //Add code to compute CRC, if using CRC. By default, the media 
                //doesn't use CRC unless it is enabled manually during the card 
                //initialization sequence.
                sd_spi_read_write_spi(0xFF);
				sd_spi_read_write_spi(0xFF);//Send 16-bit CRC for the data block just sent
                
                //Read response token byte from media, mask out top three don't 
                //care bits, and check if there was an error
                if((sd_spi_read_write_spi(0xFF) & WRITE_RESPONSE_TOKEN_MASK) != DATA_ACCEPTED)
                {
                    //Something went wrong.  Try and terminate as gracefully as 
                    //possible, so as allow possible recovery.
                    info->bStateVariable = ASYNC_WRITE_ABORT; 
                    return ASYNC_WRITE_BUSY;
                }
                
                //The media will now send busy token (0x00) bytes until
                //it is internally ready again (after the block is successfully
                //writen and the card is ready to accept a new block.
                info->bStateVariable = ASYNC_WRITE_MEDIA_BUSY;
                sd_spi_WriteTimeout = WRITE_TIMEOUT;       //Initialize timeout counter
                return ASYNC_WRITE_BUSY;
            }//if(sd_spi_blockCounter == 0)
            
            //If we get to here, we haven't reached a block boundary yet.  Keep 
            //on requesting packets of data from the application.
            return ASYNC_WRITE_SEND_PACKET;   

        case ASYNC_WRITE_MEDIA_BUSY:
            if(sd_spi_WriteTimeout != 0)
            {
                sd_spi_WriteTimeout--;
                sd_spi_read_write_spi(0xFF);//mSend8ClkCycles();  //Dummy read to gobble up a byte (ex: to ensure we meet NBR timing parameter)
                sd_spi_data_byte = sd_spi_read_write_spi(0xFF);  //Poll the media.  Will return 0x00 if still busy.  Will return non-0x00 is ready for next data block.
                if(sd_spi_data_byte != 0x00)
                {
                    //The media is done and is no longer busy.  Go ahead and
                    //either send the next packet of data to the media, or the stop
                    //token if we are finshed.
                    if(sd_spi_ioInfo.dwBytesRemaining == 0)
                    {
                        sd_spi_WriteTimeout = WRITE_TIMEOUT;
                        if(sd_spi_command == WRITE_MULTI_BLOCK)
                        {
                            //We finished sending all bytes of data.  Send the stop token byte.
                            sd_spi_read_write_spi(DATA_STOP_TRAN_TOKEN);
                            //After sending the stop transmission token, we need to
                            //gobble up one byte before checking for media busy (0x00).
                            //This is to meet the NBR timing parameter.  During the NBR
                            //interval the SD card may not respond with the busy signal, even
                            //though it is internally busy.
                            sd_spi_read_write_spi(0xFF);//mSend8ClkCycles();
                                                
                            //The media still needs to finish internally writing.
                            info->bStateVariable = ASYNC_STOP_TOKEN_SENT_WAIT_BUSY;
                            return ASYNC_WRITE_BUSY;
                        }
                        else
                        {
                            //In this case we were doing a single block write,
                            //so no stop token is necessary.  In this case we are
                            //now fully complete with the write operation.
                            SD_CS = 1;          //De-select media
                            _sync();
                            sd_spi_read_write_spi(0xFF);//mSend8ClkCycles();  
                            info->bStateVariable = ASYNC_WRITE_COMPLETE;
                            return ASYNC_WRITE_COMPLETE;                            
                        }
                        
                    }
                    //Else we have more data to write in the multi-block write.    
                    info->bStateVariable = ASYNC_WRITE_TRANSMIT_PACKET;  
                    return ASYNC_WRITE_SEND_PACKET;                    
                }
                else
                {
                    //The media is still busy.
                    return ASYNC_WRITE_BUSY;
                }
            }
            else
            {
                //Timeout occurred.  Something went wrong.  The media should not 
                //have taken this long to finish the write.
                info->bStateVariable = ASYNC_WRITE_ABORT;
                return ASYNC_WRITE_BUSY;
            }
        
        case ASYNC_STOP_TOKEN_SENT_WAIT_BUSY:
            //We already sent the stop transmit token for the multi-block write 
            //operation.  Now all we need to do, is keep waiting until the card
            //signals it is no longer busy.  Card will keep sending 0x00 bytes
            //until it is no longer busy.
            if(sd_spi_WriteTimeout != 0)
            {
                sd_spi_WriteTimeout--;
                sd_spi_data_byte = sd_spi_read_write_spi(0xFF);
                //Check if card is no longer busy.  
                if(sd_spi_data_byte != 0x00)
                {
                    //If we get to here, multi-block write operation is fully
                    //complete now.  

                    //Should send CMD13 (SEND_STATUS) after a programming sequence, 
                    //to confirm if it was successful or not inside the media.
                                
                    //Prepare to receive the next command.
                    SD_CS = 1;          //De-select media
                    _sync();
                    sd_spi_read_write_spi(0xFF);//mSend8ClkCycles();  //NEC timing parameter clocking
                    info->bStateVariable = ASYNC_WRITE_COMPLETE;
                    return ASYNC_WRITE_COMPLETE;
                }
                //If we get to here, the media is still busy with the write.
                return ASYNC_WRITE_BUSY;    
            }
            //Timeout occurred.  Something went wrong.  Fall through to ASYNC_WRITE_ABORT.
        case ASYNC_WRITE_ABORT:
            //An error occurred, and we need to stop the write sequence so as to try and allow
            //for recovery/re-attempt later.
            SendMMCCmd(STOP_TRANSMISSION, 0x00000000);
            SD_CS = 1;  //deselect media
            _sync();
            sd_spi_read_write_spi(0xFF);//mSend8ClkCycles();  //After raising CS pin, media may not tri-state data out for 1 bit time.
            info->bStateVariable = ASYNC_WRITE_ERROR; 
            //Fall through to default case.
        default:
            //Used for ASYNC_WRITE_ERROR case.
            return ASYNC_WRITE_ERROR; 
    }//switch(info->stateVariable)    
    

    //Should never execute to here.  All pathways should have a hit a return already.
    info->bStateVariable = ASYNC_WRITE_ABORT;
    return ASYNC_WRITE_BUSY;
} 

static unsigned char sd_spi_read_write_spi(unsigned char data_out ) {
	unsigned int size = 1;
	sd_spi_bufferOut[0] = data_out;
	spi_readWrite_synch(SPI_USER_SDCARD, sd_spi_bufferOut, sd_spi_bufferIn, size);
	return ( sd_spi_bufferIn[0] );
}

static SPIStateEnum sd_spi_generate_80_clocks_start(void) {
	unsigned int size = 10;
	memset(sd_spi_bufferOut, 0xFF, 10);
	memset(sd_spi_bufferIn, 0xFF, 10);
	return spi_readWrite_asynch(SPI_USER_SDCARD, sd_spi_bufferOut, sd_spi_bufferIn, size);
}

static void sd_spi_low_level_init(void) {
	MMC_RESPONSE    response;
	
	switch (sd_spi_init_low_state) {
		case sd_spi_init_low_state_First : {
			//First check if SPI module is enabled or not.
			if (sd_spi_MediaDetected == 0) {
				if (read_timer(&sd_spi_global_shared_timer) == 0) {
					sd_spi_init_low_state = sd_spi_init_low_state_Slow_1;
				}
			} else {
				if (sd_spi_MediaDetectStateCnt != 0) {
					sd_spi_MediaDetectStateCnt--;
				} else {
					sd_spi_MediaDetectStateCnt = MDD_SDSPI_MEDIADETECT_TIMER;
					sd_spi_init_low_state = sd_spi_init_low_state_Finish;
				}
			}
			break;
		}
		case sd_spi_init_low_state_Slow_1 : {
			if (spi_lock(SPI_USER_SDCARD) == SPI_USER_SDCARD) {
				{
					unsigned char SMP = 1;
					unsigned char MODE = 0;
					unsigned int BRG = SDSPI_BRG_LOW;
					spi_reconfigure_mode(SPI_USER_SDCARD, SMP, MODE, BRG);
				}
				sd_spi_init_low_state = sd_spi_init_low_state_Slow_2;
			}
			break;
		}
		case sd_spi_init_low_state_Slow_2 : {
			SD_CS = 1;
			_sync();
			sd_spi_generate_80_clocks_start();   //Send some "extraneous" clock pulses.  If a previous
												//command was terminated before it completed normally,
												//the card might not have received the required clocking
												//following the transfer.
			sd_spi_init_low_state = sd_spi_init_low_state_Slow_3;
			break;
		}
		case sd_spi_init_low_state_Slow_3 : {
			if (spi_get_state() != SPI_BUSY_STATE) {
				sd_spi_init_low_state = sd_spi_init_low_state_Slow_4;
			}
			break;
		}
		case sd_spi_init_low_state_Slow_4 : {
			SD_CS = 0;
			_sync();
			sdi_spi_cmd0_retry_cnt++;
			SendMMCCmd_Async_Start(GO_IDLE_STATE, 0x0);	//CMD0
			sd_spi_init_low_state = sd_spi_init_low_state_Slow_4_wait;
			break;
		}
		case sd_spi_init_low_state_Slow_4_wait : {
			if (SendMMCCmd_Async_Poll() == 1) {
				response = SendMMCCmd_Async_Response();
				SD_CS = 1;
				_sync();

				//Check if response was invalid (R1 response byte should be = 0x01 after GO_IDLE_STATE)
				if (response.r1._byte == 0x01) {
					//Card is presumably present.  The SDI pin should have a pull up resistor on
					//it, so the odds of SDI "floating" to 0x01 after sending CMD0 is very
					//remote, unless the media is genuinely present.  Therefore, we should
					//try to perform a full card initialization sequence now.
					sdi_spi_cmd0_retry_cnt = 0;
					sd_spi_init_low_state = sd_spi_init_low_state_Init;
				} else {
					//Check if all attempts failed and we timed out.  Normally, this won't happen,
					//unless maybe the SD card was busy, because it was previously performing a
					//read or write operation, when it was interrupted by the microcontroller getting
					//reset or power cycled, without also resetting or power cycling the SD card.
					//In this case, the SD card may still be busy (ex: trying to respond with the 
					//read request data), and may not be ready to process CMD0.  In this case,
					//we can try to recover by issuing CMD12 (STOP_TRANSMISSION).
					if (sdi_spi_cmd0_retry_cnt >= 10) {
						sdi_spi_cmd0_retry_cnt = 0;
						sd_spi_init_low_state = sd_spi_init_low_state_try_stop;
					} else {
						sd_spi_init_low_state = sd_spi_init_low_state_Slow_2;
					}
				}
			}
			break;
		}
		case sd_spi_init_low_state_try_stop : {
			SD_CS = 0;
			_sync();
			SendMMCCmd_Async_Start(STOP_TRANSMISSION, 0x0);
			sd_spi_init_low_state = sd_spi_init_low_state_try_stop_wait;
			break;
		}
		case sd_spi_init_low_state_try_stop_wait : {
			if (SendMMCCmd_Async_Poll() == 1) {
				response = SendMMCCmd_Async_Response();
				SD_CS = 1;
				_sync();
				//Now retry to send send CMD0 to perform software reset on the media
				spi_unlock(SPI_USER_SDCARD);
				sd_spi_init_low_state = sd_spi_init_low_state_First;
			}
			break;
		}
		case sd_spi_init_low_state_Init : {
			sd_spi_high_level_init();    //Can block and take a long time to execute.
			if (sd_spi_mediaInformation.errorCode == MEDIA_BUSY) {
				//Need to wait since asynch is still running.
			} else if (sd_spi_mediaInformation.errorCode == MEDIA_NO_ERROR) {
				/* if the card was initialized correctly, it means it is present */
				spi_unlock(SPI_USER_SDCARD);
				sd_spi_init_low_state = sd_spi_init_low_state_Finish;
				break;
			} else {
				spi_unlock(SPI_USER_SDCARD);
				sd_spi_init_low_state = sd_spi_init_low_state_First;
				break;
			}
			break;
		}
		case sd_spi_init_low_state_Finish : {
			#ifdef SD_SPI_USE_BLOCKING_LOW_LEVEL_INIT
				if (spi_lock(SPI_USER_SDCARD) == SPI_USER_SDCARD) {
					{
						unsigned char SMP = 1;
						unsigned char MODE = 0;
						unsigned int BRG = SDSPI_BRG_HIGH;
						spi_reconfigure_mode(SPI_USER_SDCARD, SMP, MODE, BRG);
					}
					//The SPI module was already enabled.  This most likely means the media is
					//present and has already been initialized.  However, it is possible that
					//the user could have unplugged the media, in which case it is no longer
					//present.  We should send it a command, to check the status.
					response = SendMMCCmd(SEND_STATUS, 0x0);
					if((response.r2._word & 0xEC0C) != 0x0000) {
						sd_spi_MediaDetected = 0;
					} else {
						//The CMD13 response to SEND_STATUS was valid.  This presumably
						//means the card is present and working normally.
						sd_spi_MediaDetected = 1;
					}
					spi_unlock(SPI_USER_SDCARD);
					sd_spi_init_low_state = sd_spi_init_low_state_First;
				}
			#else
				if (spi_lock(SPI_USER_SDCARD) == SPI_USER_SDCARD) {
					{
						unsigned char SMP = 1;
						unsigned char MODE = 0;
						unsigned int BRG = SDSPI_BRG_HIGH;
						spi_reconfigure_mode(SPI_USER_SDCARD, SMP, MODE, BRG);
					}
					//The SPI module was already enabled.  This most likely means the media is
					//present and has already been initialized.  However, it is possible that
					//the user could have unplugged the media, in which case it is no longer
					//present.  We should send it a command, to check the status.
					SendMMCCmd_Async_Start(SEND_STATUS, 0x0);
					sd_spi_init_low_state = sd_spi_init_low_state_Finish_wait;
				}
			#endif
			break;
		}
		#ifndef SD_SPI_USE_BLOCKING_LOW_LEVEL_INIT
		case sd_spi_init_low_state_Finish_wait : {
			if (SendMMCCmd_Async_Poll() == 1) {
				response = SendMMCCmd_Async_Response();
				if((response.r2._word & 0xEC0C) != 0x0000) {
					sd_spi_MediaDetected = 0;
				} else {
					//The CMD13 response to SEND_STATUS was valid.  This presumably
					//means the card is present and working normally.
					sd_spi_MediaDetected = 1;
				}
				spi_unlock(SPI_USER_SDCARD);
				sd_spi_init_low_state = sd_spi_init_low_state_First;
			}
			break;
		}
		#endif
		default : {
			sd_spi_init_low_state = sd_spi_init_low_state_First;
			break;
		}
	}
}

static void sd_spi_high_level_init(void) {
	WORD timeout;
	BYTE CSDResponse[20];
	BYTE count;
	BYTE index;
	DWORD c_size;
	BYTE c_size_mult;
	BYTE block_len;
	MMC_RESPONSE response;

	switch (sd_spi_init_high_state) {
		case sd_spi_init_high_state_0 : {
			//Initialize global variables.  Will get updated later with valid data once
			//the data is known.
			sd_spi_mediaInformation.errorCode = MEDIA_BUSY;
			sd_spi_mediaInformation.validityFlags.value = 0;
			sd_spi_finalLBA = 0x00000000;	//Will compute a valid size later, from the CSD register values we get from the card
			sd_spi_gSDMode = SD_MODE_NORMAL;           //Will get updated later with real value, once we know based on initialization flow.

			sd_spi_init_high_state = sd_spi_init_high_state_1;
			break;
		}
		case sd_spi_init_high_state_1 : {
			//Send CMD8 (SEND_IF_COND) to specify/request the SD card interface condition (ex: indicate what voltage the host runs at).
			//0x000001AA --> VHS = 0001b = 2.7V to 3.6V.  The 0xAA LSB is the check pattern, and is arbitrary, but 0xAA is recommended (good blend of 0's and '1's).
			//The SD card has to echo back the check pattern correctly however, in the R7 response.
			//If the SD card doesn't support the operating voltage range of the host, then it may not respond.
			//If it does support the range, it will respond with a type R7 reponse packet (6 bytes/48 bits).	        
			//Additionally, if the SD card is MMC or SD card v1.x spec device, then it may respond with
			//invalid command.  If it is a v2.0 spec SD card, then it is mandatory that the card respond
			//to CMD8.
			response = SendMMCCmd(SEND_IF_COND, 0x1AA);   //Note: If changing "0x1AA", CRC value in table must also change.
			if(((response.r7.bytewise.argument._returnVal & 0xFFF) == 0x1AA) && (!response.r7.bitwise.bits.ILLEGAL_CMD))
			{
				//If we get to here, the device supported the CMD8 command and didn't complain about our host
				//voltage range.
				//Most likely this means it is either a v2.0 spec standard or high capacity SD card (SDHC)
	
				//Send CMD58 (Read OCR [operating conditions register]).  Reponse type is R3, which has 5 bytes.
				//Byte 4 = normal R1 response byte, Bytes 3-0 are = OCR register value.
				response = SendMMCCmd(READ_OCR, 0x0);//CMD58
				//Now that we have the OCR register value in the reponse packet, we could parse
				//the register contents and learn what voltage the SD card wants to run at.
				//If our host circuitry has variable power supply capability, it could 
				//theoretically adjust the SD card Vdd to the minimum of the OCR to save power.
				write_timer(&sd_spi_global_shared_timer, 2000);
				sd_spi_init_high_state = sd_spi_init_high_state_2;
			}
			else
			{
				SD_CS = 1;                              // deselect the devices
				_sync();
				sd_spi_init_high_state = sd_spi_init_high_state_3;
			}
			break;
		}
		case sd_spi_init_high_state_2 : {
			//Now send CMD55/ACMD41 in a loop, until the card is finished with its internal initialization.
			//Note: SD card specs recommend >= 1 second timeout while waiting for ACMD41 to signal non-busy.
			if (read_timer(&sd_spi_global_shared_timer) != 0) {
				//Send CMD55 (lets SD card know that the next command is application specific (going to be ACMD41)).
				SendMMCCmd(APP_CMD, 0x00000000);
				
				//Send ACMD41.  This is to check if the SD card is finished booting up/ready for full frequency and all
				//further commands.  Response is R3 type (6 bytes/48 bits, middle four bytes contain potentially useful data).
				//Note: When sending ACMD41, the HCS bit is bit 30, and must be = 1 to tell SD card the host supports SDHC
				response = SendMMCCmd(SD_SEND_OP_COND,0x40000000); //bit 30 set
				
				//The R1 response should be = 0x00, meaning the card is now in the "standby" state, instead of
				//the "idle" state (which is the default initialization state after CMD0 reset is issued).  Once
				//in the "standby" state, the SD card is finished with basic intitialization and is ready 
				//for read/write and other commands.
				if(response.r1._byte == 0)
				{
					//Now send CMD58 (Read OCR register).  The OCR register contains important
					//info we will want to know about the card (ex: standard capacity vs. SDHC).
					response = SendMMCCmd(READ_OCR, 0x0); 

					//Now check the CCS bit (OCR bit 30) in the OCR register, which is in our response packet.
					//This will tell us if it is a SD high capacity (SDHC) or standard capacity device.
					if(response.r7.bytewise.argument._returnVal & 0x40000000)    //Note the HCS bit is only valid when the busy bit is also set (indicating device ready).
					{
						sd_spi_gSDMode = SD_MODE_HC;
					}				
					else
					{
						sd_spi_gSDMode = SD_MODE_NORMAL;
					} 
					//SD Card should now be finished with initialization sequence.  Device should be ready
					//for read/write commands.
					//Temporarily deselect device
					SD_CS = 1;
					_sync();
					sd_spi_init_high_state = sd_spi_init_high_state_4;
				}	
			} else {
				sd_spi_mediaInformation.errorCode = MEDIA_CANNOT_INITIALIZE;
				sd_spi_init_high_state = sd_spi_init_high_state_0;
			}
			break;
		}
		case sd_spi_init_high_state_3 : {
			//The CMD8 wasn't supported.  This means the card is not a v2.0 card.
			//Presumably the card is v1.x device, standard capacity (not SDHC).
			write_timer(&sd_spi_global_shared_timer, 20);
			sd_spi_init_high_state = sd_spi_init_high_state_31;
			break;
		}
		case sd_spi_init_high_state_31 : {
			if (read_timer(&sd_spi_global_shared_timer) == 0) {
				//The CMD8 wasn't supported.  This means the card is definitely not a v2.0 SDHC card.
				sd_spi_gSDMode = SD_MODE_NORMAL;
				// According to the spec CMD1 must be repeated until the card is fully initialized
				write_timer(&sd_spi_global_shared_timer, 2000);
				sd_spi_init_high_state = sd_spi_init_high_state_32;
			}
			break;
		}
		case sd_spi_init_high_state_32 : {
			if (read_timer(&sd_spi_global_shared_timer) != 0) {
				SD_CS = 0;                              // select the device
				_sync();
				//Send CMD1 to initialize the media.
				response = SendMMCCmd(SEND_OP_COND, 0x00000000);    //When argument is 0x00000000, this queries MMC cards for operating voltage range
				if (response.r1._byte == 0x00) {
					//Set read/write block length to 512 bytes.  Note: commented out since
					//this theoretically isn't necessary, since all cards v1 and v2 are 
					//required to support 512 byte block size, and this is supposed to be
					//the default size selected on cards that support other sizes as well.
					//response = SendMMCCmd(SET_BLOCKLEN, 0x00000200);    //Set read/write block length to 512 bytes
					
					sd_spi_init_high_state = sd_spi_init_high_state_4;
				} else {
					//Retry
				}
				SD_CS = 1;
				_sync();
			} else {
				sd_spi_mediaInformation.errorCode = MEDIA_CANNOT_INITIALIZE;
				sd_spi_init_high_state = sd_spi_init_high_state_0;
			}
			break;
		}
		case sd_spi_init_high_state_4 : {
			//Basic initialization of media is now complete.  The card will now use push/pull
			//outputs with fast drivers.  Therefore, we can now increase SPI speed to 
			//either the maximum of the microcontroller or maximum of media, whichever 
			//is slower.  MMC media is typically good for at least 20Mbps SPI speeds.  
			//SD cards would typically operate at up to 25Mbps or higher SPI speeds.
			{
				unsigned char SMP = 1;
				unsigned char MODE = 0;
				unsigned int BRG = SDSPI_BRG_HIGH;
				spi_reconfigure_mode(SPI_USER_SDCARD, SMP, MODE, BRG);
			}
			SD_CS = 0;
			_sync();

			/* Send the CMD9 to read the CSD register */
			timeout = NCR_TIMEOUT;
			do
			{
				//Send CMD9: Read CSD data structure.
				response = SendMMCCmd(SEND_CSD, 0x00);
				timeout--;
			} while((response.r1._byte != 0x00) && (timeout != 0));
			if(timeout != 0x00)
			{
			}
			else
			{
				//Media failed to respond to the read CSD register operation.
				sd_spi_mediaInformation.errorCode = MEDIA_CANNOT_INITIALIZE;
				SD_CS = 1;
				_sync();
				sd_spi_init_high_state = sd_spi_init_high_state_0;
				break;
			}

			/* According to the simplified spec, section 7.2.6, the card will respond
			with a standard response token, followed by a data block of 16 bytes
			suffixed with a 16-bit CRC.*/
			index = 0;
			for (count = 0; count < 20u; count ++)
			{
				CSDResponse[index] = sd_spi_read_write_spi(0xFF);
				index++;			
				/* Hopefully the first byte is the datatoken, however, some cards do
				not send the response token before the CSD register.*/
				if((count == 0) && (CSDResponse[0] == DATA_START_TOKEN))
				{
					/* As the first byte was the datatoken, we can drop it. */
					index = 0;
				}
			}
			//memcpy(CSDResponseStructured._byte, CSDResponse, 16);


			//Extract some fields from the response for computing the card capacity.
			//Note: The structure format depends on if it is a CSD V1 or V2 device.
			//Therefore, need to first determine version of the specs that the card 
			//is designed for, before interpreting the individual fields.

			//-------------------------------------------------------------
			//READ_BL_LEN: CSD Structure v1 cards always support 512 byte
			//read and write block lengths.  Some v1 cards may optionally report
			//READ_BL_LEN = 1024 or 2048 bytes (and therefore WRITE_BL_LEN also 
			//1024 or 2048).  However, even on these cards, 512 byte partial reads
			//and 512 byte write are required to be supported.
			//On CSD structure v2 cards, it is always required that READ_BL_LEN 
			//(and therefore WRITE_BL_LEN) be 512 bytes, and partial reads and
			//writes are not allowed.
			//Therefore, all cards support 512 byte reads/writes, but only a subset
			//of cards support other sizes.  For best compatibility with all cards,
			//and the simplest firmware design, it is therefore preferrable to 
			//simply ignore the READ_BL_LEN and WRITE_BL_LEN values altogether,
			//and simply hardcode the read/write block size as 512 bytes.
			//-------------------------------------------------------------
			sd_spi_gMediaSectorSize = 512u;
			sd_spi_mediaInformation.sectorSize = sd_spi_gMediaSectorSize;
			sd_spi_mediaInformation.validityFlags.bits.sectorSize = TRUE;
			//-------------------------------------------------------------

			//Calculate the sd_spi_finalLBA (see SD card physical layer simplified spec 2.0, section 5.3.2).
			//In USB mass storage applications, we will need this information to 
			//correctly respond to SCSI get capacity requests.  Note: method of computing 
			//sd_spi_finalLBA depends on CSD structure spec version (either v1 or v2).
			if(CSDResponse[0] & 0xC0)	//Check CSD_STRUCTURE field for v2+ struct device
			{
				//Must be a v2 device (or a reserved higher version, that doesn't currently exist)

				//Extract the C_SIZE field from the response.  It is a 22-bit number in bit position 69:48.  This is different from v1.  
				//It spans bytes 7, 8, and 9 of the response.
				c_size = (((DWORD)CSDResponse[7] & 0x3F) << 16) | ((WORD)CSDResponse[8] << 8) | CSDResponse[9];
				
				sd_spi_finalLBA = ((DWORD)(c_size + 1) * (WORD)(1024u)) - 1; //-1 on end is correction factor, since LBA = 0 is valid.
			}
			else //if(CSDResponse[0] & 0xC0)	//Check CSD_STRUCTURE field for v1 struct device
			{
				//Must be a v1 device.
				//Extract the C_SIZE field from the response.  It is a 12-bit number in bit position 73:62.  
				//Although it is only a 12-bit number, it spans bytes 6, 7, and 8, since it isn't byte aligned.
				c_size = ((DWORD)CSDResponse[6] << 16) | ((WORD)CSDResponse[7] << 8) | CSDResponse[8];	//Get the bytes in the correct positions
				c_size &= 0x0003FFC0;	//Clear all bits that aren't part of the C_SIZE
				c_size = c_size >> 6;	//Shift value down, so the 12-bit C_SIZE is properly right justified in the DWORD.
				
				//Extract the C_SIZE_MULT field from the response.  It is a 3-bit number in bit position 49:47.
				c_size_mult = ((WORD)((CSDResponse[9] & 0x03) << 1)) | ((WORD)((CSDResponse[10] & 0x80) >> 7));

				//Extract the BLOCK_LEN field from the response. It is a 4-bit number in bit position 83:80.
				block_len = CSDResponse[5] & 0x0F;

				block_len = 1 << (block_len - 9); //-9 because we report the size in sectors of 512 bytes each
				
				//Calculate the sd_spi_finalLBA (see SD card physical layer simplified spec 2.0, section 5.3.2).
				//In USB mass storage applications, we will need this information to 
				//correctly respond to SCSI get capacity requests (which will cause MDD_SDSPI_ReadCapacity() to get called).
				sd_spi_finalLBA = ((DWORD)(c_size + 1) * (WORD)((WORD)1 << (c_size_mult + 2)) * block_len) - 1;	//-1 on end is correction factor, since LBA = 0 is valid.		
			}	

			//Turn off CRC7 if we can, might be an invalid cmd on some cards (CMD59)
			//Note: POR default for the media is normally with CRC checking off in SPI 
			//mode anyway, so this is typically redundant.
			SendMMCCmd(CRC_ON_OFF, 0x0);

			//Now set the block length to media sector size. It should be already set to this.
			SendMMCCmd(SET_BLOCKLEN, sd_spi_gMediaSectorSize);

			//Deselect media while not actively accessing the card.
			SD_CS = 1;
			_sync();

			sd_spi_mediaInformation.errorCode = MEDIA_NO_ERROR;
			sd_spi_init_high_state = sd_spi_init_high_state_0;
			break;
		}
		
		default : { 
			sd_spi_init_high_state = sd_spi_init_high_state_0;
			break;
		}
	}
}
