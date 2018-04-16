/*
 * Copyright (C) 2017 Sam88651.
 * 
 * Module Name:
 *     LoadKey.c
 * Abstract:
 *      Load and parse JSON HASP key description.
 * Notes:
 * Revision History:
 */
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <wchar.h>
#include <sys/stat.h>
#include <string.h>
#include <libusb_vhci.h>
#include <jansson.h>
#include <syslog.h>
#include "USBKeyEmu.h"

#define min(a,b)    ((a)<(b)?(a):(b))

/**
 * Load and parse JSON file
 * 
 * @param text - JSON text
 * @return - JSON as a structure or NULL in case of error
 */
static json_t *LoadJson(const uint8_t *text) {
    json_t *root;
    json_error_t error;

    root = json_loads(text, 0, &error);

    if (root) {
        return root;
    } else {
        return (json_t *)NULL;
    }
}

/**
 * Get value of hex-encoded JSON key
 * @param jval - key
 * @return - value
 */
static unsigned long GetLongHexValue (json_t *jval) {
        char    *ptr;
        unsigned long val = 0;
        
    if ( jval != NULL ) {
        val = strtoul(json_string_value(jval), &ptr, 16);
    }
    return val;
}

/**
 * String to upper. Works only with ASCII
 * @param str - string
 * @return - upper string
 */
static char *StrUpr (char *str) {
    
    for ( char *pval = str; *pval; pval++ ) {
        *pval = toupper(*pval);
    }
    return str;
}

/**
 * Count hex-encoded values in string
 * @param str - hex-encoded values string
 * @return - number of values
 */
static int GetHexByteArraySize (char *str) {
    int count = 0;
    
    for ( char *pval = str; *pval; ) {
        if ( !strncmp(pval,"0X",2) ) {      // hex-value prefix
            for ( ; *pval && *pval != ','; pval++ );// skip to the end of line or comma
                ++count;
        } else {
            ++pval;
        }
    }
    return count;
}

/**
 * Get hex-encoded values as an array of values
 *
 * @param val - resulting array
 * @param count - bytes count
 * @param sval - hex-encoded values string
 * @return - parsed bytes count
 */
static int GetHexBytesString (uint8_t *val, int count, char *sval) {
        int     n;
        char    *ptr;
        unsigned long lval;
        
    for ( n = 0; *sval && n < count; ) {
        if ( !strncmp(sval,"0X",2) ) {      // hex-value prefix
            lval = strtol(sval, &ptr, 16);  // next byte
            val [n] = (uint8_t)lval;
            for ( ; *sval && *sval != ','; sval++ );// skip to EOL or comma
                ++n;
        } else {
            ++sval;
        }
    }
    return n;
}

/**
 * Get JSON key value as array of bytes
 * 
 * @param jval - key
 * @return - array with its length
 */
static PBYTE_ARRAY GetHexByteArray (json_t *jval) {
        PBYTE_ARRAY val = NULL;
        char    *sval;
        int     count, n;
        size_t  i;
        
    val = malloc(sizeof(BYTE_ARRAY));
    val->size = 0;
    val->bytes = NULL;
    if ( jval != NULL && val != NULL ) {    // skip empty values
        if ( jval->type == JSON_STRING ) {  // JSON key value is a string
            count = 0;
            sval = StrUpr((char *)json_string_value(jval));
            count = GetHexByteArraySize(sval);
            if ( count > 0 ) {
                val->bytes = malloc(count*sizeof(uint8_t));
                if ( val->bytes != NULL ) {
                    val->size = count;
                    GetHexBytesString ( val->bytes, count, sval );
                }
            }
        } else if ( jval->type == JSON_ARRAY ) {// value can be multi-string
            size_t size = json_array_size(jval);// count strings
            count = 0;
            for ( i = 0; i < size; i++) {       // walk on strings
                json_t *item = json_array_get(jval, i);
                if ( item != NULL ) {
                    sval = StrUpr((char *)json_string_value(item));
                    n = GetHexByteArraySize(sval);
                    count += n;
                }
            }
            if ( count > 0 ) {                  // prepare result
                val->bytes = malloc(count*sizeof(uint8_t));
                if ( val->bytes != NULL ) {
                    val->size = count;
                    for ( n = 0, i = 0; i < size; i++) {
                        json_t *item = json_array_get(jval, i);
                        sval = (char *)json_string_value(item);
                        if ( item != NULL ) {
                            n += GetHexBytesString ( val->bytes+n, count-n, sval );
                        }
                    }
                }
            }   
        }
    }
    return val;
}

