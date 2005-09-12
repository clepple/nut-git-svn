/*!@file tripplite_usb.c 
 * @brief Driver for Tripp Lite entry-level USB models.  (tested with: "OMNIVS1000")
 */
/*
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

#define DRV_VERSION "0.4"

/* % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % 
 *
 * OMNIVS Commands: (capital letters are literals, lower-case are variables)
 * :B     -> Bxxxxyy (xxxx/55.0: Hz in, yy/16: battery voltage)
 * :F     -> F1143_A (where _ = \0) Firmware version?
 * :L     -> LvvvvXX (vvvv/2.0: VAC out)
 * :P     -> P01000X (1000VA unit)
 * :S     -> Sbb_XXX (bb = 10: on-line, 11: on battery)
 * :V     -> V102XXX (firmware/protocol version?)
 * :Wt    -> Wt      (watchdog; t = time in seconds (binary, not hex), 
 *                   0 = disable; if UPS is not pinged in this interval, it
 *                   will power off the load, and then power it back on after
 *                   a delay.)
 *
 * % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % 
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
 * % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % 
 *
 * SMARTPRO commands:
 *
 * :A     -> ?          (start self-test)
 * :D     -> D7187      (? - doesn't match tripplite.c)
 * :F     -> F1019 A
 * :K0/1  ->            (untested, but a switchable bank exists)
 * :L     -> L290D_X
 * :M     -> M007F      (127 - max voltage?)
 * :P     -> P01500X    (max power)
 * :Q     ->            (while online: reboot)
 * :R     -> R<01><FF>
 * :S     -> S100_Z0    (status?)
 * :T     -> T7D2581    (temperature?)
 * :U     -> U<FF><FF>
 * :V     -> V1062XX
 * :Z     -> Z
 * 
 * % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % 
 *
 * The SMARTPRO unit seems to be slightly saner with regard to message
 * polling. It specifies an interrupt in interval of 100 ms, but I just
 * started at a 2 second timeout to obtain the above table. 
 *
 * % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % 
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

/*

POD ("Plain Old Documentation") - run through pod2html or perldoc. See
perlpod(1) for more information.

pod2man --name='TRIPPLITE_USB' --section=8 --release=' ' --center='Network UPS Tools (NUT)' tripplite_usb.c

=head1 NAME

tripplite_usb - Driver for Tripp Lite OMNISV1000 and OMNISV1500XL USB-based UPS
equipment

=head1 NOTE

This  man  page  only  documents  the hardware-specific features of the
tripplite driver.  For information about the core driver, see nutupsdrv(8).

=head1 SUPPORTED HARDWARE

This driver should work with the OMNISV series UPSes, which are detected as
USB HID-class devices. If your Tripp Lite UPS uses a serial port, you may wish
to investigate the tripplite(8) or tripplite_su(8) driver.

=head1 EXTRA ARGUMENTS

This driver supports the following optional setting in the ups.conf(5) file:

=over

=item offdelay

This setting controls the delay between receiving the "kill" command (C<-k>)
and actually cutting power to the computer.

=back

=head1 RUNTIME VARIABLES

=over

=item ups.delay.shutdown

This variable is the same as the C<offdelay> setting, but it can be changed at
runtime by upsrw(8).

=back

=head1 KNOWN ISSUES AND BUGS

The driver was not developed with any official documentation from Tripp Lite,
so certain events may confuse the driver. If you observe any strange behavior,
please re-run the driver with "-DDD" to increase the verbosity.

So far, the Tripp Lite UPSes do not seem to have any serial number or other
unique identifier accessible through USB. Therefore, you are limited to one
Tripp Lite USB UPS per system (and in practice, this driver will not play well
with other USB UPSes, either).

=head1 AUTHORS

Charles Lepple E<lt>clepple+nut@ghz.ccE<gt>, based on the tripplite driver by
Rickard E. (Rik) Faith E<lt>faith@alephnull.comE<gt> and Nicholas Kain
E<lt>nicholas@kain.usE<gt>.

=head1 SEE ALSO

=head2 The core driver:

nutupsdrv(8)

=head2 Internet resources:

The NUT (Network UPS Tools) home page: http://www.networkupstools.org/

=cut

*/

#include "main.h"
#include "libhid.h"
#include "hid-usb.h"
#include <math.h>
#include <ctype.h>

/*!@brief If a character is not printable, return a dot. */
#define toprint(x) (isprint(x) ? (x) : '.')

#define ENDCHAR 13

#define MAX_SEND_TRIES 3
#define SEND_WAIT_SEC 0
#define SEND_WAIT_NSEC (1000*1000*100)

#define MAX_RECV_TRIES 3
#define RECV_WAIT_MSEC 100

#define MAX_RECONNECT_TRIES 10
#define RECONNECT_DELAY 2	/*!< in seconds */

