/* mge-xml.c	Model specific routines for Eaton / MGE XML protocol UPSes

   Copyright (C)
	2008-2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
	2009		Arnaud Quette <ArnaudQuette@Eaton.com>

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ne_xml.h>

#include "common.h"
#include "dstate.h"

#include "netxml-ups.h"
#include "mge-xml.h"

#define MGE_XML_VERSION		"MGEXML/0.22"
#define MGE_XML_INITUPS		"/"
#define MGE_XML_INITINFO	"/mgeups/product.xml /product.xml /ws/product.xml"

#define ST_FLAG_RW		0x0001
#define ST_FLAG_STATIC		0x0002

static int	mge_ambient_value = 0;

static char	mge_scratch_buf[256];

static char	var[128];
static char	val[128];

typedef enum {
	ROOTPARENT = NE_XML_STATEROOT,

	_UNEXPECTED,
	_PARSEERROR,

	PRODUCT_INFO = 100,	/* "/mgeups/product.xml" */

		PI_SUMMARY = 110,
			PI_HTML_PROPERTIES_PAGE,
			PI_XML_SUMMARY_PAGE,
			PI_CENTRAL_CFG,
			PI_CSV_LOGS,
		/* /PI_SUMMARY */

		PI_ALARMS = 120,
			PI_SUBSCRIPTION,
			PI_POLLING,
		/* /ALARMS */

		PI_MANAGEMENT = 130,
			PI_MANAGEMENT_PAGE,
			PI_XML_MANAGEMENT_PAGE,
		/* /MANAGEMENT */

		PI_UPS_DATA = 140,
			PI_GET_OBJECT,
			PI_SET_OBJECT,
		/* /UPS_DATA */

	/* /PRODUCT_INFO */

	SUMMARY = 200,		/* "/upsprop.xml" */
		SU_OBJECT,
	/* /SUMMARY */

	GET_OBJECT = 300,	/* "/getvalue.cgi" */
		GO_OBJECT,
	/* /GET_OBJECT */

	SET_OBJECT = 400,	/* "/setvalue.cgi" */
		SO_OBJECT,
	/* /SET_OBJECT */

	ALARM = 500,

	XML_CLIENT = 600,
		XC_GENERAL = 610,
			XC_STARTUP,
			XC_SHUTDOWN,
			XC_BROADCAST

} mge_xml_state_t;

typedef struct {
	const char	*nutname;	/* NUT variable name */
	uint32_t nutflags;	/* NUT flags (to set in addinfo) */
	size_t	nutlen;		/* length of the NUT string */
	const char	*xmlname;	/* XML variable name */
	uint32_t xmlflags;	/* XML flags (to be used to determine what kind of variable this is */
	size_t	xmllen;		/* length of the XML string */
	const char	*(*convert)(const char *value);	/* conversion function from XML<->NUT value (returns
						   NULL if no further processing is required) */
} xml_info_t;

static const char *online_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(ONLINE);
	} else {
		STATUS_CLR(ONLINE);
	}

	return NULL;
}

static const char *discharging_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(DISCHRG);
	} else {
		STATUS_CLR(DISCHRG);
	}

	return NULL;
}

static const char *charging_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(CHRG);
	} else {
		STATUS_CLR(CHRG);
	}

	return NULL;
}

static const char *lowbatt_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(LOWBATT);
	} else {
		STATUS_CLR(LOWBATT);
	}

	return NULL;
}

static const char *overload_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(OVERLOAD);
	} else {
		STATUS_CLR(OVERLOAD);
	}

	return NULL;
}

static const char *replacebatt_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(REPLACEBATT);
	} else {
		STATUS_CLR(REPLACEBATT);
	}

	return NULL;
}

static const char *trim_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(TRIM);
	} else {
		STATUS_CLR(TRIM);
	}

	return NULL;
}

static const char *boost_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(BOOST);
	} else {
		STATUS_CLR(BOOST);
	}

	return NULL;
}

static const char *bypass_aut_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(BYPASSAUTO);
	} else {
		STATUS_CLR(BYPASSAUTO);
	}

	return NULL;
}

static const char *bypass_man_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(BYPASSMAN);
	} else {
		STATUS_CLR(BYPASSMAN);
	}

	return NULL;
}

static const char *off_info(const char *val)
{
	if (val[0] == '0') {
		STATUS_SET(OFF);
	} else {
		STATUS_CLR(OFF);
	}

	return NULL;
}

/* note: this value is reverted (0=set, 1=not set). We report "battery
   not installed" rather than "battery installed", so that devices
   that don't implement this variable have a battery by default */
static const char *nobattery_info(const char *val)
{
	if (val[0] == '0') {
		STATUS_SET(NOBATTERY);
	} else {
		STATUS_CLR(NOBATTERY);
	}

	return NULL;
}

static const char *fanfail_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(FANFAIL);
	} else {
		STATUS_CLR(FANFAIL);
	}

	return NULL;
}
/*
static const char *shutdownimm_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(SHUTDOWNIMM);
	} else {
		STATUS_CLR(SHUTDOWNIMM);
	}

	return NULL;
}
 */
static const char *overheat_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(OVERHEAT);
	} else {
		STATUS_CLR(OVERHEAT);
	}

	return NULL;
}

static const char *commfault_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(COMMFAULT);
	} else {
		STATUS_CLR(COMMFAULT);
	}

	return NULL;
}

static const char *internalfailure_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(INTERNALFAULT);
	} else {
		STATUS_CLR(INTERNALFAULT);
	}

	return NULL;
}

static const char *battvoltlo_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(BATTVOLTLO);
	} else {
		STATUS_CLR(BATTVOLTLO);
	}

	return NULL;
}

static const char *battvolthi_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(BATTVOLTHI);
	} else {
		STATUS_CLR(BATTVOLTHI);
	}

	return NULL;
}

static const char *chargerfail_info(const char *val)
{
	if ((val[0] == '1') || !strncasecmp(val, "Yes", 3)) {
		STATUS_SET(CHARGERFAIL);
	} else {
		STATUS_CLR(CHARGERFAIL);
	}

	return NULL;
}

