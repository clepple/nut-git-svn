/* tripplite_usb.c - model specific routines for Tripp Lite entry-level USB
   models.  (tested with: "OMNIVS1000")

   tripplite_usb.c was derived from tripplite.c by Charles Lepple
   tripplite.c was derived from Russell Kroll's bestups.c by Rik Faith.

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
   Copyright (C) 2001  Rickard E. (Rik) Faith <faith@alephnull.com>
   Copyright (C) 2004  Nicholas J. Kain <nicholas@kain.us>
   Copyright (C) 2005  Charles Lepple <clepple+nut@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/* UPS Commands: (capital letters are literals, lower-case are variables)
 * :B     -> Bxxxxyy (xxxx/55.0: Hz in? yy/16: battery voltage)
 * :F     -> F1143_A (where _ = \0) Firmware version?
 * :L     -> LvvvvXX (vvvv/2.0: VAC out?)
 * :P     -> P01000X (1000VA unit)
 * :S     -> Sbb_XXX (bb = 10: on-line, 11: on battery)
 * :V     -> V102XXX (firmware version)
 * :Wt    -> Wt      (watchdog; t = time in seconds (binary, not hex), 
 *                   0 = disable)
 *
 * The outgoing commands are sent with HID Set_Report commands over EP0
 * (control message), and incoming commands are received on EP1IN (interrupt
 * endpoint). The UPS completely ignores the conventions of Set_Idle (where
 * you NAK the interrupt read if you have no new data), so you constantly have
 * to poll EP1IN.
 *
 * The descriptors say that bInterval is 10 ms. You generally need to wait at
 * least 80-90 ms to get some characters back from the device.  If it takes
 * more than 250 ms, you probably need to resend the command.
 * 
 * All outgoing commands are followed by a checksum, which is 255 - (sum of
 * characters after ':'), and then by '\r'. All responses should start with
 * the command letter that was sent (no colon), and should be followed by
 * '\r'. If the command is not supported (or apparently if there is a serial
 * timeout internally), the previous response will be echoed back.
 *
 * Commands from serial tripplite.c:
 *
 * :N%02X -- delay the UPS for provided time (hex seconds)
 * :H%06X -- reboot the UPS.  UPS will restart after provided time (hex s)
 * :A     -- begins a self-test
 * :C     -- fetches result of a self-test
 * :K1    -- turns on power receptacles
 * :K0    -- turns off power receptacles
 * :G     -- unconfirmed: shuts down UPS until power returns
 * :Q1    -- enable "Remote Reboot"
 * :Q0    -- disable "Remote Reboot"
 * :W     -- returns 'W' data
 * :L     -- returns 'L' data
 * :V     -- returns 'V' data (firmware revision)
 * :X     -- returns 'X' data (firmware revision)
 * :D     -- returns general status data
 * :B     -- returns battery voltage (hexadecimal decivolts)
 * :I     -- returns minimum input voltage (hexadecimal hertz)
 * :M     -- returns maximum input voltage (hexadecimal hertz)
 * :P     -- returns power rating
 * :Z     -- unknown
 * :U     -- unknown
 * :O     -- unknown
 * :E     -- unknown
 * :Y     -- returns mains frequency  (':D' is preferred)
 * :T     -- returns ups temperature  (':D' is preferred)
 * :R     -- returns input voltage    (':D' is preferred)
 * :F     -- returns load percentage  (':D' is preferred)
 * :S     -- enables remote reboot/remote power on
 */

#define DRV_VERSION "0.1"

#include "main.h"
#include "libhid.h"
#include "hid-usb.h"
#include "newhidups.h"
#include <math.h>
#include <ctype.h>

#define toprint(x) (isprint(x) ? (x) : '.')

#define ENDCHAR '\r'           /* replies end with CR LF -- use LF to end */
#define IGNCHAR '\r'           /* ignore CR */

#define MAX_SYNC_TRIES 1

#define MAX_SEND_TRIES 3
#define SEND_WAIT_SEC 0
#define SEND_WAIT_NSEC (1000*1000*100)

#define MAX_RECV_TRIES 3
#define RECV_WAIT_MSEC 100

// #define DEFAULT_OFFDELAY   64  /* seconds (max 0xFF) */
#define DEFAULT_STARTDELAY 60  /* seconds (max 0xFFFFFF) */
#define DEFAULT_BOOTDELAY  64  /* seconds (max 0xFF) */
#define MAX_VOLT 13.4          /* Max battery voltage (100%) */
#define MIN_VOLT 11.0          /* Min battery voltage (10%) */

#define USB_VID_TRIPP_LITE	0x09ae
#define USB_PID_TRIPP_LITE_OMNI	0x0001

static HIDDevice *hd;
static MatchFlags flg;