#define DEFAULT_OFFDELAY   64  /*!< seconds (max 0xFF) */
#define DEFAULT_STARTDELAY 60  /*!< seconds (max 0xFFFFFF) */
#define DEFAULT_BOOTDELAY  64  /*!< seconds (max 0xFF) */
#define MAX_VOLT 13.4          /*!< Max battery voltage (100%) */
#define MIN_VOLT 11.0          /*!< Min battery voltage (10%) */

#define USB_VID_TRIPP_LITE	0x09ae /*!< USB Vendor ID for Tripp Lite */
#define USB_PID_TRIPP_LITE	0x0001 /*!< Seems to be the same for all Tripp Lite USB UPSes */

static HIDDevice *hd = NULL;
static MatchFlags flg = { USB_VID_TRIPP_LITE, USB_PID_TRIPP_LITE, };

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

/*! Time in seconds to delay before shutting down. */
static unsigned int offdelay = DEFAULT_OFFDELAY;
/* static unsigned int bootdelay = DEFAULT_BOOTDELAY; */

/*!@brief Convert N characters from hex to decimal
 *
 * @param start		Beginning of string to convert
 * @param len		Maximum number of characters to consider (max 32)
 *
 * @a len characters of @a start are copied to a temporary buffer, then passed
 * to strtol() to be converted to decimal.
 *
 * @return See strtol(3)
 */
static int hex2d(char *start, unsigned int len)
{
	char buf[32];
	buf[32] = '\0';

	strncpy(buf, start, (len < (sizeof buf) ? len : (sizeof buf - 1)));
	if(len < sizeof(buf)) buf[len] = '\0';
	return strtol(buf, NULL, 16);
}

/*!@brief Dump message in both hex and ASCII
 *
 * @param[in] msg	Buffer to dump
 * @param[in] len	Number of bytes to dump
 *
 * @return		Pointer to static buffer with decoded message
 */
static const char *hexascdump(char *msg, size_t len)
{
	size_t i;
	static char buf[256], *bufp;

	bufp = buf;
	for(i=0; i<len; i++) {
		bufp += sprintf(bufp, "%02x ", msg[i]);
	}
	*bufp++ = '"';
	for(i=0; i<len; i++) {
		*bufp++ = toprint(msg[i]);
	}
	*bufp++ = '"';
	*bufp++ = '\0';

	return buf;
}

int find_tripplite_ups(void);
void upsdrv_initinfo(void);

/*!@brief Report a USB comm failure, and reconnect if necessary
 * 
 * @param[in] res	Result code from libusb/libhid call
 * @param[in] msg	Error message to display
 */
void usb_comm_fail(int res, const char *msg)
{
	int try = 0;

	switch(res) {
		case -ENODEV:
			upslogx(LOG_WARNING, "%s: Device detached?", msg);
			upsdrv_cleanup();

			do {
				sleep(RECONNECT_DELAY);
				upslogx(LOG_NOTICE, "Reconnect attempt #%d", ++try);
				find_tripplite_ups();
			} while (!hd && (try < MAX_RECONNECT_TRIES));

			if(hd) {
				upslogx(LOG_NOTICE, "Successfully reconnected");
				upsdrv_initinfo();
			} else {
				fatalx("Too many unsuccessful reconnection attempts");
			}
			break;

		case -EBUSY:
			upslogx(LOG_WARNING, "%s: Device claimed by another process", msg);
			fatalx("Terminating: EBUSY");
			upsdrv_cleanup();
			break;

		default:
			upslogx(LOG_NOTICE, "%s: Unknown error %d", msg, res);
			break;
	}
}

/*!@brief Send a command to the UPS, and wait for a reply.
 *
 * All of the UPS commands are challenge-response. If a command does not have
 * anything to return, it simply returns the command character.
 *
 * @param[in] msg	Command string, minus the ':' or CR
 * @param[in] msg_len	Be sure to use sizeof(msg) instead of strlen(msg),
 * since some commands have embedded NUL characters
 * @param[out] reply	Reply (but check return code for validity)
 * @param[out] reply_len (currently unused)
 *
 * @return number of chars in reply, excluding terminating NUL
 * @return 0 if command was not accepted
 */
