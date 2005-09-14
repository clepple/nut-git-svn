/* newhidups.c - Driver for serial/USB HID UPS units
 * 
 * Copyright (C)
 *   2003-2005 Arnaud Quette <http://arnaud.quette.free.fr/contact.html>
 *   2005 John Stamp <kinsayder@hotmail.com>
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

/* for regular expressions: */
#include <sys/types.h>
#include <regex.h>

#include "main.h"
#include "libhid.h"
#include "newhidups.h"

/* include all known hid2nut lookup tables */
#include "mge-hid.h"
#include "apc-hid.h"
#include "belkin-hid.h"

/* pointer to the good lookup tables */
static hid_info_t *hid_ups;
static models_name_t *model_names;

/* Global vars */
static HIDDevice *hd;
static int offdelay = DEFAULT_OFFDELAY;
static int ondelay = DEFAULT_ONDELAY;
static int pollfreq = DEFAULT_POLLFREQ;
static int ups_status = 0;
static bool data_has_changed = FALSE; /* for SEMI_STATIC data polling */
static time_t lastpoll; /* Timestamp the last polling */

/* support functions */
static int instcmd(const char *cmdname, const char *extradata);
static int setvar(const char *varname, const char *val);
static hid_info_t *find_nut_info(const char *varname);
static hid_info_t *find_nut_info_valid(const char *varname);
static hid_info_t *find_hid_info(const char *hidname);
static char *hu_find_infoval(info_lkp_t *hid2info, long value);
static char *get_model_name(char *iProduct, char *iModel);
static void process_status_info(char *nutvalue);
static void ups_status_set(void);
static void identify_ups ();
static bool hid_ups_walk(int mode);
static void reconnect_ups(void);

/* ---------------------------------------------
 * driver functions implementations
 * --------------------------------------------- */
void upsdrv_shutdown(void)
{
	char delay[7];

	/* Retrieve user defined delay settings */
	if ( getval(HU_VAR_ONDELAY) )
		ondelay = atoi( getval(HU_VAR_ONDELAY) );

	if ( getval(HU_VAR_OFFDELAY) )
		offdelay = atoi( getval(HU_VAR_OFFDELAY) );

	/* Apply specific method(s) */
	switch (hd->VendorID)
	{
		case APC:
			/* FIXME: the data (or command) should appear in
			 * the hid2nut table, so that it can be autodetected
			 * upon startup, and then calable through setvar()
			 * or instcmd(), ie below
			 */

			/* From apcupsd, usb.c/killpower() */
			/* 1) APCBattCapBeforeStartup */
			/* 2) BackUPS Pro => */
	
			/* Misc method B */
			upsdebugx(2, "upsdrv_shutdown: APC ForceShutdown style shutdown.\n");
			if (instcmd("load.off", NULL) == STAT_INSTCMD_HANDLED) {
				return;
			}
			upsdebugx(2, "ForceShutdown command failed");

			upsdebugx(2, "upsdrv_shutdown: APC Delay style shutdown.\n");
			if (instcmd("shutdown.return", NULL) == STAT_INSTCMD_HANDLED) {
				return;
			}
			upsdebugx(2, "Delayed Shutdown command failed");

		/* Don't "break" as the general method might also be supported! */;
		case MGE_UPS_SYSTEMS:
		default:
			/* 1) set DelayBeforeStartup */
			sprintf(&delay[0], "%i", ondelay);
			if (setvar("ups.delay.start", &delay[0])!= STAT_SET_HANDLED)
				fatalx("Shutoff command failed (setting ondelay)");
			
			/* 2) set DelayBeforeShutdown */
			sprintf(&delay[0], "%i", offdelay);
			if (setvar("ups.delay.shutdown", &delay[0]) == STAT_SET_HANDLED)
				return;
			fatalx("Shutoff command failed (setting offdelay)");
		break;
	}
}

