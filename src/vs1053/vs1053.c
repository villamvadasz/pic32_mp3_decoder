#include "vs1053.h"
#include "c_vs1053.h"

#include "mal.h"
#include "tmr.h"

#include "fatfs.h"

#include "spi_sw.h"
#include "c_spi_sw.h"
#define SPI_USER_VS1053 SPI_SW_USER_VS1053
#define spi_calculate_BRG spi_sw_calculate_BRG
#define spi_lock spi_sw_lock
#define spi_unlock spi_sw_unlock
#define spi_reconfigure spi_sw_reconfigure
#define spi_readWrite_synch spi_sw_readWrite_synch


//VS10xx SCI Registers
#define SCI_MODE 0x00
#define SCI_STATUS 0x01
#define SCI_BASS 0x02
#define SCI_CLOCKF 0x03
#define SCI_DECODE_TIME 0x04
#define SCI_AUDATA 0x05
#define SCI_WRAM 0x06
#define SCI_WRAMADDR 0x07
#define SCI_HDAT0 0x08
#define SCI_HDAT1 0x09
#define SCI_AIADDR 0x0A
#define SCI_VOL 0x0B
#define SCI_AICTRL0 0x0C
#define SCI_AICTRL1 0x0D
#define SCI_AICTRL2 0x0E
#define SCI_AICTRL3 0x0F

#define VS1053_SPI_SLOW_FREQ 1000000
#define VS1053_SPI_FAST_FREQ 6000000
//#warning TODO change back to non debug values
//#define VS1053_SPI_SLOW_FREQ 100000
//#define VS1053_SPI_FAST_FREQ 150000

#define SM_DIFF           (1<< 0)
#define SM_LAYER12        (1<< 1) /* VS1063, VS1053, VS1033, VS1011 */
#define SM_RESET          (1<< 2)
#define SM_CANCEL         (1<< 3) /* VS1063, VS1053 */
#define SM_EARSPEAKER_LO  (1<< 4) /* VS1053, VS1033 */
#define SM_TESTS          (1<< 5)
#define SM_STREAM         (1<< 6) /* VS1053, VS1033, VS1003, VS1011 */
#define SM_EARSPEAKER_HI  (1<< 7) /* VS1053, VS1033 */
#define SM_DACT           (1<< 8)
#define SM_SDIORD         (1<< 9)
#define SM_SDISHARE       (1<<10)
#define SM_SDINEW         (1<<11)
#define SM_ADPCM          (1<<12) /* VS1053, VS1033, VS1003 */
#define SM_LINE1          (1<<14) /* VS1063, VS1053 */
#define SM_CLK_RANGE      (1<<15) /* VS1063, VS1053, VS1033 */

typedef enum _VS1053_States {
	VS1053_States_InitReset = 0,
	VS1053_States_InitResetWait,
	VS1053_States_Configure,
	VS1053_States_Idle,
} VS1053_States;

VS1053_States VS1053_State = VS1053_States_InitReset;
uint8 vs1053_bufferOut[32];
uint8 vs1053_bufferIn[32];
uint8 vs1053_buffer_size = 0;
Timer vs1053_resetTimer = 0;
unsigned char vs1053_SMP = 0;
unsigned char vs1053_CKP = 0;
unsigned char vs1053_CKE = 1;
unsigned int vs1053_BRG = 0;
uint16 MP3Mode = 0;
uint16 MP3Status = 0;
uint16 MP3Clock = 0;
uint8 vs1053_doWriteStream = 0;
uint8 vs1053_sizeWork = 0;
uint8 vs1053_sizeWorkToTransmit = 4;
uint8 *vs1053_SteamBuffer = NULL;

uint8 vs1053_lefchannel = 0;
uint8 vs1053_rightchannel = 0;
uint8 vs1053_volumechanged = 1;

volatile uint32 do_vs1053_1ms = 0;

void do_vs1053_task_1ms(void);
void vs1053_Mp3WriteRegister(uint8 addressbyte, uint8 highbyte, uint8 lowbyte);
uint16 vs1053_Mp3ReadRegister(uint8 addressbyte);
void vs1053_Mp3SetVolume(unsigned char leftchannel, unsigned char rightchannel);

