//
//  IOUSBDescriptorDebug.h
//  IOHIDFamily
//
//  Created by yg on 12/11/18.
//

#ifndef IOUSBDescriptorDebug_h
#define IOUSBDescriptorDebug_h

//#define DumpStructFiled(t,f) DLog ("[%p] " #t "." #f ": %d", this, (int)f)
//#include "IOUSBDescriptorDebug.h"

struct __IOUSBInterfaceDescriptor : public IOUSBInterfaceDescriptor
{
    void dump ()
    {
        DumpStructFiled (IOUSBInterfaceDescriptor,bLength);
        DumpStructFiled (IOUSBInterfaceDescriptor,bDescriptorType);
        DumpStructFiled (IOUSBInterfaceDescriptor,bInterfaceNumber);
        DumpStructFiled (IOUSBInterfaceDescriptor,bAlternateSetting);
        DumpStructFiled (IOUSBInterfaceDescriptor,bNumEndpoints);
        DumpStructFiled (IOUSBInterfaceDescriptor,bInterfaceClass);
        DumpStructFiled (IOUSBInterfaceDescriptor,bInterfaceSubClass);
        DumpStructFiled (IOUSBInterfaceDescriptor,bInterfaceProtocol);
        DumpStructFiled (IOUSBInterfaceDescriptor,iInterface);
    }
} __attribute__((packed));

struct __IOUSBEndpointDescriptor : public IOUSBEndpointDescriptor {
    void dump ()
    {
        DumpStructFiled (IOUSBEndpointDescriptor,bLength);
        DumpStructFiled (IOUSBEndpointDescriptor,bDescriptorType);
        DumpStructFiled (IOUSBEndpointDescriptor,bEndpointAddress);
        DumpStructFiled (IOUSBEndpointDescriptor,bmAttributes);
        DumpStructFiled (IOUSBEndpointDescriptor,wMaxPacketSize);
        DumpStructFiled (IOUSBEndpointDescriptor,bInterval);
    }
} __attribute__((packed));

struct __IOUSBDeviceDescriptor : public IOUSBDeviceDescriptor {
    void dump ()
    {
        DumpStructFiled (IOUSBDeviceDescriptor,bLength);
        DumpStructFiled (IOUSBDeviceDescriptor,bDescriptorType);
        DumpStructFiled (IOUSBDeviceDescriptor,bcdUSB);
        DumpStructFiled (IOUSBDeviceDescriptor,bDeviceClass);
        DumpStructFiled (IOUSBDeviceDescriptor,bDeviceSubClass);
        DumpStructFiled (IOUSBDeviceDescriptor,bDeviceProtocol);
        DumpStructFiled (IOUSBDeviceDescriptor,bDeviceProtocol);
        DumpStructFiled (IOUSBDeviceDescriptor,bMaxPacketSize0);
        DumpStructFiled (IOUSBDeviceDescriptor,idVendor);
        DumpStructFiled (IOUSBDeviceDescriptor,idProduct);
        DumpStructFiled (IOUSBDeviceDescriptor,bcdDevice);
        DumpStructFiled (IOUSBDeviceDescriptor,iManufacturer);
        DumpStructFiled (IOUSBDeviceDescriptor,iProduct);
        DumpStructFiled (IOUSBDeviceDescriptor,iSerialNumber);
        DumpStructFiled (IOUSBDeviceDescriptor,bNumConfigurations);

    }
} __attribute__((packed));

struct __IOUSBDescriptorHeader : public IOUSBDescriptorHeader
{
    void dump ()
    {
        DumpStructFiled (IOUSBDescriptorHeader,bLength);
        DumpStructFiled (IOUSBDescriptorHeader,bDescriptorType);
    }
} __attribute__((packed));

struct __IOUSBConfigurationDescriptor : public IOUSBConfigurationDescriptor
{
    void dump ()
    {
        DumpStructFiled (IOUSBConfigurationDescriptor,bLength);
        DumpStructFiled (IOUSBConfigurationDescriptor,bDescriptorType);
        DumpStructFiled (IOUSBConfigurationDescriptor,wTotalLength);
        DumpStructFiled (IOUSBConfigurationDescriptor,bNumInterfaces);
        DumpStructFiled (IOUSBConfigurationDescriptor,bConfigurationValue);
        DumpStructFiled (IOUSBConfigurationDescriptor,iConfiguration);
        DumpStructFiled (IOUSBConfigurationDescriptor,bmAttributes);
        DumpStructFiled (IOUSBConfigurationDescriptor,MaxPower);
        
        const IOUSBDescriptorHeader * desc = NULL;
        while (( desc = IOUSBGetNextDescriptor (this, desc))) {
            switch (desc->bDescriptorType) {
                case kUSBDeviceDesc:
                    ((__IOUSBDeviceDescriptor *) desc)->dump ();
                    break;
                case kUSBInterfaceDesc:
                    ((__IOUSBInterfaceDescriptor *) desc)->dump ();
                    break;
                case kUSBEndpointDesc:
                    ((__IOUSBEndpointDescriptor *) desc)->dump ();
                    break;
                case kUSBDeviceQualifierDesc:
                case kUSBOtherSpeedConfDesc:
                case kUSBInterfacePowerDesc:
                case kUSBStringDesc:
                case kUSBConfDesc:
                default:
                    ((__IOUSBDescriptorHeader*)desc)->dump ();
                    break;
            }
        }
    }
} __attribute__((packed));




#endif /* IOUSBDescriptorDebug_h */