static const char *vrange_info(const char *val)
{
	if ((val[0] == '1') || !strncasecmp(val, "Yes", 3)) {
		STATUS_SET(VRANGE);
	} else {
		STATUS_CLR(VRANGE);
	}

	return NULL;
}

static const char *frange_info(const char *val)
{
	if ((val[0] == '1') || !strncasecmp(val, "Yes", 3)) {
		STATUS_SET(FRANGE);
	} else {
		STATUS_CLR(FRANGE);
	}

	return NULL;
}

static const char *fuse_fault_info(const char *val)
{
	if (val[0] == '1') {
		STATUS_SET(FUSEFAULT);
	} else {
		STATUS_CLR(FUSEFAULT);
	}

	return NULL;
}

static const char *yes_no_info(const char *val)
{
	switch(val[0])
	{
	case '1':
		return "yes";
	case '0':
		return "no";
	default:
		upsdebugx(2, "%s: unexpected value [%s]", __func__, val);
		return "<unknown>";
	}
}

static const char *on_off_info(const char *val)
{
	switch(val[0])
	{
	case '1':
		return "on";
	case '0':
		return "off";
	default:
		upsdebugx(2, "%s: unexpected value [%s]", __func__, val);
		return "<unknown>";
	}
}

static const char *convert_deci(const char *val)
{
	snprintf(mge_scratch_buf, sizeof(mge_scratch_buf), "%.1f", 0.1 * (float)atoi(val));

	return mge_scratch_buf;
}

/* Ignore a zero value if the UPS is not switched off */
static const char *ignore_if_zero(const char *val)
{
	if (atoi(val) == 0) {
		return NULL;
	}

	return convert_deci(val);
}

/* Set the 'ups.date' from the combined value
 * (ex. 2008/03/01 15:23:26) and return the time */
static const char *split_date_time(const char *val)
{
	char	*last = NULL;

	snprintf(mge_scratch_buf, sizeof(mge_scratch_buf), "%s", val);
	dstate_setinfo("ups.date", "%s", strtok_r(mge_scratch_buf, " -", &last));

	return strtok_r(NULL, " ", &last);
}

static const char *url_convert(const char *val)
{
	char	buf[256], *last = NULL;

	snprintf(buf, sizeof(buf), "%s", val);
	snprintf(mge_scratch_buf, sizeof(mge_scratch_buf), "/%s", strtok_r(buf, " \r\n\t", &last));

	return mge_scratch_buf;
}

static const char *mge_battery_capacity(const char *val)
{
	snprintf(mge_scratch_buf, sizeof(mge_scratch_buf), "%.2f", (float)atoi(val) / 3600);
	return mge_scratch_buf;
}

static const char *mge_powerfactor_conversion(const char *val)
{
	snprintf(mge_scratch_buf, sizeof(mge_scratch_buf), "%.2f", (float)atoi(val) / 100);
	return mge_scratch_buf;
}

static const char *mge_beeper_info(const char *val)
{
	switch (atoi(val))
	{
	case 1:
		return "disabled";
	case 2:
		return "enabled";
	case 3:
		return "muted";
	}
	return NULL;
}

static const char *mge_upstype_conversion(const char *val)
{
	switch (atoi(val))
	{
	case 1:
		return "offline / line interactive";
	case 2:
		return "online";
	case 3:
		return "online - unitary/parallel";
	case 4:
		return "online - parallel with hot standy";
	case 5:
		return "online - hot standby redundancy";
	}
	return NULL;
}

static const char *mge_sensitivity_info(const char *val)
{
	switch (atoi(val))
	{
	case 0:
		return "normal";
	case 1:
		return "high";
	case 2:
		return "low";
	}
	return NULL;
}

static const char *mge_test_result_info(const char *val)
{
	switch (atoi(val))
	{
	case 1:
		return "done and passed";
	case 2:
		return "done and warning";
	case 3:
		return "done and error";
	case 4:
		return "aborted";
	case 5:
		return "in progress";
	case 6:
		return "no test initiated";
	case 7:
		return "test scheduled";
	}
	return NULL;
}

static const char *mge_ambient_info(const char *val)
{
	switch (mge_ambient_value)
	{
	case 1:
		return val;
	default:
		return NULL;
	}
}

static const char *mge_timer_shutdown(const char *val)
{
	const char	*delay = dstate_getinfo("ups.delay.shutdown");

	if ((delay) && (atoi(val) > -1) && (atoi(val) < atoi(delay))) {
		STATUS_SET(SHUTDOWNIMM);
	} else {
		STATUS_CLR(SHUTDOWNIMM);
	}

	return val;
}

