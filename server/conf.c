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

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "upsd.h"
#include "conf.h"
#include "sstate.h"
#include "access.h"
#include "user.h"
#include "access.h"

	extern	int	maxage;
	extern	char	*statepath, *datapath, *certfile;
	extern	upstype_t	*firstups;

/* add another UPS for monitoring */
static void ups_create(const char *fn, const char *name, const char *desc)
{
	upstype_t	*temp, *last;

	temp = last = firstups;

	/* find end of linked list */
	while (temp != NULL) {
		last = temp;
		temp = temp->next;
	}

	/* grab some memory and add the info */
	temp = xmalloc(sizeof(upstype_t));

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
}

/* change the configuration of an existing UPS (used during reloads) */
static void ups_update(const char *fn, const char *name, const char *desc)
{
	upstype_t	*temp;

	temp = get_ups_ptr(name);

	if (!temp) {
		upslogx(LOG_ERR, "UPS %s disappeared during reload", name);
		return;
	}

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
		free(statepath);
		statepath = xstrdup(arg[1]);
		return 1;
	}

	/* DATAPATH <dir> */
	if (!strcmp(arg[0], "DATAPATH")) {
		free(datapath);
		datapath = xstrdup(arg[1]);
		return 1;
	}

	/* CERTFILE <dir> */
	if (!strcmp(arg[0], "CERTFILE")) {
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

	/* LISTEN <address> [<port>] */
	if (!strcmp(arg[0], "LISTEN")) {
		if (numargs < 3)
			listen_add(arg[1], string_const(PORT));
		else
			listen_add(arg[1], arg[2]);
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

void load_upsdconf(int reloading)
{
	char	fn[SMALLBUF];
	PCONF_CTX_t	ctx;

	snprintf(fn, sizeof(fn), "%s/upsd.conf", confpath());

	check_perms(fn);

	pconf_init(&ctx, upsd_conf_err);

	if (!pconf_file_begin(&ctx, fn)) {
		pconf_finish(&ctx);

		if (!reloading)
			fatalx(EXIT_FAILURE, "%s", ctx.errmsg);

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

/* add valid UPSes from statepath to the internal structures */
void upsconf_add(int reloading)
{
	DIR		*dirp;
	struct dirent	*dp;
	struct stat	st;
	char		*upsconf, *upsname, buf[SMALLBUF];

	if ((dirp = opendir(".")) == NULL) {
 	       fatal_with_errno(EXIT_FAILURE, "couldn't open '.'");
	}

	/* Loop through directory entries */
	while (1) {
		errno = 0;

		if ((dp = readdir(dirp)) == NULL) {
			break;
		}

		if (stat(dp->d_name, &st) == -1) {
			continue;
		}

		/* If this isn't a socket, go to next entry */
		if (!S_ISSOCK(st.st_mode)) {
			upsdebugx(3, "Skipping %s (not a socket)", dp->d_name);
			continue;
		}

		strncpy(buf, dp->d_name, sizeof(buf));

		upsconf = strtok(buf, "-");
		upsname = strtok(NULL, "-");

		if ((!upsconf) || (!upsname)) {
			upslogx(LOG_ERR, "failed to parse socket [%s]", dp->d_name);
			continue;
		}

		/* if a UPS exists, update it, else add it as new */
		if ((reloading) && (get_ups_ptr(upsname) != NULL)) {
			ups_update(dp->d_name, upsname, upsconf);
		} else {
			ups_create(dp->d_name, upsname, upsconf);
		}
	}

	if (errno != 0) {
        	fatal_with_errno(EXIT_FAILURE, "error reading from statepath");
	}

	closedir(dirp);
}

/* remove a UPS from the linked list */
static void delete_ups(upstype_t *target)
{
	upstype_t	*ptr, *last;

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

			free(ptr->fn);
			free(ptr->name);
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
	upstype_t	*upstmp, *upsnext;

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
