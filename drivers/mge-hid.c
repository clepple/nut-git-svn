/*  mge-hid.c - data to monitor MGE UPS SYSTEMS HID (USB and serial) devices
 *
 *  Copyright (C) 2003 - 2005
 *  			Arnaud Quette <arnaud.quette@mgeups.fr>
 *
 *  Sponsored by MGE UPS SYSTEMS <http://www.mgeups.com>
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "usbhid-ups.h"
#include "mge-hid.h"
#include "extstate.h" /* for ST_FLAG_STRING */
#include "main.h"     /* for getval() */
#include "common.h"

#define MGE_HID_VERSION	"MGE HID 1.01"

#define MGE_VENDORID 0x0463


/* returns statically allocated string - must not use it again before
   done with result! */
static char *mge_battery_voltage_nominal_fun(long value) {
	static char buf[10];
	const char *model;

	model = dstate_getinfo("ups.model");

	/* Work around for Evolution 650 bug(?) */
	if (!strcmp(model, "Evolution 650"))
		value = 12;

	snprintf(buf, sizeof(buf), "%ld", value);
	return buf;
}

static info_lkp_t mge_battery_voltage_nominal[] = {
	{ 0, NULL, mge_battery_voltage_nominal_fun }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *mge_powerfactor_conversion_fun(long value) {
	static char buf[20];

	snprintf(buf, sizeof(buf), "%.2f", (double)value / 100);
	return buf;
}

static info_lkp_t mge_powerfactor_conversion[] = {
	{ 0, NULL, mge_powerfactor_conversion_fun }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *mge_battery_capacity_fun(long value) {
	static char buf[10];

	snprintf(buf, sizeof(buf), "%.2f", (double)value / 3600);
	return buf;
}

static info_lkp_t mge_battery_capacity[] = {
	{ 0, NULL, mge_battery_capacity_fun }
};

static info_lkp_t mge_sensitivity_info[] = {
	{ 0, "normal", NULL },
	{ 1, "high", NULL },
	{ 2, "low", NULL },
	{ 0, NULL, NULL }
};

static info_lkp_t mge_emergency_stop[] = {
	{ 1, "Emergency stop!", NULL },
	{ 0, NULL, NULL }
};


/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* MGE UPS SYSTEMS usage table */
static usage_lkp_t mge_usage_lkp[] = {
	{ "Undefined",				0xffff0000 },
	{ "STS",				0xffff0001 },
	{ "Environment",			0xffff0002 },
	/* 0xffff0003-0xffff000f	=>	Reserved */
	{ "Phase",				0xffff0010 },
	{ "PhaseID",				0xffff0011 },
	{ "Chopper",				0xffff0012 },
	{ "ChopperID",				0xffff0013 },
	{ "Inverter",				0xffff0014 },
	{ "InverterID",				0xffff0015 },
	{ "Rectifier",				0xffff0016 },
	{ "RectifierID",			0xffff0017 },
	{ "LCMSystem",				0xffff0018 },
	{ "LCMSystemID",			0xffff0019 },
	{ "LCMAlarm",				0xffff001a },
	{ "LCMAlarmID",				0xffff001b },
	{ "HistorySystem",			0xffff001c },
	{ "HistorySystemID",			0xffff001d },
	{ "Event",				0xffff001e },
	{ "EventID",				0xffff001f },
	{ "CircuitBreaker",			0xffff0020 },
	{ "TransferForbidden",			0xffff0021 },
	{ "OverallAlarm",			0xffff0022 },
	{ "Dephasing",				0xffff0023 },
	{ "BypassBreaker",			0xffff0024 },
	{ "PowerModule",			0xffff0025 },
	{ "PowerRate",				0xffff0026 },
	{ "PowerSource",			0xffff0027 },
	{ "CurrentPowerSource",			0xffff0028 },
	{ "RedundancyLevel",			0xffff0029 },
	{ "RedundancyLost",			0xffff002a },
	{ "NotificationStatus",			0xffff002b },
	/* 0xffff002c-0xffff003f	=>	Reserved */
	{ "SwitchType",				0xffff0040 },
	{ "ConverterType",			0xffff0041 },
	{ "FrequencyConverterMode",		0xffff0042 },
	{ "AutomaticRestart",			0xffff0043 },
	{ "ForcedReboot",			0xffff0044 },
	{ "TestPeriod",				0xffff0045 },
	{ "EnergySaving",			0xffff0046 },
	{ "StartOnBattery",			0xffff0047 },
	{ "Schedule",				0xffff0048 },
	{ "DeepDischargeProtection",		0xffff0049 },
	{ "ShortCircuit",			0xffff004a },
	{ "ExtendedVoltageMode",		0xffff004b },
	{ "SensitivityMode",			0xffff004c },
	{ "RemainingCapacityLimitSetting",	0xffff004d },
	{ "ExtendedFrequencyMode",		0xffff004e },
	{ "FrequencyConverterModeSetting",	0xffff004f },
	{ "LowVoltageBoostTransfer",		0xffff0050 },
	{ "HighVoltageBoostTransfer",		0xffff0051 },
	{ "LowVoltageBuckTransfer",		0xffff0052 },
	{ "HighVoltageBuckTransfer",		0xffff0053 },
	{ "OverloadTransferEnable",		0xffff0054 },
	{ "OutOfToleranceTransferEnable",	0xffff0055 },
	{ "ForcedTransferEnable",		0xffff0056 },
	{ "LowVoltageBypassTransfer",		0xffff0057 },
	{ "HighVoltageBypassTransfer",		0xffff0058 },
	{ "FrequencyRangeBypassTransfer",	0xffff0059 },
	{ "LowVoltageEcoTransfer",		0xffff005a },
	{ "HighVoltageEcoTransfer",		0xffff005b },
	{ "FrequencyRangeEcoTransfer",		0xffff005c },
	{ "ShutdownTimer",			0xffff005d },
	{ "StartupTimer",			0xffff005e },
	{ "RestartLevel",			0xffff005f },
	{ "PhaseOutOfRange", 			0xffff0060 },
	{ "CurrentLimitation", 			0xffff0061 },
	{ "ThermalOverload", 			0xffff0062 },
	{ "SynchroSource", 			0xffff0063 },
	{ "FuseFault", 				0xffff0064 },
	{ "ExternalProtectedTransfert", 	0xffff0065 },
	{ "ExternalForcedTransfert", 		0xffff0066 },
	{ "Compensation", 			0xffff0067 },
	{ "EmergencyStop", 			0xffff0068 },
	{ "PowerFactor", 			0xffff0069 },
	{ "PeakFactor", 			0xffff006a },
	{ "ChargerType", 			0xffff006b },
	{ "HighPositiveDCBusVoltage", 		0xffff006c },
	{ "LowPositiveDCBusVoltage", 		0xffff006d },
	{ "HighNegativeDCBusVoltage", 		0xffff006e },
	{ "LowNegativeDCBusVoltage", 		0xffff006f },
	{ "FrequencyRangeTransfer", 		0xffff0070 },
	{ "WiringFaultDetection", 		0xffff0071 },
	{ "ControlStandby", 			0xffff0072 },
	{ "ShortCircuitTolerance", 		0xffff0073 },
	{ "VoltageTooHigh", 			0xffff0074 },
	{ "VoltageTooLow", 			0xffff0075 },
	{ "DCBusUnbalanced", 			0xffff0076 },
	{ "FanFailure", 			0xffff0077 },
	{ "WiringFault", 			0xffff0078 },
	/* 0xffff0079-0xffff007f	=>	Reserved */
	{ "Sensor",				0xffff0080 },
	{ "LowHumidity",			0xffff0081 },
	{ "HighHumidity",			0xffff0082 },
	{ "LowTemperature",			0xffff0083 },
	{ "HighTemperature",			0xffff0084 },
	/* 0xffff0085-0xffff008f	=>	Reserved */
	{ "Count",				0xffff0090 },
	{ "Timer",				0xffff0091 },
	{ "Interval",				0xffff0092 },
	{ "TimerExpired",			0xffff0093 },
	{ "Mode",				0xffff0094 },
	{ "Country",				0xffff0095 },
	{ "State",				0xffff0096 },
	{ "Time",				0xffff0097 },
	{ "Code",				0xffff0098 },
	{ "DataValid",				0xffff0099 },
	/* 0xffff009a-0xffff00df	=>	Reserved */
	{ "COPIBridge",				0xffff00e0 },
	/* 0xffff00e1-0xffff00ef	=>	Reserved */
	{ "iModel",				0xffff00f0 },
	{ "iVersion",				0xffff00f1 },
	/* 0xffff00f2-0xffff00ff	=>	Reserved */

	/* end of table */
	{ NULL, 0 }
};

static usage_tables_t mge_utab[] = {
	mge_usage_lkp,
	hid_usage_lkp,
	NULL,
};


/* --------------------------------------------------------------- */
/*      Model Name formating entries                               */
/* --------------------------------------------------------------- */

static models_name_t mge_model_names [] =
{
	/* Ellipse models */
	{ "ELLIPSE", "300", -1, "ellipse 300" },
	{ "ELLIPSE", "500", -1, "ellipse 500" },
	{ "ELLIPSE", "650", -1, "ellipse 650" },
	{ "ELLIPSE", "800", -1, "ellipse 800" },
	{ "ELLIPSE", "1200", -1, "ellipse 1200" },
	/* Ellipse Premium models */
	{ "ellipse", "PR500", -1, "ellipse premium 500" },
	{ "ellipse", "PR650", -1, "ellipse premium 650" },
	{ "ellipse", "PR800", -1, "ellipse premium 800" },
	{ "ellipse", "PR1200", -1, "ellipse premium 1200" },
	/* Ellipse "Pro" */
	{ "ELLIPSE", "600", -1, "Ellipse 600" },
	{ "ELLIPSE", "750", -1, "Ellipse 750" },
	{ "ELLIPSE", "1000", -1, "Ellipse 1000" },
	{ "ELLIPSE", "1500", -1, "Ellipse 1500" },
	/* Ellipse "MAX" */
	{ "Ellipse MAX", "600", -1, "Ellipse MAX 600" },
	{ "Ellipse MAX", "850", -1, "Ellipse MAX 850" },
	{ "Ellipse MAX", "1100", -1, "Ellipse MAX 1100" },
	{ "Ellipse MAX", "1500", -1, "Ellipse MAX 1500" },
	/* Protection Center */
	{ "PROTECTIONCENTER", "420", -1, "Protection Center 420" },
	{ "PROTECTIONCENTER", "500", -1, "Protection Center 500" },
	{ "PROTECTIONCENTER", "675", -1, "Protection Center 675" },
	/* Evolution models */
	{ "Evolution", "500", -1, "Pulsar Evolution 500" },
	{ "Evolution", "800", -1, "Pulsar Evolution 800" },
	{ "Evolution", "1100", -1, "Pulsar Evolution 1100" },
	{ "Evolution", "1500", -1, "Pulsar Evolution 1500" },
	{ "Evolution", "2200", -1, "Pulsar Evolution 2200" },
	{ "Evolution", "3000", -1, "Pulsar Evolution 3000" },
	{ "Evolution", "3000XL", -1, "Pulsar Evolution 3000 XL" },
	/* Newer Evolution models */
	{ "Evolution", "650", -1, "Evolution 650" },
	{ "Evolution", "850", -1, "Evolution 850" },
	{ "Evolution", "1150", -1, "Evolution 1150" },
	{ "Evolution", "S 1250", -1, "Evolution S 1250" },
	{ "Evolution", "1550", -1, "Evolution 1550" },
	{ "Evolution", "S 1750", -1, "Evolution S 1750" },
	{ "Evolution", "2000", -1, "Evolution 2000" },
	{ "Evolution", "S 2500", -1, "Evolution S 2500" },
	{ "Evolution", "S 3000", -1, "Evolution S 3000" },
	/* Pulsar M models */
	{ "PULSAR M", "2200", -1, "Pulsar M 2200" },
	{ "PULSAR M", "3000", -1, "Pulsar M 3000" },
	{ "PULSAR M", "3000 XL", -1, "Pulsar M 3000 XL" },
	/* Pulsar models */
	{ "Pulsar", "700", -1, "Pulsar 700" },
	{ "Pulsar", "1000", -1, "Pulsar 1000" },
	{ "Pulsar", "1500", -1, "Pulsar 1500" },
	{ "Pulsar", "1000 RT2U", -1, "Pulsar 1000 RT2U" },
	{ "Pulsar", "1500 RT2U", -1, "Pulsar 1500 RT2U" },
	/* Pulsar MX models */
	{ "PULSAR", "MX4000", -1, "Pulsar MX 4000 RT" },
	{ "PULSAR", "MX5000", -1, "Pulsar MX 5000 RT" },
	/* NOVA models */	
	{ "NOVA AVR", "600", -1, "NOVA 600 AVR" },
	{ "NOVA AVR", "1100", -1, "NOVA 1100 AVR" },
	/* EXtreme C (EMEA) */
	{ "EXtreme", "700C", -1, "Pulsar EXtreme 700C" },
	{ "EXtreme", "1000C", -1, "Pulsar EXtreme 1000C" },
	{ "EXtreme", "1500C", -1, "Pulsar EXtreme 1500C" },
	{ "EXtreme", "1500CCLA", -1, "Pulsar EXtreme 1500C CLA" },
	{ "EXtreme", "2200C", -1, "Pulsar EXtreme 2200C" },
	{ "EXtreme", "3200C", -1, "Pulsar EXtreme 3200C" },
	/* EXtreme C (USA, aka "EX RT") */
	{ "EX", "700RT", -1, "Pulsar EX 700 RT" },
	{ "EX", "1000RT", -1, "Pulsar EX 1000 RT" },
	{ "EX", "1500RT", -1, "Pulsar EX 1500 RT" },
	{ "EX", "2200RT", -1, "Pulsar EX 2200 RT" },
	{ "EX", "3200RT", -1, "Pulsar EX 3200 RT" },
	/* Comet EX RT three phased */
	{ "EX", "5RT31", -1, "EX 5 RT 3:1" },
	{ "EX", "7RT31", -1, "EX 7 RT 3:1" },
	{ "EX", "11RT31", -1, "EX 11 RT 3:1" },
	/* Comet EX RT mono phased */
	{ "EX", "5RT", -1, "EX 5 RT" },
	{ "EX", "7RT", -1, "EX 7 RT" },
	{ "EX", "11RT", -1, "EX 11 RT" },
	/* Galaxy 3000 */
	{ "GALAXY", "3000_10", -1, "Galaxy 3000 10 kVA" },
	{ "GALAXY", "3000_15", -1, "Galaxy 3000 15 kVA" },
	{ "GALAXY", "3000_20", -1, "Galaxy 3000 20 kVA" },
	{ "GALAXY", "3000_30", -1, "Galaxy 3000 30 kVA" },

	/* FIXME: To be completed (Comet, Galaxy, Esprit, ...) */

	/* end of structure. */
	{ NULL, NULL, -1, "Generic MGE HID model" }
};


/* --------------------------------------------------------------- */
/*                 Data lookup table (HID <-> NUT)                 */
/* --------------------------------------------------------------- */

static hid_info_t mge_hid2nut[] =
{
	/* Server side variables */
	{ "driver.version.internal", 0, 0, NULL, NULL, DRIVER_VERSION, HU_FLAG_ABSENT, NULL },
	{ "driver.version.data", 0, 0, NULL, NULL, MGE_HID_VERSION, HU_FLAG_ABSENT, NULL },

	/* Battery page */
	{ "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
	{ "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerSummary.RemainingCapacityLimitSetting", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "battery.charge.low", 0, 0, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_STATIC , NULL }, /* Read only */
	{ "battery.charge.restart", ST_FLAG_RW | ST_FLAG_STRING, 3, "UPS.PowerSummary.RestartLevel", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "battery.capacity", 0, 0, "UPS.BatterySystem.Battery.DesignCapacity", NULL, "%s", HU_FLAG_STATIC, mge_battery_capacity },	/* conversion needed from As to Ah */
	{ "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
	{ "battery.temperature", 0, 0, "UPS.BatterySystem.Battery.Temperature", NULL, "%.1f", 0, NULL },
	{ "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", HU_FLAG_STATIC, stringid_conversion },
	{ "battery.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.1f", 0, NULL },
	{ "battery.voltage.nominal", 0, 0, "UPS.BatterySystem.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "battery.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%s", HU_FLAG_STATIC, mge_battery_voltage_nominal },
	{ "battery.protection", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.BatterySystem.Battery.DeepDischargeProtection", NULL, "%s", HU_FLAG_SEMI_STATIC, yes_no_info },
	{ "battery.energysave", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Input.[3].EnergySaving", NULL, "%s", HU_FLAG_SEMI_STATIC, yes_no_info },

	/* UPS page */
	{ "ups.firmware", 0, 0, "UPS.PowerSummary.iVersion", NULL, "%s", HU_FLAG_STATIC, stringid_conversion },
	{ "ups.load", 0, 0, "UPS.PowerSummary.PercentLoad", NULL, "%.0f", 0, NULL },
	{ "ups.load.high", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.Flow.[4].ConfigPercentLoad", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "ups.delay.shutdown", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", 0, NULL},
	{ "ups.delay.reboot", 0, 0, "UPS.PowerSummary.DelayBeforeReboot", NULL, "%.0f", 0, NULL},
	{ "ups.delay.start", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, "%.0f", 0, NULL},
	{ "ups.test.result", 0, 0, "UPS.BatterySystem.Battery.Test", NULL, "%s", HU_FLAG_SEMI_STATIC, test_read_info },
	{ "ups.test.interval", ST_FLAG_RW | ST_FLAG_STRING, 8, "UPS.BatterySystem.Battery.TestPeriod", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "ups.beeper.status", 0 ,0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", HU_FLAG_SEMI_STATIC, beeper_info },
	{ "ups.temperature", 0, 0, "UPS.PowerSummary.Temperature", NULL, "%.1f", 0, NULL },
	{ "ups.power", 0, 0, "UPS.PowerConverter.Output.ApparentPower", NULL, "%.0f", 0, NULL },
	{ "ups.power.nominal", 0, 0, "UPS.Flow.[4].ConfigApparentPower", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "ups.realpower", 0, 0, "UPS.PowerConverter.Output.ActivePower", NULL, "%.0f", 0, NULL },
	{ "ups.realpower.nominal", 0, 0, "UPS.Flow.[4].ConfigActivePower", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "ups.start.auto", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Input.[1].AutomaticRestart", NULL, "%s", HU_FLAG_SEMI_STATIC, yes_no_info },
	{ "ups.start.battery", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Input.[3].StartOnBattery", NULL, "%s", HU_FLAG_SEMI_STATIC, yes_no_info },
	{ "ups.start.reboot", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Output.ForcedReboot", NULL, "%s", HU_FLAG_SEMI_STATIC, yes_no_info },

	/* Special case: boolean values that are mapped to ups.status and ups.alarm */
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Overload", NULL, NULL, 0, overload_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, NULL, 0, replacebatt_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.Input.[1].PresentStatus.Buck", NULL, NULL, 0, trim_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.Input.[1].PresentStatus.Boost", NULL, NULL, 0, boost_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.Input.[1].PresentStatus.VoltageOutOfRange", NULL, NULL, 0, vrange_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.Input.[1].PresentStatus.FrequencyOutOfRange", NULL, NULL, 0, frange_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Good", NULL, NULL, 0, off_info },
	/* Manual bypass */
	{ "BOOL", 0, 0, "UPS.PowerConverter.Input.[4].PresentStatus.Used", NULL, NULL, 0, bypass_info },
	/* { "BOOL", 0, 0, "UPS.PowerConverter.Input.[3].PresentStatus.Used", NULL, NULL, 0, onbatt_info }, */
	/* Automatic bypass */
	{ "BOOL", 0, 0, "UPS.PowerConverter.Input.[2].PresentStatus.Used", NULL, NULL, 0, bypass_info },
	/* { "BOOL", 0, 0, "UPS.PowerConverter.Input.[1].PresentStatus.Used", NULL, NULL, 0, online_info }, */
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FanFailure", NULL, NULL, 0, fanfail_info },
	{ "BOOL", 0, 0, "UPS.BatterySystem.Battery.PresentStatus.Present", NULL, NULL, 0, nobattery_info },
	{ "BOOL", 0, 0, "UPS.BatterySystem.Charger.PresentStatus.InternalFailure", NULL, NULL, 0, chargerfail_info },
	{ "BOOL", 0, 0, "UPS.BatterySystem.Charger.PresentStatus.VoltageTooHigh", NULL, NULL, 0, battvolthi_info },
	{ "BOOL", 0, 0, "UPS.BatterySystem.Charger.PresentStatus.VoltageTooLow", NULL, NULL, 0, battvoltlo_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.InternalFailure", NULL, NULL, 0, commfault_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.OverTemperature", NULL, NULL, 0, overheat_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, NULL, 0, shutdownimm_info },

	/* Vendor specific ups.alarm */
	{ "ups.alarm", 0, 0, "UPS.PowerSummary.PresentStatus.EmergencyStop", NULL, NULL, 0, mge_emergency_stop },

	/* Input page */
	{ "input.voltage", 0, 0, "UPS.PowerConverter.Input.[1].Voltage", NULL, "%.1f", 0, NULL },
	{ "input.voltage.nominal", 0, 0, "UPS.Flow.[1].ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.voltage.extended", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Output.ExtendedVoltageMode", NULL, "%s", HU_FLAG_SEMI_STATIC, yes_no_info },
	{ "input.frequency", 0, 0, "UPS.PowerConverter.Input.[1].Frequency", NULL, "%.1f", 0, NULL },
	{ "input.frequency.nominal", 0, 0, "UPS.Flow.[1].ConfigFrequency", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.frequency.extended", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Output.ExtendedFrequencyMode", NULL, "%s", HU_FLAG_SEMI_STATIC, yes_no_info },
	/* same as "input.transfer.boost.low" */
	{ "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Output.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "input.transfer.boost.low", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Output.LowVoltageBoostTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "input.transfer.boost.high", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Output.HighVoltageBoostTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "input.transfer.trim.low", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Output.LowVoltageBuckTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	/* same as "input.transfer.trim.high" */
	{ "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Output.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "input.transfer.trim.high", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Output.HighVoltageBuckTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "input.sensitivity", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerConverter.Output.SensitivityMode", NULL, "%s", HU_FLAG_SEMI_STATIC, mge_sensitivity_info },

	/* Output page */
	{ "output.voltage", 0, 0, "UPS.PowerConverter.Output.Voltage", NULL, "%.1f", 0, NULL },
	{ "output.voltage.nominal", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.Flow.[4].ConfigVoltage", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "output.current", 0, 0, "UPS.PowerConverter.Output.Current", NULL, "%.2f", 0, NULL },
	{ "output.powerfactor", 0, 0, "UPS.PowerConverter.Output.PowerFactor", NULL, "%s", 0, mge_powerfactor_conversion },
	{ "output.frequency", 0, 0, "UPS.PowerConverter.Output.Frequency", NULL, "%.1f", 0, NULL },
	{ "output.frequency.nominal", 0, 0, "UPS.Flow.[4].ConfigFrequency", NULL, "%.0f", HU_FLAG_STATIC, NULL },

	/* Outlet page (using MGE UPS SYSTEMS - PowerShare technology) */
	{ "outlet.0.id", 0, 0, "UPS.OutletSystem.Outlet.[1].OutletID", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "outlet.0.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, "UPS.OutletSystem.Outlet.[1].OutletID", NULL, "Main Outlet",
		HU_FLAG_ABSENT | HU_FLAG_STATIC, NULL },
	{ "outlet.0.switchable", 0, 0, "UPS.OutletSystem.Outlet.[1].PresentStatus.Switchable", NULL, "%s", HU_FLAG_STATIC, yes_no_info },
	{ "outlet.1.id", 0, 0, "UPS.OutletSystem.Outlet.[2].OutletID", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "outlet.1.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, "UPS.OutletSystem.Outlet.[2].OutletID", NULL, "PowerShare Outlet 1", HU_FLAG_ABSENT | HU_FLAG_STATIC, NULL },
	{ "outlet.1.switchable", 0, 0, "UPS.OutletSystem.Outlet.[2].PresentStatus.Switchable", NULL, "%s", HU_FLAG_STATIC, yes_no_info },
	{ "outlet.1.status", 0, 0, "UPS.OutletSystem.Outlet.[2].PresentStatus.SwitchOn/Off", NULL, "%s", 0, on_off_info },
	/* For low end models, with 1 non backup'ed outlet */
	{ "outlet.1.status", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, "%s", 0, on_off_info },
	{ "outlet.1.autoswitch.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 3, "UPS.OutletSystem.Outlet.[2].RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "outlet.1.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.OutletSystem.Outlet.[2].ShutdownTimer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "outlet.1.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.OutletSystem.Outlet.[2].StartupTimer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "outlet.2.id", 0, 0, "UPS.OutletSystem.Outlet.[3].OutletID", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "outlet.2.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, "UPS.OutletSystem.Outlet.[3].OutletID", NULL, "PowerShare Outlet 2", HU_FLAG_ABSENT | HU_FLAG_STATIC, NULL },
	{ "outlet.2.switchable", 0, 0, "UPS.OutletSystem.Outlet.[3].PresentStatus.Switchable", NULL, "%s", HU_FLAG_STATIC, yes_no_info },
	{ "outlet.2.status", 0, 0, "UPS.OutletSystem.Outlet.[3].PresentStatus.SwitchOn/Off", NULL, "%s", 0, on_off_info },
	{ "outlet.2.autoswitch.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 3, "UPS.OutletSystem.Outlet.[3].RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "outlet.2.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.OutletSystem.Outlet.[3].ShutdownTimer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "outlet.2.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.OutletSystem.Outlet.[3].StartupTimer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },

	/* instant commands. */
	/* splited into subset while waiting for extradata support
	* ie: test.battery.start quick
	*/
	{ "test.battery.start.quick", 0, 0, "UPS.BatterySystem.Battery.Test", NULL, "1", HU_TYPE_CMD, NULL },
	{ "test.battery.start.deep", 0, 0, "UPS.BatterySystem.Battery.Test", NULL, "2", HU_TYPE_CMD, NULL },
	{ "test.battery.stop", 0, 0, "UPS.BatterySystem.Battery.Test", NULL, "3", HU_TYPE_CMD, NULL },
	{ "load.off", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "0", HU_TYPE_CMD, NULL },
	{ "load.on", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, "0", HU_TYPE_CMD, NULL },
	{ "shutdown.stayoff", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "20", HU_TYPE_CMD, NULL },
	{ "shutdown.return", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, "30", HU_TYPE_CMD, NULL },
	{ "shutdown.reboot", 0, 0, "UPS.PowerSummary.DelayBeforeReboot", NULL, "10", HU_TYPE_CMD, NULL},
	{ "shutdown.stop", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "-1", HU_TYPE_CMD, NULL },
	{ "beeper.off", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "1", HU_TYPE_CMD, NULL },
	{ "beeper.on", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },
	{ "beeper.mute", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD, NULL },
	{ "beeper.disable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "1", HU_TYPE_CMD, NULL },
	{ "beeper.enable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },

	/* Command for the outlet collection */
	{ "outlet.1.load.off", 0, 0, "UPS.OutletSystem.Outlet.[2].DelayBeforeShutdown", NULL, "0", HU_TYPE_CMD, NULL },
	{ "outlet.1.load.on", 0, 0, "UPS.OutletSystem.Outlet.[2].DelayBeforeStartup", NULL, "0", HU_TYPE_CMD, NULL },
	{ "outlet.2.load.off", 0, 0, "UPS.OutletSystem.Outlet.[3].DelayBeforeShutdown", NULL, "0", HU_TYPE_CMD, NULL },
	{ "outlet.2.load.on", 0, 0, "UPS.OutletSystem.Outlet.[3].DelayBeforeStartup", NULL, "0", HU_TYPE_CMD, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

/* All the logic for finely formatting the MGE model name */
static char *get_model_name(const char *iProduct, char *iModel)
{
	models_name_t *model = NULL;

	upsdebugx(2, "get_model_name(%s, %s)\n", iProduct, iModel);

	/* Search for formatting rules */
	for (model = mge_model_names; model->iProduct; model++)
	{
		upsdebugx(2, "comparing with: %s", model->finalname);

		if (strncmp(iProduct, model->iProduct, strlen(model->iProduct)))
			continue;

		if (strncmp(iModel, model->iModel, strlen(model->iModel)))
			continue;

		upsdebugx(2, "Found %s\n", model->finalname);
		break;
	}

	return model->finalname;
}

static char *mge_format_model(HIDDevice_t *hd) {
	char *product;
	char model[64];
	double value;

	/* Get iProduct and iModel strings */
	product = hd->Product ? hd->Product : "unknown";

	HIDGetItemString(udev, "UPS.PowerSummary.iModel", model, sizeof(model), mge_utab);

	/* Fallback to ConfigApparentPower */
	if ((strlen(model) < 1) && (HIDGetItemValue(udev, "UPS.Flow.[4].ConfigApparentPower", &value, mge_utab) == 1 )) {
		snprintf(model, sizeof(model), "%i", (int)value);
	}

	if (strlen(model) < 1) {
		return product;
	}

	snprintf(model, sizeof(model), "%s", get_model_name(product, model));

	free(hd->Product);
	hd->Product = strdup(model);
	return hd->Product;
}

static char *mge_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "MGE UPS SYSTEMS";
}

static char *mge_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int mge_claim(HIDDevice_t *hd) {
	if (hd->VendorID != MGE_VENDORID) {
		return 0;
	}
	switch (hd->ProductID)
	{
	case  0x0001:
	case  0xffff:
		return 1;  /* accept known UPSs */

	default:
		if (getval("productid")) {
			return 1;
		}
		possibly_supported("MGE", hd);
		return 0;
		
	}
}

subdriver_t hid_subdriver = {
	MGE_HID_VERSION,
	mge_claim,
	mge_utab,
	mge_hid2nut,
	mge_format_model,
	mge_format_mfr,
	mge_format_serial,
};
