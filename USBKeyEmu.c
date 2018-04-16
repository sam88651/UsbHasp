/*
 * Copyright (C) 2004 Chingachguk & Denger2k All Rights Reserved
 * Copyright (C) 2017 Revisited by Sam88651 as Linux user space application
 * 
 * Module Name:
 *      USBKeyEmu.c
 * Abstract:
 *     This module contains routines for emulation of USB bus and USB HASP key.
 * Notes:
 * Revision History:
 */
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <syslog.h>
#include <libusb_vhci.h>
#include "USBKeyEmu.h"

/**
 * Encode/decode response/request to key
 * 
 * @param bufPtr - pointer to a encoded/decoded data
 * @param bufSize - size of encoded information
 * @param key1Ptr - ptr to chiper key1
 * @param key2Ptr - ptr to chiper key2
 */
static void _Chiper(uint8_t *bufPtr, uint32_t bufSize, uint16_t *key1Ptr, uint16_t *key2Ptr) {
	uint32_t i, j;
	uint8_t tmpDL;
	uint8_t *p= (uint8_t *)bufPtr;

	if (bufSize) {
            for (i= 0; i < bufSize; i++) {
		tmpDL= 0;
		for (j= 0; j < 4; j++) {
                    tmpDL<<= 1;
                    if ( (*key1Ptr)&0x01 ) {
			tmpDL|= 0x1;
			*key1Ptr= ((*key1Ptr^*key2Ptr)>>1)|0x8000;
                    } else {
                        *key1Ptr>>= 1;
                    }
                    tmpDL<<= 1;
                    if ( (*key1Ptr)&0x80 ) {
                        tmpDL|= 0x01;
                    }
		}
		*p++^= tmpDL;
            }
	}
}

/**
 * Encode/decode response/request to key (stub only)
 * 
 * @param buf - pointer to a encoded/decoded data
 * @param size - size of encoded information
 * @param pKeyData - ptr to key data
 */
static void Chiper(void *buf, uint32_t size, PKEYDATA pKeyData) {
#ifdef DEBUG    
    syslog (LOG_DEBUG, "Chiper inChiperKey1=0x%hX, inChiperKey2=0x%hX, length=0x%X\n",
                            pKeyData->chiperKey1, pKeyData->chiperKey2, size);
#endif    
    _Chiper(buf, size, &pKeyData->chiperKey1, &pKeyData->chiperKey2);
#ifdef DEBUG    
    syslog (LOG_DEBUG, "Chiper outChiperKey1=0x%hX, outChiperKey2=0x%hX\n",
                            pKeyData->chiperKey1, pKeyData->chiperKey2);
#endif    
}


/**
 * Borrowed from vusbsrm project.
 * 
 * @param ValidateByte
 * @param loopCnt
 */
static void sub_12D50 (uint8_t validateByte, uint8_t *loopCnt) {
	int i;

    for ( i= 7; i >= 0; i-- ) {
        if ( (*loopCnt= (*loopCnt<<1)|((validateByte>>i)&0x01))&0x10 )
            *loopCnt^= 0x0D;
        *loopCnt&= 0x0F;
    }
}

/**
 * Borrowed from vusbsrm project.
 * 
 * @param AdjustedReqCode
 * @param SetupKeysResult
 * @param BufPtr
 * @return 
 */
static uint32_t CheckEncodedStatus(uint8_t adjustedReqCode, uint8_t setupKeysResult, uint8_t *bufPtr) {
	uint8_t loopCnt= 0x0F;

    if ( ( !adjustedReqCode ) || ( setupKeysResult < 2 ) )
        return( (*bufPtr <= 0x0F ) ? 1 : 0 );
    if ( *bufPtr > 0x1F )
        return(0);
    sub_12D50 (*bufPtr, &loopCnt);
    sub_12D50 (*(bufPtr+1), &loopCnt);
    return( ( loopCnt > 0 ) ? 0 : 1 );
}

/**
 * HASP key memory size by its type.
 * 
 * @param pKeyData
 * @return 
 */