/* process instant command and take action. */
static int instcmd(const char *cmdname, const char *extradata)
{
	hid_info_t *hidups_item;
	
	upsdebugx(2, "entering instcmd(%s, %s)\n", cmdname, extradata);

	/* Retrieve and check netvar & item_path */	
	hidups_item = find_nut_info_valid(cmdname);
	
	/* Check validity of the found the item */
	if (hidups_item == NULL || hidups_item->info_type == NULL ||
		!(hidups_item->hidflags & HU_FLAG_OK))
	{
		upsdebugx(2, "instcmd: info element unavailable %s\n", cmdname);
		/* TODO: manage items handled "manually" */
		return STAT_INSTCMD_UNKNOWN;
	}

	/* Check if the item is an instant command */
	if (!hidups_item->hidflags & HU_TYPE_CMD)
	{
		upsdebugx(2, "instcmd: %s is not an instant command\n", cmdname);
		return STAT_INSTCMD_UNKNOWN;
	}
	
	/* Actual variable setting */
	if (HIDSetItemValue(hidups_item->hidpath, atol(hidups_item->dfl)))
	{
		upsdebugx(3, "SUCCEED\n");
		/* Set the status so that SEMI_STATIC vars are polled */
		data_has_changed = TRUE;
		return STAT_INSTCMD_HANDLED;
	}
	else
		upsdebugx(3, "FAILED\n"); /* TODO: HANDLED but FAILED, not UNKNOWN! */
	
	/* TODO: to be completed */
	return STAT_INSTCMD_UNKNOWN;
}

/* set r/w variable to a value. */
static int setvar(const char *varname, const char *val)
{
	hid_info_t *hidups_item;
	
	upsdebugx(2, "entering setvar(%s, %s)\n", varname, val);
	
	/* 1) retrieve and check netvar & item_path */	
	hidups_item = find_nut_info(varname);
	
	if (hidups_item == NULL || hidups_item->info_type == NULL ||
		!(hidups_item->hidflags & HU_FLAG_OK))
	{
		upsdebugx(2, "setvar: info element unavailable %s\n", varname);
		return STAT_SET_UNKNOWN;
	}

	/* Checking item writability and HID Path */
	if (!hidups_item->info_flags & ST_FLAG_RW)
	{
		upsdebugx(2, "setvar: not writable %s\n", varname);
		return STAT_SET_UNKNOWN;
	}

	/* handle server side variable */
	if (hidups_item->hidflags & HU_FLAG_ABSENT)
	{
		upsdebugx(2, "setvar: setting server side variable %s\n", varname);
		dstate_setinfo(hidups_item->info_type, "%s", val);
		return STAT_SET_HANDLED;
	}
	else
	{
		/* SHUT_FLAG_ABSENT is the only case of HID Path == NULL */
		if (hidups_item->hidpath == NULL)
		{
			upsdebugx(2, "setvar: ID Path is NULL for %s\n", varname);
			return STAT_SET_UNKNOWN;
		}
	}

	/* Actual variable setting */
	if (HIDSetItemValue(hidups_item->hidpath, atol(val)))
	{
		/* FIXME: GetValue(hidups_item->hidpath) to ensure success on non volatile */
		upsdebugx(3, "SUCCEED\n");
		/* Delay a bit not to flood the device */
		sleep(1);
		/* Set the status so that SEMI_STATIC vars are polled */
		data_has_changed = TRUE;
		return STAT_SET_HANDLED;
	}
	else
		upsdebugx(3, "FAILED\n"); /* FIXME: HANDLED but FAILED, not UNKNOWN! */
	
	return STAT_SET_UNKNOWN;
}

void upsdrv_help(void)
{
  /* FIXME: to be completed */
}