/* We calculate battery charge (q) as a function of voltage (V).
 * It seems that this function probably varies by firmware revision or
 * UPS model - the Windows monitoring software gives different q for a
 * given V than the old open source Tripp Lite monitoring software.
 *
 * The discharge curve should be the same for any given battery chemistry,
 * so it should only be necessary to specify the minimum and maximum
 * voltages likely to be seen in operation.
 */

/* Interval notation for Q% = 10% <= [minV, maxV] <= 100%  */
static float V_interval[2] = {MIN_VOLT, MAX_VOLT};

/* Time in seconds to delay before shutting down. */
static unsigned int offdelay = DEFAULT_OFFDELAY;
static unsigned int startdelay = DEFAULT_STARTDELAY;
static unsigned int bootdelay = DEFAULT_BOOTDELAY;

static int hex2d(char *start, unsigned int len)
{
	char buf[32];
	buf[32] = '\0';

	strncpy(buf, start, (len < (sizeof buf) ? len : (sizeof buf - 1)));
	return strtol(buf, NULL, 16);
}

/* All UPS commands are challenge-response, so this function makes things
 * very clean.
 *
 * You do not need to pass in the ':' or '\r'. Be sure to use sizeof(msg) instead of strlen(msg).
 *
 * return: # of chars in buf, excluding terminating \0 */
static int send_cmd(const char *msg, size_t msg_len, char *reply, size_t reply_len)
{
	char c, buffer_out[8];
	unsigned char csum = 0;
	int ret, send_try, recv_try=0, done = 0;
	size_t i = 0;

	upsdebugx(3, "send_cmd(msg_len=%d)", msg_len);

	if(msg_len > 5) {
		fatalx("send_cmd(): Trying to pass too many characters to UPS (%u)", (unsigned)msg_len);
	}

	buffer_out[0] = ':';
	for(i=1; i<8; i++) buffer_out[i] = '\0';

	for(i=0; i<msg_len; i++) {
		buffer_out[i+1] = msg[i];
		csum += msg[i];
	}

	buffer_out[i] = 255-csum;
	buffer_out[i+1] = 0x0d;

	upsdebugx(5, "send_cmd: sending %02x %02x %02x %02x %02x %02x %02x %02x",
		buffer_out[0], buffer_out[1], buffer_out[2], buffer_out[3],
		buffer_out[4], buffer_out[5], buffer_out[6], buffer_out[7]);

	for(send_try=0; !done && send_try < MAX_SEND_TRIES; send_try++) {
		upsdebugx(6, "send_cmd send_try %d", send_try+1);

		ret = libusb_set_report(0, buffer_out, sizeof(buffer_out));

		if(ret != sizeof(buffer_out)) {
			upslogx(1, "libusb_set_report() returned %d instead of %d", ret, sizeof(buffer_out));
			return -1;
		}

		if(!done) { usleep(1000*100); /* TODO: nanosleep */ }

		for(recv_try=0; !done && recv_try < MAX_RECV_TRIES; recv_try++) {
			upsdebugx(7, "send_cmd recv_try %d", recv_try+1);
			ret = libusb_get_interrupt(reply, sizeof(buffer_out), RECV_WAIT_MSEC);
			if(ret != sizeof(buffer_out)) {
				upslogx(1, "libusb_get_interrupt() returned %d instead of %d", ret, sizeof(buffer_out));
			}
			done = (ret == sizeof(buffer_out)) && (buffer_out[1] == reply[0]);
		}
	}

	if(ret == sizeof(buffer_out)) {
		upsdebugx(5, "send_cmd: received %02x %02x %02x %02x %02x %02x %02x %02x (%s)",
				reply[0], reply[1], reply[2], reply[3],
				reply[4], reply[5], reply[6], reply[7], done ? "OK" : "bad");
	}
	
	upsdebugx(((send_try != 1) || (recv_try != 1)) ? 3 : 6, 
			"send_cmd: send_try = %d, recv_try = %d\n", send_try, recv_try);

	return 8;
}

static void ups_sync(void)
{
	char msg[] = "S", buf[256];
	int tries, ret;

	for (tries = 0; tries < MAX_SYNC_TRIES; ++tries) {
		upsdebugx(3, "Trying to sync (attempt %d)", tries+1);
		ret = send_cmd(msg, sizeof(msg), buf, sizeof buf);
		if (ret > 0)
			return;
	}
	fatalx("\nFailed to find UPS - giving up...");
}

static int do_reboot_now(void)
{
#if 0
	char buf[256], cmd[16];

	snprintf(cmd, sizeof cmd, ":H%06X\r", startdelay);
	return send_cmd(cmd, buf, sizeof buf);
#else
	return 0;
#endif
}

static void do_reboot(void)
{
#if 0
	char buf[256], cmd[16];

	snprintf(cmd, sizeof cmd, ":N%02X\r", bootdelay);
	send_cmd(cmd, buf, sizeof buf);
	do_reboot_now();
#endif
}

