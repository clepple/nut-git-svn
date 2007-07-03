/* liebert-hid.c - subdriver to monitor Liebert USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2005 Arnaud Quette <arnaud.quette@free.fr>
 *  2005 - 2006 Peter Selinger <selinger@users.sourceforge.net>         
 *
 *  Note: this subdriver was initially generated as a "stub" by the
 *  path-to-subdriver script. It must be customized.
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
 *
 */

#include "usbhid-ups.h"
#include "liebert-hid.h"
#include "extstate.h" /* for ST_FLAG_STRING */
#include "dstate.h"   /* for STAT_INSTCMD_HANDLED */
#include "main.h"     /* for getval() */
#include "common.h"

#define LIEBERT_HID_VERSION      "Liebert HID 0.1"

#define LIEBERT_VENDORID 0x06da

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* LIEBERT usage table */
static usage_lkp_t liebert_usage_lkp[] = {
	{  "\0", 0x0 }
};

static usage_tables_t liebert_utab[] = {
	liebert_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t liebert_hid2nut[] = {

  { "unmapped.ups.powersummary.flowid", 0, 0, "UPS.PowerSummary.FlowID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.powersummaryid", 0, 0, "UPS.PowerSummary.PowerSummaryID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.designcapacity", 0, 0, "UPS.PowerSummary.DesignCapacity", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.capacitygranularity1", 0, 0, "UPS.PowerSummary.CapacityGranularity1", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.capacitymode", 0, 0, "UPS.PowerSummary.CapacityMode", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.rechargeable", 0, 0, "UPS.PowerSummary.Rechargeable", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.idevicechemistry", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.iproduct", 0, 0, "UPS.PowerSummary.iProduct", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.imanufacturer", 0, 0, "UPS.PowerSummary.iManufacturer", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.configvoltage", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.percentload", 0, 0, "UPS.PowerSummary.PercentLoad", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.remainingcapacity", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.remainingcapacity", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.runtimetoempty", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.runtimetoempty", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.presentstatus.acpresent", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.presentstatus.belowremainingcapacitylimit", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.presentstatus.charging", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.presentstatus.discharging", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.presentstatus.overload", 0, 0, "UPS.PowerSummary.PresentStatus.Overload", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.presentstatus.shutdownimminent", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.presentstatus.acpresent", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.presentstatus.belowremainingcapacitylimit", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.presentstatus.charging", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.presentstatus.discharging", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.presentstatus.overload", 0, 0, "UPS.PowerSummary.PresentStatus.Overload", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ups.powersummary.presentstatus.shutdownimminent", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, "%.0f", HU_FLAG_OK, NULL },

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

/* shutdown method for Liebert */
static int liebert_shutdown(int ondelay, int offdelay) {
	/* FIXME: ondelay, offdelay currently not used */
	
	/* Default method */
	upsdebugx(2, "Trying load.off.");
	if (instcmd("load.off", NULL) == STAT_INSTCMD_HANDLED) {
		return 1;
	}
	upsdebugx(2, "Shutdown failed.");
	return 0;
}

static char *liebert_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static char *liebert_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "Liebert";
}

static char *liebert_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int liebert_claim(HIDDevice_t *hd) {
	if (hd->VendorID != LIEBERT_VENDORID) {
		return 0;
	}
	switch (hd->ProductID) {

	/* accept any known UPS - add devices here as needed */
	case 0xffff:
		return 1;

	/* by default, reject, unless the productid option is given */
	default:
		if (getval("productid")) {
			return 1;
		} else {
			upsdebugx(1,
"This Liebert device (%04x/%04x) is not (or perhaps not yet) supported\n"
"by usbhid-ups. Please make sure you have an up-to-date version of NUT. If\n"
"this does not fix the problem, try running the driver with the\n"
"'-x productid=%04x' option. Please report your results to the NUT user's\n"
"mailing list <nut-upsuser@lists.alioth.debian.org>.\n",
						 hd->VendorID, hd->ProductID, hd->ProductID);
			return 0;
		}
	}
}

subdriver_t liebert_subdriver = {
	LIEBERT_HID_VERSION,
	liebert_claim,
	liebert_utab,
	liebert_hid2nut,
	liebert_shutdown,
	liebert_format_model,
	liebert_format_mfr,
	liebert_format_serial,
};