void upsdrv_makevartable(void) 
{
	char temp [MAX_STRING_SIZE];
	
	sprintf(temp, "Set shutdown delay, in seconds (default=%d).",
		DEFAULT_OFFDELAY);
	addvar (VAR_VALUE, HU_VAR_OFFDELAY, temp);
	
	sprintf(temp, "Set startup delay, in ten seconds units for MGE (default=%d).",
		DEFAULT_ONDELAY);
	addvar (VAR_VALUE, HU_VAR_ONDELAY, temp);
	
	sprintf(temp, "Set polling frequency, in seconds, to reduce USB flow (default=%i).",
		DEFAULT_POLLFREQ);
	addvar(VAR_VALUE, HU_VAR_POLLFREQ, temp);
	
	/* allow -x vendor=X, vendorid=X, product=X, productid=X, serial=X */
	addvar(VAR_VALUE, "vendor", "Regular expression to match UPS Manufacturer string");
	addvar(VAR_VALUE, "product", "Regular expression to match UPS Product string");
	addvar(VAR_VALUE, "serial", "Regular expression to match UPS Serial number");
	addvar(VAR_VALUE, "vendorid", "Regular expression to match UPS Manufacturer numerical ID (4 digits hexadecimal)");
	addvar(VAR_VALUE, "productid", "Regular expression to match UPS Product numerical ID (4 digits hexadecimal)");
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools: New USB/HID UPS driver %s (%s)\n\n",
		DRIVER_VERSION, UPS_VERSION);
}

void upsdrv_updateinfo(void) 
{
	hid_info_t *item;
	char *nutvalue;
	int retcode, evtCount = 0;
	HIDItem *eventsList[10];

	upsdebugx(1, "upsdrv_updateinfo...");


	/* check for device availability to set datastale! */
	if (hd == NULL)
	  {
		upsdebugx(1, "\n=>Got to reconnect!\n");
		reconnect_ups();
	  }

	/* Only do a full update (polling) every pollfreq 
	 * or upon data change (ie setvar/instcmd) */
	if ( (time(NULL) <= (lastpoll + pollfreq))
		 && (data_has_changed != TRUE) )
	  {
		/* Wait for HID notifications on Interrupt pipe */
		if ((evtCount = HIDGetEvents(NULL, eventsList)) > 0)
		  {
			upsdebugx(1, "\n=>Got %i HID Objects...", evtCount);
			
			/* Process pending events (HID notifications on Interrupt pipe) */
			while (evtCount > 0)
			  {
				/* Check if we are asked to stop (reactivity++) */
				if (exit_flag != 0)
				  return;

				upsdebugx(3, "Object: %s = %ld", 
						  eventsList[evtCount-1]->Path,
						  eventsList[evtCount-1]->Value);
				
				if ((item = find_hid_info(eventsList[evtCount-1]->Path)) != NULL)
				  {
					/* Does it need value lookup? */
					if (item->hid2info != NULL)
					  {
						nutvalue = hu_find_infoval(item->hid2info, (long)eventsList[evtCount-1]->Value);
						if (nutvalue != NULL)
						  {
							upsdebugx(2, "%s = %s", item->info_type,nutvalue);
							
							/* Is it ups.status? */
							if (!strncmp(item->info_type, "ups.status", 10))
							  {
								/* bitwise status to process it globally */
								process_status_info(nutvalue);
								ups_status_set();
							  }
							else
							  dstate_setinfo(item->info_type, item->dfl, nutvalue);
						  }
						/* FIXME: else => revert the status, ie -LB == reset LB... */
					  }
					else
					  upsdebugx(2, "%s = %ld", item->info_type, eventsList[evtCount-1]->Value);
				  }
				free(eventsList[evtCount-1]);
				evtCount--;
			  }
			dstate_dataok();
		  }
		else
		  retcode = evtCount; /* propagate error code */

		/* Quick poll on main ups.status data */
		hid_ups_walk(HU_WALKMODE_QUICK_UPDATE);
	  }
	else /* Polling update */
	  {
		hid_ups_walk(HU_WALKMODE_FULL_UPDATE);
	  }

	/* Reset SEMI_STATIC flag */
	data_has_changed = FALSE;
}


