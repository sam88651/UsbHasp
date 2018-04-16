/*
 * Copyright (C) 2009-2011 Michael Singer <michael@a-singer.de>
 * Copyright (C) 2017 Revisited by Sam88651 as Linux user space application
 * 
 * Module Name:
 *      USBHasp.c
 * Abstract:
 *      Main for application.
 * Notes:
 * Revision History:
 */
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include "USBKeyEmu.h"

static const uint8_t devDesc [MAX_DEVDESC] = {
	18,     // descriptor length
	0x1,    // type: device descriptor
	0x00,   // bcd usb release number
	0x02,   //  "
	0xFF,   // device class: per interface
	0x0,    // device sub class
	0x0,    // device protocol
	0x8,    // max packet size
	0x29,   // vendor id
	0x05,   // "
	0x01,   // product id
	0x00,   //  "
	0x25,   // bcd device release number
	0x03,   //  "
	0x1,    // manufacturer string
	0x2,    // product string
	0x0,    // serial number string
	0x1     // number of configurations
};

static const uint8_t confDesc [MAX_CONFDESC] = {
	9,      // descriptor length
	2,      // type: configuration descriptor
	18,     // total descriptor length (configuration+interface)
	0,      //  "
	1,      // number of interfaces
	1,      // configuration index
	0,      // configuration string
	0x80,   // attributes: none
	54/2,   // max power
	9,      // descriptor length
	4,      // type: interface
	0,      // interface number
	0,      // alternate setting
	0,      // number of endpoints
	0xFF,   // interface class
	0,      // interface sub class
	0,      // interface protocol
	0       // interface string
};

static const uint8_t strDesc [MAX_STRDESC] = {
	4,      // descriptor length
	3,      // type: string
	0x09,   // lang id: english (us)
	0x04    //  "
};

// Device name UTF16 encoded
static uint16_t *deviceName = VENDORFW_H7;

// Keys arrays
static USB_HASP haspKeys [MAX_HASPKEYS];

// threads "stop" semaphore
static sem_t mutex;

/**
 * Standard signals handler
 * 
 * @param signo
 */
void SignalHandler (int signo) {
    
    if (signo == SIGINT || signo == SIGKILL || signo == SIGQUIT || 
        signo == SIGABRT || signo == SIGTERM || signo == SIGSTOP) {
        syslog (LOG_INFO, "Received signal to stop.\n");
        sem_post (&mutex);
    }
}

/**
 * Daemonize. Taken from C Posix example.
 */
static void Daemonize(void)
{
        pid_t pid;
                
    pid = fork();                   // Fork off the parent process
    if (pid < 0) {                  // An error occurred
        syslog (LOG_ERR, "Unable to fork off parent process.\n");
        exit (EXIT_FAILURE);
    } else if (pid > 0) {           // Success: Let the parent terminate
        exit (EXIT_SUCCESS);
    } if (setsid() < 0) {           // On success: The child process becomes session leader
        syslog (LOG_ERR, "Unable to setsid.\n");
        exit (EXIT_FAILURE);
    }
    pid = fork();                   // Fork off for the second time
    if (pid < 0) {                  // An error occurred
        syslog (LOG_ERR, "Unable to fork for the second time.\n");
        exit (EXIT_FAILURE);
    } if (pid > 0) {                // Success: Let the parent terminate
        exit (EXIT_SUCCESS);
    }
    umask(0);                       // Set new file permissions
                                    // Close standard file descriptors
    fclose (stderr);
    fclose (stdin);
    fclose (stdout);
}

/**
 * Main. Receives command line arguments - daemonize or not and list of key files.
 * Files are in JSON format.
 * 
 * @return 
 */
int main (int argc, char *argv[]) {
        int     numKeys, i;
        char    *bus_id;
        int32_t usb_bus_num;
        int32_t id;
        int     fd;
        int     opt;
        int     rc;
        bool    daemonize = false;

    numKeys = 0;
    while ((opt=getopt(argc,argv, "?hd")) != -1) {
        switch (opt) {
        case 'd':
            daemonize = true;
            break;
        default:
        case '?':
        case 'h':
            fprintf (stderr,"Usage: #%s [-d] keyfile1.json ... keyfile%d.json\n", argv[0], MAX_HASPKEYS);
            return -1;
        }
    }
    // Prepare log file    
    openlog ("usbhasp", LOG_CONS | LOG_PID | LOG_NDELAY | (daemonize?0:LOG_PERROR), LOG_LOCAL1);
    // Load keys    
    for ( numKeys = 0, i = optind; i < argc && numKeys < MAX_HASPKEYS; i++ ) {
        int result = LoadKey (argv[i], &haspKeys[numKeys].keyData);
        if ( result > 0 ) {
            syslog (LOG_ERR, "Error %s loading keyfile %s.\n", strerror(result), argv[i]);
        } else if ( result < 0 ) {
            syslog (LOG_ERR, "Error parsing key file %s\n", argv[i]);
        } else {                            // key has been loaded
        syslog (LOG_INFO, "Loaded key %d: '%s', Created: %s\n", numKeys, haspKeys [numKeys].keyData.name, 
                                                       haspKeys [numKeys].keyData.created);
                                            // contains the address of our device connected
                                            // to the port (the device is not yet connected)
        haspKeys [numKeys].addr = 0xFF;     // address not set yet
                                            // contains the status of the port
        memset (&haspKeys [numKeys].stat, 0, sizeof(haspKeys [numKeys].stat));
        haspKeys [numKeys].port = numKeys+1;
        memcpy (&haspKeys [numKeys].devDesc, devDesc, sizeof(haspKeys [numKeys].devDesc));
        memcpy (&haspKeys [numKeys].confDesc, confDesc, sizeof(haspKeys [numKeys].confDesc));
        memcpy (&haspKeys [numKeys].strDesc, strDesc, sizeof(haspKeys [numKeys].strDesc));
        haspKeys [numKeys].deviceName = deviceName;
        strncpy (haspKeys [numKeys].keyfileName, argv[i], sizeof(haspKeys [numKeys].keyfileName));
        haspKeys [numKeys].keyfileName [sizeof(haspKeys [numKeys].keyfileName)-1] = '\0';
        ++numKeys;
        }
    }
    sem_init (&mutex, 0, 0);
    if ( signal (SIGINT, SignalHandler) == SIG_ERR ) {
        syslog(LOG_ERR, "Can't catch SIGINT\n");
        rc =  errno;
    } else {
        if ( numKeys > 0 ) {
            bus_id = NULL;
            fd = usb_vhci_open (numKeys, &id, &usb_bus_num, &bus_id);
            if ( fd < 0 ) {
                syslog (LOG_ERR, "Unable to create USB device. Is vhci_hcd driver loaded?\n");
                rc = -1;
            } else {
                syslog (LOG_INFO, "USB device created %s (bus# %d)\n", bus_id, usb_bus_num);

                if ( daemonize ) {
                    Daemonize();
                }
                UsbDevice (fd, haspKeys, numKeys, &mutex);

                sem_destroy (&mutex);
                usb_vhci_close (fd);
                syslog (LOG_INFO, "USB device removed %s (bus# %d)\n", bus_id, usb_bus_num);
                rc = EXIT_SUCCESS;
            }
        } else {
            syslog(LOG_WARNING, "No keys loaded. Nothing to emulate.\n");
            rc = -1;
        }
    }
    closelog ();
    return rc;
}
