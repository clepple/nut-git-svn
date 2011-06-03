/* scan_usb.c: detect NUT supported USB devices
 * 
 *  Copyright (C) 2011 - Frederic Bohe <fredericbohe@eaton.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "config.h"
#ifdef HAVE_USB_H
#include "upsclient.h"
#include "nutscan-usb.h"
#include <stdio.h>
#include "device.h"

static char* is_usb_device_supported(usb_device_id_t *usb_device_id_list, int dev_VendorID, int dev_ProductID)
{
        usb_device_id_t *usbdev;

        for (usbdev = usb_device_id_list; usbdev->driver_name != NULL; usbdev++) {

                if ( (usbdev->vendorID == dev_VendorID)
                        && (usbdev->productID == dev_ProductID) ) {

                        return usbdev->driver_name;
                }
        }

        return NULL;
}

/* return NULL if no error */
device_t * scan_usb()
{
        int ret;
        char string[256];
        char *driver_name = NULL;
        char *serialnumber = NULL;
        char *device_name = NULL;
        char *vendor_name = NULL;
        struct usb_device *dev;
        struct usb_bus *bus;
        usb_dev_handle *udev;

	device_t * nut_dev = NULL;
	device_t * current_nut_dev = NULL;

        /* libusb base init */
        usb_init();
        usb_find_busses();
        usb_find_devices();

        for (bus = usb_busses; bus; bus = bus->next) {
                for (dev = bus->devices; dev; dev = dev->next) {

                        /*printf("Checking USB device %04x:%04x (Bus: %s, Device: %s)\n",
                                dev->descriptor.idVendor, dev->descriptor.idProduct,
                                bus->dirname, dev->filename);*/

                        if ((driver_name = is_usb_device_supported(usb_device_table,
                                dev->descriptor.idVendor, dev->descriptor.idProduct)) != NULL) {

                                /*printf("=== supported...\n");*/

                                /* open the device */
                                udev = usb_open(dev);
                                if (!udev) {
                                        fprintf(stderr,"Failed to open device, skipping. (%s)\n", usb_strerror());
                                        continue;
                                }

                                /* get serial number */
                                if (dev->descriptor.iSerialNumber) {
                                        ret = usb_get_string_simple(udev, dev->descriptor.iSerialNumber,
                                                string, sizeof(string));
                                        if (ret > 0) {
                                                serialnumber = strdup(string);
                                        }
                                }
                                /* get product name */
                                if (dev->descriptor.iProduct) {
                                        ret = usb_get_string_simple(udev, dev->descriptor.iProduct,
                                                string, sizeof(string));
                                        if (ret > 0) {
                                                device_name = strdup(string);
                                        }
                                }

                                /* get vendor name */
                                if (dev->descriptor.iManufacturer) {
                                        ret = usb_get_string_simple(udev, dev->descriptor.iManufacturer, 
                                                string, sizeof(string));
                                        if (ret > 0) {
                                                vendor_name = strdup(string);
                                        }
                                }

				nut_dev = new_device();
				if(nut_dev == NULL) {
                                        fprintf(stderr,"Memory allocation error\n");
					free_device(current_nut_dev);
					free(serialnumber);
					free(device_name);
					free(vendor_name);
					return NULL;
				}

				nut_dev->type = TYPE_USB;
				if(driver_name) {
					nut_dev->driver = strdup(driver_name);
				}
				nut_dev->port = strdup("auto");
				nut_dev->opt.usb_opt.vendorid = dev->descriptor.idVendor;
				nut_dev->opt.usb_opt.productid = dev->descriptor.idProduct;
				if(device_name) {
					nut_dev->opt.usb_opt.product_name = strdup(device_name);
					free(device_name);
				}
				if(serialnumber) {
					nut_dev->opt.usb_opt.serial_number = strdup(serialnumber);
					free(serialnumber);
				}
				if(vendor_name) {
					nut_dev->opt.usb_opt.vendor_name = strdup(vendor_name);
					free(vendor_name);
				}
				nut_dev->opt.usb_opt.bus = strdup(bus->dirname);
				

				if(current_nut_dev==NULL) {
					current_nut_dev = nut_dev;
				}
				else {
					current_nut_dev->next = nut_dev;
					current_nut_dev  = nut_dev;
				}
				
                                memset (string, 0, 256);

                                usb_close(udev);
                        }
                }
        }

	return current_nut_dev;
}
#endif /* HAVE_USB_H */