static xml_info_t mge_xml2nut[] = {
	/* Special case: boolean values that are mapped to ups.status and ups.alarm */
	{ NULL, 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", 0, 0, online_info },
	{ NULL, 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", 0, 0, discharging_info },
	{ NULL, 0, 0, "UPS.PowerSummary.PresentStatus.Charging", 0, 0, charging_info },
	{ NULL, 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", 0, 0, lowbatt_info },
	{ NULL, 0, 0, "UPS.PowerSummary.PresentStatus.Overload", 0, 0, overload_info },
	{ NULL, 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", 0, 0, replacebatt_info },
	{ NULL, 0, 0, "UPS.PowerConverter.Input[1].PresentStatus.Buck", 0, 0, trim_info },
	{ NULL, 0, 0, "UPS.PowerConverter.Input[1].PresentStatus.Boost", 0, 0, boost_info },
	{ NULL, 0, 0, "UPS.PowerConverter.Input[1].PresentStatus.VoltageOutOfRange", 0, 0, vrange_info },
	{ NULL, 0, 0, "UPS.PowerConverter.Input[1].PresentStatus.FrequencyOutOfRange", 0, 0, frange_info },
	{ NULL, 0, 0, "UPS.PowerConverter.Input[1].PresentStatus.FuseFault", 0, 0, fuse_fault_info },
	{ NULL, 0, 0, "UPS.PowerConverter.Input[1].PresentStatus.InternalFailure", 0, 0, internalfailure_info },
	{ NULL, 0, 0, "UPS.PowerSummary.PresentStatus.Good", 0, 0, off_info },
	/* { NULL, 0, 0, "UPS.PowerConverter.Input[1].PresentStatus.Used", 0, 0, online_info }, */
	{ NULL, 0, 0, "UPS.PowerConverter.Input[2].PresentStatus.Used", 0, 0, bypass_aut_info }, /* Automatic bypass */
	/* { NULL, 0, 0, "UPS.PowerConverter.Input[3].PresentStatus.Used", 0, 0, onbatt_info }, */
	{ NULL, 0, 0, "UPS.PowerConverter.Input[4].PresentStatus.Used", 0, 0, bypass_man_info }, /* Manual bypass */
	{ NULL, 0, 0, "UPS.PowerSummary.PresentStatus.FanFailure", 0, 0, fanfail_info },
	{ NULL, 0, 0, "UPS.BatterySystem.Battery.PresentStatus.Present", 0, 0, nobattery_info },
	{ NULL, 0, 0, "UPS.BatterySystem.Charger.PresentStatus.InternalFailure", 0, 0, chargerfail_info },
	{ NULL, 0, 0, "UPS.BatterySystem.Charger.PresentStatus.VoltageTooHigh", 0, 0, battvolthi_info },
	{ NULL, 0, 0, "UPS.BatterySystem.Charger.PresentStatus.VoltageTooLow", 0, 0, battvoltlo_info },
	{ NULL, 0, 0, "UPS.PowerSummary.PresentStatus.InternalFailure", 0, 0, internalfailure_info },
	{ NULL, 0, 0, "UPS.PowerSummary.PresentStatus.CommunicationLost", 0, 0, commfault_info },
	{ NULL, 0, 0, "UPS.PowerSummary.PresentStatus.OverTemperature", 0, 0, overheat_info },
	/* { NULL, 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", 0, 0, shutdownimm_info }, */

	/* Battery page */
	{ "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", 0, 0, NULL },
	{ "battery.charge.low", 0, 0, "UPS.PowerSummary.RemainingCapacityLimitSetting", 0, 0, NULL },
	{ "battery.charge.low", 0, 0, "UPS.PowerSummary.RemainingCapacityLimit", 0, 0, NULL }, /* Read only */
	{ "battery.charge.restart", 0, 0, "UPS.PowerSummary.RestartLevel", 0, 0, NULL },
	{ "battery.capacity", 0, 0, "UPS.BatterySystem.Battery.DesignCapacity", 0, 0, mge_battery_capacity }, /* conversion needed from As to Ah */
	{ "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", 0, 0, NULL },
	{ "battery.runtime.low", 0, 0, "System.RunTimeToEmptyLimit", 0, 0, NULL },
	{ "battery.temperature", 0, 0, "UPS.BatterySystem.Battery.Temperature", 0, 0, NULL },
	{ "battery.type", ST_FLAG_STATIC, 0, "UPS.PowerSummary.iDeviceChemistry", 0, 0, NULL },
	{ "battery.type", ST_FLAG_STATIC, 0, "UPS.PowerSummary.iDeviceChemistery", 0, 0, NULL }, /* [sic] */
	{ "battery.voltage", 0, 0, "UPS.PowerSummary.Voltage", 0, 0, NULL },
	{ "battery.voltage.nominal", ST_FLAG_STATIC, 0, "UPS.BatterySystem.ConfigVoltage", 0, 0, NULL },
	{ "battery.voltage.nominal", ST_FLAG_STATIC, 0, "UPS.PowerSummary.ConfigVoltage", 0, 0, NULL }, /* mge_battery_voltage_nominal */
	{ "battery.current", 0, 0, "UPS.PowerSummary.Current", 0, 0, NULL },
	{ "battery.protection", 0, 0, "UPS.BatterySystem.Battery.DeepDischargeProtection", 0, 0, yes_no_info },
	{ "battery.energysave", 0, 0, "UPS.PowerConverter.Input[3].EnergySaving", 0, 0, yes_no_info },

	/* UPS page */
	{ "ups.mfr", ST_FLAG_STATIC, 0, "UPS.PowerSummary.iManufacturer", 0, 0, NULL },
	{ "ups.model", ST_FLAG_STATIC, 0, "System.Description", 0, 0, NULL },
	{ "ups.model", ST_FLAG_STATIC, 0, "UPS.PowerSummary.iProduct", 0, 0, NULL },
	{ "ups.model.aux", ST_FLAG_STATIC, 0, "UPS.PowerSummary.iModel", 0, 0, NULL },
	{ "ups.time", 0, 0, "System.LastAcquisition", 0, 0, split_date_time },
	/* -> XML variable System.Location [Computer Room] doesn't map to any NUT variable */
	/* -> XML variable System.Contact [Computer Room Manager] doesn't map to any NUT variable */
	/* -> XML variable UPS.PowerSummary.iProduct [Evolution] doesn't map to any NUT variable */
	/* -> XML variable UPS.PowerSummary.iModel [650] doesn't map to any NUT variable */
	{ "ups.serial", ST_FLAG_STATIC, 0, "UPS.PowerSummary.iSerialNumber", 0, 0, NULL },
	{ "ups.firmware", ST_FLAG_STATIC, 0, "UPS.PowerSummary.iVersion", 0, 0, NULL },
	{ "ups.load", 0, 0, "UPS.PowerSummary.PercentLoad", 0, 0, NULL },
	{ "ups.load.high", 0, 0, "UPS.Flow[4].ConfigPercentLoad", 0, 0, NULL },
	{ "ups.delay.shutdown", 0, 0, "System.ShutdownDuration", 0, 0, NULL },
	{ "ups.timer.start", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", 0, 0, NULL},
	{ "ups.timer.shutdown", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", 0, 0, mge_timer_shutdown },
	{ "ups.timer.reboot", 0, 0, "UPS.PowerSummary.DelayBeforeReboot", 0, 0, NULL },
	{ "ups.test.result", 0, 0, "UPS.BatterySystem.Battery.Test", 0, 0, mge_test_result_info },
	{ "ups.test.interval", 0, 0, "UPS.BatterySystem.Battery.TestPeriod", 0, 0, NULL },
	{ "ups.beeper.status", 0 ,0, "UPS.BatterySystem.Battery.AudibleAlarmControl", 0, 0, mge_beeper_info },
	{ "ups.beeper.status", 0 ,0, "UPS.PowerSummary.AudibleAlarmControl", 0, 0, mge_beeper_info },
	{ "ups.temperature", 0, 0, "UPS.PowerSummary.Temperature", 0, 0, NULL },
	{ "ups.power", 0, 0, "UPS.PowerConverter.Output.ApparentPower", 0, 0, NULL },
	{ "ups.L1.power", 0, 0, "UPS.PowerConverter.Output.Phase[1].ApparentPower", 0, 0, ignore_if_zero },
	{ "ups.L2.power", 0, 0, "UPS.PowerConverter.Output.Phase[2].ApparentPower", 0, 0, ignore_if_zero },
	{ "ups.L3.power", 0, 0, "UPS.PowerConverter.Output.Phase[3].ApparentPower", 0, 0, ignore_if_zero },
	{ "ups.power.nominal", ST_FLAG_STATIC, 0, "UPS.Flow[4].ConfigApparentPower", 0, 0, NULL },
	{ "ups.realpower", 0, 0, "UPS.PowerConverter.Output.ActivePower", 0, 0, NULL },
	{ "ups.L1.realpower", 0, 0, "UPS.PowerConverter.Output.Phase[1].ActivePower", 0, 0, ignore_if_zero },
	{ "ups.L2.realpower", 0, 0, "UPS.PowerConverter.Output.Phase[2].ActivePower", 0, 0, ignore_if_zero },
	{ "ups.L3.realpower", 0, 0, "UPS.PowerConverter.Output.Phase[3].ActivePower", 0, 0, ignore_if_zero },
	{ "ups.realpower.nominal", ST_FLAG_STATIC, 0, "UPS.Flow[4].ConfigActivePower", 0, 0, NULL },
	{ "ups.start.auto", 0, 0, "UPS.PowerConverter.Input[1].AutomaticRestart", 0, 0, yes_no_info },
	{ "ups.start.battery", 0, 0, "UPS.PowerConverter.Input[3].StartOnBattery", 0, 0, yes_no_info },
	{ "ups.start.reboot", 0, 0, "UPS.PowerConverter.Output.ForcedReboot", 0, 0, yes_no_info },
	{ "ups.type", ST_FLAG_STATIC, 0, "UPS.PowerConverter.ConverterType", 0, 0, mge_upstype_conversion },

	/* Input page */
	{ "input.voltage", 0, 0, "UPS.PowerConverter.Input[1].Voltage", 0, 0, NULL },
	{ "input.L1-N.voltage", 0, 0, "UPS.PowerConverter.Input[1].Phase[1].Voltage", 0, 0, NULL },
	{ "input.L2-N.voltage", 0, 0, "UPS.PowerConverter.Input[1].Phase[2].Voltage", 0, 0, NULL },
	{ "input.L3-N.voltage", 0, 0, "UPS.PowerConverter.Input[1].Phase[3].Voltage", 0, 0, NULL },
	{ "input.L1-L2.voltage", 0, 0, "UPS.PowerConverter.Input[1].Phase[12].Voltage", 0, 0, NULL },
	{ "input.L2-L3.voltage", 0, 0, "UPS.PowerConverter.Input[1].Phase[23].Voltage", 0, 0, NULL },
	{ "input.L3-L1.voltage", 0, 0, "UPS.PowerConverter.Input[1].Phase[31].Voltage", 0, 0, NULL },
	{ "input.L1-L2.voltage", 0, 0, "UPS.PowerConverter.Input[1].Phase[11].Voltage", 0, 0, convert_deci },
	{ "input.L2-L3.voltage", 0, 0, "UPS.PowerConverter.Input[1].Phase[22].Voltage", 0, 0, convert_deci },
	{ "input.L3-L1.voltage", 0, 0, "UPS.PowerConverter.Input[1].Phase[33].Voltage", 0, 0, convert_deci },
	{ "input.voltage.nominal", 0, 0, "UPS.Flow[1].ConfigVoltage", 0, 0, NULL },
	{ "input.current", 0, 0, "UPS.PowerConverter.Input[1].Current", 0, 0, NULL },
	{ "input.L1.current", 0, 0, "UPS.PowerConverter.Input[1].Phase[1].Current", 0, 0, convert_deci },
	{ "input.L2.current", 0, 0, "UPS.PowerConverter.Input[1].Phase[2].Current", 0, 0, convert_deci },
	{ "input.L3.current", 0, 0, "UPS.PowerConverter.Input[1].Phase[3].Current", 0, 0, convert_deci },
	{ "input.current.nominal", 0, 0, "UPS.Flow[1].ConfigCurrent", 0, 0, NULL },
	{ "input.frequency", 0, 0, "UPS.PowerConverter.Input[1].Frequency", 0, 0, NULL },
	{ "input.frequency.nominal", 0, 0, "UPS.Flow[1].ConfigFrequency", 0, 0, NULL },
	{ "input.voltage.extended", 0, 0, "UPS.PowerConverter.Output.ExtendedVoltageMode", 0, 0, yes_no_info },
	{ "input.frequency.extended", 0, 0, "UPS.PowerConverter.Output.ExtendedFrequencyMode", 0, 0, yes_no_info },
	/* same as "input.transfer.boost.low" */
	{ "input.transfer.low", 0, 0, "UPS.PowerConverter.Output.LowVoltageTransfer", 0, 0, NULL },
	{ "input.transfer.boost.low", 0, 0, "UPS.PowerConverter.Output.LowVoltageBoostTransfer", 0, 0, NULL },
	{ "input.transfer.boost.high", 0, 0, "UPS.PowerConverter.Output.HighVoltageBoostTransfer", 0, 0, NULL },
	{ "input.transfer.trim.low", 0, 0, "UPS.PowerConverter.Output.LowVoltageBuckTransfer", 0, 0, NULL },
	/* same as "input.transfer.trim.high" */
	{ "input.transfer.high", 0, 0, "UPS.PowerConverter.Output.HighVoltageTransfer", 0, 0, NULL },
	{ "input.transfer.trim.high", 0, 0, "UPS.PowerConverter.Output.HighVoltageBuckTransfer", 0, 0, NULL },
	{ "input.sensitivity", 0, 0, "UPS.PowerConverter.Output.SensitivityMode", 0, 0, mge_sensitivity_info },

	/* Bypass page */
	{ "input.bypass.voltage", 0, 0, "UPS.PowerConverter.Input[2].Voltage", 0, 0, NULL },
	{ "input.bypass.L1-N.voltage", 0, 0, "UPS.PowerConverter.Input[2].Phase[1].Voltage", 0, 0, NULL },
	{ "input.bypass.L2-N.voltage", 0, 0, "UPS.PowerConverter.Input[2].Phase[2].Voltage", 0, 0, NULL },
	{ "input.bypass.L3-N.voltage", 0, 0, "UPS.PowerConverter.Input[2].Phase[3].Voltage", 0, 0, NULL },
	{ "input.bypass.L1-L2.voltage", 0, 0, "UPS.PowerConverter.Input[2].Phase[12].Voltage", 0, 0, NULL },
	{ "input.bypass.L2-L3.voltage", 0, 0, "UPS.PowerConverter.Input[2].Phase[23].Voltage", 0, 0, NULL },
	{ "input.bypass.L3-L1.voltage", 0, 0, "UPS.PowerConverter.Input[2].Phase[31].Voltage", 0, 0, NULL },
	{ "input.bypass.L1-L2.voltage", 0, 0, "UPS.PowerConverter.Input[2].Phase[11].Voltage", 0, 0, NULL },
	{ "input.bypass.L2-L3.voltage", 0, 0, "UPS.PowerConverter.Input[2].Phase[22].Voltage", 0, 0, NULL },
	{ "input.bypass.L3-L1.voltage", 0, 0, "UPS.PowerConverter.Input[2].Phase[33].Voltage", 0, 0, NULL },
	{ "input.bypass.voltage.nominal", 0, 0, "UPS.Flow[2].ConfigVoltage", 0, 0, NULL },
	{ "input.bypass.current", 0, 0, "UPS.PowerConverter.Input[2].Current", 0, 0, NULL },
	{ "input.bypass.L1.current", 0, 0, "UPS.PowerConverter.Input[2].Phase[1].Current", 0, 0, NULL },
	{ "input.bypass.L2.current", 0, 0, "UPS.PowerConverter.Input[2].Phase[2].Current", 0, 0, NULL },
	{ "input.bypass.L3.current", 0, 0, "UPS.PowerConverter.Input[2].Phase[3].Current", 0, 0, NULL },
	{ "input.bypass.current.nominal", 0, 0, "UPS.Flow[2].ConfigCurrent", 0, 0, NULL },
	{ "input.bypass.frequency", 0, 0, "UPS.PowerConverter.Input[2].Frequency", 0, 0, NULL },
	{ "input.bypass.frequency.nominal", 0, 0, "UPS.Flow[2].ConfigFrequency", 0, 0, NULL },

	/* Output page */
	{ "output.voltage", 0, 0, "UPS.PowerConverter.Output.Voltage", 0, 0, NULL },
	{ "output.L1-N.voltage", 0, 0, "UPS.PowerConverter.Output.Phase[1].Voltage", 0, 0, NULL },
	{ "output.L2-N.voltage", 0, 0, "UPS.PowerConverter.Output.Phase[2].Voltage", 0, 0, NULL },
	{ "output.L3-N.voltage", 0, 0, "UPS.PowerConverter.Output.Phase[3].Voltage", 0, 0, NULL },
	{ "output.L1-L2.voltage", 0, 0, "UPS.PowerConverter.Output.Phase[12].Voltage", 0, 0, NULL },
	{ "output.L2-L3.voltage", 0, 0, "UPS.PowerConverter.Output.Phase[23].Voltage", 0, 0, NULL },
	{ "output.L3-L1.voltage", 0, 0, "UPS.PowerConverter.Output.Phase[31].Voltage", 0, 0, NULL },
	{ "output.L1-L2.voltage", 0, 0, "UPS.PowerConverter.Output.Phase[11].Voltage", 0, 0, ignore_if_zero },
	{ "output.L2-L3.voltage", 0, 0, "UPS.PowerConverter.Output.Phase[22].Voltage", 0, 0, ignore_if_zero },
	{ "output.L3-L1.voltage", 0, 0, "UPS.PowerConverter.Output.Phase[33].Voltage", 0, 0, ignore_if_zero },
	{ "output.voltage.nominal", 0, 0, "UPS.Flow[4].ConfigVoltage", 0, 0, NULL },
	{ "output.current", 0, 0, "UPS.PowerConverter.Output.Current", 0, 0, NULL },
	{ "output.L1.current", 0, 0, "UPS.PowerConverter.Output.Phase[1].Current", 0, 0, convert_deci },
	{ "output.L2.current", 0, 0, "UPS.PowerConverter.Output.Phase[2].Current", 0, 0, convert_deci },
	{ "output.L3.current", 0, 0, "UPS.PowerConverter.Output.Phase[3].Current", 0, 0, convert_deci },
	{ "output.current.nominal", 0, 0, "UPS.Flow[4].ConfigCurrent", 0, 0, NULL },
	{ "output.frequency", 0, 0, "UPS.PowerConverter.Output.Frequency", 0, 0, NULL },
	{ "output.frequency.nominal", 0, 0, "UPS.Flow[4].ConfigFrequency", 0, 0, NULL },
	{ "output.powerfactor", 0, 0, "UPS.PowerConverter.Output.PowerFactor", 0, 0, mge_powerfactor_conversion },

	/* Ambient page */
	{ "ambient.humidity", 0, 0, "Environment.Humidity", 0, 0, NULL },
	{ "ambient.humidity.high", 0, 0, "Environment.Humidity.HighThreshold", 0, 0, NULL },
	{ "ambient.humidity.low", 0, 0, "Environment.Humidity.LowThreshold", 0, 0, NULL },
	{ "ambient.humidity.maximum", 0, 0, "Environment.PresentStatus.HighHumidity", 0, 0, mge_ambient_info },
	{ "ambient.humidity.minimum", 0, 0, "Environment.PresentStatus.LowHumidity", 0, 0, mge_ambient_info },
	{ "ambient.temperature", 0, 0, "Environment.Temperature", 0, 0, NULL },
	{ "ambient.temperature.high", 0, 0, "Environment.Temperature.HighThreshold", 0, 0, NULL },
	{ "ambient.temperature.low", 0, 0, "Environment.Temperature.LowThreshold", 0, 0, NULL },
	{ "ambient.temperature.maximum", 0, 0, "Environment.PresentStatus.HighTemperature", 0, 0, mge_ambient_info },
	{ "ambient.temperature.minimum", 0, 0, "Environment.PresentStatus.LowTemperature", 0, 0, mge_ambient_info },

	/* Outlet page (using MGE UPS SYSTEMS - PowerShare technology) */
	{ "outlet.id", 0, 0, "UPS.OutletSystem.Outlet[1].OutletID", 0, 0, NULL },
	{ "outlet.desc", 0, 0, "UPS.OutletSystem.Outlet[1].iName", 0, 0, NULL },
	{ "outlet.switchable", 0, 0, "UPS.OutletSystem.Outlet[1].PresentStatus.Switchable", 0, 0, yes_no_info },

	{ "outlet.1.id", 0, 0, "UPS.OutletSystem.Outlet[2].OutletID", 0, 0, NULL },
	{ "outlet.1.desc", 0, 0, "UPS.OutletSystem.Outlet[2].iName", 0, 0, NULL },
	{ "outlet.1.switchable", 0, 0, "UPS.OutletSystem.Outlet[2].PresentStatus.Switchable", 0, 0, yes_no_info },
	{ "outlet.1.status", 0, 0, "UPS.OutletSystem.Outlet[2].PresentStatus.SwitchOnOff", 0, 0, on_off_info },
	/* For low end models, with 1 non backup'ed outlet */
	{ "outlet.1.status", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", 0, 0, NULL }, /* on_off_info */
	{ "outlet.1.battery.charge.low", 0, 0, "UPS.OutletSystem.Outlet[2].RemainingCapacityLimit", 0, 0, NULL },
	{ "outlet.1.timer.start", 0, 0, "UPS.OutletSystem.Outlet[2].DelayBeforeStartup", 0, 0, NULL },
	{ "outlet.1.timer.shutdown", 0, 0, "UPS.OutletSystem.Outlet[2].DelayBeforeShutdown", 0, 0, NULL },
	{ "outlet.1.delay.start", 0, 0, "UPS.OutletSystem.Outlet[2].StartupTimer", 0, 0, NULL },
	/* { "outlet.1.delay.shutdown", 0, 0, "UPS.OutletSystem.Outlet[2].ShutdownTimer", 0, 0, NULL }, */
	{ "outlet.1.delay.shutdown", 0, 0, "System.Outlet[2].ShutdownDuration", 0, 0, NULL },

	{ "outlet.2.id", 0, 0, "UPS.OutletSystem.Outlet[3].OutletID", 0, 0, NULL },
	{ "outlet.2.desc", 0, 0, "UPS.OutletSystem.Outlet[3].iName", 0, 0, NULL },
	{ "outlet.2.switchable", 0, 0, "UPS.OutletSystem.Outlet[3].PresentStatus.Switchable", 0, 0, yes_no_info },
	{ "outlet.2.status", 0, 0, "UPS.OutletSystem.Outlet[3].PresentStatus.SwitchOnOff", 0, 0, on_off_info },
	{ "outlet.2.battery.charge.low", 0, 0, "UPS.OutletSystem.Outlet[3].RemainingCapacityLimit", 0, 0, NULL },
	{ "outlet.2.timer.start", 0, 0, "UPS.OutletSystem.Outlet[3].DelayBeforeStartup", 0, 0, NULL },
	{ "outlet.2.timer.shutdown", 0, 0, "UPS.OutletSystem.Outlet[3].DelayBeforeShutdown", 0, 0, NULL },
	{ "outlet.2.delay.start", 0, 0, "UPS.OutletSystem.Outlet[3].StartupTimer", 0, 0, NULL },
	/* { "outlet.2.delay.shutdown", 0, 0, "UPS.OutletSystem.Outlet[3].ShutdownTimer", 0, 0, NULL }, */
	{ "outlet.2.delay.shutdown", 0, 0, "System.Outlet[3].ShutdownDuration", 0, 0, NULL },

	/* For newer ePDU Monitored */
	{ "outlet.1.desc", 0, 0, "PDU.OutletSystem.Outlet[1].iName", 0, 0, NULL },
	{ "outlet.1.current", 0, 0, "PDU.OutletSystem.Outlet[1].Current", 0, 0, convert_deci },
	/* FIXME: also map these?
	 * "PDU.OutletSystem.Outlet[1].CurrentLimit" => settable, triggers CurrentTooHigh
	 * "PDU.OutletSystem.Outlet[1].PresentStatus.CurrentTooHigh" (0/1)
	 */
	{ "outlet.2.desc", 0, 0, "PDU.OutletSystem.Outlet[2].iName", 0, 0, NULL },
	{ "outlet.2.current", 0, 0, "PDU.OutletSystem.Outlet[2].Current", 0, 0, convert_deci },
	{ "outlet.3.desc", 0, 0, "PDU.OutletSystem.Outlet[3].iName", 0, 0, NULL },
	{ "outlet.3.current", 0, 0, "PDU.OutletSystem.Outlet[3].Current", 0, 0, convert_deci },
	{ "outlet.4.desc", 0, 0, "PDU.OutletSystem.Outlet[2].iName", 0, 0, NULL },
	{ "outlet.4.current", 0, 0, "PDU.OutletSystem.Outlet[2].Current", 0, 0, convert_deci },
	{ "outlet.5.desc", 0, 0, "PDU.OutletSystem.Outlet[3].iName", 0, 0, NULL },
	{ "outlet.5.current", 0, 0, "PDU.OutletSystem.Outlet[3].Current", 0, 0, convert_deci },
	{ "outlet.6.desc", 0, 0, "PDU.OutletSystem.Outlet[2].iName", 0, 0, NULL },
	{ "outlet.6.current", 0, 0, "PDU.OutletSystem.Outlet[2].Current", 0, 0, convert_deci },
	{ "outlet.7.desc", 0, 0, "PDU.OutletSystem.Outlet[3].iName", 0, 0, NULL },
	{ "outlet.7.current", 0, 0, "PDU.OutletSystem.Outlet[3].Current", 0, 0, convert_deci },
	{ "outlet.8.desc", 0, 0, "PDU.OutletSystem.Outlet[2].iName", 0, 0, NULL },
	{ "outlet.8.current", 0, 0, "PDU.OutletSystem.Outlet[2].Current", 0, 0, convert_deci },

	{ NULL, 0, 0, NULL, 0, 0, NULL }
};

/* A start-element callback for element with given namespace/name. */
static int mge_xml_startelm_cb(void *userdata, int parent, const char *nspace, const char *name, const char **atts)
{
	int	state = _UNEXPECTED;

	switch(parent)
	{
	case ROOTPARENT:
		if (!strcasecmp(name, "PRODUCT_INFO")) {
			/* name="Network Management Card" type="Mosaic M" version="BA" */
			/* name="Network Management Card" type="Transverse" version="GB (SN 49EH29101)" */
			/* name="Monitored ePDU" type="Monitored ePDU" version="Version Upgrade" */
			int	i;
			for (i = 0; atts[i] && atts[i+1]; i += 2) {
				if (!strcasecmp(atts[i], "name")) {
					snprintf(val, sizeof(val), "%s", atts[i+1]);
				}
				if (!strcasecmp(atts[i], "type")) {
					snprintfcat(val, sizeof(val), "/%s", atts[i+1]);
					if (!strcasecmp(atts[i+1], "Transverse")) {
						mge_ambient_value = 1;
					} else if (strstr(atts[i+1], "ePDU")) {
						dstate_setinfo("device.type", "pdu");
					}
				}
				if (!strcasecmp(atts[i], "version")) {
					char	*s;
					snprintfcat(val, sizeof(val), "/%s", atts[i+1]);
					s = strstr(val, " (SN ");
					if (s) {
						dstate_setinfo("ups.serial", "%s", rtrim(s + 5, ')'));
						s[0] = '\0';
					}
					dstate_setinfo("ups.firmware.aux", "%s", val);
				}
			}
			state = PRODUCT_INFO;
			break;
		}
		if (!strcasecmp(name, "SUMMARY")) {
			state = SUMMARY;
			break;
		}
		if (!strcasecmp(name, "GET_OBJECT")) {
			state = GET_OBJECT;
			break;
		}
		if (!strcasecmp(name, "SET_OBJECT")) {
			state = SET_OBJECT;
			break;
		}
		if (!strcasecmp(name, "ALARM")) {
			int	i;
			var[0] = val[0] = '\0';
			for (i = 0; atts[i] && atts[i+1]; i += 2) {
				if (!strcasecmp(atts[i], "object")) {
					snprintf(var, sizeof(var), "%s", atts[i+1]);
				}
				if (!strcasecmp(atts[i], "value")) {
					snprintf(val, sizeof(var), "%s", atts[i+1]);
				}
				if (!strcasecmp(atts[i], "date")) {
					dstate_setinfo("ups.time", "%s", split_date_time(atts[i+1]));
				}
			}
			state = ALARM;
			break;
		}
		if (!strcasecmp(name, "XML-CLIENT")) {
			state = XML_CLIENT;
			break;
		}
		break;

	case PRODUCT_INFO:
		if (!strcasecmp(name, "SUMMARY")) {
			state = PI_SUMMARY;
			break;
		}

		if (!strcasecmp(name, "ALARMS")) {
			state = PI_ALARMS;
			break;
		}

		if (!strcasecmp(name, "MANAGEMENT")) {
			state = PI_MANAGEMENT;
			break;
		}

		if ( (!strcasecmp(name, "UPS_DATA"))
			|| (!strcasecmp(name, "DEV_DATA")) ) {
			state = PI_UPS_DATA;
			break;
		}
		break;

	case PI_SUMMARY:
		if (!strcasecmp(name, "HTML_PROPERTIES_PAGE")) {
			/* url="mgeups/default.htm" */
			state = PI_HTML_PROPERTIES_PAGE;
			break;
		}
		if (!strcasecmp(name, "XML_SUMMARY_PAGE")) {
			/* url="upsprop.xml" or url="ws/summary.xml" */
			int	i;
			for (i = 0; atts[i] && atts[i+1]; i += 2) {
				if (!strcasecmp(atts[i], "url")) {
					free(mge_xml_subdriver.summary);
					mge_xml_subdriver.summary = strdup(url_convert(atts[i+1]));
				}
			}
			state = PI_XML_SUMMARY_PAGE;
			break;
		}
		if (!strcasecmp(name, "CENTRAL_CFG")) {
			/* url="config.xml" */
			int	i;
			for (i = 0; atts[i] && atts[i+1]; i += 2) {
				if (!strcasecmp(atts[i], "url")) {
					free(mge_xml_subdriver.configure);
					mge_xml_subdriver.configure = strdup(url_convert(atts[i+1]));
				}
			}
			state = PI_CENTRAL_CFG;
			break;
		}
		if (!strcasecmp(name, "CSV_LOGS")) {
			/* url="logevent.csv" dateRange="no" eventFiltering="no" */
			state = PI_CSV_LOGS;
			break;
		}
		break;

	case PI_ALARMS:
		if (!strcasecmp(name, "SUBSCRIPTION")) {
			/* url="subscribe.cgi" security="basic" */
			int	i;
			for (i = 0; atts[i] && atts[i+1]; i += 2) {
				if (!strcasecmp(atts[i], "url")) {
					free(mge_xml_subdriver.subscribe);
					mge_xml_subdriver.subscribe = strdup(url_convert(atts[i+1]));
				}
			}
			state = PI_SUBSCRIPTION;
			break;
		}

		if (!strcasecmp(name, "POLLING")) {
			/* url="mgeups/lastalarms.cgi" security="none" */
			state = PI_POLLING;
			break;
		}
		break;


	case PI_MANAGEMENT:
		if (!strcasecmp(name, "MANAGEMENT_PAGE")) {
			/* name="Manager list" id="ManagerList" url="FS/FLASH0/TrapReceiverList.cfg" security="none" */
			/* name="Shutdown criteria settings" id="Shutdown" url="FS/FLASH0/ShutdownParameters.cfg" security="none" */
			/* name="Network settings" id="Network" url="FS/FLASH0/NetworkSettings.cfg" security="none" */
			/* name="Centralized configuration settings" id="ClientCfg" url="FS/FLASH0/CentralizedConfig.cfg" security="none" */
			state = PI_MANAGEMENT_PAGE;
			break;
		}
		if (!strcasecmp(name, "XML_MANAGEMENT_PAGE")) {
			/* name="Set Card Time" id="SetTime" url="management/set_time.xml" security="none" */
			state = PI_XML_MANAGEMENT_PAGE;
			break;
		}
		break;

	case PI_UPS_DATA:
		if (!strcasecmp(name, "GET_OBJECT")) {
			/* url="getvalue.cgi" security="none" */
			int	i;
			for (i = 0; atts[i] && atts[i+1]; i += 2) {
				if (!strcasecmp(atts[i], "url")) {
					free(mge_xml_subdriver.getobject);
					mge_xml_subdriver.getobject = strdup(url_convert(atts[i+1]));
				}
			}
			state = PI_GET_OBJECT;
			break;
		}
		if (!strcasecmp(name, "SET_OBJECT")) {
			/* url="setvalue.cgi" security="ssl" */
			int	i;
			for (i = 0; atts[i] && atts[i+1]; i += 2) {
				if (!strcasecmp(atts[i], "url")) {
					free(mge_xml_subdriver.setobject);
					mge_xml_subdriver.setobject = strdup(url_convert(atts[i+1]));
				}
			}
			state = PI_SET_OBJECT;
			break;
		}
		break;

	case SUMMARY:
		if (!strcasecmp(name, "OBJECT")) {
			/* name="UPS.PowerSummary.iProduct" */
			int	i;
			for (i = 0; atts[i] && atts[i+1]; i += 2) {
				if (!strcasecmp(atts[i], "name")) {
					snprintf(var, sizeof(var), "%s", atts[i+1]);
					val[0] = '\0';	/*don't inherit something from another object */
				}
			}
			state = SU_OBJECT;
			break;
		}
		break;

	case GET_OBJECT:
		if (!strcasecmp(name, "OBJECT")) {
			/* name="System.RunTimeToEmptyLimit" unit="s" access="RW" */
			int	i;
			for (i = 0; atts[i] && atts[i+1]; i += 2) {
				if (!strcasecmp(atts[i], "name")) {
					snprintf(var, sizeof(var), "%s", atts[i+1]);
					val[0] = '\0';	/*don't inherit something from another object */
				}
				if (!strcasecmp(atts[i], "access")) {
					/* do something with RO/RW access? */
				}
			}
			state = GO_OBJECT;
			break;
		}
		break;

	case XML_CLIENT:
		if (!strcasecmp(name, "GENERAL")) {
			state = XC_GENERAL;
			break;
		}

	case XC_GENERAL:
		if (!strcasecmp(name, "STARTUP")) {
			/* config="CENTRALIZED" */
			state = XC_STARTUP;
			break;
		}
		if (!strcasecmp(name, "SHUTDOWN")) {
			/* shutdownTimer="NONE" shutdownDuration="150" */
			int	i;
			for (i = 0; atts[i] && atts[i+1]; i += 2) {
				if (!strcasecmp(atts[i], "shutdownTimer")) {
					dstate_setinfo("driver.timer.shutdown", "%s", atts[i+1]);
				}
				if (!strcasecmp(atts[i], "shutdownDuration")) {
					dstate_setinfo("driver.delay.shutdown", "%s", atts[i+1]);
				}
			}
			state = XC_SHUTDOWN;
			break;
		}
		if (!strcasecmp(name, "BROADCAST")) {
			/* admins="ON" users="ON" */
			state = XC_BROADCAST;
			break;
		}
	}

	upsdebugx(3, "%s: name <%s> (parent = %d, state = %d)", __func__, name, parent, state);
	return state;
}

/* Character data callback; may return non-zero to abort the parse. */
static int mge_xml_cdata_cb(void *userdata, int state, const char *cdata, size_t len)
{
	/* skip empty lines */
	if ((len == 1) && (cdata[0] == '\n')) {
		upsdebugx(3, "%s: cdata ignored (state = %d)", __func__, state);
		return 0;
	}

	upsdebugx(3, "%s: cdata [%.*s] (state = %d)", __func__, (int)len, cdata, state);

	switch(state)
	{
	case ALARM:
		upsdebugx(2, "ALARM%.*s", (int)len, cdata);
		break;

	case SU_OBJECT:
	case GO_OBJECT:
		snprintfcat(val, sizeof(val), "%.*s", (int)len, cdata);
		break;
	}

	return 0;
}

/* End element callback; may return non-zero to abort the parse. */
static int mge_xml_endelm_cb(void *userdata, int state, const char *nspace, const char *name)
{
	xml_info_t	*info;
	const char	*value;

	/* ignore objects for which no value was set */
	if (strlen(val) == 0) {
		upsdebugx(3, "%s: name </%s> ignored, no value set (state = %d)", __func__, name, state);
		return 0;
	}

	upsdebugx(3, "%s: name </%s> (state = %d)", __func__, name, state);

	switch(state)
	{
	case ALARM:
	case SU_OBJECT:
	case GO_OBJECT:
		for (info = mge_xml2nut; info->xmlname != NULL; info++) {
			if (strcasecmp(var, info->xmlname)) {
				continue;
			}

			upsdebugx(3, "-> XML variable %s [%s] maps to NUT variable %s", var, val, info->nutname);

			if ((info->nutflags & ST_FLAG_STATIC) && dstate_getinfo(info->nutname)) {
				return 0;
			}

			if (info->convert) {
				value = info->convert(val);
			} else {
				value = val;
			}

			if (value != NULL) {
				dstate_setinfo(info->nutname, "%s", value);
			}

			return 0;
		}

		upsdebugx(3, "-> XML variable %s [%s] doesn't map to any NUT variable", var, val);
		break;
	}

	return 0;
}

subdriver_t mge_xml_subdriver = {
	MGE_XML_VERSION,
	MGE_XML_INITUPS,
	MGE_XML_INITINFO,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	mge_xml_startelm_cb,
	mge_xml_cdata_cb,
	mge_xml_endelm_cb,
};