static int send_cmd(const char *msg, size_t msg_len, char *reply, size_t reply_len)
{
	char buffer_out[8];
	unsigned char csum = 0;
	int ret = 0, send_try, recv_try=0, done = 0;
	size_t i = 0;

	upsdebugx(3, "send_cmd(msg_len=%d, type='%c')", msg_len, msg[0]);

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
	buffer_out[i+1] = ENDCHAR;

	upsdebugx(5, "send_cmd: sending  %s", hexascdump(buffer_out, sizeof(buffer_out)));

	for(send_try=0; !done && send_try < MAX_SEND_TRIES; send_try++) {
		upsdebugx(6, "send_cmd send_try %d", send_try+1);

		ret = libusb_set_report(0, buffer_out, sizeof(buffer_out));

		if(ret != sizeof(buffer_out)) {
			upslogx(1, "libusb_set_report() returned %d instead of %d", ret, sizeof(buffer_out));
			return ret;
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
		upsdebugx(5, "send_cmd: received %s (%s)", hexascdump(reply, sizeof(buffer_out)),
				done ? "OK" : "bad");
	}
	
	upsdebugx(((send_try > 2) || (recv_try > 2)) ? 3 : 6, 
			"send_cmd: send_try = %d, recv_try = %d\n", send_try, recv_try);

	return done ? sizeof(buffer_out) : 0;
}

#if 0
/* using the watchdog to reboot won't work while polling */
static void do_reboot_wait(unsigned dly)
{
	int ret;
	char buf[256], cmd_W[]="Wx"; 

	cmd_W[1] = dly;
	upsdebugx(3, "do_reboot_wait(wait=%d): N", dly);

	ret = send_cmd(cmd_W, sizeof(cmd_W), buf, sizeof(buf));
}

static int do_reboot_now(void)
{
	do_reboot_wait(1);
	return 0;
}

static void do_reboot(void)
{
	do_reboot_wait(bootdelay);
}
#endif

/*! Called by 'tripplite_usb -k' */
static int soft_shutdown(void)
{
	int ret;
	char buf[256], cmd_N[]="N\0x", cmd_G[] = "G";

	cmd_N[2] = offdelay;
	cmd_N[1] = offdelay >> 8;
	upsdebugx(3, "soft_shutdown(offdelay=%d): N", offdelay);

	ret = send_cmd(cmd_N, sizeof(cmd_N), buf, sizeof(buf));
	if(ret != 8) return ret;

	sleep(2);
	
	/*! The unit must be on battery for this to work. 
	 *
	 * @todo check for on-battery condition, and print error if not.
	 */
	ret = send_cmd(cmd_G, sizeof(cmd_G), buf, sizeof(buf));
	return (ret == 8);
}

#if 0
static int hard_shutdown(void)
{
	int ret;
	char buf[256], cmd_N[]="N\0x", cmd_K[] = "K\0";

	cmd_N[2] = offdelay;
	cmd_N[1] = offdelay >> 8;
	upsdebugx(3, "hard_shutdown(offdelay=%d): N", offdelay);

	ret = send_cmd(cmd_N, sizeof(cmd_N), buf, sizeof(buf));
	if(ret != 8) return ret;

	sleep(2);
	
	ret = send_cmd(cmd_K, sizeof(cmd_K), buf, sizeof(buf));
	return (ret == 8);
}
#endif

/*!@brief Handler for "instant commands"
 */
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
	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		hard_shutdown();
		return STAT_INSTCMD_HANDLED;
	}
