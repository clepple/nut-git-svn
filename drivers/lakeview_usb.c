/*
 * lakeview_usb.c - driver for UPS with lakeview chipset, such as
 *                  'Sweex Manageable UPS 1000VA' (ca. 2006)
 *
 * May also work on 'Kebo UPS-650D', not tested as of 05/23/2007
 *
 * Copyright (C) 2007 Peter van Valderen <p.v.valderen@probu.nl>
 *                    Dirk Teurlings <dirk@upexia.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "main.h"
#include "libusb.h"
#include "usb-common.h"
#include "lakeview_usb.h"

static usb_device_id_t lakeview_usb_id[] = {
	/* Sweex 1000VA */
	{ USB_DEVICE(0x0925, 0x1234),  NULL },
	/* end of list */
	{-1, -1, NULL}
};

static usb_dev_handle	*udev = NULL;
static unsigned int	comm_failures = 0;

static int execute_and_retrieve_query(char *query, char *reply)
{
	int	ret;

	ret = usb_control_msg(udev, STATUS_REQUESTTYPE, REQUEST_VALUE,
		MESSAGE_VALUE, INDEX_VALUE, query, QUERY_PACKETSIZE, 1000);

	if (ret < 0) {
		upsdebug_with_errno(3, "send");
		return ret;
	}

	if (ret == 0) {
		upsdebugx(3, "send: timeout");
		return ret;
	}

	upsdebug_hex(3, "send", query, ret);

	ret = usb_interrupt_read(udev, REPLY_REQUESTTYPE, reply, REPLY_PACKETSIZE, 1000);

	if (ret < 0) {
		upsdebug_with_errno(3, "read");
		return ret;
	}

	if (ret == 0) {
		upsdebugx(3, "read: timeout");
		return ret;
	}

	upsdebug_hex(3, "read", reply, ret);
	return ret;
}

static int query_ups(char *reply)
{
	/*
	 * This packet is a status request to the UPS
	 */
	char	query[QUERY_PACKETSIZE] = { 0x01, 0x00, 0x00, 0x30 };

	return execute_and_retrieve_query(query, reply);
}

static void usb_comm_fail(const char *fmt, ...)
{
	int	ret;
	char	why[SMALLBUF];
	va_list	ap;

	/* this means we're probably here because select was interrupted */
	if (exit_flag != 0) {
		return;	 /* ignored, since we're about to exit anyway */
	}

	comm_failures++;

	if ((comm_failures == USB_ERR_LIMIT) || ((comm_failures % USB_ERR_RATE) == 0)) {
		upslogx(LOG_WARNING, "Warning: excessive comm failures, limiting error reporting");
	}

	/* once it's past the limit, only log once every USB_ERR_LIMIT calls */
	if ((comm_failures > USB_ERR_LIMIT) && ((comm_failures % USB_ERR_LIMIT) != 0)) {
		return;
	}

	/* generic message if the caller hasn't elaborated */
	if (!fmt) {
		upslogx(LOG_WARNING, "Communications with UPS lost - check cabling");
		return;
	}

	va_start(ap, fmt);
	ret = vsnprintf(why, sizeof(why), fmt, ap);
	va_end(ap);

	if ((ret < 1) || (ret >= (int) sizeof(why))) {
		upslogx(LOG_WARNING, "usb_comm_fail: vsnprintf needed more than %d bytes", sizeof(why));
	}

	upslogx(LOG_WARNING, "Communications with UPS lost: %s", why);
}

static void usb_comm_good()
{
	if (comm_failures == 0) {
		return;
	}

	upslogx(LOG_NOTICE, "Communications with UPS re-established");	
	comm_failures = 0;
}

static usb_dev_handle *open_lakeview_usb(void)
{
	struct usb_bus	*busses = usb_get_busses();
	struct usb_bus	*bus;

	for (bus = busses; bus; bus = bus->next) {

		struct usb_device *dev;

		for (dev = bus->devices; dev; dev = dev->next) {
			/* XXX Check for Lakeview USB compatible devices  */
			if (dev->descriptor.bDeviceClass != USB_CLASS_PER_INTERFACE) {
				continue;
			}

			if (is_usb_device_supported(lakeview_usb_id,
					dev->descriptor.idVendor, dev->descriptor.idProduct) == SUPPORTED) {
				return usb_open(dev);
			}
		}
	}

	return NULL;
}

/*
 * Connect to the UPS
 */
