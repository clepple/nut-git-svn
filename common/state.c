/* state.c - Network UPS Tools common state management functions

   Copyright (C) 2003  Russell Kroll <rkroll@exploits.org>

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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "common.h"
#include "state.h"
#include "parseconf.h"

static void val_escape(struct st_tree_t *node)
{
	char	etmp[ST_MAX_VALUE_LEN];

	/* escape any tricky stuff like \ and " */
	pconf_encode(node->val, etmp, sizeof(etmp));

	/* if nothing was escaped, we don't need to do anything else */
	if (!strcmp(node->raw, etmp)) {
		node->val = node->raw;
		return;
	}

	/* first time: set a good starting place */
	if (node->safesize == 0) {
		node->safesize = strlen(etmp) + 1;
		node->safe = xmalloc(node->safesize);
	}

	/* if the escaped value grew, deal with it */
	if (strlen(etmp) > (node->safesize - 1)) {
		node->safesize = strlen(etmp) + 1;
		node->safe = xrealloc(node->safe, node->safesize);
	}

	snprintf(node->safe, node->safesize, "%s", etmp);
	node->val = node->safe;
}

static void st_tree_enum_free(struct enum_t *list)
{
	if (!list)
		return;

	st_tree_enum_free(list->next);

	free(list->val);
	free(list);
}

/* free all memory associated with a node */
static void st_tree_node_free(struct st_tree_t *node)
{
	free(node->var);
	free(node->raw);
	free(node->safe);

	/* never free node->val, since it's just a pointer to raw or safe */

	/* blow away the list of enums */
	st_tree_enum_free(node->enum_list);

	/* now finally kill the node itself */
	free(node);
}

/* add a subtree to another subtree */
static void st_tree_node_add(struct st_tree_t **nptr, struct st_tree_t *sptr)
{
	struct	st_tree_t 	*node = *nptr;

	if (!sptr)
		return;

	if (!node) {
		*nptr = sptr;
		return;
	}

	if (strcmp(sptr->var, node->var) < 0)
		st_tree_node_add(&node->left, sptr);
	else
		st_tree_node_add(&node->right, sptr);
}

/* remove a variable from a tree */
int state_delinfo(struct st_tree_t **nptr, const char *var)
{
	struct	st_tree_t	*node = *nptr;

	if (!node)
		return 0;	/* variable not found! */

	if (strcasecmp(var, node->var) < 0)
		return state_delinfo(&node->left, var);

	if (strcasecmp(var, node->var) > 0)
		return state_delinfo(&node->right, var);

	/* apparently, we've found it! */

	/* whatever is on the left, hang it off current right */
	st_tree_node_add(&node->right, node->left);

	/* now point the parent at the old right child */
	*nptr = node->right;

	st_tree_node_free(node);

	return 1;
}	

/* interface */

int state_setinfo(struct st_tree_t **nptr, const char *var, const char *val)
{
	struct	st_tree_t	*node = *nptr;

	if (!node) {
		node = xmalloc(sizeof(struct st_tree_t));

		node->var = xstrdup(var);

		node->rawsize = strlen(val) + 1;
		node->raw = xmalloc(node->rawsize);
		snprintf(node->raw, node->rawsize, "%s", val);

		/* this is usually sufficient if nothing gets escaped */
		node->val = node->raw;
		node->safesize = 0;
		node->safe = NULL;

		/* but see if it needs escaping anyway */
		val_escape(node);

		/* these are updated by other functions */
		node->flags = 0;
		node->aux = 0;
		node->enum_list = NULL;

		node->left = NULL;
		node->right = NULL;

		/* now we're done with creating a new node, add it to the tree */
		*nptr = node;

		return 1;	/* added */
	}

	if (strcasecmp(var, node->var) < 0)
		return state_setinfo(&node->left, var, val);

	if (strcasecmp(var, node->var) > 0)
		return state_setinfo(&node->right, var, val);

	/* var must equal node->var - updating an existing entry */

	if (!strcasecmp(node->raw, val))
		return 0;		/* no change */

	/* expand the buffer if the value grows */
	if (strlen(val) > (node->rawsize - 1)) {
		node->rawsize = strlen(val) + 1;
		node->raw = xrealloc(node->raw, node->rawsize);
		node->val = node->raw;
	}

	/* store the literal value for later comparisons */
	snprintf(node->raw, node->rawsize, "%s", val);

	val_escape(node);

	return 1;	/* changed */
}

static void st_tree_enum_add(struct enum_t **list, const char *enc)
{
	struct	enum_t	*item = *list;

	if (!item) {	/* end of list reached */
		item = xmalloc(sizeof(struct enum_t));
		item->val = xstrdup(enc);
		item->next = NULL;

		/* now we're done creating it, add it to the list */
		*list = item;

		return;
	}

	/* don't add duplicates - silently ignore them */
	if (!strcmp(item->val, enc))
		return;

	st_tree_enum_add(&item->next, enc);
}

