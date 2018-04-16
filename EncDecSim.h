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
#ifndef EncDecSimH
#define EncDecSimH

#pragma pack(1)
typedef struct {
    uint8_t  columnMask;
    uint8_t  cryptInitVect;
    uint8_t  secTable[8];
    uint8_t  isInvSecTab;
    uint32_t prepNotMask;
    uint32_t curLFSRState;
    uint8_t  first5bit;
    uint32_t password; 
} KEY_INFO;
#pragma pack()

void Transform (uint32_t *Data, KEY_INFO *keyInfo);
void Encode (uint32_t *bufPtr, uint32_t *nextBufPtr, KEY_INFO *keyInfo);
void Decode (uint32_t *bufPtr, uint32_t *nextBufPtr, KEY_INFO *keyInfo);
void GetCode (uint16_t seed, uint32_t *bufPtr, uint8_t *secTable);

#endif

