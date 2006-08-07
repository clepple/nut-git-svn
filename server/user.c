/* user.c - user handling functions for upsd

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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"
//#include "parseconf.h"
#include "../lib/libupsconfig.h"

#include "user.h"
#include "user-data.h"
#include "access.h"

	ulist_t	*users = NULL;

	static	ulist_t	*curr_user;

/* create a new user entry */
static void user_add(const char *un)
{
	ulist_t	*tmp, *last;

	if (!un)
		return;
	
	tmp = last = users;

	while (tmp) {
		last = tmp;

		if (!strcmp(tmp->username, un)) {
			fprintf(stderr, "Ignoring duplicate user %s\n", un);
			return;
		}

		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(ulist_t));
	tmp->username = xstrdup(un);
	tmp->firstacl = NULL;
	tmp->password = NULL;
	tmp->firstcmd = NULL;
	tmp->firstaction = NULL;
	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		users = tmp;	

	/* remember who we're working on */
	curr_user = tmp;
}

static acllist *addallow(acllist *base, const char *acl)
{
	acllist	*tmp, *last;

	if (!acl)
		return base;

	tmp = last = base;

	while (tmp) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(struct acl_t));
	tmp->aclname = xstrdup(acl);
	tmp->next = NULL;

	if (last) {
		last->next = tmp;
		return base;
	}

	return tmp;
}

/* attach allowed hosts to user */
static void user_add_allow(const char *host)
{
	if (!curr_user) {
		upslogx(LOG_WARNING, "Ignoring allowfrom definition outside "
			"user section");
		return;
	}

	curr_user->firstacl = addallow(curr_user->firstacl, host);
}

/* set password */
static void user_password(char *pw)
{
	if (!curr_user) {
		upslogx(LOG_WARNING, "Ignoring password definition outside "
			"user section");
		return;
	}

	if (!pw)
		return;

	if (curr_user->password) {
		fprintf(stderr, "Ignoring duplicate password for %s\n", 
			curr_user->username);
		free(pw);
		return;
	}

	curr_user->password = xstrdup(pw);
	free(pw);
}

/* attach allowed instcmds to user */
static void user_add_instcmd(const char *cmd)
{
	instcmdlist	*tmp, *last;

	if (!curr_user) {
		upslogx(LOG_WARNING, "Ignoring instcmd definition outside "
			"user section");
		return;
	}

	if (!cmd)
		return;

	tmp = curr_user->firstcmd;
	last = NULL;

	while (tmp) {
		last = tmp;

		/* ignore duplicates */
		if (!strcasecmp(tmp->cmd, cmd))
			return;

		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(instcmdlist));

	tmp->cmd = xstrdup(cmd);
	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		curr_user->firstcmd = tmp;
}

static actionlist *addaction(actionlist *base, const char *action)
{
	actionlist	*tmp, *last;

	if (!action)
		return base;

	tmp = last = base;

	while (tmp) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(actionlist));
	tmp->action = xstrdup(action);
	tmp->next = NULL;

	if (last) {
		last->next = tmp;
		return base;
	}

	return tmp;
}

/* attach allowed actions to user */
static void user_add_action(const char *act)
{
	if (!curr_user) {
		upslogx(LOG_WARNING, "Ignoring action definition outside "
			"user section");
		return;
	}

	curr_user->firstaction = addaction(curr_user->firstaction, act);
}

static void flushacl(acllist *first)
{
	acllist	*ptr, *next;

	ptr = first;

	while (ptr) {
		next = ptr->next;

		if (ptr->aclname)
			free(ptr->aclname);

		free(ptr);
		ptr = next;
	}
}

static void flushcmd(instcmdlist *first)
{
	instcmdlist	*ptr, *next;

	ptr = first;

	while (ptr) {
		next = ptr->next;

		if (ptr->cmd)
			free(ptr->cmd);
		free(ptr);

		ptr = next;
	}
}

static void flushaction(actionlist *first)
{
	actionlist	*ptr, *next;

	ptr = first;

	while (ptr) {
		next = ptr->next;

		if (ptr->action)
			free(ptr->action);
		free(ptr);

		ptr = next;
	}
}

/* flush all user attributes - used during reload */
void user_flush(void)
{
	ulist_t	*ptr, *next;

	ptr = users;

	while (ptr) {
		next = ptr->next;

		if (ptr->username)
			free(ptr->username);

		if (ptr->firstacl)
			flushacl(ptr->firstacl);

		if (ptr->password)
			free(ptr->password);

		if (ptr->firstcmd)
			flushcmd(ptr->firstcmd);

		if (ptr->firstaction)
			flushaction(ptr->firstaction);

		free(ptr);

		ptr = next;
	}

	users = NULL;
}	

static int user_matchacl(ulist_t *user, const struct sockaddr_in *addr)
{
	acllist	*tmp;

	tmp = user->firstacl;

	/* no acls means no access (fail-safe) */
	if (!tmp)
		return 0;		/* good */

	while (tmp) {
		if (acl_check(tmp->aclname, addr) == 1)
			return 1;	/* good */
	
		tmp = tmp->next;
	}

	return 0;	/* fail */
}