void init_vs1053(void) {
	VS1053_XRESET_TRIS = 0;
	VS1053_XRESET_LAT = 0; //reset IC

	VS1053_XDCS_TRIS = 0;
	VS1053_XDCS_LAT = 1; //deselect

	VS1053_XCS_TRIS = 0;
	VS1053_XCS_LAT = 1; //deselect

	VS1053_DREQ_TRIS = 1;
	//VS1053_DREQ_PORT
	
	remove_timer(&vs1053_resetTimer);
	add_timer(&vs1053_resetTimer);

}

void do_vs1053(void) {
	if (do_vs1053_1ms) {
		do_vs1053_1ms = 0;
		do_vs1053_task_1ms();
	}

	if (vs1053_doWriteStream) {
		if (spi_lock(SPI_USER_VS1053) == SPI_USER_VS1053) {
			unsigned int size = vs1053_sizeWorkToTransmit;
			spi_reconfigure(SPI_USER_VS1053, vs1053_SMP, vs1053_CKP, vs1053_CKE, vs1053_BRG);
			VS1053_XDCS_LAT = 0;//Select control
			if ((vs1053_buffer_size - vs1053_sizeWork) < vs1053_sizeWorkToTransmit) {
				size = (vs1053_buffer_size - vs1053_sizeWork);
			}
			spi_readWrite_synch(SPI_USER_VS1053, vs1053_SteamBuffer + vs1053_sizeWork, vs1053_bufferIn + vs1053_sizeWork, size);
			VS1053_XDCS_LAT = 1;//Deselect control
			vs1053_sizeWork += vs1053_sizeWorkToTransmit;
			if (vs1053_sizeWork >= vs1053_buffer_size) {
				vs1053_sizeWork = 0;
				vs1053_doWriteStream = 0;
			}
			spi_unlock(SPI_USER_VS1053);
		}
	}
}

void do_vs1053_task_1ms(void) {
	switch (VS1053_State) {
		case VS1053_States_InitReset : {
			if (fatfs_isDiskReady()) {
				vs1053_BRG = spi_calculate_BRG(VS1053_SPI_SLOW_FREQ);//1MHz
				VS1053_XRESET_LAT = 0; //reset IC
				write_timer(&vs1053_resetTimer, 10);
				vs1053_doWriteStream = 0;
				VS1053_State = VS1053_States_InitResetWait;
			}
			break;
		}
		case VS1053_States_InitResetWait : {
			if (read_timer(&vs1053_resetTimer) == 0) {
				write_timer(&vs1053_resetTimer, 10);
				VS1053_XRESET_LAT = 1; //reset IC
				VS1053_State = VS1053_States_Configure;
			}
			break;
		}
		case VS1053_States_Configure : {
			if (read_timer(&vs1053_resetTimer) == 0) {
				if (spi_lock(SPI_USER_VS1053) == SPI_USER_VS1053) {
					vs1053_Mp3SetVolume(vs1053_lefchannel, vs1053_rightchannel);
					//vs1053_Mp3SetVolume(0, 0); //Maximum
					//vs1053_Mp3SetVolume(40, 40); //Set initial volume (20 = -10dB) Manageable
					//vs1053_Mp3SetVolume(0xFE, 0xFE); //Lowest
					MP3Mode = vs1053_Mp3ReadRegister(SCI_MODE);
					MP3Status = vs1053_Mp3ReadRegister(SCI_STATUS);
					MP3Clock = vs1053_Mp3ReadRegister(SCI_CLOCKF);
					MP3Mode |= SM_EARSPEAKER_LO | SM_LAYER12 | SM_DIFF;
					//Now that we have the VS1053 up and running, increase the internal clock multiplier and up our SPI rate
					vs1053_Mp3WriteRegister(SCI_MODE, (MP3Mode >> 8) & 0xFF, MP3Mode & 0xFF); //Set multiplier to 3.0x
					//vs1053_Mp3WriteRegister(SCI_BASS, 0x44, 0x44);
					//vs1053_Mp3WriteRegister(SCI_CLOCKF, 0x60, 0x00); //Set multiplier to 3.0x
					vs1053_Mp3WriteRegister(SCI_CLOCKF, 0xA0, 0x00); //Set multiplier to 4.0x
					vs1053_BRG = spi_calculate_BRG(VS1053_SPI_FAST_FREQ);//6MHz
					//MP3Clock = Mp3ReadRegister(SCI_CLOCKF);
					VS1053_State = VS1053_States_Idle;
					spi_unlock(SPI_USER_VS1053);
				}
			}
			break;
		}
		case VS1053_States_Idle : {
			if(vs1053_volumechanged) {
				if (spi_lock(SPI_USER_VS1053) == SPI_USER_VS1053) {
					vs1053_Mp3SetVolume(vs1053_lefchannel, vs1053_rightchannel);
					vs1053_volumechanged = 0;
					spi_unlock(SPI_USER_VS1053);
				}
			}
			break;
		}
		default : {
			VS1053_State = VS1053_States_InitReset;
			break;
		}
	}
}