#endif
	if (!strcasecmp(cmdname, "shutdown.return")) {
		soft_shutdown();
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
#if 0
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
	const char f_msg[] = "F", p_msg[] = "P",
		s_msg[] = "S", v_msg[] = "V", w_msg[] = "W\0";
	char *model, *model_end, 
	     f_value[9], p_value[9], s_value[9], v_value[9], w_value[9], buf[256];
	int  va, ret;

	/* Reset watchdog: */
	ret = send_cmd(w_msg, sizeof(w_msg), w_value, sizeof(w_value)-1);
	if(ret <= 0) {
		fatalx("Could not reset watchdog... is this an OMNIVS model?");
	}

	ret = send_cmd(s_msg, sizeof(s_msg), s_value, sizeof(s_value)-1);
	if(ret <= 0) {
		fatalx("Could not retrieve status ... is this an OMNIVS model?");
	}

	dstate_setinfo("ups.mfr", "%s", "Tripp Lite");

	ret = send_cmd(p_msg, sizeof(p_msg), p_value, sizeof(p_value)-1);
	va = strtol(p_value+1, NULL, 10);

	/* trim "TRIPP LITE" from beginning of model */
	model = strdup(hd->Product);
	if(strstr(model, hd->Vendor) == model) {
		model += strlen(hd->Vendor);
	}

	for(; *model == ' '; model++);

	/* Trim trailing spaces */
	for(model_end = model + strlen(model) - 1;
			model_end > model && *model_end == ' ';
			model_end--) {
		*model_end = '\0';
	}

	dstate_setinfo("ups.model", model);

	dstate_setinfo("ups.power.nominal", "%d", va);

	ret = send_cmd(f_msg, sizeof(f_msg), f_value, sizeof(f_value)-1);

	dstate_setinfo("ups.firmware", "F%c%c%c%c %c",
			toprint(f_value[1]), toprint(f_value[2]), toprint(f_value[3]),
			toprint(f_value[4]), toprint(f_value[6]));

	ret = send_cmd(v_msg, sizeof(v_msg), v_value, sizeof(v_value)-1);

	dstate_setinfo("ups.firmware.aux", "V%c%c%c",
			toprint(v_value[1]), toprint(v_value[2]), toprint(v_value[3]));

	snprintf(buf, sizeof buf, "%d", offdelay);
	dstate_setinfo("ups.delay.shutdown", buf);
	dstate_setflags("ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.shutdown", 3);

#if 0
	snprintf(buf, sizeof buf, "%d", startdelay);
	dstate_setinfo("ups.delay.start", buf);
	dstate_setflags("ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.start", 8);

	snprintf(buf, sizeof buf, "%d", bootdelay);
	dstate_setinfo("ups.delay.reboot", buf);
	dstate_setflags("ups.delay.reboot", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.reboot", 3);
#endif

	dstate_addcmd("shutdown.return");

#if 0
	dstate_addcmd("shutdown.stayoff");

	dstate_addcmd("test.battery.start"); /* Turns off automatically */

	dstate_addcmd("load.off");
	dstate_addcmd("load.on");

	dstate_addcmd("shutdown.reboot");
	dstate_addcmd("shutdown.reboot.graceful");
#endif

	upsh.instcmd = instcmd;
	upsh.setvar = setvar;

	printf("Attached to %s %s\n",
			dstate_getinfo("ups.mfr"), dstate_getinfo("ups.model"));

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
	double bv;

	int ret;

	status_init();

	/* General status (e.g. "S10") */
	ret = send_cmd(s_msg, sizeof(s_msg), s_value, sizeof(s_value));
	if(ret <= 0) {
		dstate_datastale();
		usb_comm_fail(ret, "Error reading S value");
		return;
	}

	switch(s_value[2]) {
		case '0':
			status_set("OL");
			break;
		case '1':
			status_set("OB");
			break;
		case '2': /* "charge-only" mode, no AC in or out... the PC
			     shouldn't see this, because there is no power in
			     that case */
		case '3': /* I have seen this once when switching from off+LB to charging */
			upslogx(LOG_WARNING, "Unknown value for s[2]: 0x%02x", s_value[2]);
			break;
		default:
			upslogx(LOG_ERR, "Unknown value for s[2]: 0x%02x", s_value[2]);
			dstate_datastale();
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
			dstate_datastale();
			break;
	}

	status_commit();

	ret = send_cmd(b_msg, sizeof(b_msg), b_value, sizeof(b_value));
	if(ret <= 0) {
		dstate_datastale();
		usb_comm_fail(ret, "Error reading B value");
		return;
	}

	dstate_setinfo("input.voltage", "%.2f", hex2d(b_value+1, 4)/30.0);

	ret = send_cmd(l_msg, sizeof(l_msg), l_value, sizeof(l_value));
	if(ret <= 0) {
		dstate_datastale();
		usb_comm_fail(ret, "Error reading L value");
		return;
	}

	dstate_setinfo("output.voltage", "%.1f", hex2d(l_value+1, 4)/2.0);

	bv = hex2d(b_value+5, 2)/16.0;

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

	dstate_setinfo("battery.voltage", "%.2f", (float)bv);
	dstate_setinfo("battery.charge",  "%3d", bp);

	dstate_dataok();
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
	char msg[256];

	snprintf(msg, sizeof msg, "Set shutdown delay, in seconds (default=%d).",
		DEFAULT_OFFDELAY);
	addvar(VAR_VALUE, "offdelay", msg);
#if 0
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
	printf("Network UPS Tools - Tripp Lite OMNIVS and SMARTPRO driver %s (%s)\n",
			DRV_VERSION, UPS_VERSION);

	experimental_driver = 1;
}

int find_tripplite_ups(void)
{
        /* Search for the first supported UPS, no matter Mfr or exact product */
        if ((hd = HIDOpenDevice(device_path, &flg, MODE_OPEN)) == NULL)
		return -1;
        else
                upslogx(1, "Detected an UPS: %s/%s\n", hd->Vendor, hd->Product);

	HIDDumpTree(NULL);

        if (hd->VendorID != USB_VID_TRIPP_LITE)
        {
		upslogx(LOG_ERR, "This driver only supports Tripp Lite Omni UPSes. Try the newhidups driver instead.");
		return -2;
	}
	return 0;
}

/*!@brief Find USB UPS, and open it
 *
 * @todo Allow binding based on firmware version (which seems to vary wildly
 * from unit to unit)
 */
void upsdrv_initups(void)
{
	if(find_tripplite_ups() < 0) {
                fatalx("No Tripp Lite USB HID UPS found");
	}

	if (getval("offdelay"))
		offdelay = atoi(getval("offdelay"));
#if 0
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
/* vim:se tw=78: */