/* Process status info globally to avoid inconsistencies */
static void process_status_info(char *nutvalue)
{
	status_lkp_t *status_item;

	upsdebugx(2, "process_status_info: %s", nutvalue);

	for (status_item = status_info; status_item->status_value != 0 ; status_item++)
	{
		if (!strcasecmp(status_item->status_str, nutvalue))
		{
			switch (status_item->status_value)
			{
				case STATUS_OL: /* clear OB, set OL */
					ups_status &= ~STATUS_OB;
					ups_status |= STATUS_OL;
				break;
				case STATUS_OB: /* clear OL, set OB */
					ups_status &= ~STATUS_OL;
					ups_status |= STATUS_OB;
				break;
				case STATUS_CLEAR_LB: /* clear LB */
					/* Note: the CLEAR flag is processed before the LB flag */
					/* otherwise, we loose LB when it's really set by one var */
					/* and cleared by another */	
					ups_status &= ~STATUS_LB;
				break;
				case STATUS_LB: /* set LB */
					ups_status |= STATUS_LB;
				break;
				case STATUS_CHRG: /* clear DISCHRG, set CHRG */
					ups_status &= ~STATUS_DISCHRG;
					ups_status |= STATUS_CHRG;
				break;
				case STATUS_DISCHRG: /* clear CHRG, set DISCHRG */
					ups_status &= ~STATUS_CHRG;
					ups_status |= STATUS_DISCHRG;
				break;
				/* FIXME: to be checked (+ how to clear) */
				default: /* CAL, TRIM, BOOST, OVER, RB, BYPASS, OFF */
					ups_status |= status_item->status_value;
					/* FIXME: When do we reset these? */
				break;
			}
		}
	}
}

void upsdrv_initinfo(void)
{
	/* identify unit: fill ups.{mfr, model, serial} */
	identify_ups ();

	/* TODO: load lookup file (WARNING: should be in initups()
	because of -k segfault (=> not calling upsdrv_initinfo())
	*/

	/* Device capabilities enumeration */
	hid_ups_walk(HU_WALKMODE_INIT);

	/* install handlers */
	upsh.setvar = setvar;
	upsh.instcmd = instcmd;
}

/* Compile an extended, case insensitive regular expression. On
   success, return an allocated regex_t item. On failure, log an error
   message and return NULL. Returned regex_t must first be freed with
   regfree(), then free(), see regex(3). As a special case, if
   regex==NULL, return NULL (this allows the user to specify no
   regular expression at all). */
static inline regex_t *compile_regex(char *regex) {
	int r;
	regex_t *preg;
	char errbuf[256];

	if (regex == NULL) {
		return NULL;
	}
	preg = (regex_t *)xmalloc(sizeof(regex_t));

	r = regcomp(preg, regex, REG_ICASE | REG_EXTENDED);
	if (r) {
		regerror(r, preg, errbuf, sizeof(errbuf));
		fatalx("Unable to parse regular expression %s: %s\n", regex, errbuf);
		return NULL;
	}
	return preg;
}