static int32_t GetMemorySize(PKEYDATA pKeyData) {

    if ( pKeyData->memoryType == 1 )
        return 0x80;
    if (pKeyData->memoryType == 0x20 )
        return 0xFD0;
    else 
        return 0xFD0;           // memoryType == 0x21
}

//
// Borrowed from vusbsrm project for KEY_FN_READ_STRUCT request processing
//
static const uint8_t FuncA1_Val0 [] = { 0x01, 0x00, 0x00 };
static const uint8_t FuncA1_Val1 [] = { 0x3b, 0x07, 0xc4, 0x53, 0x06, 0x01, 0x00, 0x00, 0x02, 0xca, 0x00, 0x0b, 0x00, 0x00, 0x3e, 0xdc,
                                        0x02, 0x54, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x03, 0x19, 0x22, 0xc3, 0x7b, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x60, 0x00, 0x01, 0x16, 0xe1, 0x00, 0x00, 0x00 };
static const uint8_t FuncA1_Val2 [] = { 0x62, 0xE4, 0x95, 0x34, 0x00, 0x00, 0x01, 0x00,
                                        0x00, 0x03, 0x00, 0x00, 0x01, 0x00 };
static const uint8_t FuncA1_Val3 [] = { 0x00, 0x01, 0xCC, 0x00, 0x00, 0x00, 0x00, 0x00 };

/**
 * Emulation of key main procedure (IOCTL_INTERNAL_USB_SUBMIT_URB handler)
 * 
 * @param pKeyData - ptr to key data
 * @param request - ptr to request buffer
 * @param outBufLen - ptr to out buffer size variable
 * @param outBuf - ptr to out buffer
 */
