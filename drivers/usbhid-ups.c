/* usbhid-ups.c - Driver for USB and serial (MGE SHUT) HID UPS units
 * 
 * Copyright (C)
 *   2003-2005 Arnaud Quette <http://arnaud.quette.free.fr/contact.html>
 *   2005      John Stamp <kinsayder@hotmail.com>
 *   2005-2006 Peter Selinger <selinger@users.sourceforge.net>
 *   2007      Arjen de Korte <adkorte-guest@alioth.debian.org>
 *
 * This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com
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

#include <ltdl.h>

#include "main.h"
#include "libhid.h"
#include "usbhid-ups.h"
#include "hidparser.h"
#include "hidtypes.h"

#ifdef HAVE_SSL
#include <openssl/sha.h>
#endif

#ifdef	SHUT_MODE
#include "mge-hid.h"
/* there is currently only one SHUT subdriver */
static subdriver_t	*subdriver = &mge_hid_LTX_subdriver;
#else
/* master list of avaiable subdrivers */
static char *subdriver_list[] = {
	"explore-hid",
	"apc-hid",
	"belkin-hid",
	"cps-hid",
	"liebert-hid",
	"mge-hid",
	"tripplite-hid",
	NULL
};

static subdriver_t	*subdriver = NULL;
#endif

/* Data walk modes */
typedef enum {
	HU_WALKMODE_INIT = 0,
	HU_WALKMODE_QUICK_UPDATE,
	HU_WALKMODE_FULL_UPDATE,
	HU_WALKMODE_RELOAD
} walkmode_t;


/* Global vars */
static HIDDevice_t *hd = NULL;
static HIDDevice_t curDevice = { 0x0000, 0x0000, NULL, NULL, NULL, NULL };
static HIDDeviceMatcher_t *subdriver_matcher = NULL;
#ifndef	SHUT_MODE
static HIDDeviceMatcher_t *exact_matcher = NULL;
static HIDDeviceMatcher_t *regex_matcher = NULL;
static unsigned char *checksum = NULL;
#endif
static int pollfreq = DEFAULT_POLLFREQ;
static int ups_status = 0;
static bool_t data_has_changed = FALSE; /* for SEMI_STATIC data polling */
static time_t lastpoll; /* Timestamp the last polling */
hid_dev_handle_t udev;

/* support functions */
static hid_info_t *find_nut_info(const char *varname);
static hid_info_t *find_hid_info(const HIDData_t *hiddata);
static char *hu_find_infoval(info_lkp_t *hid2info, const double value);
static long hu_find_valinfo(info_lkp_t *hid2info, const char* value);
static void process_boolean_info(char *nutvalue);
static void ups_alarm_set(void);
static void ups_status_set(void);
static bool_t hid_ups_walk(walkmode_t mode);
static int reconnect_ups(void);
static int ups_infoval_set(hid_info_t *item, double value);
static int callback(hid_dev_handle_t udev, HIDDevice_t *hd, unsigned char *rdbuf, int rdlen);
#ifdef DEBUG
static double interval(void);
#endif

/* global variables */
HIDDesc_t	*pDesc = NULL;		/* parsed Report Descriptor */
reportbuf_t	*reportbuf = NULL;	/* buffer for most recent reports */
#ifndef	SHUT_MODE
matching_t	matching = NORMAL;
#endif

/* ---------------------------------------------------------------------- */
/* data for processing boolean values from UPS */

#define	STATUS(x)	((unsigned)1<<x)

typedef enum {
	ONLINE = 0,	/* on line */
	DISCHRG,	/* discharging */
	CHRG,		/* charging */
	LOWBATT,	/* low battery */
	OVERLOAD,	/* overload */
	REPLACEBATT,	/* replace battery */
	SHUTDOWNIMM,	/* shutdown imminent */
	TRIM,		/* SmartTrim */
	BOOST,		/* SmartBoost */
	BYPASS,		/* on bypass */
	OFF,		/* ups is off */
	CAL,		/* calibration */
	OVERHEAT,	/* overheat; Belkin, TrippLite */
	COMMFAULT,	/* UPS fault; Belkin, TrippLite */
	DEPLETED,	/* battery depleted; Belkin */
	TIMELIMITEXP,	/* time limit expired; APC */
	FULLYCHARGED,	/* battery full; CyberPower */
	AWAITINGPOWER,	/* awaiting power; Belkin, TrippLite */
	FANFAIL,	/* fan failure; MGE */
	NOBATTERY,	/* battery missing; MGE */
	BATTVOLTLO,	/* battery voltage too low; MGE */
	BATTVOLTHI,	/* battery voltage too high; MGE */
	CHARGERFAIL,	/* battery charger failure; MGE */
	VRANGE,		/* voltage out of range */
	FRANGE		/* frequency out of range */
} status_bit_t;


/* --------------------------------------------------------------- */
/* Struct & data for boolean processing                            */
/* --------------------------------------------------------------- */

/* Note: this structure holds internal status info, directly as
   collected from the hardware; not yet converted to official NUT
   status or alarms */
typedef struct {
	char	*status_str;	/* ups status string */
	int	status_mask;	/* ups status mask */
} status_lkp_t;

static status_lkp_t status_info[] = {
	/* map internal status strings to bit masks */
	{ "online", STATUS(ONLINE) },
	{ "dischrg", STATUS(DISCHRG) },
	{ "chrg", STATUS(CHRG) },
	{ "lowbatt", STATUS(LOWBATT) },
	{ "overload", STATUS(OVERLOAD) },
	{ "replacebatt", STATUS(REPLACEBATT) },
	{ "shutdownimm", STATUS(SHUTDOWNIMM) },
	{ "trim", STATUS(TRIM) },
	{ "boost", STATUS(BOOST) },
	{ "bypass", STATUS(BYPASS) },
	{ "off", STATUS(OFF) },
	{ "cal", STATUS(CAL) },
	{ "overheat", STATUS(OVERHEAT) },
	{ "commfault", STATUS(COMMFAULT) },
	{ "depleted", STATUS(DEPLETED) },
	{ "timelimitexp", STATUS(TIMELIMITEXP) },
	{ "fullycharged", STATUS(FULLYCHARGED) },
	{ "awaitingpower", STATUS(AWAITINGPOWER) },
	{ "fanfail", STATUS(FANFAIL) },
	{ "nobattery", STATUS(NOBATTERY) },
	{ "battvoltlo", STATUS(BATTVOLTLO) },
	{ "battvolthi", STATUS(BATTVOLTHI) },
	{ "chargerfail", STATUS(CHARGERFAIL) },
	{ "vrange", STATUS(VRANGE) },
	{ "frange", STATUS(FRANGE) },
	{ NULL, 0 },
};

