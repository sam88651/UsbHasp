/**
 * Copyright (C) 2017 Sam88651.
 * 
 * Module Name:
 *     USBDevice.c
 * Abstract:
 *      HASP key emulator
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
#include <semaphore.h> 
#include <signal.h>
#include <syslog.h>
#include <libusb_vhci.h>
#include "USBKeyEmu.h"

/**
 * General USB devices URB request manager
 * 
 * @param urb
 */
static void ProcessUrb (PUSBHASP pusbDevice, struct usb_vhci_urb *urb) {
        int c;
        
    if ( !usb_vhci_is_control (urb->type) )	{
        return;
    }
    if ( urb->epadr & 0x7f ) {
#if DEBUG > 2
        syslog (LOG_DEBUG, "DEVICE STALLED\n");
#endif        
	urb->status = USB_VHCI_STATUS_STALL;
	return;
    }
    uint8_t rt = urb->bmRequestType;
    uint8_t r = urb->bRequest;
    if ( rt == 0x00 && r == URB_RQ_SET_CONFIGURATION ) {
#if DEBUG > 2
        syslog (LOG_DEBUG, "SET_CONFIGURATION\n");
#endif        
        urb->status = USB_VHCI_STATUS_SUCCESS;
    } else if ( rt == 0x00 && r == URB_RQ_SET_INTERFACE ) {
#if DEBUG > 2        
        syslog (LOG_DEBUG, "SET_INTERFACE\n");
#endif        
        urb->status = USB_VHCI_STATUS_SUCCESS;
    } else if ( rt == 0x80 && r == URB_RQ_GET_DESCRIPTOR ) {
#if DEBUG > 2
        syslog (LOG_DEBUG, "GET_DESCRIPTOR\n");
#endif        
        int l = urb->wLength;
        uint8_t *buffer = urb->buffer;
        switch(urb->wValue >> 8) {
        case 1:
#if DEBUG > 2
            syslog (LOG_DEBUG, "DEVICE_DESCRIPTOR\n");
#endif        
            if ( pusbDevice->devDesc != NULL ) {
                if ( pusbDevice->devDesc[0] < l ) {
                l = pusbDevice->devDesc[0];
                }
                memcpy (buffer, pusbDevice->devDesc, l);
                urb->buffer_actual = l;
                urb->status = USB_VHCI_STATUS_SUCCESS;
            }
            break;
        case 2:
#if DEBUG > 2
            syslog (LOG_DEBUG, "CONFIGURATION_DESCRIPTOR\n");
#endif       
            if ( pusbDevice->confDesc != NULL ) {
                if ( pusbDevice->confDesc[2] < l ) {
                    l = pusbDevice->confDesc[2];
                }
                memcpy (buffer, pusbDevice->confDesc, l);
                urb->buffer_actual = l;
                urb->status = USB_VHCI_STATUS_SUCCESS;
            }
            break;
        case 3:
#if DEBUG > 2
            syslog (LOG_DEBUG, "STRING_DESCRIPTOR\n");
#endif            
            switch(urb->wValue & 0xff) {
            case 0:
                if ( pusbDevice->strDesc != NULL ) {
                    if ( pusbDevice->strDesc[0] < l ) {
                        l = pusbDevice->strDesc[0];
                    }
                    memcpy (buffer, pusbDevice->strDesc, l);
                    urb->buffer_actual = l;
                    urb->status = USB_VHCI_STATUS_SUCCESS;
                }
                break;                
            case 1:
                if ( pusbDevice->deviceName != NULL ) {
                    for ( c = 0; pusbDevice->deviceName[c] != 0; c++ );
                    c *= 2;
                    if ( c+2 < l ) {
                        l = c+2;
                    }
                    buffer [0] = (uint8_t)l;
                    buffer [1] = 0x03;
                    memcpy (&buffer[2], pusbDevice->deviceName, l);
                    urb->buffer_actual = l;
                    urb->status = USB_VHCI_STATUS_SUCCESS;
                }
                break;
            default:
                urb->status = USB_VHCI_STATUS_STALL;
                break;
            }
            break;
        default:
#if DEBUG > 2
            syslog (LOG_DEBUG, "DEVICE STALL\n");
#endif            
            urb->status = USB_VHCI_STATUS_STALL;
            break;
        }
    } else if ( rt == 0xc0 ) {
        //
        // IO request to USB hardware
        // 
#ifdef DEBUG        
        syslog (LOG_DEBUG, "urb->bRequest 0x%hhx, urb->wValue 0x%hx, urb->wIndex 0x%hx, urb->wLength 0x%hx\n", 
                urb->bRequest, urb->wValue, urb->wIndex, urb->wLength);
        syslog (LOG_DEBUG, "HASP FUNCTION - ");
#endif        
        KEY_REQUEST request;
        request.majorFnCode = urb->bRequest; // Requested fn number (type of KEY_FN_LIST)
        request.param1 = urb->wValue;        // Key parameters
        request.param2 = urb->wIndex;
        request.param3 = urb->wLength;
        EmulateKey (&pusbDevice->keyData,(PKEY_REQUEST)&request, &urb->buffer_length, (PKEY_RESPONSE)urb->buffer);
        urb->buffer_actual = urb->buffer_length;
        urb->status = USB_VHCI_STATUS_SUCCESS;
    } else {
#if DEBUG > 2
        syslog (LOG_DEBUG, "DEVICE STALL\n");
#endif        
        urb->status = USB_VHCI_STATUS_STALL;
    }
}