static int soft_shutdown(void)
{
#if 0
	char buf[256], cmd[16];

	snprintf(cmd, sizeof cmd, ":N%02X\r", offdelay);
	send_cmd(cmd, buf, sizeof buf);
	return send_cmd(":G\r", buf, sizeof buf);
#endif
}

static int hard_shutdown(void)
{
#if 0
	char buf[256], cmd[16];

	snprintf(cmd, sizeof cmd, ":N%02X\r", offdelay);
	send_cmd(cmd, buf, sizeof buf);
	return send_cmd(":K0\r", buf, sizeof buf);
#endif
}

static int instcmd(const char *cmdname, const char *extra)
{
#if 0
	char buf[256];

	if (!strcasecmp(cmdname, "test.battery.start")) {
		send_cmd(":A\r", buf, sizeof buf);
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "load.off")) {
		send_cmd(":K0\r", buf, sizeof buf);
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "load.on")) {
		send_cmd(":K1\r", buf, sizeof buf);
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "shutdown.reboot")) {
		do_reboot_now();
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "shutdown.reboot.graceful")) {
		do_reboot();
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "shutdown.return")) {
		soft_shutdown();
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		hard_shutdown();
		return STAT_INSTCMD_HANDLED;
	}
#endif

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

static int setvar(const char *varname, const char *val)
{
#if 0
	if (!strcasecmp(varname, "ups.delay.shutdown")) {
		offdelay = atoi(val);
		dstate_setinfo("ups.delay.shutdown", val);
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "ups.delay.start")) {
		startdelay = atoi(val);
		dstate_setinfo("ups.delay.start", val);
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "ups.delay.reboot")) {
		bootdelay = atoi(val);
		dstate_setinfo("ups.delay.reboot", val);
		return STAT_SET_HANDLED;
	}
#endif
	return STAT_SET_UNKNOWN;
}

void upsdrv_initinfo(void)
{
	const char *model, f_msg[] = "F", l_msg[] = "L", p_msg[] = "P",
		s_msg[] = "S", v_msg[] = "V", w_msg[] = "W\0";
	char f_value[9], l_value[9], p_value[9], s_value[9], v_value[9], w_value[9];
	int  va, ret;
	long w, l;

	/* Detect the UPS or die. */
	ups_sync();

	/* Reset watchdog: */
	ret = send_cmd(w_msg, sizeof(w_msg), w_value, sizeof(w_value)-1);

	dstate_setinfo("ups.mfr", "%s", "Tripp Lite");

	ret = send_cmd(p_msg, sizeof(p_msg), p_value, sizeof(p_value)-1);
	// p_value[6] = '\0';
	va = strtol(p_value+1, NULL, 10);

	model = "OMNIVS%d";

	dstate_setinfo("ups.model", model, va);

	ret = send_cmd(v_msg, sizeof(v_msg), v_value, sizeof(v_value)-1);

	dstate_setinfo("ups.firmware", "%c%c%c",
			toprint(v_value[1]), toprint(v_value[2]), toprint(v_value[3]));

#if 0
	snprintf(buf, sizeof buf, "%d", offdelay);
	dstate_setinfo("ups.delay.shutdown", buf);
	dstate_setflags("ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.shutdown", 3);
	snprintf(buf, sizeof buf, "%d", startdelay);
	dstate_setinfo("ups.delay.start", buf);
	dstate_setflags("ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.start", 8);
	snprintf(buf, sizeof buf, "%d", bootdelay);
	dstate_setinfo("ups.delay.reboot", buf);
	dstate_setflags("ups.delay.reboot", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.reboot", 3);

	dstate_addcmd("test.battery.start"); /* Turns off automatically */
	dstate_addcmd("load.off");
	dstate_addcmd("load.on");
	dstate_addcmd("shutdown.reboot");
	dstate_addcmd("shutdown.reboot.graceful");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stayoff");
#endif

	upsh.instcmd = instcmd;
	upsh.setvar = setvar;

	printf("Detected %s %s on %s\n",
	dstate_getinfo("ups.mfr"), dstate_getinfo("ups.model"), device_path);

	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);
}

void upsdrv_shutdown(void)
{
	soft_shutdown();
}

