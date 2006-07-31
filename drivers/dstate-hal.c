/* dstate-hal.c - Network UPS Tools driver-side state management
   This is a compatibility interface that encapsulate the HAL bridge
   into the NUT dstate API for NUT drivers

   Copyright (C) 2006  Arnaud Quette <aquette.dev@gmail.com>

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
#include <stdarg.h>
#include <sys/stat.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "common.h"
#include "dstate.h"
#include "extstate.h"
/* #include "state.h" */
#include "parseconf.h"

#include <hal/libhal.h>


// 	static	int	sockfd = -1, stale = 1, alarm_active = 0;
// 	static	struct	st_tree_t	*dtree_root = NULL;
// 	static	struct	conn_t	*connhead = NULL;
// 	static	struct	cmdlist_t *cmdhead = NULL;
// 	static	char	*sockfn = NULL;
// 	static	char	status_buf[ST_MAX_VALUE_LEN], 
// 			alarm_buf[ST_MAX_VALUE_LEN];
// 
struct	ups_handler	upsh;

LibHalContext *ctx;
const char *udi;
DBusError error;

/* HAL fdi data
 * newhidups		=> MGE		0x463:0001,ffff
 * newhidups		=> APC		0x051d:
 * newhidups		=> Belkin	0x050d:
 * newhidups		=> CyberPower	0x0764:
 * newhidups		=> TrippLite	0x09ae:0x2005
 * tripplite_usb	=> TrippLite	0x09ae:????? => Charles
 * bcmxcp_usb		=> 
 *ev->descriptor.idVendor == 0x0592 ||
                             dev->descriptor.idVendor == 0x06da) &&
                            dev->descriptor.idProduct == 0x0002)
 */

/* Structure to lookup between NUT and HAL */
typedef struct {
	char	*nut_name;		/* NUT variable name */
	char	*hal_name;		/* HAL variable name */
	int	hal_type;		/* HAL variable type */
	char    *(*fun)(char *value);	/* conversion function. */
} info_lkp_t;

enum hal_type
{
	NONE = 0,
	HAL_TYPE_INT,
	HAL_TYPE_BOOL,
	HAL_TYPE_STRING
};

/* Data to lookup between NUT and HAL for dstate_setinfo() */
static info_lkp_t nut2hal[] =
{
	{ "battery.charge", "battery.charge_level.current", HAL_TYPE_INT, NULL },
	{ "battery.charge", "battery.charge_level.percentage", HAL_TYPE_INT, NULL },
	{ "battery.charge", "battery.reporting.current", HAL_TYPE_INT, NULL },
	{ "battery.charge", "battery.reporting.percentage", HAL_TYPE_INT, NULL },
	{ "battery.runtime", "battery.remaining_time", HAL_TYPE_INT, NULL },
	{ "battery.type", "battery.technology", HAL_TYPE_STRING, NULL },
	{ "ups.mfr", "battery.vendor", HAL_TYPE_STRING, NULL },
	{ "ups.model", "battery.model", HAL_TYPE_STRING, NULL },
	{ "ups.serial", "battery.serial", HAL_TYPE_STRING, NULL },
	/* Terminating element */
	{ NULL, NULL, NONE, NULL }
};

/* Functions to lookup between NUT and HAL */
static info_lkp_t *find_nut_info(const char *nut_varname, info_lkp_t *prev_info_item);

int convert_to_int(char *value)
{
	int intValue = atoi(value);
	return intValue;
}

#if 0
  // FIXME: to be processed (need possible NUT extension)
	case FIXME: not present in NUT "battery.present":
		libhal_device_set_property_bool (
			ctx, udi, "battery.present", uref.value != 0, &error);
		break;

	case ???:
		libhal_device_set_property_string (
			ctx, udi, "foo", 
			ups_get_string (fd, uref.value), &error);
		break;

	case UPS_RECHARGEABLE:
		libhal_device_set_property_bool (
			ctx, udi, "battery.is_rechargeable", uref.value != 0, &error);
		break;

	case "???":
		libhal_device_set_property_int (
			ctx, udi, "battery.charge_level.design", uref.value, &error);
		libhal_device_set_property_int (
			ctx, udi, "battery.charge_level.last_full", uref.value, &error);
		libhal_device_set_property_int (
			ctx, udi, "battery.reporting.design", uref.value, &error);
		libhal_device_set_property_int (
			ctx, udi, "battery.reporting.last_full", uref.value, &error);
		break;

	default:
						break;
#endif


/********************************************************************
 * dstate compatibility interface
 *******************************************************************/