void isr_vs1053_1ms(void) {
	do_vs1053_1ms = 1;
}

uint8 vs1053_IsDataRequested(void) {
	uint8 result = 0;
	if (VS1053_DREQ_PORT) {
		result = 1;
	}
	return result;
}

uint8 vs1053_WriteStream_nb(uint8 *ptr, uint16 size) {
	uint8 result = 0;
	if ((ptr != NULL) && (size != 0) && (size <= (sizeof(vs1053_bufferOut) / sizeof(*vs1053_bufferOut))) ) {
		if (VS1053_DREQ_PORT == 1) {
			if (VS1053_State == VS1053_States_Idle) {
				if (vs1053_doWriteStream == 0) {
					//memcpy(vs1053_bufferOut, ptr, size);
					vs1053_SteamBuffer = ptr;//Caller must hold the buffer
					vs1053_buffer_size = size;
					vs1053_doWriteStream = 1;
					vs1053_sizeWork = 0;
					result = 1;
				}
			}
		}
	}
	return result;
}

uint8 vs1053_getState(void) {
	uint8 result = 0;
	if (vs1053_doWriteStream == 0) {
		result = 1;
	}
	return result;
}

//Write to VS10xx register
//SCI: Data transfers are always 16bit. When a new SCI operation comes in 
//DREQ goes low. We then have to wait for DREQ to go high again.
//XCS should be low for the full duration of operation.
void vs1053_Mp3WriteRegister(uint8 addressbyte, uint8 highbyte, uint8 lowbyte) {
	while(!VS1053_DREQ_PORT) ; //Wait for DREQ to go high indicating IC is available
	spi_reconfigure(SPI_USER_VS1053, vs1053_SMP, vs1053_CKP, vs1053_CKE, vs1053_BRG);
	VS1053_XCS_LAT = 0;//Select control
	//SCI consists of instruction byte, address byte, and 16-bit data word.
	vs1053_bufferOut[0] = 0x02;//Write instruction
	vs1053_bufferOut[1] = addressbyte;
	vs1053_bufferOut[2] = highbyte;
	vs1053_bufferOut[3] = lowbyte;
	spi_readWrite_synch(SPI_USER_VS1053, vs1053_bufferOut, vs1053_bufferIn, 4);
	while(!VS1053_DREQ_PORT) ; //Wait for DREQ to go high indicating IC is available
	VS1053_XCS_LAT = 1;//Deselect Control
}

//Read the 16-bit value of a VS10xx register
uint16 vs1053_Mp3ReadRegister(uint8 addressbyte) {
	uint16 resultvalue = 0;
	spi_reconfigure(SPI_USER_VS1053, vs1053_SMP, vs1053_CKP, vs1053_CKE, vs1053_BRG);
	while(!VS1053_DREQ_PORT) ; //Wait for DREQ to go high indicating IC is available
	VS1053_XCS_LAT = 0;//Select control

	//SCI consists of instruction byte, address byte, and 16-bit data word.
	vs1053_bufferOut[0] = 0x03;//Read instruction
	vs1053_bufferOut[1] = addressbyte;
	vs1053_bufferOut[2] = 0xFF;
	vs1053_bufferOut[3] = 0xFF;
	spi_readWrite_synch(SPI_USER_VS1053, vs1053_bufferOut, vs1053_bufferIn, 4);

	VS1053_XCS_LAT = 1;//Deselect Control

	resultvalue = vs1053_bufferIn[2] << 8;
	resultvalue |= vs1053_bufferIn[3];
	return resultvalue;
}

//Set VS10xx Volume Register
void vs1053_Mp3SetVolume(unsigned char leftchannel, unsigned char rightchannel) {
	vs1053_Mp3WriteRegister(SCI_VOL, leftchannel, rightchannel);
}

void vs1053_SetVolume(unsigned char leftchannel, unsigned char rightchannel) {
	vs1053_lefchannel = leftchannel;
	vs1053_rightchannel = rightchannel;
	vs1053_volumechanged = 1;
}

void vc1054_recovery(void) {
	vs1053_doWriteStream = 0;
	VS1053_State = VS1053_States_InitReset;
}