void upsdrv_updateinfo(void)
{
	char b_msg[] = "B", l_msg[] = "L", s_msg[] = "S";
	char b_value[9], l_value[9], s_value[9];
	int bp;
	float bv;

	int ret;

	status_init();

	/* General status (e.g. "S10") */
	ret = send_cmd(s_msg, sizeof(s_msg), s_value, sizeof(s_value));
	if(ret <= 0) {
		/* TODO: implement ser_comm_fail() for USB */
		upslogx(LOG_WARNING, "Error reading S value");
		dstate_datastale();
		return;
	}

	switch(s_value[2]) {
		case '0':
			status_set("OL");
			break;
		case '1':
			status_set("OB");
			break;
		case '3': /* I have seen this once when switching from off+LB to charging */
			upslogx(LOG_WARNING, "Unknown value for s[2]: 0x%02x", s_value[2]);
			break;
		default:
			upslogx(LOG_ERR, "Unknown value for s[2]: 0x%02x", s_value[2]);
			break;
	}

	switch(s_value[1]) {
		case '0':
			status_set("LB");
			break;
		case '1':
			break;
		default:
			upslogx(LOG_ERR, "Unknown value for s[1]: 0x%02x", s_value[1]);
	}

	ret = send_cmd(b_msg, sizeof(b_msg), b_value, sizeof(b_value));
	if(ret <= 0) {
		upslogx(LOG_WARNING, "Error reading B value");
		dstate_datastale();
		return;
	}

	dstate_setinfo("battery.voltage", "%.2f", hex2d(b_value+5, 2)/16.0);
	dstate_setinfo("input.frequency", "%.2f", hex2d(b_value+1, 4)/55.0);

	ret = send_cmd(l_msg, sizeof(l_msg), l_value, sizeof(l_value));
	if(ret <= 0) {
		upslogx(LOG_WARNING, "Error reading L value");
		dstate_datastale();
		return;
	}

	dstate_setinfo("output.voltage", "%.1f", hex2d(l_value+1, 4)/2.0);

	status_commit();

#if 0
	send_cmd(":B\r", buf, sizeof buf);
	bv = (float)hex2d(buf, 2) / 10.0;

	/* dq ~= sqrt(dV) is a reasonable approximation
	 * Results fit well against the discrete function used in the Tripp Lite
	 * source, but give a continuous result. */
	if (bv >= V_interval[1])
		bp = 100;
	else if (bv <= V_interval[0])
		bp = 10;
	else
		bp = (int)(100*sqrt((bv - V_interval[0])
							/ (V_interval[1] - V_interval[0])));

	dstate_setinfo("battery.voltage", "%.1f", bv);
	dstate_setinfo("battery.charge",  "%3d", bp);

	send_cmd(":M\r", buf, sizeof buf);
	dstate_setinfo("input.voltage.maximum", "%d", hex2d(buf, 2));

	send_cmd(":I\r", buf, sizeof buf);
	dstate_setinfo("input.voltage.minimum", "%d", hex2d(buf, 2));

	send_cmd(":C\r", buf, sizeof buf);
	switch (atoi(buf)) {
		case 0:
			dstate_setinfo("ups.test.result", "%s", "OK");
			break;
		case 1:
			dstate_setinfo("ups.test.result", "%s", "Battery Bad (Replace)");
			break;
		case 2:
			dstate_setinfo("ups.test.result", "%s", "In Progress");
			break;
		case 3:
			dstate_setinfo("ups.test.result", "%s", "Bad Inverter");
			break;
		default:
			dstate_setinfo("ups.test.result", "Unknown (%s)", buf);
			break;
	}

	/* ser_comm_good(); */
#endif
	dstate_dataok();
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
#if 0
	char msg[256];

	snprintf(msg, sizeof msg, "Set shutdown delay, in seconds (default=%d).",
		DEFAULT_OFFDELAY);
	addvar(VAR_VALUE, "offdelay", msg);
	snprintf(msg, sizeof msg, "Set start delay, in seconds (default=%d).",
		DEFAULT_STARTDELAY);
	addvar(VAR_VALUE, "startdelay", msg);
	snprintf(msg, sizeof msg, "Set reboot delay, in seconds (default=%d).",
		DEFAULT_BOOTDELAY);
	addvar(VAR_VALUE, "rebootdelay", msg);
#endif
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools - Tripp Lite Omni driver %s (%s)\n",
			DRV_VERSION, UPS_VERSION);
}

void upsdrv_initups(void)
{
        /* Search for the first supported UPS, no matter Mfr or exact product */
        if ((hd = HIDOpenDevice(device_path, &flg, MODE_OPEN)) == NULL)
                fatalx("No USB HID UPS found");
        else
                upslogx(1, "Detected an UPS: %s/%s\n", hd->Vendor, hd->Product);

	HIDDumpTree(NULL);

        if (hd->VendorID != USB_VID_TRIPP_LITE)
        {
		fatalx("This driver only supports Tripp Lite Omni UPSes. Try the newhidups driver instead.");
	}

#if 0
	if (getval("offdelay"))
		offdelay = atoi(getval("offdelay"));
	if (getval("startdelay"))
		startdelay = atoi(getval("startdelay"));
	if (getval("rebootdelay"))
		bootdelay = atoi(getval("rebootdelay"));
#endif
}

void upsdrv_cleanup(void)
{
        if (hd != NULL)
                HIDCloseDevice(hd);
}

