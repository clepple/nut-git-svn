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
#include "newhidups.h"
#include <math.h>
#include <ctype.h>

#define ENDCHAR '\r'           /* replies end with CR LF -- use LF to end */
#define IGNCHAR '\r'           /* ignore CR */
#define MAXTRIES 3
#define SER_WAIT_SEC  3        /* allow 3.0 sec for ser_get calls */
#define SER_WAIT_USEC 0
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

/* The UPS that I'm using (SMART700SER) has the bizarre characteristic
 * of innately echoing back commands.  Therefore, we cannot use
 * ser_get_line and must manually discard our echoed command.
 *
 * All UPS commands are challenge-response, so this function makes things
 * very clean.
 *
 * return: # of chars in buf, excluding terminating \0 */
static int send_cmd(const char *str, char *buf, size_t len)
{
	char c;
	int ret;
	size_t i = 0;

/* TODO: send USB control message */
#if 0
	ser_send(upsfd, str);

	if (!len || !buf)
		return -1;

	for (;;) {
		ret = ser_get_char(upsfd, &c, SER_WAIT_SEC, SER_WAIT_USEC);
		if (ret == -1)
			return -1;
		if (c == ENDCHAR)
			break;
	}
	do {
		ret = ser_get_char(upsfd, &c, SER_WAIT_SEC, SER_WAIT_USEC);
		if (ret == -1)
			return -1;

		if (c == IGNCHAR || c == ENDCHAR)
			continue;
		buf[i++] = c;
	} while (c != ENDCHAR && i < len);
	buf[i] = '\0';
	ser_flush_in(upsfd, NULL, 0);
#endif
	return i;
}

static void ups_sync(void)
{
	char buf[256];
	int tries, ret;

	for (tries = 0; tries < MAXTRIES; ++tries) {
		ret = send_cmd(":W\r", buf, sizeof buf);
		if ((ret > 0) && isdigit((unsigned char)buf[0]))
			return;
	}
	fatalx("\nFailed to find UPS - giving up...");
}

static int do_reboot_now(void)
{
	char buf[256], cmd[16];

	snprintf(cmd, sizeof cmd, ":H%06X\r", startdelay);
	return send_cmd(cmd, buf, sizeof buf);
}

static void do_reboot(void)
{
	char buf[256], cmd[16];

	snprintf(cmd, sizeof cmd, ":N%02X\r", bootdelay);
	send_cmd(cmd, buf, sizeof buf);
	do_reboot_now();
}

static int soft_shutdown(void)
{
	char buf[256], cmd[16];

	snprintf(cmd, sizeof cmd, ":N%02X\r", offdelay);
	send_cmd(cmd, buf, sizeof buf);
	return send_cmd(":G\r", buf, sizeof buf);
}

static int hard_shutdown(void)
{
	char buf[256], cmd[16];

	snprintf(cmd, sizeof cmd, ":N%02X\r", offdelay);
	send_cmd(cmd, buf, sizeof buf);
	return send_cmd(":K0\r", buf, sizeof buf);
}

static int instcmd(const char *cmdname, const char *extra)
{
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

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

static int setvar(const char *varname, const char *val)
{
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
	return STAT_SET_UNKNOWN;
}

void upsdrv_initinfo(void)
{
	const char *model;
	char buf[16], w_value[16], l_value[16], v_value[16], x_value[16];
	int  va;
	long w, l;


	/* Detect the UPS or die. */
	ups_sync();

	send_cmd(":W\r", w_value, sizeof w_value);
	send_cmd(":L\r", l_value, sizeof l_value);
	send_cmd(":V\r", v_value, sizeof v_value);
	send_cmd(":X\r", x_value, sizeof x_value);

	dstate_setinfo("ups.mfr", "%s", "Tripp Lite");

	w = hex2d(w_value, 2);
	l = hex2d(l_value, 2);

	model = "Smart %d";
	if (w & 0x40)
		model = "Unison %d";

	va = ((w & 0x3f) * 32 + (l >> 3)) * 5;  /* New formula */
	if (!(w & 0x80))
		va = l / 2;   /* Old formula */

	dstate_setinfo("ups.model", model, va);
	dstate_setinfo("ups.firmware", "%c%c",
			'A'+v_value[0]-'0', 'A'+v_value[1]-'0');

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
	char buf[256];
	int bp;
	float bv;

	send_cmd(":D\r", buf, sizeof buf);

#if 0
	if (strlen(buf) < 21) {
		ser_comm_fail("Failed to get data: short read from UPS");
		dstate_datastale();
		return;
	}

	if (strlen(buf) > 21) {
		ser_comm_fail("Failed to get data: oversized read from UPS");
		dstate_datastale();
		return;
	}
#endif

	dstate_setinfo("input.voltage", "%0d", hex2d(buf + 2, 2));
	dstate_setinfo("ups.temperature", "%3d",
			(int)(hex2d(buf + 6, 2)*0.3636 - 21.0));
	dstate_setinfo("ups.load", "%3d", hex2d(buf + 12, 2));
	dstate_setinfo("input.frequency", "%02.2f", hex2d(buf + 18, 3) / 10.0);

	status_init();

	/* Battery Voltage Condition */
	switch (buf[0]) {
		case '0': /* Low Battery */
			status_set("LB");
			break;
		case '1': /* Normal */
			break;
		default: /* Unknown */
			upslogx(LOG_ERR, "Unknown battery state: %c", buf[0]);
			break;
	}

	/* Load State */
	switch (buf[1]) {
		case '0': /* Overload */
			status_set("OVER");
			break;
		case '1': /* Normal */
			break;
		default: /* Unknown */
			upslogx(LOG_ERR, "Unknown load state: %c", buf[1]);
			break;
	}

	/* Tap State */
	switch (buf[4]) {
		case '0': /* Normal */
			break;
		case '1': /* Reducing */
			status_set("TRIM");
			break;
		case '2': /* Boost */
		case '3': /* Extra Boost */
			status_set("BOOST");
			break;
		default: /* Unknown */
			upslogx(LOG_ERR, "Unknown tap state: %c", buf[4]);
			break;
	}

	/* Mode */
	switch (buf[5]) {
		case '0': /* Off */
			status_set("OFF");
			break;
		case '1': /* On Battery */
			status_set("OB");
			break;
		case '2': /* On Line */
			status_set("OL");
			break;
		case '3': /* On Battery */
			status_set("OB");
			break;
		default: /* Unknown */
			upslogx(LOG_ERR, "Unknown mode state: %c", buf[4]);
			break;
	}

	status_commit();
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

	dstate_dataok();
	/* ser_comm_good(); */
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
                fatalx("No USB/HID UPS found");
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