void EmulateKey(PKEYDATA pKeyData, PKEY_REQUEST request, uint32_t *outBufLen, PKEY_RESPONSE outBuf) {
        uint8_t encodeOutData, status, encodedStatus;
        uint32_t outDataLen;
        KEY_RESPONSE    keyResponse;
        struct timeval  tv;
    
    gettimeofday(&tv,NULL);
    memset (&keyResponse, 0, sizeof(keyResponse));
    keyResponse.status = KEY_OPERATION_STATUS_ERROR;
    outDataLen = 0; encodeOutData = 0;
    switch (request->majorFnCode) {                 // HASP functions
    case KEY_FN_ECHO_REQUEST:
#ifdef DEBUG        
        syslog (LOG_DEBUG, "KEY_FN_ECHO_REQUEST 0x%0hhx\n",request->majorFnCode);
#endif        
        keyResponse.status = KEY_OPERATION_STATUS_OK;
        keyResponse.data [2] = 0x00;
        *outBufLen = 1;
        memcpy (outBuf, &keyResponse.data, *outBufLen);
        return;        
    case KEY_FN_SET_CHIPER_KEYS:
#ifdef DEBUG        
        syslog (LOG_DEBUG, "KEY_FN_SET_CHIPER_KEYS\n");
#endif        
        pKeyData->chiperKey1 = request->param1;
        pKeyData->chiperKey2 = 0xA0CB;
        pKeyData->encodedStatus = pKeyData->netMemory[0]+pKeyData->netMemory[1]+
                                pKeyData->netMemory[2]+pKeyData->netMemory[3];
                                                    // Setup random encoded status begin value
        pKeyData->isInitDone = 1;
        keyResponse.status = KEY_OPERATION_STATUS_OK;// Make key response
        keyResponse.data [0] = 0x02;                // Time hasp or usual hasp
        if ( (pKeyData->netMemory [4] == 3) || (pKeyData->netMemory [4] == 5) ) {
            keyResponse.data [1] = 0x1A;
        } else {
            if ( pKeyData->keyType > 5 )
                keyResponse.data [1] = pKeyData->keyType;
            else
                keyResponse.data [1] = 0x0A;        // default value
        }
        keyResponse.data [2] = 0x00;                // Bytes 3, 4 - key sn, set it to low word of ptr to key data
        keyResponse.data[3] = pKeyData->netMemory[0]+pKeyData->netMemory[1];
        keyResponse.data[4] = pKeyData->netMemory[2]+pKeyData->netMemory[3];
        outDataLen = 5;
        encodeOutData = 1;                    
        break;
    case KEY_FN_CHECK_PASS:                         // Decode pass
        Chiper(&request->param1, 4, pKeyData);
#ifdef DEBUG        
        syslog (LOG_DEBUG, "KEY_FN_CHECK_PASS pass=0x%08X, pKeyData->password=0x%X, pKeyData->isInitDone=0x%hhX\n",
            *((uint32_t *)&request->param1), pKeyData->password, pKeyData->isInitDone);
#endif                
                                                    // Compare pass
        if (*((uint32_t *)&request->param1) == pKeyData->password && pKeyData->isInitDone == 1 ) {
            keyResponse.status = KEY_OPERATION_STATUS_OK;
                                                    // data[0], data[1] - memory size
            keyResponse.data [0] = (uint8_t)((GetMemorySize(pKeyData)) & 0xFF);
            keyResponse.data [1] = (uint8_t)((GetMemorySize(pKeyData) >> 8) & 0xFF);
            keyResponse.data [2] = 0x10;
            outDataLen = 3;
            encodeOutData = 1;
            pKeyData->isKeyOpened = 1;              // FN_OPEN_KEY
        }
        break;
    case KEY_FN_READ_NETMEMORY_3WORDS:
        Chiper(&request->param1, 2, pKeyData);
#ifdef DEBUG        
        syslog (LOG_DEBUG, "KEY_FN_READ_NETMEMORY_3WORDS, request->param1 - 0x%0hx\n", request->param1);
#endif        
        // Typical data in NetMemory:
        // 12 1A 12 0F 03 00 70 00 02 FF 00 00 FF FF FF FF
        // 12 1A 12 0F - sn
        // 03 00 - key type
        // 70 00 - memory size in bytes
        // 02 FF - ?
        // 00 00 - net user count
        // FF FF - ?
        // FF - key type (FF - local, FE - net, FD - time)
        // FF - ?
        // Analyse memory offset
        if ( pKeyData->isKeyOpened && request->param1 >= 0 && request->param1 <= 7 ) {
            keyResponse.status = KEY_OPERATION_STATUS_OK;
            memcpy (keyResponse.data, &pKeyData->netMemory[request->param1*2], sizeof(uint16_t)*3);
            outDataLen = sizeof(uint16_t)*3;
            encodeOutData = 1;
        }
        break;
    case KEY_FN_READ_3WORDS:                        // Do read
        Chiper(&request->param1, 2, pKeyData);
#ifdef DEBUG        
        syslog (LOG_DEBUG, "KEY_FN_READ_3WORDS, request->param1 - 0x%0hx\n", request->param1);
#endif        
        if ( pKeyData->isKeyOpened && request->param1>=0 && (request->param1*2)<GetMemorySize(pKeyData) ) {
            keyResponse.status = KEY_OPERATION_STATUS_OK;
            memcpy (keyResponse.data, &pKeyData->memory[request->param1*2], sizeof(uint16_t)*3);
            outDataLen = sizeof(uint16_t)*3;
            encodeOutData = 1;
        }
        break;
    case KEY_FN_WRITE_WORD:                         // Do write
#ifdef DEBUG        
        syslog (LOG_DEBUG, "KEY_FN_WRITE_WORD\n");
#endif        
        // Decode memory offset & value
        Chiper(&request->param1, 4, pKeyData);
#ifdef DEBUG        
        syslog (LOG_DEBUG, "offset=0x%hX data=0x%hX\n", request->param1, request->param2);
#endif        
        if ( pKeyData->isKeyOpened && request->param1>=0 && (request->param1*2)<GetMemorySize(pKeyData) ) {
            keyResponse.status = KEY_OPERATION_STATUS_OK;
            memcpy (&pKeyData->memory[request->param1*2], &request->param2, sizeof(uint16_t));
            outDataLen = 0;
            encodeOutData = 0;
        }
        break;
    case KEY_FN_READ_ST:                            // Do read ST
#ifdef DEBUG        
        syslog (LOG_DEBUG, "KEY_FN_READ_ST\n");
#endif        
        if ( pKeyData->isKeyOpened ) {
            int32_t i;
            keyResponse.status = KEY_OPERATION_STATUS_OK;
            for ( i = 7; i >= 0; i-- ) 
                keyResponse.data [7-i] = pKeyData->secTable [i];
            outDataLen = 8;
            encodeOutData = 1;
        }
        break;
    case KEY_FN_HASH_DWORD:                         // Do hash dword
        Chiper(&request->param1, 4, pKeyData);
#ifdef DEBUG        
        syslog (LOG_DEBUG, "KEY_FN_HASH_DWORD\n");
#endif        
        if ( pKeyData->isKeyOpened ) {
            keyResponse.status = KEY_OPERATION_STATUS_OK;
            memcpy (keyResponse.data, &request->param1, 4);
            Transform ((uint32_t *)keyResponse.data, (KEY_INFO *)pKeyData->edStruct);
            outDataLen = sizeof(uint32_t);
            encodeOutData = 1;
        }
        break;
    case KEY_FN_READ_STRUCT:
#ifdef DEBUG        
        syslog (LOG_DEBUG, "KEY_FN_READ_STRUCT, request->param1 - 0x%0hx\n", request->param1);
#endif        
        switch(request->param1) {
        case 0:
            memcpy (&keyResponse.data, &FuncA1_Val0[0], 3);
            outDataLen = 3;
            *outBufLen = 3;
            break;
        case 1:
            memcpy (&keyResponse.data, &FuncA1_Val1[0], 47);
            outDataLen = 47;
            *outBufLen = 47;
            break;
        case 2:
            memcpy (&keyResponse.data, &FuncA1_Val2[0], 14);
            outDataLen = 14;
            *outBufLen = 14;
            break;
        case 3:
            memcpy (&keyResponse.data, &FuncA1_Val3[0], 8);
            outDataLen = 8;
            *outBufLen = 8;            
            break;
        default:
            break;
        }
        outDataLen = outDataLen < *outBufLen ? outDataLen : *outBufLen;
        memcpy (outBuf, &keyResponse.data, *outBufLen);
        return;
    default:
#ifdef DEBUG        
        syslog (LOG_DEBUG, "UNKOWN KEY_FN\n");
#endif        
        break;
    }
#ifdef DEBUG    
    syslog (LOG_DEBUG, "Create encodedStatus\n");
#endif    
                                                    // Randomize encodedStatus
    pKeyData->encodedStatus ^= tv.tv_usec & 0xFFFF;
    // If status in range KEY_OPERATION_STATUS_OK...KEY_OPERATION_STATUS_LAST
    if ( keyResponse.status >= KEY_OPERATION_STATUS_OK && keyResponse.status <= KEY_OPERATION_STATUS_LAST ) {
            // Then create encoded status
        do {
            keyResponse.encodedStatus=++pKeyData->encodedStatus;
        } while (CheckEncodedStatus ((uint8_t)(request->majorFnCode&0x7F), 0x02, (uint8_t *)&keyResponse.status)==0);
    }
    status = keyResponse.status;                    // Store encoded status
    encodedStatus = keyResponse.encodedStatus;
#ifdef DEBUG    
    syslog (LOG_DEBUG, "Encoded status: %hhX\n", encodedStatus);
#endif    
    Chiper (&keyResponse.status, 2, pKeyData);      // Crypt status & encoded status 
    if ( encodeOutData ) {                          // Crypt output data
        Chiper (&keyResponse.data, outDataLen, pKeyData);
    }
    if ( status == 0 ) {                            // Shuffle encoding keys + Ching
        pKeyData->chiperKey2 = (pKeyData->chiperKey2 & 0xFF) | (encodedStatus << 8);
#ifdef DEBUG        
        syslog (LOG_DEBUG, "Shuffle keys: chiperKey1=%hX, chiperKey2=%hX,\n",
                    pKeyData->chiperKey1, pKeyData->chiperKey2);
#endif        
    }
                                                    // Set out data size
    *outBufLen = (sizeof(uint16_t) + outDataLen) < *outBufLen ? sizeof(uint16_t) + outDataLen : *outBufLen;
#ifdef DEBUG    
    syslog (LOG_DEBUG, "Out data size: %X\n", *outBufLen);
#endif    
    memcpy (outBuf, &keyResponse, *outBufLen);      // Copy data into output buffer
}