void dstate_init(const char *prog, const char *port)
{
	dbus_error_init (&error);

	/* UPS always report charge as percent */
	libhal_device_set_property_string (
			ctx, udi, "battery.charge_level.unit", "percent", &error);
	libhal_device_set_property_string (
			ctx, udi, "battery.reporting.unit", "percent", &error);

        /* UPS are always rechargeable! */
        /* FIXME: Check for NUT extension however: UPS.PowerSummary.Rechargeable
         * into battery.rechargeable
         */
        libhal_device_set_property_bool (
            ctx, udi, "battery.is_rechargeable", TRUE, &error);

	/* Set generic properties */
	libhal_device_set_property_bool (ctx, udi, "battery.present", TRUE, &error);
	libhal_device_set_property_string (ctx, udi, "battery.type", "ups", &error);
	libhal_device_add_capability (ctx, udi, "battery", &error);
}


const char *dstate_getinfo(const char *var)
{
	return NULL;
}

int dstate_setinfo(const char *var, const char *fmt, ...)
{
	int	ret = 1;
	char	value[ST_MAX_VALUE_LEN];
	va_list	ap;
	info_lkp_t *nut2hal_info = NULL, *prev_nut2hal_info = NULL;

	va_start(ap, fmt);
	vsnprintf(value, sizeof(value), fmt, ap);
	va_end(ap);

	/* Loop on getting HAL variable(s) matching this NUT variable */
	while ( (nut2hal_info = find_nut_info(var, prev_nut2hal_info)) != NULL)
	{
		switch (nut2hal_info->hal_type)
		{
			case HAL_TYPE_INT:
				libhal_device_set_property_int (ctx, udi, nut2hal_info->hal_name,
						atoi(value), &error);
				break;
			case HAL_TYPE_BOOL:
				libhal_device_set_property_bool (ctx, udi, nut2hal_info->hal_name, TRUE, &error);
				break;
			case HAL_TYPE_STRING:
				libhal_device_set_property_string (ctx, udi, nut2hal_info->hal_name, value, &error);
				break;
		}
		prev_nut2hal_info = nut2hal_info;
	}

	return ret;
}


void dstate_setflags(const char *var, int flags)
{
	return;
}

void dstate_setaux(const char *var, int aux)
{
	return;
}

void dstate_dataok(void)
{
	return;
}

void dstate_datastale(void)
{
	return;
}

void dstate_free(void)
{
	return;
}

/* extrafd: provided for waking up based on the driver's UPS fd */
int dstate_poll_fds(int interval, int extrafd)
{
	return 0;
}	

/* clean out the temp space for a new pass */
void status_init(void)
{
	/* Nothing to do */
	return;
}

/* ups.status element conversion */
void status_set(const char *buf)
{
	upsdebugx(2, "status_set: %s\n", buf);

	if ( (strcmp(buf, "DISCHRG") == 0) || (strcmp(buf, "OB") == 0) )
	{
		/* Set AC present status */
		libhal_device_set_property_bool (ctx, udi,
				"ac_adaptor.present", FALSE, &error);

		/* Set discharging status */
		libhal_device_set_property_bool (ctx, udi,
				"battery.rechargeable.is_discharging", TRUE, &error);
		
		/* Set charging status */
		libhal_device_set_property_bool (ctx, udi,
				"battery.rechargeable.is_charging", FALSE, &error);
	}
	else if ( (strcmp(buf, "CHRG") == 0) || (strcmp(buf, "OL") == 0) )
	{
		/* Set AC present status */
		libhal_device_set_property_bool (ctx, udi,
				"ac_adaptor.present", TRUE, &error);

		/* Set charging status */
		libhal_device_set_property_bool (ctx, udi,
				"battery.rechargeable.is_charging", TRUE, &error);

		/* Set discharging status */
		libhal_device_set_property_bool (ctx, udi,
				"battery.rechargeable.is_discharging", FALSE, &error);
	}
	else 
		upsdebugx(2, "status_set: dropping status %s (not managed)\n", buf);

	return;
}

/* write the status_buf into the externally visible dstate storage */
void status_commit(void)
{
	/* Nothing to do */
	return;
}

void dstate_addcmd(const char *cmdname)
{
	return;
}

/********************************************************************
 * internal functions
 *******************************************************************/

/* find the next info element definition in info array
 * that matches nut_varname, and that is after prev_info_item
 * if specified.
 * Note that 1 nut item can matches several HAL items
 */
static info_lkp_t *find_nut_info(const char *nut_varname, info_lkp_t *prev_info_item)
{
	info_lkp_t *info_item;

	upsdebugx(2, "find_nut_info: looking up => %s\n", nut_varname);
	
	if (prev_info_item != NULL) {
		/* Start from the item following prev_info_item */
		info_item = prev_info_item;
		info_item++;
	}
	else {
		info_item = nut2hal;
	}

	for ( ; info_item->nut_name != NULL ; info_item++) {

		if (!strcasecmp(info_item->nut_name, nut_varname))
			return info_item;
	}

	return NULL;
}