static int user_matchinstcmd(ulist_t *user, const char * cmd)
{
	instcmdlist	*tmp = user->firstcmd;

	/* no commands means no access (fail-safe) */
	if (!tmp)
		return 0;	/* fail */

	while (tmp) {
		if ((!strcasecmp(tmp->cmd, cmd)) || 
			(!strcasecmp(tmp->cmd, "all")))
			return 1;	/* good */
		tmp = tmp->next;
	}

	return 0;	/* fail */
}

int user_checkinstcmd(const struct sockaddr_in *addr, 
	const char *un, const char *pw, const char *cmd)
{
	ulist_t	*tmp = users;

	if ((!un) || (!pw) || (!cmd))
		return 0;	/* failed */

	while (tmp) {

		/* let's be paranoid before we call strcmp */

		if ((!tmp->username) || (!tmp->password)) {
			tmp = tmp->next;
			continue;
		}

		if (!strcmp(tmp->username, un)) {
			if (!strcmp(tmp->password, pw)) {
				if (!user_matchacl(tmp, addr))
					return 0;		/* fail */

				if (!user_matchinstcmd(tmp, cmd))
					return 0;		/* fail */

				/* passed all checks */
				return 1;	/* good */
			}

			/* password mismatch */
			return 0;	/* fail */
		}

		tmp = tmp->next;
	}		

	/* username not found */
	return 0;	/* fail */
}

static int user_matchaction(ulist_t *user, const char *action)
{
	actionlist	*tmp = user->firstaction;

	/* no actions means no access (fail-safe) */
	if (!tmp)
		return 0;	/* fail */

	while (tmp) {
		if (!strcasecmp(tmp->action, action))
			return 1;	/* good */
		tmp = tmp->next;
	}

	return 0;	/* fail */
}

int user_checkaction(const struct sockaddr_in *addr, 
	const char *un, const char *pw, const char *action)
{
	ulist_t	*tmp = users;

	if ((!un) || (!pw) || (!action))
		return 0;	/* failed */

	while (tmp) {

		/* let's be paranoid before we call strcmp */

		if ((!tmp->username) || (!tmp->password)) {
			tmp = tmp->next;
			continue;
		}

		if (!strcmp(tmp->username, un)) {
			if (!strcmp(tmp->password, pw)) {

				if (!user_matchacl(tmp, addr)) {
					upsdebugx(2, "user_matchacl: failed");
					return 0;		/* fail */
				}

				if (!user_matchaction(tmp, action)) {
					upsdebugx(2, "user_matchaction: failed");
					return 0;		/* fail */
				}

				/* passed all checks */
				return 1;	/* good */
			}

			/* password mismatch */
			upsdebugx(2, "user_checkaction: password mismatch");
			return 0;	/* fail */
		}

		tmp = tmp->next;
	}		

	/* username not found */
	return 0;	/* fail */
}

/* handle "upsmon master" and "upsmon slave" for nicer configurations */
static void set_upsmon_type(char *type)
{
	/* master: login, master, fsd */
	if (!strcasecmp(type, "master")) {
		user_add_action("login");
		user_add_action("master");
		user_add_action("fsd");
		return;
	}

	/* slave: just login */
	if (!strcasecmp(type, "slave")) {
		user_add_action("login");
		return;
	}

	upslogx(LOG_WARNING, "Unknown upsmon type %s", type);
}

/* Read the users from the configuration */ 
void read_users(void)
{
	t_enum_string users, first_user;
	t_enum_string list, first_value;
	t_string pw;
	
	/* Get the list of users */
	first_user = users = get_users_list();
	while (users != NULL) {

		/* Set the current user (for libupsconfig) */
		search_user(users->value);
		
		pw = get_password();
		
		if (strlen(pw) == 0 || strcmp(pw, "!") == 0) {
			upslogx(LOG_ERR, "Invalid password for user %s. Ignoring the user", users->value);
			users = users->next_value;
			continue;
		}
		user_add(users->value);
		
		
		user_password(get_password());
		
		/* Add the list of instcmds of the user */
		first_value = list = get_instcmds();
		while (list != NULL) {
			user_add_instcmd(list->value);
			list = list->next_value;
		}
		free_enum_string(first_value);
		
		/* Add the list of actions of the user */
		first_value = list = get_actions();
		while (list != NULL) {
			user_add_action(list->value);
			list = list->next_value;
		}
		free_enum_string(first_value);
		
		/* Add the list of allowfrom of the user */
		first_value = list = get_allowfrom();
		while (list != NULL) {
			user_add_allow(list->value);
			list = list->next_value;
		}
		free_enum_string(first_value);
		
		/* What is the type of the user */
		switch (get_type()) {
			case admin :
				// Add default actions for admin type
				user_add_action("SET");
				// Add defaults instcmds for admin type
				user_add_instcmd("all");
				break;
			case upsmon_master :
				set_upsmon_type("master");
				break;
			case upsmon_slave :
				set_upsmon_type("slave");
				break;
			default : ;
		}
		
		users = users->next_value;
	}
	free_enum_string(first_user);
}