int state_addenum(struct st_tree_t *root, const char *var, const char *val)
{
	struct	st_tree_t	*sttmp;
	char	enc[ST_MAX_VALUE_LEN];

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		upslogx(LOG_ERR, "state_addenum: base variable (%s) "
			"does not exist", var);
		return 0;	/* failed */
	}

	/* smooth over any oddities in the enum value */
	pconf_encode(val, enc, sizeof(enc));

	st_tree_enum_add(&sttmp->enum_list, enc);

	return 1;
}

int state_setaux(struct st_tree_t *root, const char *var, const char *auxs)
{
	struct	st_tree_t	*sttmp;
	int	aux;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		upslogx(LOG_ERR, "state_addenum: base variable (%s) "
			"does not exist", var);
		return -1;	/* failed */
	}

	aux = strtol(auxs, (char **) NULL, 10);

	/* silently ignore matches */
	if (sttmp->aux == aux)
		return 0;

	sttmp->aux = aux;

	return 1;
}

const char *state_getinfo(struct st_tree_t *root, const char *var)
{
	struct	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp)
		return NULL;

	return sttmp->val;
}

int state_getflags(struct st_tree_t *root, const char *var)
{
	struct	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp)
		return -1;

	return sttmp->flags;
}

int state_getaux(struct st_tree_t *root, const char *var)
{
	struct	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp)
		return -1;

	return sttmp->aux;
}

const struct enum_t *state_getenumlist(struct st_tree_t *root, const char *var)
{
	struct	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp)
		return NULL;

	return sttmp->enum_list;
}

void state_setflags(struct st_tree_t *root, const char *var, int numflags,
	char **flag)
{	
	int	i;
	struct	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		upslogx(LOG_ERR, "state_setflags: base variable (%s) "
			"does not exist", var);
		return;
	}

	sttmp->flags = 0;

	for (i = 0; i < numflags; i++) {

		if (!strcasecmp(flag[i], "RW")) {
			sttmp->flags |= ST_FLAG_RW;
			continue;
		}

		if (!strcasecmp(flag[i], "STRING")) {
			sttmp->flags |= ST_FLAG_STRING;
			continue;
		}

		upsdebugx(2, "Unrecognized flag [%s]", flag[i]);
	}
}

int state_addcmd(struct cmdlist_t **list, const char *cmd)
{
	struct cmdlist_t	*item;

	while (*list) {

		if (strcasecmp((*list)->name, cmd) > 0) {
			/* insertion point reached */
			break;
		}

		if (strcasecmp((*list)->name, cmd) < 0) {
			list = &(*list)->next;
			continue;
		}

		return 0;	/* duplicate */
	}

	item = xcalloc(1, sizeof(*item));
	item->name = xstrdup(cmd);
	item->next = *list;

	/* now we're done creating it, insert it in the list */
	*list = item;

	return 1;
}

void state_infofree(struct st_tree_t *node)
{
	if (!node)
		return;

	state_infofree(node->left);

	state_infofree(node->right);

	st_tree_node_free(node);
}

void state_cmdfree(struct cmdlist_t *list)
{
	if (!list)
		return;

	state_cmdfree(list->next);

	free(list->name);
	free(list);
}

int state_delcmd(struct cmdlist_t **list, const char *cmd)
{
	while (*list) {

		struct cmdlist_t	*item = *list;

		if (strcasecmp(item->name, cmd) > 0) {
			/* not found */
			break;
		}

		if (strcasecmp(item->name, cmd) < 0) {
			list = &item->next;
			continue;
		}

		/* we found it! */

		*list = item->next;

		free(item->name);
		free(item);

		return 1;	/* deleted */
	}

	return 0;
}

static int st_tree_del_enum(struct enum_t **list, const char *val)
{
	struct	enum_t	*item = *list;

	if (!item)		/* not found */
		return 0;

	/* if this is not the right value, go on to the next */
	if (strcmp(item->val, val))
		return st_tree_del_enum(&item->next, val);

	/* we found it! */

	*list = item->next;

	free(item->val);
	free(item);

	return 1;	/* deleted */
}

int state_delenum(struct st_tree_t *root, const char *var, const char *val)
{
	struct	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp)
		return 0;

	return st_tree_del_enum(&sttmp->enum_list, val);
}

struct st_tree_t *state_tree_find(struct st_tree_t *node, const char *var)
{
	if (!node)
		return NULL;

	if (strcasecmp(var, node->var) < 0)
		return state_tree_find(node->left, var);

	if (strcasecmp(var, node->var) > 0)
		return state_tree_find(node->right, var);

	return node;
}
