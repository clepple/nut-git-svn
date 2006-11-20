/* conf.c - configuration handlers for upsd

   Copyright (C) 2001  Russell Kroll <rkroll@exploits.org>

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

#include "upsd.h"
#include "conf.h"
#include "../lib/libupsconfig.h"
#include "sstate.h"
#include "access.h"
#include "user.h"
#include "access.h"

	extern	int	maxage;
	extern	char	*statepath, *datapath, *certfile;
	extern	upstype	*firstups;
	ups_t	*upstable = NULL;
	int	num_ups = 0;

/* add another UPS for monitoring from ups.conf */
static void ups_create(const char *fn, const char *name, const char *desc)
{
	upstype	*temp, *last;

	temp = last = firstups;

	/* find end of linked list */
	while (temp != NULL) {
		last = temp;

		if (!strcasecmp(temp->name, name)) {
			upslogx(LOG_ERR, "UPS name [%s] is already in use!", 
				name);
			return;
		}

		temp = temp->next;
	}

	/* grab some memory and add the info */
	temp = xmalloc(sizeof(upstype));

	temp->name = xstrdup(name);
	temp->fn = xstrdup(fn);

	if (desc)
		temp->desc = xstrdup(desc);
	else
		temp->desc = NULL;

	temp->stale = 1;

	temp->numlogins = 0;
	temp->fsd = 0;
	temp->retain = 1;
	temp->next = NULL;

	temp->dumpdone = 0;
	temp->data_ok = 0;

	/* preload this to the current time to avoid false staleness */
	time(&temp->last_heard);

	temp->last_ping = 0;
	temp->last_connfail = 0;
	temp->inforoot = NULL;
	temp->cmdlist = NULL;

	if (last == NULL)
		firstups = temp;
	else
		last->next = temp;

	temp->sock_fd = sstate_connect(temp);

	num_ups++;
}

/* change the configuration of an existing UPS (used during reloads) */
static void ups_update(const char *fn, const char *name, const char *desc)
{
	upstype	*temp;

	temp = get_ups_ptr(name);

	if (!temp) {
		upslogx(LOG_ERR, "UPS %s disappeared during reload", name);
		return;
	}

	/* paranoia */
	if (!temp->fn) {
		upslogx(LOG_ERR, "UPS %s had a NULL filename!", name);

		/* let's give it something quick to use later */
		temp->fn = xstrdup("");
	}

	/* when the filename changes, force a reconnect */
	if (strcmp(temp->fn, fn) != 0) {

		upslogx(LOG_NOTICE, "Redefined UPS [%s]", name);

		/* release all data */
		sstate_infofree(temp);
		sstate_cmdfree(temp);
		pconf_finish(&temp->sock_ctx);

		close(temp->sock_fd);
		temp->sock_fd = -1;
		temp->dumpdone = 0;

		/* now redefine the filename and wrap up */
		free(temp->fn);
		temp->fn = xstrdup(fn);
	}

	/* update the description */

	if (temp->desc)
		free(temp->desc);

	if (desc)
		temp->desc = xstrdup(desc);
	else
		temp->desc = NULL;

	/* always set this on reload */
	temp->retain = 1;
}		

/* called for fatal errors in parseconf like malloc failures */
static void upsd_conf_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in nutparser : %s", errmsg);
}

static void read_upsdconf(int reloading)
{
	t_string s;
	t_typed_value value;
	t_enum_string enum_acl, enum_acl_begining;
	
	/* maxage <seconds> */
	if (get_maxage() > 0) {
		maxage = get_maxage();
	}
	
	/* statepath <dir> */
	value = get_variable("nut.upsd.statepath");
	if (value.has_value && value.type == string_type) {
		if (statepath)
			free(statepath);

		statepath = value.value.string_value;
	}
	
	/* datapath <dir> */
	value = get_variable("nut.upsd.datapath");
	if (value.has_value && value.type == string_type) {
		if (datapath)
			free(datapath);

		datapath = value.value.string_value;
	}
	
	/* certfile <dir> */
	value = get_variable("nut.upsd.certfile");
	if (value.has_value && value.type == string_type) {
		if (certfile)
			free(certfile);

		certfile = value.value.string_value;
	}
	
	/* reject { <aclname> [<aclname>...] } */
	enum_acl_begining = enum_acl = get_reject();
	while (enum_acl != NULL) {
		access_append(ACCESS_REJECT, enum_acl->value);
		enum_acl = enum_acl->next_value;
	}
	free_enum_string(enum_acl_begining);
	
	/* accept { <aclname> [<aclname>...] } */
	enum_acl_begining = enum_acl = get_accept();
	while (enum_acl != NULL) {
		access_append(ACCESS_ACCEPT, enum_acl->value);
		enum_acl = enum_acl->next_value;
	}
	free_enum_string(enum_acl_begining);
	
	/* acl.<aclname> = <ip block> */
	enum_acl_begining = enum_acl = get_acl_list();
	while (enum_acl != NULL) {
		s = get_acl_value(enum_acl->value);
		acl_add(enum_acl->value, s);
		free(s);
		enum_acl = enum_acl->next_value;
	}
	free_enum_string(enum_acl);

	
}

