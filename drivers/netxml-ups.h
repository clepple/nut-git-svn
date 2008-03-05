/* netxml-ups.h	Driver data/defines for network XML UPS units 

   Copyright (C)
	2008		Arjen de Korte <adkorte-guest@alioth.debian.org>

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

#ifndef NETXML_UPS_H
#define NETXML_UPS_H

#include "nut_stdint.h"

struct subdriver_s {
	const char	*version;		/* name of this subdriver */
	char		*initups;
	char		*initinfo;
	char		*getobject;
	char		*setobject;
	int		(*startelm_cb)(void *userdata, int parent, const char *nspace, const char *name, const char **atts);
	int		(*cdata_cb)(void *userdata, int state, const char *cdata, size_t len);
	int		(*endelm_cb)(void *userdata, int state, const char *nspace, const char *name);
};

typedef struct subdriver_s subdriver_t;

/* ---------------------------------------------------------------------- */
/* data for processing boolean values from UPS */

#define STATUS_BIT(x)	(ups_status & (uint32_t)1<<x)
#define STATUS_SET(x)	(ups_status |= (uint32_t)1<<x)
#define STATUS_CLR(x)	(ups_status &= ~((uint32_t)1<<x))

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
	BYPASSAUTO,	/* on automatic bypass */
	BYPASSMAN,	/* on manual/service bypass */
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
	FRANGE,		/* frequency out of range */
	FUSEFAULT	/* fuse fault */
} status_bit_t;

extern uint32_t	ups_status;

#endif /* NETXML_UPS_H */