/**
 * "Classified" vectors table for old HASPs SecTable
 */
#define HASP_ROWS   8
#define HASP_COLS   8
static uint8_t HASP_rows [HASP_ROWS] [HASP_COLS] = {
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 0, 1, 0, 1, 0, 1 },
    { 1, 0, 1, 0, 1, 0, 1, 0 },
    { 0, 0, 1, 1, 0, 0, 1, 1 },
    { 1, 1, 0, 0, 1, 1, 0, 0 },
    { 0, 0, 0, 0, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 1, 1, 1 }
    };

#ifdef DEBUG
/**
 * 
 * @param p
 * @param size
 */
static void dumpArray(uint8_t *p, int size) {
    
    for ( int ii = 0; ii < size; ii++) {
        if ( ii%16 == 0 && ii )
            syslog (LOG_DEBUG, "\n");                        
        syslog (LOG_DEBUG, "0x%02hx, ", p[ii]);
    }
    syslog (LOG_DEBUG, "\n");
}
#endif


/**
 * Build standard SecTable
 * 
 * @param pKeyData
 */
static void BuildStandardSecTable (PKEYDATA pKeyData) {
        uint32_t i, j;
        uint8_t alBuf[8];

    unsigned long password = pKeyData->password;
    password ^= 0x09071966;
    for ( i = 0; i < sizeof(alBuf); i++ ) {
        alBuf [i] = (uint8_t)(password & 7);
        password = password >> 3;
    }
    memset(pKeyData->secTable, 0, sizeof(pKeyData->secTable));
    for ( i = 0; i < HASP_ROWS; i++) {
        for ( j = 0; j < HASP_COLS; j++ ) {
            pKeyData->secTable[j] |= HASP_rows[alBuf[i]][j] << (7 - i);
        }
    }
}                        

/**
 * Load HASP key description into memory from file.
 * 
 * @param file - filename
 * @param pKeyData - memory structure, key description
 * @return - 0 in case of success or error code. Positive values - standard runtime errno codes.
 * Negative values - file processing/parsing errors.
 */