#ifndef	SHUT_MODE
typedef struct {
	char		*val;
	matching_t	mode;
} matching_lkp_t;

static matching_lkp_t matching_info[] = {
	{ "normal", NORMAL },
	{ "checksum", CHECKSUM },
	{ "delayed", DELAYED },
	{ NULL, NORMAL },
};
#endif

/* ---------------------------------------------------------------------- */
/* value lookup tables and generic lookup functions */

/* Actual value lookup tables => should be fine for all Mfrs (TODO: validate it!) */

/* the purpose of the following status conversions is to collect
   information, not to interpret it. The function
   process_boolean_info() remembers these values by updating the global
   variable ups_status. Interpretation happens in ups_status_set,
   where they are converted to standard NUT status strings. Notice
   that the below conversions do not yield standard NUT status
   strings; this in indicated being in lower-case characters. 

   The reason to separate the collection of information from its
   interpretation is that not each report received from the UPS may
   contain all the status flags, so they must be stored
   somewhere. Also, there can be more than one status flag triggering
   a certain condition (e.g. a certain UPS might have variables
   low_battery, shutdown_imminent, timelimit_exceeded, and each of
   these would trigger the NUT status LB. But we have to ensure that
   these variables don't unset each other, so they are remembered
   separately)  */

info_lkp_t online_info[] = {
	{ 1, "online", NULL },
	{ 0, "!online", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t discharging_info[] = {
	{ 1, "dischrg", NULL },
	{ 0, "!dischrg", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t charging_info[] = {
	{ 1, "chrg", NULL },
	{ 0, "!chrg", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t lowbatt_info[] = {
	{ 1, "lowbatt", NULL },
	{ 0, "!lowbatt", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t overload_info[] = {
	{ 1, "overload", NULL },
	{ 0, "!overload", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t replacebatt_info[] = {
	{ 1, "replacebatt", NULL },
	{ 0, "!replacebatt", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t trim_info[] = {
	{ 1, "trim", NULL },
	{ 0, "!trim", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t boost_info[] = {
	{ 1, "boost", NULL },
	{ 0, "!boost", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t bypass_info[] = {
	{ 1, "bypass", NULL },
	{ 0, "!bypass", NULL },
	{ 0, NULL, NULL }
};
/* note: this value is reverted (0=set, 1=not set). We report "being
   off" rather than "being on", so that devices that don't implement
   this variable are "on" by default */
info_lkp_t off_info[] = {
	{ 0, "off", NULL },
	{ 1, "!off", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t calibration_info[] = {
	{ 1, "cal", NULL },
	{ 0, "!cal", NULL },
	{ 0, NULL, NULL }
};
/* note: this value is reverted (0=set, 1=not set). We report "battery
   not installed" rather than "battery installed", so that devices
   that don't implement this variable have a battery by default */
info_lkp_t nobattery_info[] = {
	{ 1, "!nobattery", NULL },
	{ 0, "nobattery", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t fanfail_info[] = {
	{ 1, "fanfail", NULL },
	{ 0, "!fanfail", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t shutdownimm_info[] = {
	{ 1, "shutdownimm", NULL },
	{ 0, "!shutdownimm", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t overheat_info[] = {
	{ 1, "overheat", NULL },
	{ 0, "!overheat", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t awaitingpower_info[] = {
	{ 1, "awaitingpower", NULL },
	{ 0, "!awaitingpower", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t commfault_info[] = {
	{ 1, "commfault", NULL },
	{ 0, "!commfault", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t timelimitexpired_info[] = {
	{ 1, "timelimitexp", NULL },
	{ 0, "!timelimitexp", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t battvoltlo_info[] = {
	{ 1, "battvoltlo", NULL },
	{ 0, "!battvoltlo", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t battvolthi_info[] = {
	{ 1, "battvolthi", NULL },
	{ 0, "!battvolthi", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t chargerfail_info[] = {
	{ 1, "chargerfail", NULL },
	{ 0, "!chargerfail", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t fullycharged_info[] = { /* used by CyberPower and TrippLite */
	{ 1, "fullycharged", NULL },
	{ 0, "!fullycharged", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t depleted_info[] = {
	{ 1, "depleted", NULL },
	{ 0, "!depleted", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t vrange_info[] = {
	{ 0, "!vrange", NULL },
	{ 1, "vrange", NULL },
	{ 0, NULL, NULL }
};
info_lkp_t frange_info[] = {
	{ 0, "!frange", NULL },
	{ 1, "frange", NULL },
	{ 0, NULL, NULL }
};

info_lkp_t test_write_info[] = {
	{ 0, "No test", NULL },
	{ 1, "Quick test", NULL },
	{ 2, "Deep test", NULL },
	{ 3, "Abort test", NULL },
	{ 0, NULL, NULL }
};

info_lkp_t test_read_info[] = {
	{ 1, "Done and passed", NULL },
	{ 2, "Done and warning", NULL },
	{ 3, "Done and error", NULL },
	{ 4, "Aborted", NULL },
	{ 5, "In progress", NULL },
	{ 6, "No test initiated", NULL },
	{ 0, NULL, NULL }
};

info_lkp_t beeper_info[] = {
	{ 1, "disabled", NULL },
	{ 2, "enabled", NULL },
	{ 3, "muted", NULL },
	{ 0, NULL, NULL }
};

info_lkp_t yes_no_info[] = {
	{ 0, "no", NULL },
	{ 1, "yes", NULL },
	{ 0, NULL, NULL }
};

info_lkp_t on_off_info[] = {
	{ 0, "off", NULL },
	{ 1, "on", NULL },
	{ 0, NULL, NULL }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *date_conversion_fun(long value) {
	static char buf[20];
	int year, month, day;

	if (value == 0) {
		return "not set";
	}

	year = 1980 + (value >> 9); /* negative value represents pre-1980 date */ 
	month = (value >> 5) & 0x0f;
	day = value & 0x1f;

	snprintf(buf, sizeof(buf), "%04d/%02d/%02d", year, month, day);
	return buf;
}

info_lkp_t date_conversion[] = {
	{ 0, NULL, date_conversion_fun }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *hex_conversion_fun(long value) {
	static char buf[20];
	
	snprintf(buf, sizeof(buf), "%08lx", value);
	return buf;
}

info_lkp_t hex_conversion[] = {
	{ 0, NULL, hex_conversion_fun }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *stringid_conversion_fun(long value) {
	static char buf[20];

	return HIDGetIndexString(udev, value, buf, sizeof(buf));
}

info_lkp_t stringid_conversion[] = {
	{ 0, NULL, stringid_conversion_fun }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *divide_by_10_conversion_fun(long value) {
	static char buf[20];
	
	snprintf(buf, sizeof(buf), "%0.1f", value * 0.1);
	return buf;
}

info_lkp_t divide_by_10_conversion[] = {
	{ 0, NULL, divide_by_10_conversion_fun }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *kelvin_celsius_conversion_fun(long value) {
	static char buf[20];
	
	/* we should be working with doubles, not integers, but integers it
	   is for now */
	snprintf(buf, sizeof(buf), "%d", (int)(value - 273.15));
	return buf;
}

info_lkp_t kelvin_celsius_conversion[] = {
	{ 0, NULL, kelvin_celsius_conversion_fun }
};

/*!
 * subdriver matcher: only useful for USB mode
 * as SHUT is only supported by MGE UPS SYSTEMS units
 */
#ifndef SHUT_MODE
static int match_function_subdriver(HIDDevice_t *d, void *privdata)
{
	int	i;

	for (i=0; subdriver_list[i] != NULL; i++) {

		static lt_dlhandle	handle = NULL;

		if (handle) {
			lt_dlclose(handle);
		}

		handle = lt_dlopenext(subdriver_list[i]);
		if (!handle) {
			upsdebugx(2, "%s", lt_dlerror());
			continue;
		}

		subdriver = (subdriver_t *)lt_dlsym(handle, "subdriver");
		if (!subdriver) {
			upsdebugx(2, "%s", lt_dlerror());
			continue;
		}

		if (subdriver->claim(d)) {
			upsdebugx(4, "subdriver match: %s", subdriver_list[i]);
			return 1;
		}
	}

	upsdebugx(4, "no subdriver match");
	return 0;
}

static HIDDeviceMatcher_t subdriver_matcher_struct = {
	match_function_subdriver,
	NULL,
	NULL
};
#endif

/* ---------------------------------------------
 * driver functions implementations
 * --------------------------------------------- */
/* process instant command and take action. */
int instcmd(const char *cmdname, const char *extradata)
{
	hid_info_t	*hidups_item;
	const char	*val;
	double		value;
	
	if (!strcasecmp(cmdname, "beeper.off")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.off' command has been renamed to 'beeper.disable'");
		return instcmd("beeper.disable", NULL);
	}

	if (!strcasecmp(cmdname, "beeper.on")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.on' command has been renamed to 'beeper.enable'");
		return instcmd("beeper.enable", NULL);
	}

	upsdebugx(1, "instcmd(%s, %s)", cmdname, extradata ? extradata : "[NULL]");

	/* Retrieve and check netvar & item_path */	
	hidups_item = find_nut_info(cmdname);

	/* Check validity of the found the item */
	if (hidups_item == NULL) {
		upsdebugx(2, "instcmd: info element unavailable %s\n", cmdname);
		/* TODO: manage items handled "manually" */
		return STAT_INSTCMD_UNKNOWN;
	}

	/* Check if the item is an instant command */
	if (!hidups_item->hidflags & HU_TYPE_CMD) {
		upsdebugx(2, "instcmd: %s is not an instant command\n", cmdname);
		return STAT_INSTCMD_UNKNOWN;
	}
	
	/* If extradata is empty, use the default value from the HID-to-NUT table */
	val = extradata ? extradata : hidups_item->dfl;

	/* Lookup the new value if needed */
	if (hidups_item->hid2info != NULL) {
		value = hu_find_valinfo(hidups_item->hid2info, val);
	} else {
		value = atol(val);
	}

	/* Actual variable setting */
	if (HIDSetDataValue(udev, hidups_item->hiddata, value) == 1) {
		upsdebugx(5, "instcmd: SUCCEED\n");
		/* Set the status so that SEMI_STATIC vars are polled */
		data_has_changed = TRUE;
		return STAT_INSTCMD_HANDLED;
	}
	
	upsdebugx(3, "instcmd: FAILED\n"); /* TODO: HANDLED but FAILED, not UNKNOWN! */
	return STAT_INSTCMD_UNKNOWN;
}

/* set r/w variable to a value. */
int setvar(const char *varname, const char *val)
{
	hid_info_t	*hidups_item;
	double		value;

	upsdebugx(1, "setvar(%s, %s)", varname, val);
	
	/* retrieve and check netvar & item_path */	
	hidups_item = find_nut_info(varname);
	
	if (hidups_item == NULL) {
		upsdebugx(2, "setvar: info element unavailable %s\n", varname);
		return STAT_SET_UNKNOWN;
	}

	/* Checking item writability and HID Path */
	if (!hidups_item->info_flags & ST_FLAG_RW) {
		upsdebugx(2, "setvar: not writable %s\n", varname);
		return STAT_SET_UNKNOWN;
	}

	/* handle server side variable */
	if (hidups_item->hidflags & HU_FLAG_ABSENT) {
		upsdebugx(2, "setvar: setting server side variable %s\n", varname);
		dstate_setinfo(hidups_item->info_type, "%s", val);
		return STAT_SET_HANDLED;
	}

	/* SHUT_FLAG_ABSENT is the only case of HID Path == NULL */
	if (hidups_item->hidpath == NULL) {
		upsdebugx(2, "setvar: ID Path is NULL for %s\n", varname);
		return STAT_SET_UNKNOWN;
	}

	/* Lookup the new value if needed */
	if (hidups_item->hid2info != NULL) {
		value = hu_find_valinfo(hidups_item->hid2info, val);
	} else {
		value = atol(val);
	}

	/* Actual variable setting */
	if (HIDSetDataValue(udev, hidups_item->hiddata, value) == 1) {
		upsdebugx(5, "setvar: SUCCEED\n");
		/* Set the status so that SEMI_STATIC vars are polled */
		data_has_changed = TRUE;
		return STAT_SET_HANDLED;
	}

	upsdebugx(3, "setvar: FAILED\n"); /* FIXME: HANDLED but FAILED, not UNKNOWN! */
	return STAT_SET_UNKNOWN;
}

void upsdrv_shutdown(void)
{
	char	ondelay[8], offdelay[8];
	char	*val;

	upsdebugx(1, "upsdrv_shutdown...");
	
	/* Retrieve user defined delay settings */
	val = getval(HU_VAR_ONDELAY);
	snprintf(ondelay, sizeof(ondelay), "%i", val ? atoi(val) : DEFAULT_ONDELAY);

	val = getval(HU_VAR_OFFDELAY);
	snprintf(offdelay, sizeof(offdelay), "%i", val ? atoi(val) : DEFAULT_OFFDELAY);

	/* Try to shutdown with delay */
	if (instcmd("shutdown.return", ondelay) != STAT_INSTCMD_HANDLED) {
		upsdebugx(2, "Shutdown failed (setting ondelay)");
	} else if (instcmd("shutdown.stayoff", offdelay) != STAT_INSTCMD_HANDLED) {
		upsdebugx(2, "Shutdown failed (setting offdelay)");
	} else {
		/* Shutdown successful */
		return;
	}

	/* If the above doesn't work, try shutdown.reboot */
	if (instcmd("shutdown.reboot", ondelay) == STAT_INSTCMD_HANDLED) {
		/* Shutdown successful */
		return;
	}

	fatalx(EXIT_FAILURE, "Shutdown failed!");
}

void upsdrv_help(void)
{
  /* FIXME: to be completed */
}

void upsdrv_makevartable(void) 
{
	char temp [MAX_STRING_SIZE];
	
	upsdebugx(1, "upsdrv_makevartable...");

	snprintf(temp, sizeof(temp), "Set shutdown delay, in seconds (default=%d)",
		DEFAULT_OFFDELAY);
	addvar(VAR_VALUE, HU_VAR_OFFDELAY, temp);
	
	snprintf(temp, sizeof(temp), "Set startup delay, in ten seconds units for MGE (default=%d)",
		DEFAULT_ONDELAY);
	addvar(VAR_VALUE, HU_VAR_ONDELAY, temp);
	
	snprintf(temp, sizeof(temp), "Set polling frequency, in seconds, to reduce data flow (default=%d).",
		DEFAULT_POLLFREQ);
	addvar(VAR_VALUE, HU_VAR_POLLFREQ, temp);
#ifndef SHUT_MODE
	/* allow -x vendor=X, vendorid=X, product=X, productid=X, serial=X */
	addvar(VAR_VALUE, "vendor", "Regular expression to match UPS Manufacturer string");
	addvar(VAR_VALUE, "product", "Regular expression to match UPS Product string");
	addvar(VAR_VALUE, "serial", "Regular expression to match UPS Serial number");
	addvar(VAR_VALUE, "vendorid", "Regular expression to match UPS Manufacturer numerical ID (4 digits hexadecimal)");
	addvar(VAR_VALUE, "productid", "Regular expression to match UPS Product numerical ID (4 digits hexadecimal)");
	addvar(VAR_VALUE, "bus", "Regular expression to match USB bus name");
	addvar(VAR_VALUE, "matching", "Use 'normal' or 'delayed' matching");
	addvar(VAR_FLAG, "explore", "Diagnostic matching of unsupported UPS");
#endif
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools: %s %s - core %s (%s)\n\n",
		comm_driver->name, comm_driver->version,
		DRIVER_VERSION, UPS_VERSION);
}

#define	MAX_EVENT_NUM	16

void upsdrv_updateinfo(void) 
{
	hid_info_t	*item;
	HIDData_t	*event[MAX_EVENT_NUM];
	int		i, evtCount;
	double		value;
	time_t		now;

	upsdebugx(1, "upsdrv_updateinfo...");

	time(&now);

	/* check for device availability to set datastale! */
	if (hd == NULL) {
		/* don't flood reconnection attempts */
		if (now < (int)(lastpoll + poll_interval)) {
			return;
		}

		upsdebugx(1, "Got to reconnect!\n");

		if (!reconnect_ups()) {
			lastpoll = now;
			dstate_datastale();
			return;
		}

		hd = &curDevice;

		if (hid_ups_walk(HU_WALKMODE_INIT) == FALSE) {
			hd = NULL;
			return;
		}
	}
#ifdef DEBUG
	interval();
#endif
	/* Get HID notifications on Interrupt pipe first */
	/* TODO: cap number of times we check for events? */
	while ((evtCount = HIDGetEvents(udev, event, MAX_EVENT_NUM)) > 0) {

		upsdebugx(1, "Got %i HID objects...", evtCount);
			
		/* Process pending events (HID notifications on Interrupt pipe) */
		for (i = 0; i < evtCount; i++) {

			if (HIDGetDataValue(udev, event[i], &value, poll_interval) != 1)
				continue;

			if (nut_debug_level >= 2) {
				upsdebugx(2, "Path: %s, Type: %s, ReportID: 0x%02x, Offset: %i, Size: %i, Value: %f",
					HIDGetDataItem(udev, event[i], subdriver->utab),
					HIDDataType(event[i]), event[i]->ReportID,
					event[i]->Offset, event[i]->Size, value);
			}

			/* Skip Input reports, if we don't use the Feature report */
			item = find_hid_info(FindObject_with_Path(pDesc, &(event[i]->Path), ITEM_FEATURE));
			if (!item) {
				upsdebugx(1, "NUT doesn't use this HID object");
				continue;
			}

			ups_infoval_set(item, value);
		}
	}
#ifdef DEBUG
	upsdebugx(1, "took %.3f seconds handling interrupt reports...\n", interval());
#endif
	/* clear status buffer before begining */
	status_init();

	/* Do a full update (polling) every pollfreq or upon data change (ie setvar/instcmd) */
	if ((now > (lastpoll + pollfreq)) || (data_has_changed == TRUE)) {
		upsdebugx(1, "Full update...");

		alarm_init();

		if (hid_ups_walk(HU_WALKMODE_FULL_UPDATE) == FALSE)
			return;

		lastpoll = now;
		data_has_changed = FALSE;

		ups_alarm_set();
		alarm_commit();
	} else {
		upsdebugx(1, "Quick update...");

		/* Quick poll data only to see if the UPS is still connected */
		if (hid_ups_walk(HU_WALKMODE_QUICK_UPDATE) == FALSE)
			return;
	}

	ups_status_set();
	status_commit();

	dstate_dataok();
#ifdef DEBUG
	upsdebugx(1, "took %.3f seconds handling feature reports...\n", interval());
#endif
}

void upsdrv_initinfo(void)
{
	char	ondelay[8], offdelay[8];
	char	*val;

	upsdebugx(1, "upsdrv_initinfo...");

	/* Retrieve user defined delay settings */
	val = getval(HU_VAR_ONDELAY);
	snprintf(ondelay, sizeof(ondelay), "%i", val ? atoi(val) : DEFAULT_ONDELAY);

	val = getval(HU_VAR_OFFDELAY);
	snprintf(offdelay, sizeof(offdelay), "%i", val ? atoi(val) : DEFAULT_OFFDELAY);

	/* Abort if ondelay is too small */
	if (atoi(ondelay) <= atoi(offdelay)) {
		fatalx(EXIT_FAILURE, "%s (%s) must be greater than %s (%s)",
			HU_VAR_ONDELAY, ondelay, HU_VAR_OFFDELAY, offdelay);
	}

	/* init polling frequency */
	val = getval(HU_VAR_POLLFREQ);
	if (val)
		pollfreq = atoi(val);

	dstate_setinfo("driver.parameter.pollfreq", "%d", pollfreq);

	/* install handlers */
	upsh.setvar = setvar;
	upsh.instcmd = instcmd;

	time(&lastpoll);
}

void upsdrv_initups(void)
{
	int ret;
#ifdef SHUT_MODE
	/*!
	 * SHUT is a serial protocol, so it needs
	 * the device path
	 */
	upsdebugx(1, "upsdrv_initups...");

	subdriver_matcher = device_path;
#else
	char *val;
	char *regex_array[6];
	matching_lkp_t	*key;

	upsdebugx(1, "upsdrv_initups...");

	/* initialize dynamic loader */
	lt_dlinit();
	lt_dlsetsearchpath(DRVPATH);

	subdriver_matcher = &subdriver_matcher_struct;

	/* enforce use of the "vendorid" option if "explore" is given */
	if (testvar("explore") && getval("vendorid")==NULL) {
		fatalx(EXIT_FAILURE, "must specify \"vendorid\" when using \"explore\"");
	}

        /* process the UPS selection options */
	regex_array[0] = getval("vendorid");
	regex_array[1] = getval("productid");
	regex_array[2] = getval("vendor");
	regex_array[3] = getval("product");
	regex_array[4] = getval("serial");
	regex_array[5] = getval("bus");

	ret = USBNewRegexMatcher(&regex_matcher, regex_array, REG_ICASE | REG_EXTENDED);
	switch(ret)
	{
	case 0:
		break;
	case -1:
		fatal_with_errno(EXIT_FAILURE, "HIDNewRegexMatcher()");
	default:
		fatalx(EXIT_FAILURE, "invalid regular expression: %s", regex_array[ret]);
	}

	val = getval("matching");

	for (key = matching_info; val && key->val; key++) {
		if (!strcasecmp(val, key->val)) {
			matching = key->mode;
			dstate_setinfo("driver.parameter.matching", "%s", key->val);
			break;
		}
	}

	if (val && !key->val) {
		fatalx(EXIT_FAILURE, "Error: '%s' not a valid value for 'matching'", val);
	}

	if (matching != DELAYED) {
		/* link the matchers */
		subdriver_matcher->next = regex_matcher;
	}
#endif /* SHUT_MODE */

	/* Search for the first supported UPS matching the
	   regular expression (USB) or device_path (SHUT) */
	ret = comm_driver->open(&udev, &curDevice, subdriver_matcher, &callback);
	if (ret < 1)
		fatalx(EXIT_FAILURE, "No matching HID UPS found");

	hd = &curDevice;

	upsdebugx(1, "Detected a UPS: %s/%s", hd->Vendor ? hd->Vendor : "unknown",
		 hd->Product ? hd->Product : "unknown");

	if (hid_ups_walk(HU_WALKMODE_INIT) == FALSE) {
		fatalx(EXIT_FAILURE, "Can't initialize data from HID UPS");
	}
}

void upsdrv_cleanup(void)
{
	upsdebugx(1, "upsdrv_cleanup...");

	comm_driver->close(udev);
	Free_ReportDesc(pDesc);
	free_report_buffer(reportbuf);
#ifndef SHUT_MODE
	/* close dynamic loader */
	lt_dlexit();

	USBFreeExactMatcher(exact_matcher);
	USBFreeRegexMatcher(regex_matcher);

	free(curDevice.Vendor);
	free(curDevice.Product);
	free(curDevice.Serial);
	free(curDevice.Bus);
#endif
}

/**********************************************************************
 * Support functions
 *********************************************************************/

void possibly_supported(const char *mfr, HIDDevice_t *hd)
{
	upsdebugx(0,
"This %s device (%04x:%04x) is not (or perhaps not yet) supported\n"
"by usbhid-ups. Please make sure you have an up-to-date version of NUT. If\n"
"this does not fix the problem, try running the driver with the\n"
"'-x productid=%04x' option. Please report your results to the NUT user's\n"
"mailing list <nut-upsuser@lists.alioth.debian.org>.\n",
	mfr, hd->VendorID, hd->ProductID, hd->ProductID);
}

/* Update ups_status to remember this status item. Interpretation is
   done in ups_status_set(). */
static void process_boolean_info(char *nutvalue)
{
	status_lkp_t *status_item;
	int clear = 0;

	upsdebugx(5, "process_boolean_info: %s", nutvalue);

	if (*nutvalue == '!') {
		nutvalue++;
		clear = 1;
	}

	for (status_item = status_info; status_item->status_str != NULL ; status_item++)
	{
		if (strcasecmp(status_item->status_str, nutvalue))
			continue;

		if (clear) {
			ups_status &= ~status_item->status_mask;
		} else {
			ups_status |= status_item->status_mask;
		}

		return;
	}

	upsdebugx(5, "Warning: %s not in list of known values", nutvalue);
}

static int callback(hid_dev_handle_t udev, HIDDevice_t *hd, unsigned char *rdbuf, int rdlen)
{
	char *mfr = NULL, *model = NULL, *serial = NULL;
#ifndef	SHUT_MODE
	int ret;
#endif
	upsdebugx(2, "Report Descriptor size = %d", rdlen);
	
#ifndef SHUT_MODE
	if (matching == CHECKSUM) {
#ifdef HAVE_SSL
		unsigned char buf[SHA_DIGEST_LENGTH];

		SHA1(rdbuf, rdlen, buf);

		upsdebug_hex(3, "SHA-1 checksum of report descriptor\n", buf, sizeof(buf));

		if (checksum) {
			if (!memcmp(checksum, buf, sizeof(buf))) {
				upsdebugx(2, "SHA-1 checksum matches!");
				return 1;
			}

			upsdebugx(2, "SHA-1 checksum doesn't match!");
			return 0;
		}

		checksum = calloc(1, sizeof(buf));
		if (!checksum) {
			fatal_with_errno(EXIT_FAILURE, "Can't store SHA-1 checksum of report descriptor");
		}

		memcpy(checksum, buf, sizeof(buf));
#else
		fatalx(EXIT_FAILURE, "Checksum matching requires SSL support!");
#endif /* HAVE_SSL */
	}
#endif /* SHUT_MODE */

	upsdebug_hex(3, "Report Descriptor", rdbuf, rdlen);

	/* Parse Report Descriptor */
	Free_ReportDesc(pDesc);
	pDesc = Parse_ReportDesc(rdbuf, rdlen);
	if (!pDesc) {
		upsdebug_with_errno(1, "Failed to parse report descriptor!");
		return 0;
	}

	/* prepare report buffer */
	free_report_buffer(reportbuf);
	reportbuf = new_report_buffer(pDesc);
	if (!reportbuf) {
		upsdebug_with_errno(1, "Failed to allocate report buffer!");
		Free_ReportDesc(pDesc);
		return 0;
	}

	HIDDumpTree(udev, subdriver->utab);

#ifndef SHUT_MODE
	if (matching == DELAYED) {
		USBDeviceMatcher_t *m;

		/* apply subdriver specific formatting
		 */
		mfr = subdriver->format_mfr(hd);
		model = subdriver->format_model(hd);
		serial = subdriver->format_serial(hd);

		upsdebugx(2, "- VendorID: %04x", hd->VendorID);
		upsdebugx(2, "- ProductID: %04x", hd->ProductID);
		upsdebugx(2, "- Manufacturer: %s", hd->Vendor ? hd->Vendor : "unknown");
		upsdebugx(2, "- Product: %s", hd->Product ? hd->Product : "unknown");
		upsdebugx(2, "- Serial Number: %s", hd->Serial ? hd->Serial : "unknown");
		upsdebugx(2, "- Bus: %s", hd->Bus ? hd->Bus : "unknown");

		for (m = regex_matcher; m; m = m->next) {
			ret = matches(m, hd);
			if (ret != 1) {
				return 0;
			}
		}

		if (exact_matcher) {
			/* reload all NUT variables & commands */
			hid_ups_walk(HU_WALKMODE_RELOAD);
			return 1;
		}
	}

	/* create a new matcher for later matching */
	USBFreeExactMatcher(exact_matcher);
	ret = USBNewExactMatcher(&exact_matcher, hd);
	if (ret) {
	        upsdebug_with_errno(1, "USBNewExactMatcher()");
	        return 0;
	}

	regex_matcher->next = exact_matcher;

	if (matching != DELAYED) {
#endif /* SHUT_MODE */
		/* apply subdriver specific formatting
		 */
		mfr = subdriver->format_mfr(hd);
		model = subdriver->format_model(hd);
		serial = subdriver->format_serial(hd);
#ifndef SHUT_MODE
	}
#endif
	if (mfr != NULL) {
		dstate_setinfo("ups.mfr", "%s", mfr);
	} else {
		dstate_delinfo("ups.mfr");
	}

	if (model != NULL) {
		dstate_setinfo("ups.model", "%s", model);
	} else {
		dstate_delinfo("ups.model");
	}

	if (serial != NULL) {
		dstate_setinfo("ups.serial", "%s", serial);
	} else {
		dstate_delinfo("ups.serial");
	}

	dstate_setinfo("ups.vendorid", "%04x", hd->VendorID);
	dstate_setinfo("ups.productid", "%04x", hd->ProductID);

#ifndef SHUT_MODE
	if (exact_matcher) {
		/* reload all NUT variables & commands */
		hid_ups_walk(HU_WALKMODE_RELOAD);
		return 1;
	}
#endif
	return 1;
}

#ifdef DEBUG
static double interval(void)
{
	struct timeval		now;
	static struct timeval	last;
	double	ret;

	gettimeofday(&now, NULL);

	ret = now.tv_sec - last.tv_sec	+ ((double)(now.tv_usec - last.tv_usec)) / 1000000;
	last = now;

	return ret;
}
#endif

/* walk ups variables and set elements of the info array. */
static bool_t hid_ups_walk(walkmode_t mode)
{
	hid_info_t	*item;
	double		value;
	int		retcode;

	/* 3 modes: HU_WALKMODE_INIT, HU_WALKMODE_QUICK_UPDATE and HU_WALKMODE_FULL_UPDATE */
	
	/* Device data walk ----------------------------- */
	for (item = subdriver->hid2nut; item->info_type != NULL; item++) {

#ifdef SHUT_MODE
		/* Check if we are asked to stop (reactivity++) in SHUT mode.
		 * In USB mode, looping through this takes well under a second,
		 * so any effort to improve reactivity here is wasted. */
		if (exit_flag != 0)
			return TRUE;
#endif
		/* filter data according to mode */
		switch (mode)
		{
		/* Device capabilities enumeration */
		case HU_WALKMODE_RELOAD:
			/* Special case for handling server side variables */
			if (item->hidflags & HU_FLAG_ABSENT) {
				continue;
			}

			/* We never mapped this HID item, so skip it */
			if (item->hiddata == NULL)
				continue;

			/* Refresh the NUT-to-HID mapping and fetch new data */
			item->hiddata = HIDGetItemData(udev, item->hidpath, subdriver->utab);
			if (item->hiddata != NULL)
				continue;

			upsdebugx(1, "hid_ups_walk: RELOAD of %s failed!", item->info_type);

			/* Uh oh, it was lost after reconnect, remove it */
			if (item->hidflags & HU_TYPE_CMD) {
				dstate_delcmd(item->info_type);
				continue;
			}

			/* If NUT tupe is not "BOOL", remove this variable */
			if (strncmp(item->info_type, "BOOL", 4)) {
				dstate_delinfo(item->info_type);
				continue;
			}

			/* We only flag here, no communication should take place */
			continue;
			
		case HU_WALKMODE_INIT:
			/* Apparently, we are reconnecting, so
			 * NUT-to-HID translation is already good */
			if (item->hiddata != NULL)
				break;

			/* Special case for handling server side variables */
			if (item->hidflags & HU_FLAG_ABSENT) {

				dstate_setinfo(item->info_type, item->dfl);
				dstate_setflags(item->info_type, item->info_flags);

				/* Set max length for strings, if needed */
				if (item->info_flags & ST_FLAG_STRING)
					dstate_setaux(item->info_type, item->info_len);

				continue;
			}

			/* Create the NUT-to-HID mapping */
			item->hiddata = HIDGetItemData(udev, item->hidpath, subdriver->utab);
			if (item->hiddata == NULL)
				continue;

			/* Allow duplicates for these NUT variables... */
			if (!strncmp(item->info_type, "ups.alarm", 9)) {
				break;
			}

			/* ...this one doesn't exist yet... */
			if (dstate_getinfo(item->info_type) == NULL) {
				break;
			}

			/* ...but this one does, so don't use it! */
			item->hiddata = NULL;
			continue;

		case HU_WALKMODE_QUICK_UPDATE:
			/* Quick update only deals with status and alarms! */
			if (!(item->hidflags & HU_FLAG_QUICK_POLL))
				continue;
			
			break; 

		case HU_WALKMODE_FULL_UPDATE:
			/* These don't need polling after initinfo() */
			if (item->hidflags & (HU_FLAG_ABSENT | HU_TYPE_CMD | HU_FLAG_STATIC))
				continue;

			/* These need to be polled after user changes (setvar / instcmd) */
			if ( (item->hidflags & HU_FLAG_SEMI_STATIC) && (data_has_changed == FALSE) )
				continue;

			break; 
		
		default:
			fatalx(EXIT_FAILURE, "hid_ups_walk: unknown update mode!");
		}

		retcode = HIDGetDataValue(udev, item->hiddata, &value, poll_interval);

		switch (retcode)
		{
		case -EBUSY:
			upslog_with_errno(LOG_DEBUG, "Got disconnected by another driver");
		case -EPERM:
		case -EPIPE:
		case -ENODEV:
		case -EACCES:
		case -EIO:
		case -ENOENT:
			/* Uh oh, got to reconnect! */
			hd = NULL;
			return FALSE;

		case 1:
			break;	/* Found! */

		case 0:
			continue;

		default:
			/* Don't know what happened, try again later... */
			upslog_with_errno(LOG_DEBUG, "HIDGetDataValue");
			return FALSE;
		}

		upsdebugx(2, "Path: %s, Type: %s, ReportID: 0x%02x, Offset: %i, Size: %i, Value: %f",
			item->hidpath, HIDDataType(item->hiddata), item->hiddata->ReportID,
			item->hiddata->Offset, item->hiddata->Size, value);

		if (item->hidflags & HU_TYPE_CMD) {
			dstate_addcmd(item->info_type);
			continue;
		}

		/* Process the value we got back (set status bits and
		 * set the value of other parameters) */
		if (ups_infoval_set(item, value) != 1)
			continue;

		if (mode == HU_WALKMODE_INIT) {

			dstate_setflags(item->info_type, item->info_flags);

			/* Set max length for strings */
			if (item->info_flags & ST_FLAG_STRING) {
				dstate_setaux(item->info_type, item->info_len);
			}
		}
	}

	return TRUE;
}

static int reconnect_ups(void)
{
	int ret;

	upsdebugx(4, "==================================================");
	upsdebugx(4, "= device has been disconnected, try to reconnect =");
	upsdebugx(4, "==================================================");

	ret = comm_driver->open(&udev, &curDevice, subdriver_matcher,
#ifdef	SHUT_MODE
		NULL);
#else
		(matching == NORMAL) ? NULL : &callback);
#endif
	if (ret > 0) {
		return 1;
	}

	return 0;
}

/* Convert the local status information to NUT format and set NUT
   alarms. */
static void ups_alarm_set(void)
{
	if (ups_status & STATUS(REPLACEBATT)) {
		alarm_set("Replace battery!");
	}
	if (ups_status & STATUS(SHUTDOWNIMM)) {
		alarm_set("Shutdown imminent!");
	}
	if (ups_status & STATUS(FANFAIL)) {
		alarm_set("Fan failure!");
	}
	if (ups_status & STATUS(NOBATTERY)) {
		alarm_set("No battery installed!");
	}
	if (ups_status & STATUS(BATTVOLTLO)) {
		alarm_set("Battery voltage too low!");
	}
	if (ups_status & STATUS(BATTVOLTHI)) {
		alarm_set("Battery voltage too high!");
	}
	if (ups_status & STATUS(CHARGERFAIL)) {
		alarm_set("Battery charger fail!");
	}
	if (ups_status & STATUS(OVERHEAT)) {
		alarm_set("Temperature too high!");	/* overheat; Belkin, TrippLite */
	}
	if (ups_status & STATUS(COMMFAULT)) {
		alarm_set("Internal UPS fault!");	/* UPS fault; Belkin, TrippLite */
	}
	if (ups_status & STATUS(AWAITINGPOWER)) {
		alarm_set("Awaiting power!");		/* awaiting power; Belkin, TrippLite */
	}
}

/* Convert the local status information to NUT format and set NUT
   status. */
static void ups_status_set(void)
{
	if (ups_status & STATUS(VRANGE)) {
		dstate_setinfo("input.transfer.reason", "input voltage out of range");
	} else if (ups_status & STATUS(FRANGE)) {
		dstate_setinfo("input.transfer.reason", "input frequency out of range");
	} else {
		dstate_delinfo("input.transfer.reason");
	}

	if (ups_status & STATUS(ONLINE)) {
		status_set("OL");		/* on line */
	} else {
		status_set("OB");               /* on battery */
	}
	if ((ups_status & STATUS(DISCHRG)) &&
		!(ups_status & STATUS(DEPLETED))) {
		status_set("DISCHRG");	        /* discharging */
	}
	if ((ups_status & STATUS(CHRG)) &&
		!(ups_status & STATUS(FULLYCHARGED))) {
		status_set("CHRG");		/* charging */
	}
	if (ups_status & (STATUS(LOWBATT) | STATUS(TIMELIMITEXP) | STATUS(SHUTDOWNIMM))) {
		status_set("LB");		/* low battery */
	}
	if (ups_status & STATUS(OVERLOAD)) {
		status_set("OVER");		/* overload */
	}
	if (ups_status & STATUS(REPLACEBATT)) {
		status_set("RB");		/* replace batt */
	}
	if (ups_status & STATUS(TRIM)) {
		status_set("TRIM");		/* SmartTrim */
	}
	if (ups_status & STATUS(BOOST)) {
		status_set("BOOST");	        /* SmartBoost */
	}
	if (ups_status & STATUS(BYPASS)) {
		status_set("BYPASS");	        /* on bypass */   
	}
	if (ups_status & STATUS(OFF)) {
		status_set("OFF");              /* ups is off */
	}
	if (ups_status & STATUS(CAL)) {
		status_set("CAL");		/* calibration */
	}
}

/* find info element definition in info array
 * by NUT varname.
 */
static hid_info_t *find_nut_info(const char *varname)
{
	hid_info_t *hidups_item;

	for (hidups_item = subdriver->hid2nut; hidups_item->info_type != NULL ; hidups_item++) {

		if (strcasecmp(hidups_item->info_type, varname))
			continue;

		if (hidups_item->hiddata != NULL)
			return hidups_item;
	}

	upsdebugx(2, "find_nut_info: unknown info type: %s\n", varname);
	return NULL;
}

/* find info element definition in info array
 * by HID data pointer.
 */
static hid_info_t *find_hid_info(const HIDData_t *hiddata)
{
	hid_info_t *hidups_item;
	
	for (hidups_item = subdriver->hid2nut; hidups_item->info_type != NULL ; hidups_item++) {

		/* Skip NULL HID path (server side vars) */
		if (hidups_item->hidpath == NULL)
			continue;
	
		if (hidups_item->hiddata == hiddata)
			return hidups_item;
	}

	return NULL;
}

/* find the HID Item value matching that NUT value */
/* useful for set with value lookup... */
static long hu_find_valinfo(info_lkp_t *hid2info, const char* value)
{
	info_lkp_t *info_lkp;
	
	for (info_lkp = hid2info; info_lkp->nut_value != NULL; info_lkp++) {

		if (!(strcmp(info_lkp->nut_value, value))) {
			upsdebugx(5, "hu_find_valinfo: found %s (value: %ld)\n",
				  info_lkp->nut_value, info_lkp->hid_value);
	
			return info_lkp->hid_value;
		}
	}
	upsdebugx(3, "hu_find_valinfo: no matching HID value for this INFO_* value (%s)", value);
	return -1;
}

/* find the NUT value matching that HID Item value */
static char *hu_find_infoval(info_lkp_t *hid2info, const double value)
{
	info_lkp_t *info_lkp;
	char *nut_value;

	upsdebugx(5, "hu_find_infoval: searching for value = %g\n", value);

	/* if a conversion function is defined,
	 * use 'value' as argument for it */
	if (hid2info->fun != NULL) {
		nut_value = hid2info->fun((long)value);
		upsdebugx(5, "hu_find_infoval: found %s (value: %ld)\n",
			nut_value, (long)value);
		return nut_value;
	}

	/* use 'value' as an index for a lookup in an array */
	for (info_lkp = hid2info; info_lkp->nut_value != NULL; info_lkp++) {
		if (info_lkp->hid_value == (long)value) {
			upsdebugx(5, "hu_find_infoval: found %s (value: %ld)\n",
					info_lkp->nut_value, (long)value);

			return info_lkp->nut_value;
		}
	}
	upsdebugx(3, "hu_find_infoval: no matching INFO_* value for this HID value (%g)", value);
	return NULL;
}

/* return -1 on failure, 0 for a status update and 1 in all other cases */
static int ups_infoval_set(hid_info_t *item, double value)
{
	char *nutvalue;

	/* need lookup'ed translation? */
	if (item->hid2info != NULL){

		if ((nutvalue = hu_find_infoval(item->hid2info, value)) == NULL) {
			upsdebugx(5, "Lookup [%g] failed for [%s]", value, item->info_type);
			return -1;
		}

		/* deal with boolean items */
		if (!strncmp(item->info_type, "BOOL", 4)) {
			process_boolean_info(nutvalue);
			return 0;
		}

		/* deal with alarm items */
		if (!strncmp(item->info_type, "ups.alarm", 9)) {
			alarm_set(nutvalue);
			return 0;
		}

		dstate_setinfo(item->info_type, item->dfl, nutvalue);
	} else {
		dstate_setinfo(item->info_type, item->dfl, value);
	}

	return 1;
}