void upsdrv_initups(void)
{
	MatchFlags_t match;

        /* parse the UPS selection options */
	match.re_Vendor = compile_regex(getval("vendor"));
	match.re_VendorID = compile_regex(getval("vendorid"));
	match.re_Product = compile_regex(getval("product"));
	match.re_ProductID = compile_regex(getval("productid"));
	match.re_Serial = compile_regex(getval("serial"));
	match.str_Vendor = getval("vendor");
	match.str_VendorID = getval("vendorid");
	match.str_Product = getval("product");
	match.str_ProductID = getval("productid");
	match.str_Serial = getval("serial");

	/* Search for the first supported UPS, no matter Mfr or exact product */
	if ((hd = HIDOpenDevice(device_path, &match, MODE_OPEN)) == NULL)
		fatalx("No matching USB/HID UPS found");
	else
		upslogx(1, "Detected an UPS: %s/%s\n", hd->Vendor, hd->Product);

	/* free the regular expressions */
	if (match.re_Vendor) {
		regfree(match.re_Vendor);
		free(match.re_Vendor);
	}
	if (match.re_VendorID) {
		regfree(match.re_VendorID);
		free(match.re_VendorID);
	}
	if (match.re_Product) {
		regfree(match.re_Product);
		free(match.re_Product);
	}
	if (match.re_ProductID) {
		regfree(match.re_ProductID);
		free(match.re_ProductID);
	}
	if (match.re_Serial) {
		regfree(match.re_Serial);
		free(match.re_Serial);
	}
	
	/* See initinfo for WARNING */
	switch (hd->VendorID)
	{
		case MGE_UPS_SYSTEMS:
			hid_ups = hid_mge;
			model_names = mge_models_names;
      			HIDDumpTree(NULL);
		break;
		case APC:
			hid_ups = hid_apc;
			model_names = apc_models_names;
      			HIDDumpTree(NULL);
		break;
		case BELKIN:
	  	        hid_ups = hid_belkin;
			model_names = belkin_model_names;
			HIDDumpTree(NULL);
		break;
		case MUSTEK:
		case TRIPPLITE:
		case UNITEK:
		default:
			upslogx(1, "Manufacturer not supported!");
			upslogx(1, "Contact the driver author <arnaud.quette@free.fr / @mgeups.com> with the below information");
      			HIDDumpTree(NULL);
			fatalx("Aborting");
		break;
	}

	/* init polling frequency */
	if ( getval(HU_VAR_POLLFREQ) )
		pollfreq = atoi ( getval(HU_VAR_POLLFREQ) );
}

void upsdrv_cleanup(void)
{
	if (hd != NULL)
		HIDCloseDevice(hd);
}

/**********************************************************************
 * Support functions
 *********************************************************************/

void identify_ups ()
{
	char *string;
	char *ptr1, *ptr2, *str;
	char *finalname = NULL;
	float appPower;

	upsdebugx (2, "entering identify_ups(0x%04x, 0x%04x)\n", 
			   hd->VendorID,
			   hd->ProductID);

	switch (hd->VendorID)
	{
		case MGE_UPS_SYSTEMS:
			/* Get iModel and iProduct strings */
			if ((string = HIDGetItemString("UPS.PowerSummary.iModel")) != NULL)
				finalname = get_model_name(hd->Product, string);
			else
			{
				/* Try with ConfigApparentPower */
				if (HIDGetItemValue("UPS.Flow.[4].ConfigApparentPower", &appPower) != 0 )
				{
					string = xmalloc(16);
					sprintf(string, "%i", (int)appPower);
					finalname = get_model_name(hd->Product, string);
					free (string);
				}
				else
				finalname = hd->Product;
			}
		break;
		case APC:
			/* FIXME?: what is the path "UPS.APC_UPS_FirmwareRevision"? */
			str = hd->Product;
			ptr1 = strstr(str, "FW:");
			if (ptr1)
			{
				*(ptr1 - 1) = '\0';
				ptr1 += strlen("FW:");
				ptr2 = strstr(ptr1, "USB FW:");
				if (ptr2)
				{
					*(ptr2 - 1) = '\0';
					ptr2 += strlen("USB FW:");
					dstate_setinfo("ups.firmware.aux", "%s", ptr2);
				}
				dstate_setinfo("ups.firmware", "%s", ptr1);
			}
			finalname = str;
			break;
		case BELKIN:
		        finalname = hd->Product;
			/* trim leading whitespace */
			while (*hd->Vendor == ' ') {
				hd->Vendor++;
			}
		        if (strlen(hd->Vendor) == 0) {
			        hd->Vendor = "Belkin";
			}
			if (strlen(hd->Product) == 0) {
			        finalname = "unknown";
                        }
			break;
		default: /* Nothing to do */
		break;
	}

	/* Actual information setting */
	dstate_setinfo("ups.mfr", "%s", hd->Vendor);
	dstate_setinfo("ups.model", "%s", finalname);
	dstate_setinfo("ups.serial", "%s", (hd->Serial != NULL)?hd->Serial:"unknown");
}