int LoadKey (char file[], PKEYDATA pKeyData) {
    FILE    *fp;
    char    *js = NULL;
    size_t  jslen;
    struct stat st;
    int     result = 0;

    stat(file, &st);
    jslen = st.st_size;
    js = malloc (jslen+1);
    if ( js != NULL ) {
        if ( (fp=fopen(file,"r")) != NULL ) {
            jslen = fread (js, sizeof(uint8_t), jslen, fp);
            fclose (fp);
            if ( jslen > 0 ) {
                js [jslen] = '\0';
                json_t *root = LoadJson (js);// Parse JSON
                if ( root != NULL ) {       // check for HASP description
                    json_t *key = json_object_get (root,"HASP Key");
                    if ( key != NULL ) {    // get keys
                        json_t *jname = json_object_get (key,"Name");
                        if ( jname ) {
                            char *sval = (char *)json_string_value(jname);
                            int l = min(sizeof(pKeyData->name)-1,strlen(sval));
                            strncpy (pKeyData->name, sval, l);
                            pKeyData->name [l] = '\0';
                        } else {
                            strncpy (pKeyData->name, "None", sizeof(pKeyData->name));
                        }
                        json_t *jcreated = json_object_get (key,"Created");
                        if ( jcreated ) {
                            char *sval = (char *)json_string_value(jcreated);
                            int l = min(sizeof(pKeyData->created)-1,strlen(sval));
                            strncpy (pKeyData->created, sval, l);                        
                            pKeyData->created [l] = '\0';
                        } else {
                            strncpy (pKeyData->created, "Not set", sizeof(pKeyData->created));
                        }
                        json_t *jpassword = json_object_get (key,"Password");
                        unsigned long password = GetLongHexValue(jpassword);
                        pKeyData->password = (password >> 16) | (password << 16);
                        json_t *jkeyType = json_object_get (key,"Type");
                        unsigned long keyType = GetLongHexValue (jkeyType);
                        pKeyData->keyType = (uint8_t)keyType;
                        json_t *jmemoryType = json_object_get (key,"Memory");
                        unsigned long memoryType = GetLongHexValue (jmemoryType);
                        pKeyData->memoryType = (uint8_t)memoryType;
                        json_t *jsn = json_object_get (key,"SN");
                        unsigned long sn = GetLongHexValue (jsn);
                        json_t *joption = json_object_get (key,"Option");
                        PBYTE_ARRAY option = GetHexByteArray (joption);
                        memcpy(pKeyData->options,option->bytes,min(option->size,sizeof(pKeyData->options)));
                        json_t *jsecTable = json_object_get (key,"SecTable");
                        PBYTE_ARRAY secTable = GetHexByteArray (jsecTable);
                        memcpy(pKeyData->secTable,secTable->bytes,min(secTable->size,sizeof(pKeyData->secTable)));
                        if ( !(joption != NULL && jsecTable != NULL && pKeyData->options[0]==1) ||
                             !(joption == NULL && jsecTable != NULL) ) {    // Universal ST case
                            BuildStandardSecTable (pKeyData);
                        }
                        json_t *jnetMemory = json_object_get(key,"NetMemory");
                        PBYTE_ARRAY netMemory = GetHexByteArray (jnetMemory);
                        memcpy(&pKeyData->netMemory[0],&sn,sizeof(sn));
                        memcpy(&pKeyData->netMemory[4],netMemory->bytes,min(netMemory->size,sizeof(pKeyData->netMemory)-4));
                        if ( jnetMemory == NULL ) {
                            memset(&pKeyData->netMemory[4], sizeof(pKeyData->netMemory)-4, 0xFF);
                            if ( pKeyData->memoryType==4 ) {    // Unlimited Net key
                                pKeyData->netMemory [6+4] = 0xFF;
                                pKeyData->netMemory [7+4] = 0xFF;
                                pKeyData->netMemory [10+4] = 0xFE;
                            } else {                           // Local key
                                pKeyData->netMemory [6+4] = 0;
                                pKeyData->netMemory [7+4] = 0;
                                pKeyData->netMemory [10+4] = 0;
                            }
                        }
                        json_t *jmemory = json_object_get(key,"Data");
                        PBYTE_ARRAY memory = GetHexByteArray (jmemory);
                        memcpy(pKeyData->memory,memory->bytes,min(memory->size,sizeof(pKeyData->memory)));
                        json_t *jedStruct = json_object_get(key,"EDStruct");
                        PBYTE_ARRAY edStruct = GetHexByteArray (jedStruct);
                        memcpy(pKeyData->edStruct,edStruct->bytes,min(edStruct->size,sizeof(pKeyData->memory)));
#ifdef DEBUG
                        syslog (LOG_DEBUG, "Password 0x%x\n", pKeyData->password);
                        syslog (LOG_DEBUG, "keyType 0x%hhx\n", pKeyData->keyType);
                        syslog (LOG_DEBUG, "MemoryType 0x%hhx\n", pKeyData->memoryType);
                        syslog (LOG_DEBUG, "Option %d bytes\n", option->size);
                        dumpArray(pKeyData->options,sizeof(pKeyData->options));
                        syslog (LOG_DEBUG, "NetMemory %d bytes\n", netMemory->size);
                        dumpArray(pKeyData->netMemory,sizeof(pKeyData->netMemory));
                        syslog (LOG_DEBUG, "SecTable %d bytes\n", secTable->size);
                        dumpArray(pKeyData->secTable,sizeof(pKeyData->secTable));
                        syslog (LOG_DEBUG, "Data %d bytes\n", memory->size);
                        dumpArray(pKeyData->memory,sizeof(pKeyData->memory));
                        syslog (LOG_DEBUG, "EDStruct %d bytes\n", edStruct->size);
                        dumpArray(pKeyData->edStruct,sizeof(pKeyData->edStruct));
#endif
                        json_decref(root);
                    } else {
                        result = -1;
                    }
                } else {
                    result = -1;
                }
            } else {
                result = -1;
            }
        } else {
            result = errno;
        }
        free (js);
    } else {
        result = errno;
    }
    return result;
}