/**
 * HASP keys requests manager
 * 
 * @param arg
 */
void UsbDevice (int fd, USB_HASP haspKeys[], int numKeys, sem_t *pmutex) {
        int value = 0;
        int pindex;
        struct usb_vhci_work w;
    
    if ( fd < 0 ) {
        syslog (LOG_ERR, "USB (UsbDevice) bad file descriptor: %d.\n", fd);
        return;
    }
    while ( !value ) {
        int res = usb_vhci_fetch_work (fd, &w);
        if ( res == -1 ) {
            if ( errno != ETIMEDOUT && errno != EINTR && errno != ENODATA ) {
                syslog (LOG_ERR, "USB (usb_vhci_fetch_work) failed: %s.\n", strerror(errno));
                continue;
            }
        } else {
            uint16_t status, change;
            uint8_t flags, index;
            switch(w.type) {
            case USB_VHCI_WORK_TYPE_PORT_STAT:
                status = w.work.port_stat.status;
                change = w.work.port_stat.change;
                flags = w.work.port_stat.flags;
                index = w.work.port_stat.index;
#if DEBUG > 2
                syslog (LOG_DEBUG, "Got port %hhu stat work. Status: 0x%04hx, change: 0x%04hx, flags: 0x%02hhx\n", index, status, change, flags);
#endif                    
                if ( index > numKeys || index < 1 ) {
                    syslog (LOG_ERR, "Wrong port number %hhu\n", index);
                    continue;
                }
                pindex = index-1;
                struct usb_vhci_port_stat prev;
                memcpy (&prev, &haspKeys [pindex].stat, sizeof(prev));
                memcpy (&haspKeys [pindex].stat, &w.work.port_stat, sizeof(haspKeys [pindex].stat));
                if ( change & USB_VHCI_PORT_STAT_C_CONNECTION ) {
                                    // CONNECTION state changed -> invalidating address
                    haspKeys [pindex].addr = 0xff;
                }
                if ( change & USB_VHCI_PORT_STAT_C_RESET && ~status & USB_VHCI_PORT_STAT_RESET && status & USB_VHCI_PORT_STAT_ENABLE ) {
                                    // RESET successfull -> use default address
                    haspKeys [pindex].addr = 0;
                }
                if ( prev.status & USB_VHCI_PORT_STAT_POWER && ~status & USB_VHCI_PORT_STAT_POWER ) {
                    syslog (LOG_INFO, "Port %d is powered off.\n", haspKeys [pindex].port);
                }
                if ( ~prev.status & USB_VHCI_PORT_STAT_POWER && status & USB_VHCI_PORT_STAT_POWER ) {
                    syslog (LOG_INFO, "Port %d is powered on -> connecting device. ", haspKeys [pindex].port);
                    if ( usb_vhci_port_connect (fd, haspKeys [pindex].port, USB_VHCI_DATA_RATE_FULL) == -1 ) {
                        syslog (LOG_ERR, "USB (usb_vhci_port_connect), port %d failed: %s.\n", haspKeys [pindex].port, strerror(errno));
                        break;
                    } else {
                        syslog (LOG_INFO, "Port %d connected.\n", haspKeys [pindex].port);
                    }
                }
                if ( ~prev.status & USB_VHCI_PORT_STAT_RESET && status & USB_VHCI_PORT_STAT_RESET ) {
                                    // Port is resetting
                    if ( status & USB_VHCI_PORT_STAT_CONNECTION ) {
                                    // completing reset
                        if ( usb_vhci_port_reset_done (fd, haspKeys [pindex].port, 1) == -1 ) {
                            syslog (LOG_ERR, "USB (usb_vhci_port_reset_done) port %d failed: %s.\n", haspKeys [pindex].port, strerror(errno));
                            break;
                        }
                    }
                }
                if ( ~prev.flags & USB_VHCI_PORT_STAT_FLAG_RESUMING && flags & USB_VHCI_PORT_STAT_FLAG_RESUMING ) {
                                    // Port is resuming
                    if ( status & USB_VHCI_PORT_STAT_CONNECTION ) {
                                    // completing resume
                        if ( usb_vhci_port_resumed (fd, haspKeys [pindex].port) == -1) {
                            syslog (LOG_ERR, "USB (usb_vhci_port_resumed), port %d failed: %s.\n", haspKeys [pindex].port, strerror(errno));
                            break;
                        }
                    }
                }
                if ( ~prev.status & USB_VHCI_PORT_STAT_SUSPEND && status & USB_VHCI_PORT_STAT_SUSPEND ) {
                                    // Port is suspended
                    syslog (LOG_INFO, "Port %d is suspended.\n", haspKeys [pindex].port);
                }
                if ( prev.status & USB_VHCI_PORT_STAT_ENABLE && ~status & USB_VHCI_PORT_STAT_ENABLE ) {
                                    // Port is disabled
                    syslog (LOG_INFO, "Port %d is disabled.\n", haspKeys [pindex].port);
                }
                break;
            case USB_VHCI_WORK_TYPE_PROCESS_URB:
                for ( int i = 0; i < numKeys; i++ ) {
                    if ( haspKeys[i].addr == w.work.urb.devadr ) {
                        pindex = i;
                        break;
                    }
                }                
                if ( pindex < 0 || pindex >= numKeys ) {
                    syslog (LOG_ERR, "Wrong device address %hhu\n", w.work.urb.devadr);
                    break;                    
                }
#if DEBUG > 2
                syslog (LOG_DEBUG, "Got process urb work for port %d\n", haspKeys [pindex].port);
#endif                    
                w.work.urb.buffer = NULL;
                w.work.urb.iso_packets = NULL;
                if ( w.work.urb.devadr != haspKeys [pindex].addr ) { // not for me
                    break;
                }
                if ( w.work.urb.buffer_length ) {
                    w.work.urb.buffer = (uint8_t *)malloc(w.work.urb.buffer_length);
                }
                if ( w.work.urb.packet_count ) {
                    w.work.urb.iso_packets = (struct usb_vhci_iso_packet *)malloc(w.work.urb.packet_count * sizeof(struct usb_vhci_iso_packet));
                }
                if ( res ) {            // usb_vhci_fetch_work has returned a value != 0
                    res = usb_vhci_fetch_data (fd, &w.work.urb);
                    if ( res == -1 ) {
                        if ( errno != ECANCELED ) {
                            syslog (LOG_ERR, "USB (usb_vhci_fetch_data) port %d failed: %s.\n", haspKeys [pindex].port, strerror(errno));
                        }
                        if ( w.work.urb.buffer != NULL ) {
                            free (w.work.urb.buffer);
                            w.work.urb.buffer = NULL;
                        }
                        if ( w.work.urb.iso_packets != NULL ) {
                            free (w.work.urb.iso_packets);
                            w.work.urb.iso_packets = NULL;
                        }
                    }
                }
                                        // SET_ADDRESS?
                if ( usb_vhci_is_control (w.work.urb.type) && !(w.work.urb.epadr & 0x7f) &&
                        !w.work.urb.bmRequestType && w.work.urb.bRequest == 5 ) {
                    if ( w.work.urb.wValue > 0x7f ) {
                        w.work.urb.status = USB_VHCI_STATUS_STALL;
                    } else {
                        w.work.urb.status = USB_VHCI_STATUS_SUCCESS;
                        haspKeys [pindex].addr = (uint8_t)w.work.urb.wValue;
                        syslog (LOG_INFO, "Set device on port %d address = %d\n", haspKeys [pindex].port, haspKeys [pindex].addr);
                    }
                } else {                // any other than SET_ADDRESS?
                    ProcessUrb (&haspKeys [pindex], &w.work.urb);
                }
                if ( usb_vhci_giveback (fd, &w.work.urb) == -1 ) {
                    syslog (LOG_ERR, "USB (usb_vhci_giveback), port %d failed: %s.\n", haspKeys [pindex].port, strerror(errno));
                }
                if ( w.work.urb.buffer != NULL ) {
                    free (w.work.urb.buffer);
                    w.work.urb.buffer = NULL;
                }
                if ( w.work.urb.iso_packets != NULL ) {
                    free (w.work.urb.iso_packets);
                    w.work.urb.iso_packets = NULL;
                }
                break;
            case USB_VHCI_WORK_TYPE_CANCEL_URB: // Got cancel urb work
                break;
            default:
                syslog (LOG_ERR, "Got invalid work for port, type %d\n", w.type);
                break;
            }
        }
        sem_getvalue (pmutex, &value); 
    }
}