/* walk ups variables and set elements of the info array. */
static bool hid_ups_walk(int mode)
{
	hid_info_t *item;
	float value;
	char *nutvalue;
	int retcode = 0;

	/* 3 modes: HU_WALKMODE_INIT, HU_WALKMODE_QUICK_UPDATE and HU_WALKMODE_FULL_UPDATE */
	
	/* Device data walk ----------------------------- */
	for ( item = hid_ups ; item->info_type != NULL ; item++ )
	  {
		/* Check if we are asked to stop (reactivity++) */
		if (exit_flag != 0)
		  return TRUE;

		/* filter data according to mode */
		switch (mode)
		  {
			/* Device capabilities enumeration */
		  case HU_WALKMODE_INIT:
			{		
			  /* Avoid redundancy when multiple defines (RO/RW)
			   * Not applicable to "ups.status" items! */
			  if ((dstate_getinfo(item->info_type) != NULL)
				  && (strncmp(item->info_type, "ups.status", 10)))
				{
				  item->hidflags &= ~HU_FLAG_OK;
				  continue;
				}

			  /* Check instant commands availability */
			  if (item->hidflags & HU_TYPE_CMD)
				{
				  if (HIDGetItemValue(item->hidpath, &value) == 1 )
					dstate_addcmd(item->info_type);

				  continue;
				}
			  /* Special case for handling server side variables */
			  if (item->hidflags & HU_FLAG_ABSENT)
				{
				  /* Check if exists (if necessary) before creation */
				  if (item->hidpath != NULL)
					{
					  if ((retcode = HIDGetItemValue(item->hidpath, &value)) != 1 )
						continue;
					}
				  else
					{
					  /* Simply set the default value */
					  dstate_setinfo(item->info_type, "%s", item->dfl);
					  dstate_setflags(item->info_type, item->info_flags);
					  continue;
					}
			
				  dstate_setinfo(item->info_type, "%s", item->dfl);
				  dstate_setflags(item->info_type, item->info_flags);

				  /* Set max length for strings, if needed */
				  if (item->info_flags & ST_FLAG_STRING)
					dstate_setaux(item->info_type, item->info_len);
			
				  /* disable reading now 
					 item->shut_flags &= ~SHUT_FLAG_OK;*/
				}
			}
			break; 
		  case HU_WALKMODE_QUICK_UPDATE:
			{
			  /* Quick update only deals with status! */
			  if ( !(item->hidflags & HU_FLAG_QUICK_POLL))
				continue;
			}
			break; 
		  default:
		  case HU_WALKMODE_FULL_UPDATE:
			{
			  /* These doesn't need polling after initinfo() */
			  if ((item->hidflags & HU_FLAG_ABSENT)
				  || (item->hidflags & HU_TYPE_CMD)
				  || (item->hidflags & HU_FLAG_STATIC)
				  /* These need to be polled after user changes (setvar / instcmd) */
				  || ( (item->hidflags & HU_FLAG_SEMI_STATIC) && (data_has_changed == FALSE) ) ) 
				/* FIXME: external condition might change these, ie pushing the HW On/Off button! */
				continue;
			}
			break; 
		  }

		/* Standard variables */
		/* skip elements we shouldn't process / show. */
		if ( ( (mode == HU_WALKMODE_QUICK_UPDATE) || (mode == HU_WALKMODE_FULL_UPDATE) )
			 && !(item->hidflags & HU_FLAG_OK) )
		  continue;

		if ((retcode = HIDGetItemValue(item->hidpath, &value)) == 1 )
		  {
			/* deal with status items */
			if (!strncmp(item->info_type, "ups.status", 10))
			  {
				nutvalue = hu_find_infoval(item->hid2info, (long)value);
				if (nutvalue != NULL)
				  {
					/* bitwise status to process it globally */
					process_status_info(nutvalue);
					ups_status_set();
				  }
			  }
			else /* standard items */
			  {
				/* need lookup'ed translation? */
				if (item->hid2info != NULL)
				  {
					nutvalue = hu_find_infoval(item->hid2info, (long)value);
					if (nutvalue != NULL)
					  dstate_setinfo(item->info_type, item->dfl, nutvalue);
				  }
				else
				  dstate_setinfo(item->info_type, item->dfl, value);
				
				if (mode == HU_WALKMODE_INIT)
				  {
					dstate_setflags(item->info_type, item->info_flags);
					/* Verify variable setability/RW */
					if (get_current_data_attribute() != ATTR_DATA_CST)
						dstate_setflags(item->info_type, ST_FLAG_RW);
				  }
			  }
			if (mode == HU_WALKMODE_INIT)
			  {
				/* Set max length for strings */
				if (item->info_flags & ST_FLAG_STRING)
				  dstate_setaux(item->info_type, item->info_len);
			  }

			/* atomic call */
/* 			dstate_dataok(); */

			/* store timestamp */
/* 			lastpoll = time(NULL); */
		  }
		else
		  {
			if ( (retcode == -EPERM) || (retcode == -EPIPE)
				 || (retcode == -ENODEV) || (retcode == -EACCES) )
			  break;
			else {
			  /* atomic call */
			  dstate_dataok();
			}

			if (mode == HU_WALKMODE_INIT)
			  {
				/* invalidate item */
				item->hidflags &= ~HU_FLAG_OK;
			  }
		  }
	  } /* end for */

	if (mode == HU_WALKMODE_FULL_UPDATE)
	  {
		/* store timestamp */
		lastpoll = time(NULL);
	  }

	/* Reserved values: -1/-10 for nul delay, -2 can't get value */
	/* device has been disconnected, try to reconnect */
	if ( (retcode == -EPERM) || (retcode == -EPIPE)
		 || (retcode == -ENODEV) || (retcode == -EACCES) )
	  {
		hd = NULL;
		reconnect_ups();
	  }
	else {
	  /* atomic call */
	  dstate_dataok();
	}

  return TRUE;
}

