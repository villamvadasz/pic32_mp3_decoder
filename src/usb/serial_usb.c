#include "serial_usb.h"
#include "c_serial_usb.h"

#include "k_stdtype.h"
#include "ringBuffer.h"
#include "mal.h"
#include "sio.h"

RingBuffer myRingBuffer_hid_tx;
unsigned char hid_tx[TX_LEN_USB];
RingBuffer myRingBuffer_hid_rx;
unsigned char hid_rx[RX_LEN_USB];

unsigned long missedTxCharHID = 0;
unsigned long missedRxCharHID = 0;

int isCharOutBuffer_usb(void);

#include "usb.h"
#include "usb_function_hid.h"

extern USB_HANDLE USBOutHandle;
extern USB_HANDLE USBInHandle;
extern unsigned char ReceivedDataBuffer[64];
extern unsigned char ToSendDataBuffer[64];

void init_serial_usb(void) {
	ringBuffer_initBuffer(&myRingBuffer_hid_tx, hid_tx, sizeof(hid_tx) / sizeof(*hid_tx));
	ringBuffer_initBuffer(&myRingBuffer_hid_rx, hid_rx, sizeof(hid_rx) / sizeof(*hid_rx));
}

void deinit_serial_usb(void) {
}

void do_serial_usb(void) {
    // User Application USB tasks
    if ((USBDeviceState < CONFIGURED_STATE)||(USBSuspendControl==1)) {
	} else {
		{
			if (isCharOutBuffer_usb() != -1) {
				if(!HIDTxHandleBusy(USBInHandle)) {
					unsigned char data = 0;
					uint32 i = 0;
					memset(ToSendDataBuffer, 0x00 , 64);
					while (ringBuffer_getItem(&myRingBuffer_hid_tx, &data) != 0) {
						ToSendDataBuffer[1 + i] = data;
						i++;
					    if (i >= 63) {
						    break;
						}
					}
					ToSendDataBuffer[0] = i;
					USBInHandle = HIDTxPacket(HID_EP, (BYTE*)&ToSendDataBuffer[0], 64);
				}
			} else {
			}
		}
		{
			if(!HIDRxHandleBusy(USBOutHandle)) {				//Check if data was received from the host.
				unsigned char data = 0;
				uint32 i = ReceivedDataBuffer[0];
				uint32 j = 0;
				isr_communication();
				while (i) {
					data = ReceivedDataBuffer[1 + j];
					if (ringBuffer_addItem(&myRingBuffer_hid_rx, data) != -1) {
					} else {
						 missedRxCharHID++;
					}					
					i--;
					j++;
				}
				//Re-arm the OUT endpoint for the next packet
				USBOutHandle = HIDRxPacket(HID_EP, (BYTE*)&ReceivedDataBuffer, 64);
			}
		}
	}
}

int isCharInBuffer_usb(void) {
	int result = -1;
	unsigned int cnt = ringBuffer_getFillness(&myRingBuffer_hid_rx);
	if (cnt != 0) {
		result = cnt;
	}
	return result;
}

int isCharOutBuffer_usb(void) {
	int result = -1;
	unsigned int cnt = ringBuffer_getFillness(&myRingBuffer_hid_tx);
	if (cnt != 0) {
		result = cnt;
	}
	return result;
}

int getChar_usb(void) {
	int result = -1;
	unsigned char data = 0;
	if (ringBuffer_getItem(&myRingBuffer_hid_rx, &data) != 0) {
		result = data;
	}
	return result;
}

void putChar_usb(unsigned char data) {
	if (ringBuffer_addItem(&myRingBuffer_hid_tx, data) != -1) {
	} else {
		missedTxCharHID++;
	}
}

void executeSendMesg(void) {
	ringBuffer_addItem(&myRingBuffer_hid_rx, 0x55);
	ringBuffer_addItem(&myRingBuffer_hid_rx, 0x00);
	ringBuffer_addItem(&myRingBuffer_hid_rx, 0xFF);
}
