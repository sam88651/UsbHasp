/*
 * Copyright (C) 2004 Chingachguk & Denger2k All Rights Reserved
 * Copyright (C) 2017 Revisited by Sam88651 as Linux user space application
 * 
 * Module Name:
 *     USBKeyEmu.h
 * Abstract:
 *     This module contains the common private declarations
 *     for the emulation of USB bus and HASP key
 * Notes:
 * Revision History:
 */
#ifndef USBKEYEMU_H
#define USBKEYEMU_H

#include <semaphore.h> 
#include <libusb_vhci.h>
#include <linux/limits.h>
#include "EncDecSim.h"              // KEY_INFO

#ifndef USBKeyEmu_H
#define USBKeyEmu_H

//
// The generic ids for installation of key pdo
//
#define VENDORFW_H7 u"HASP HL 3.25"
#define VENDORFW_H6 u"HASP HL 3.21"
#define VENDORFW_H5 u"HASP HL 2.16"

//
// Description of key data
//
#pragma pack(1)

typedef struct _KEY_DATA {
    //
    // Current key state
    //
    uint8_t   isInitDone;     // Is chiperkeys given to key
    uint8_t   isKeyOpened;    // Is valid password is given to key
    uint8_t   encodedStatus;  // Last encoded status

    uint16_t  chiperKey1,     // Keys for chiper
              chiperKey2;
    //
    // Static information about HASP key 
    //
    uint8_t   keyType;        // Type of key
    uint8_t   memoryType;     // Memory size of key
    uint32_t  password;       // Password for key
    uint8_t   options[14];    // Options for key
    uint8_t   secTable[8];    // ST for key
    uint8_t   netMemory[16];  // NetMemory for key

    uint8_t   memory[512];    // Memory content
    uint8_t   edStruct[256];  // EDStruct for key} KEY_DATA, *PKEYDATA;
    char      name[128];      // key name
    char      created[24];    // date of key creation
} KEY_DATA, *PKEYDATA;
#pragma pack()

#define MAX_HASPKEYS    4
#define MAX_DEVDESC     18
#define MAX_CONFDESC    18
#define MAX_STRDESC     4
//
// One USB device description, AKA thread data
//
typedef struct _USB_HASP {
    uint8_t     keyfileName [PATH_MAX];
    KEY_DATA    keyData;
    struct usb_vhci_port_stat stat;
    int         addr;
    int         port;
    sem_t       *pmutex;
    uint8_t     devDesc [MAX_DEVDESC];
    uint8_t     confDesc [MAX_CONFDESC];
    uint8_t     strDesc [MAX_STRDESC];
    uint16_t    *deviceName;
} USB_HASP, *PUSBHASP;

//
// List of supported functions for HASP key
//
enum KEY_FN_LIST {
    KEY_FN_SET_CHIPER_KEYS       	= 0x80,
    KEY_FN_CHECK_PASS            	= 0x81,
    KEY_FN_READ_3WORDS           	= 0x82,
    KEY_FN_WRITE_WORD            	= 0x83,
    KEY_FN_READ_ST               	= 0x84,
    KEY_FN_READ_NETMEMORY_3WORDS 	= 0x8B,
    KEY_FN_HASH_DWORD            	= 0x98,
    KEY_FN_ECHO_REQUEST          	= 0xA0, // Echo request to key
    KEY_FN_GET_TIME              	= 0x9C, // Get time (for HASP time) key
    KEY_FN_PREPARE_CHANGE_TIME   	= 0x1D, // Prepare to change time (for HASP time)
    KEY_FN_COMPLETE_WRITE_TIME   	= 0x9D, // Write time (complete) (for HASP time)
    KEY_FN_QUESTION         		= 0x1E,
    KEY_FN_ANSWER                   = 0x9E,	
//-------- SRM Functions ----------------
    KEY_FN_READ_STRUCT              = 0xA1,
    KEY_FN_READ_FAT                 = 0xA2,
    KEY_FN_READ_26                  = 0x26,
    KEY_FN_READ_A6                  = 0xA6,
    KEY_FN_WRITE_27                 = 0x27,
    KEY_FN_WRITE_A7                 = 0xA7,
    KEY_FN_SIGNED_READ_28           = 0x28,
    KEY_FN_SIGNED_READ_A8           = 0xA8,
    KEY_FN_READ_DATE_TIME           = 0xAC,
    KEY_FN_AES_IN                   = 0x29,
    KEY_FN_AES_OUT                  = 0xA9,
    KEY_FN_LOGIN                    = 0xAA,
    KEY_FN_LOGOUT                   = 0xAB,
    KEY_FN_SRM_2F                   = 0x2F,
    KEY_FN_SRM_AF                   = 0xAF
};

//
// HASP key operation status
//
enum KEY_OPERATION_STATUS {
    KEY_OPERATION_STATUS_OK                     = 0,
    KEY_OPERATION_STATUS_ERROR                  = 1,
    KEY_OPERATION_STATUS_INVALID_MEMORY_ADDRESS = 4,
    KEY_OPERATION_STATUS_LAST                   = 0x1F
};

//
// HASP key request structure
//
#pragma pack(1)
typedef struct _KEY_REQUEST {
    uint8_t   majorFnCode;    // Requested fn number (type of KEY_FN_LIST)
    uint16_t  param1,         // Key parameters
    param2, param3;           // param1 = Value param2 = Index
} KEY_REQUEST, *PKEY_REQUEST;

//
// HASP key respond structure
//
typedef struct _KEY_RESPONSE {
    uint8_t    status,         // Status of operation (type of KEY_OPERATION_STATUS)
               encodedStatus;  // CRC of status and majorFnCode
    uint8_t    data[4096];     // Output data
} KEY_RESPONSE, *PKEY_RESPONSE;

#pragma pack()

//
// Array with a length
//
typedef struct _BYTE_ARRAY {
    int size;
    uint8_t *bytes;
} BYTE_ARRAY, *PBYTE_ARRAY;

//
// Public functions
//
void EmulateKey(PKEYDATA pKeyData, PKEY_REQUEST request, uint32_t *outBufLen, PKEY_RESPONSE outBuf);
int  LoadKey (char file[], PKEYDATA pKeyData);
void UsbDevice (int fd, USB_HASP haspKeys[], int numKeys, sem_t *pmutex);

#endif

#endif	// USBKEYEMU_H