static void reconnect_ups(void)
{
  if (hd == NULL)
	{	  
	  upsdebugx(2, "==================================================");
	  upsdebugx(2, "= device has been disconnected, try to reconnect =");
	  upsdebugx(2, "==================================================");
	  
	  /* Not really useful as the device is no more reachable */
	  HIDCloseDevice(NULL);
	  
	  if ((hd = HIDOpenDevice(device_path, NULL, MODE_REOPEN)) == NULL)
		dstate_datastale();
	}
}

/* Process the whole ups.status */
static void ups_status_set(void)
{
	/* clear status buffer before begining */
	status_init();
  
	if (ups_status & STATUS_CAL)
	  status_set("CAL");		/* calibration */
	if (ups_status & STATUS_TRIM)
	  status_set("TRIM");		/* SmartTrim */
	if (ups_status & STATUS_BOOST)
	  status_set("BOOST");	/* SmartBoost */
	if (ups_status & STATUS_OL)
	  status_set("OL");		/* on line */
	if (ups_status & STATUS_OB)
	  status_set("OB");		/* on battery */
	if (ups_status & STATUS_OVER)
	  status_set("OVER");		/* overload */
	if (ups_status & STATUS_LB)
	  status_set("LB");		/* low battery */
	if (ups_status & STATUS_RB)
	  status_set("RB");		/* replace batt */
	if (ups_status & STATUS_BYPASS)
	  status_set("BYPASS");	/* on bypass */   
	if (ups_status & STATUS_CHRG)
	  status_set("CHRG");		/* charging */
	if (ups_status & STATUS_DISCHRG)
	  status_set("DISCHRG");	/* discharging */         
	if ( (ups_status & STATUS_OFF) || (ups_status == 0) )
	  status_set("OFF");
	
	/* Commit the status buffer */
	status_commit();
}

/* find info element definition in info array
 * by NUT varname.
 */
static hid_info_t *find_nut_info(const char *varname)
{
  hid_info_t *hidups_item;

  for (hidups_item = hid_ups; hidups_item->info_type != NULL ; hidups_item++) {
    if (!strcasecmp(hidups_item->info_type, varname))
      return hidups_item;
  }

  upsdebugx(2, "find_nut_info: unknown info type: %s\n", varname);
  return NULL;
}