void read_upsconf() {
	t_enum_string enum_ups, enum_ups_begining;
	ups_t *ups;
	
	/* Lets make the upstable  */
	enum_ups_begining = enum_ups = get_ups_list();
	
	while (enum_ups != NULL) {
		ups = xmalloc(sizeof(ups_t));
		
		search_ups(enum_ups->value);
		
		ups->upsname = xstrdup(enum_ups->value);
		ups->driver = get_driver();	
		ups->port = get_port();
		ups->desc = get_desc();
		
		ups->next = upstable;
		
		upstable = ups;

		enum_ups = enum_ups->next_value;
	}
	free_enum_string(enum_ups_begining);
	
}


/* add valid UPSes from ups.conf to the internal structures */
static void upsconf_add(int reloading)
{
	ups_t	*tmp = upstable, *next;
	char	statefn[SMALLBUF];

	if (!tmp) {
		upslogx(LOG_WARNING, "Warning: no UPS definitions in ups.conf");
		return;
	}

	while (tmp) {

		/* save for later, since we delete as we go along */
		next = tmp->next;

		/* this should always be set, but better safe than sorry */
		if (!tmp->upsname) {
			tmp = tmp->next;
			continue;
		}

		/* don't accept an entry that's missing items */
		if ((!tmp->driver) || (!tmp->port)) {
			upslogx(LOG_WARNING, "Warning: ignoring incomplete configuration for UPS [%s]\n", 
				tmp->upsname);
		} else {
			snprintf(statefn, sizeof(statefn), "%s",
				tmp->upsname);

			/* if a UPS exists, update it, else add it as new */
			if ((reloading) && (get_ups_ptr(tmp->upsname) != NULL))
				ups_update(statefn, tmp->upsname, tmp->desc);
			else
				ups_create(statefn, tmp->upsname, tmp->desc);
		}

		/* free tmp's resources */

		if (tmp->driver)
			free(tmp->driver);
		if (tmp->port)
			free(tmp->port);
		if (tmp->desc)
			free(tmp->desc);
		if (tmp->upsname)
			free(tmp->upsname);
		free(tmp);

		tmp = next;
	}

	/* upstable should be completely gone by this point */
	upstable = NULL;
}

/* remove a UPS from the linked list */
static void delete_ups(upstype *target)
{
	upstype	*ptr, *last;

	if (!target)
		return;

	ptr = last = firstups;

	while (ptr) {
		if (ptr == target) {
			upslogx(LOG_NOTICE, "Deleting UPS [%s]", target->name);

			/* make sure nobody stays logged into this thing */
			kick_login_clients(target->name);

			/* about to delete the first ups? */
			if (ptr == last)
				firstups = ptr->next;
			else
				last->next = ptr->next;

			if (ptr->sock_fd != -1)
				close(ptr->sock_fd);

			/* release memory */
			sstate_infofree(ptr);
			sstate_cmdfree(ptr);
			pconf_finish(&ptr->sock_ctx);

			if (ptr->fn)
				free(ptr->fn);
			if (ptr->name)
				free(ptr->name);
			if (ptr->desc)
				free(ptr->desc);
			free(ptr);

			return;
		}

		last = ptr;
		ptr = ptr->next;
	}

	/* shouldn't happen */
	upslogx(LOG_ERR, "delete_ups: UPS not found");
}			

/* see if we can open a file */
static int check_file(const char *fn)
{
	char	chkfn[SMALLBUF];
	FILE	*f;

	snprintf(chkfn, sizeof(chkfn), "%s/%s", confpath(), fn);

	f = fopen(chkfn, "r");

	if (!f) {
		upslog_with_errno(LOG_ERR, "Reload failed: can't open %s", chkfn);
		return 0;	/* failed */
	}

	fclose(f);
	return 1;	/* OK */
}

/* called after SIGHUP */
void conf_reload(void)
{
	upstype	*upstmp, *upsnext;
	char fn[SMALLBUF];

	upslogx(LOG_INFO, "SIGHUP: reloading configuration");

	/* see if we can access the configuration file before blowing away the config */
	if (!check_file("nut.conf"))
		return;
	
	snprintf(fn, sizeof(fn), "%s/nut.conf", confpath());
	
	load_config(fn, upsd_conf_err);

	/* reset retain flags on all known UPS entries */
	upstmp = firstups;
	while (upstmp) {
		upstmp->retain = 0;
		upstmp = upstmp->next;
	}

	/* reload from ups.conf */
	read_upsconf();
	upsconf_add(1);			/* 1 = reloading */

	/* flush ACL/ACCESS definitions */
	acl_free();
	access_free();

	/* now reread upsd.conf */
	read_upsdconf(1);		/* 1 = reloading */

	/* now delete all UPS entries that didn't get reloaded */

	upstmp = firstups;

	while (upstmp) {
		/* upstmp may be deleted during this pass */
		upsnext = upstmp->next;

		if (upstmp->retain == 0)
			delete_ups(upstmp);

		upstmp = upsnext;
	}

	/* did they actually delete the last UPS? */
	if (firstups == NULL)
		upslogx(LOG_WARNING, "Warning: no UPSes currently defined!");


	/* delete all users */
	user_flush();

	/* and finally reread from upsd.users */
	read_users();
	
	drop_config();
}

/* startup: load config files */
void conf_load(void)
{
	char fn[SMALLBUF];
	
	snprintf(fn, sizeof(fn), "%s/nut.conf", confpath());
	
	check_perms(fn);
	
	if (!load_config(fn, upsd_conf_err)) {
		exit(EXIT_FAILURE);;
	}
	
	/* upsd.conf */
	read_upsdconf(0);	/* 0 = initial */

	/* handle ups.conf */
	read_upsconf();
	upsconf_add(0);

	/* upsd.users */
	read_users();
	
	drop_config();
}
