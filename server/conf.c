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
#include "upsconf.h"
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

/* return 1 if usable, 0 if not */
static int parse_upsd_conf_args(int numargs, char **arg)
{
	/* everything below here uses up through arg[1] */
	if (numargs < 2)
		return 0;

	/* MAXAGE <seconds> */
	if (!strcmp(arg[0], "MAXAGE")) {
		maxage = atoi(arg[1]);
		return 1;
	}

	/* STATEPATH <dir> */
	if (!strcmp(arg[0], "STATEPATH")) {
		if (statepath)
			free(statepath);

		statepath = xstrdup(arg[1]);
		return 1;
	}

	/* DATAPATH <dir> */
	if (!strcmp(arg[0], "DATAPATH")) {
		if (datapath)
			free(datapath);

		datapath = xstrdup(arg[1]);
		return 1;
	}

	/* CERTFILE <dir> */
	if (!strcmp(arg[0], "CERTFILE")) {
		if (certfile)
			free(certfile);

		certfile = xstrdup(arg[1]);
		return 1;
	}

	/* ACCEPT <aclname> [<aclname>...] */
	if (!strcmp(arg[0], "ACCEPT")) {
		access_add(ACCESS_ACCEPT, numargs - 1, 
			(const char **) &arg[1]);
		return 1;
	}

	/* REJECT <aclname> [<aclname>...] */
	if (!strcmp(arg[0], "REJECT")) {
		access_add(ACCESS_REJECT, numargs - 1, 
			(const char **) &arg[1]);
		return 1;
	}

	/* everything below here uses up through arg[2] */
	if (numargs < 3)
		return 0;

	/* ACL <aclname> <ip block> */
	if (!strcmp(arg[0], "ACL")) {
		acl_add(arg[1], arg[2]);
		return 1;
	}

	if (numargs < 4)
		return 0;

	if (!strcmp(arg[0], "ACCESS")) {
		upslogx(LOG_WARNING, "ACCESS in upsd.conf is no longer supported - switch to ACCEPT/REJECT");
		return 1;
	}

	/* not recognized */
	return 0;
}

/* called for fatal errors in parseconf like malloc failures */
static void upsd_conf_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf (upsd.conf): %s", errmsg);
}

static void load_upsdconf(int reloading)
{
	char	fn[SMALLBUF];
	PCONF_CTX	ctx;

	snprintf(fn, sizeof(fn), "%s/upsd.conf", confpath());

	check_perms(fn);

	pconf_init(&ctx, upsd_conf_err);

	if (!pconf_file_begin(&ctx, fn)) {
		pconf_finish(&ctx);

		if (!reloading)
			fatalx("%s", ctx.errmsg);

		upslogx(LOG_ERR, "Reload failed: %s", ctx.errmsg);
		return;
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		if (ctx.numargs < 1)
			continue;

		if (!parse_upsd_conf_args(ctx.numargs, ctx.arglist)) {
			unsigned int	i;
			char	errmsg[SMALLBUF];

			snprintf(errmsg, sizeof(errmsg), 
				"upsd.conf: invalid directive");

			for (i = 0; i < ctx.numargs; i++)
				snprintfcat(errmsg, sizeof(errmsg), " %s", 
					ctx.arglist[i]);

			upslogx(LOG_WARNING, "%s", errmsg);
		}

	}

	pconf_finish(&ctx);		
}

/* callback during parsing of ups.conf */
void do_upsconf_args(char *upsname, char *var, char *val)
{
	ups_t	*tmp, *last;

	/* no "global" stuff for us */
	if (!upsname)
		return;

	last = tmp = upstable;

	while (tmp) {
		last = tmp;

		if (!strcmp(tmp->upsname, upsname)) {
			if (!strcmp(var, "driver")) 
				tmp->driver = xstrdup(val);
			if (!strcmp(var, "port")) 
				tmp->port = xstrdup(val);
			if (!strcmp(var, "desc"))
				tmp->desc = xstrdup(val);
			return;
		}

		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(ups_t));
	tmp->upsname = xstrdup(upsname);
	tmp->driver = NULL;
	tmp->port = NULL;
	tmp->desc = NULL;
	tmp->next = NULL;

	if (!strcmp(var, "driver"))
		tmp->driver = xstrdup(val);
	if (!strcmp(var, "port"))
		tmp->port = xstrdup(val);
	if (!strcmp(var, "desc"))
		tmp->desc = xstrdup(val);

	if (last)
		last->next = tmp;
	else
		upstable = tmp;
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

	upslogx(LOG_INFO, "SIGHUP: reloading configuration");

	/* see if we can access upsd.conf before blowing away the config */
	if (!check_file("upsd.conf"))
		return;

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
	load_upsdconf(1);		/* 1 = reloading */

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

	/* and also make sure upsd.users can be read... */
	if (!check_file("upsd.users"))
		return;

	/* delete all users */
	user_flush();

	/* and finally reread from upsd.users */
	user_load();
}

/* startup: load config files */
void conf_load(void)
{
	/* upsd.conf */
	load_upsdconf(0);	/* 0 = initial */

	/* handle ups.conf */
	read_upsconf();
	upsconf_add(0);

	/* upsd.users */
	user_load();
}