/* find info element definition in info array by NUT varname. Only
 * return items whose HID path actually exists.  By this, we enable
 * multiple alternative definitions of an instant command; the first
 * one that works for *this* UPS will be used. 
 */
static hid_info_t *find_nut_info_valid(const char *varname)
{
  hid_info_t *hidups_item;
  float value;

  for (hidups_item = hid_ups; hidups_item->info_type != NULL ; hidups_item++) {
    if (!strcasecmp(hidups_item->info_type, varname))
      if (HIDGetItemValue(hidups_item->hidpath, &value) == 1)
	return hidups_item;
  }

  upsdebugx(2, "find_nut_info: unknown info type: %s\n", varname);
  return NULL;
}

/* find info element definition in info array
 * by HID varname.
 */
static hid_info_t *find_hid_info(const char *hidname)
{
  hid_info_t *hidups_item;

  for (hidups_item = hid_ups; hidups_item->info_type != NULL ; hidups_item++) {

	/* Skip NULL HID path (server side vars) */
	if (hidups_item->hidpath == NULL)
	  continue;
	
    if (!strcasecmp(hidups_item->hidpath, hidname))
      return hidups_item;
  }

  upsdebugx(2, "find_hid_info: unknown variable: %s\n", hidname);
  return NULL;
}

/* find the HID Item value matching that NUT value */
/* useful for set with value lookup... */
/* static long hu_find_valinfo(info_lkp_t *hid2info, char* value)
{
  info_lkp_t *info_lkp;
  
  for (info_lkp = hid2info; (info_lkp != NULL) &&
	 (strcmp(info_lkp->nut_value, "NULL")); info_lkp++) {
    
    if (!(strcmp(info_lkp->nut_value, value))) {
      upsdebugx(3, "hu_find_valinfo: found %s (value: %s)\n",
		info_lkp->nut_value, value);
      
      return info_lkp->hid_value;
    }
  }
  upsdebugx(3, "hu_find_valinfo: no matching HID value for this INFO_* value (%s)", value);
  return -1;
} */

/* find the NUT value matching that HID Item value */
static char *hu_find_infoval(info_lkp_t *hid2info, long value)
{
  info_lkp_t *info_lkp;
  char *nut_value;
  
  upsdebugx(3, "hu_find_infoval: searching for value = %ld\n", value);
  
  if (hid2info->fun != NULL) {
    nut_value = hid2info->fun(value);
    upsdebugx(3, "hu_find_infoval: found %s (value: %ld)\n",
	      nut_value, value);
    return nut_value;
  }

  for (info_lkp = hid2info; (info_lkp != NULL) &&
	 (strcmp(info_lkp->nut_value, "NULL")); info_lkp++) {
    
    if (info_lkp->hid_value == value) {
      upsdebugx(3, "hu_find_infoval: found %s (value: %ld)\n",
		info_lkp->nut_value, value);
      
      return info_lkp->nut_value;
    }
  }
  upsdebugx(3, "hu_find_infoval: no matching INFO_* value for this HID value (%ld)\n", value);
  return NULL;
}

/* All the logic for formatting finely the UPS model name */
char *get_model_name(char *iProduct, char *iModel)
{
  models_name_t *model = NULL;

  upsdebugx(2, "get_model_name(%s, %s)\n", iProduct, iModel);

  /* Search for formatting rules */
  for ( model = model_names ; model->iProduct != NULL ; model++ )
	{
	  upsdebugx(2, "comparing with: %s", model->finalname);
	  /* FIXME: use comp_size if not -1 */
	  if ( (!strncmp(iProduct, model->iProduct, strlen(model->iProduct)))
		   && (!strncmp(iModel, model->iModel, strlen(model->iModel))) )
		{
		  upsdebugx(2, "Found %s\n", model->finalname);
		  break;
		}
	}
  /* FIXME: if we end up with model->iProduct == NULL
   * then process name in a generic way (not yet supported models!)
   */
  return model->finalname;
}