static int open_ups(void)
{
	static int	libusb_init = 0;
	int		dev_claimed = 0;
	int		retry;

	if (!libusb_init) {
		/* Initialize Libusb */
		usb_init();
		libusb_init = 1;
	}

	usb_find_busses();
	usb_find_devices();

	udev = open_lakeview_usb();

	if (!udev) {
		upslogx(LOG_ERR, "Can't open Lakeview USB device");
		goto errout;
	}

#if LIBUSB_HAS_DETACH_KRNL_DRV
	/*
	 * this method requires at least libusb 0.1.8:
	 * it force device claiming by unbinding
	 * attached driver... From libhid
	 */
	for (retry = 3; usb_set_configuration(udev, 1); retry--) {

		if (retry == 0) {
			upslogx(LOG_ERR, "Can't set Lakeview USB configuration");
			goto errout;
		}

		upsdebugx(2, "Can't set Lakeview USB configuration, trying %d more time(s)...", retry);

		if (usb_detach_kernel_driver_np(udev, 0) < 0) {
			upsdebugx(2, "failed to detach kernel driver from USB device...");
		} else {
			upsdebugx(2, "detached kernel driver from USB device...");
		}
	}
#else
	if (usb_set_configuration(udev, 1) < 0) {
		upslogx(LOG_ERR, "Can't set Lakeview USB configuration");
		goto errout;
	}
#endif

	if (usb_claim_interface(udev, 0) < 0) {
		upslogx(LOG_ERR, "Can't claim Lakeview USB interface");
		goto errout;
	}

	dev_claimed = 1;

	if (usb_set_altinterface(udev, 0) < 0) {
		upslogx(LOG_ERR, "Can't set Lakeview USB alternate interface");
		goto errout;
	}

	if (usb_clear_halt(udev, 0x81) < 0) {
		upslogx(LOG_ERR, "Can't reset Lakeview USB endpoint");
		goto errout;
	}

	return 1;

errout:
	if (udev && dev_claimed) {
		usb_release_interface(udev, 0);
	}

	if (udev) {
		usb_close(udev);
		udev = NULL;
	}

	return 0;
}

static int close_ups(void)
{
	if (udev) {
		usb_release_interface(udev, 0);
		usb_close(udev);
		udev = NULL;
	}

	return 0;
}

/*
 * Initialise the UPS
 */
void upsdrv_initups(void)
{
	char	reply[REPLY_PACKETSIZE];
	int	i;

	for (i = 0; !open_ups(); i++) {

		if ((i < 32) && (sleep(5) == 0)) {
			usb_comm_fail("Can't open Lakeview USB device, retrying ...");
			continue;
		}

		fatalx(EXIT_FAILURE,
			"Unable to find Lakeview UPS device on USB bus \n\n"

			"Things to try:\n"
			" - Connect UPS device to USB bus\n"
			" - Run this driver as another user (upsdrvctl -u or 'user=...' in ups.conf).\n"
			"   See upsdrvctl(8) and ups.conf(5).\n\n"

			"Fatal error: unusable configuration");
	}

	/*
	 * Read rubbish data a few times; the UPS doesn't seem to respond properly
	 * the first few times after connecting
	 */
	for (i = 0; i < 5; i++) {
		query_ups(reply);
		sleep(1);
	}
}

void upsdrv_cleanup(void)
{
	close_ups();
}

void upsdrv_initinfo(void)
{
	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);

	dstate_setinfo("ups.mfr", "Lakeview Research compatible");
	dstate_setinfo("ups.model", "Unknown");
}

void upsdrv_updateinfo(void)
{
	char	reply[REPLY_PACKETSIZE];
	int	ret, online, battery_normal;

	if (!udev && !open_ups()) {
		usb_comm_fail("Reconnect to UPS failed");
		return;
	}

	ret = query_ups(reply);

	if (ret < 4) {
		usb_comm_fail("Query to UPS failed");
		dstate_datastale();

		close_ups();
		return;
	}

	usb_comm_good();
	dstate_dataok();

	/*
	 * 3rd bit of 4th byte indicates whether the UPS is on line (1)
	 * or on battery (0)
	 */
	online = (reply[3]&4)>>2;

	/*
	 * 2nd bit of 4th byte indicates battery status; normal (1)
	 * or low (0)
	 */
	battery_normal = (reply[3]&2)>>1;

	status_init();

	if (online) {
	    status_set("OL");
	} else {
	    status_set("OB");
	}

	if (!battery_normal) {
	    status_set("LB");
	}

	status_commit();
}

/*
 * The shutdown feature is a bit strange on this UPS IMHO, it
 * switches the polarity of the 'Shutdown UPS' signal, at which
 * point it will automatically power down once it loses power.
 *
 * It will still, however, be possible to poll the UPS and
 * reverse the polarity _again_, at which point it will
 * start back up once power comes back.
 *
 * Maybe this is the normal way, it just seems a bit strange.
 *
 * Please note, this function doesn't power the UPS off if
 * line power is connected.
 */
void upsdrv_shutdown(void)
{
	/*
	 * This packet shuts down the UPS, that is,
	 * if it is not currently on line power
	 */
	char	prepare[QUERY_PACKETSIZE] = { 0x02, 0x00, 0x00, 0x00 };

	/*
	 * This should make the UPS turn itself back on once the
	 * power comes back on; which is probably what we want
	 */
	char	restart[QUERY_PACKETSIZE] = { 0x02, 0x01, 0x00, 0x00 };
	char	reply[REPLY_PACKETSIZE];

	execute_and_retrieve_query(prepare, reply);

	/*
	 * have to, the previous command seems to be
	 * ignored if the second command comes right
	 * behind it
	 */
	sleep(1);


	execute_and_retrieve_query(restart, reply);
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools - Lakeview Research compatible USB UPS driver %s (%s)\n\n",
		DRV_VERSION, UPS_VERSION);

	experimental_driver = 1;	
}
