/* dstate.c - Network UPS Tools driver-side state management

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
#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "common.h"
#include "dstate.h"
#include "state.h"
#include "parseconf.h"

	static	int	sockfd = -1, stale = 1, alarm_active = 0;
	static	struct	st_tree_t	*dtree_root = NULL;
	static	struct	conn_t	*connhead = NULL;
	static	struct	cmdlist_t *cmdhead = NULL;
	static	char	*sockfn = NULL;
	static	char	status_buf[ST_MAX_VALUE_LEN],
			alarm_buf[ST_MAX_VALUE_LEN];

	struct	ups_handler	upsh;

/* this may be a frequent stumbling point for new users, so be verbose here */
static void sock_fail(const char *fn)
{
	int	sockerr;
	struct	passwd	*user;

	/* save this so it doesn't get overwritten */
	sockerr = errno;

	/* dispense with the usual upslog stuff since we have stderr here */

	printf("\nFatal error: unable to create listener socket\n\n");
	printf("bind %s failed: %s\n", fn, strerror(sockerr));

	user = getpwuid(getuid());

	if (!user)
		fatal_with_errno(EXIT_FAILURE, "getpwuid");

	/* deal with some common problems */
	switch (errno) {
		case EACCES:
			printf("\nCurrent user: %s (UID %d)\n\n",
				user->pw_name, (int)user->pw_uid);

			printf("Things to try:\n\n");
			printf(" - set different owners or permissions on %s\n\n", 
				dflt_statepath());
			printf(" - run this as some other user "
				"(try -u <username>)\n");
			break;

		case ENOENT:
			printf("\nThings to try:\n\n");
			printf(" - mkdir %s\n", dflt_statepath());
			break;

		case ENOTDIR:
			printf("\nThings to try:\n\n");
			printf(" - rm %s\n\n", dflt_statepath());
			printf(" - mkdir %s\n", dflt_statepath());
			break;
	}
	
	/*
	 * there - that wasn't so bad.  every helpful line of code here 
	 * prevents one more "help me" mail to the list a year from now
	 */

	printf("\n");
	fatalx(EXIT_FAILURE, "Exiting.");
}

static int open_sock(const char *fn)
{
	int	ret, fd;
	struct	sockaddr_un	ssaddr;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd < 0)
		fatal_with_errno(EXIT_FAILURE, "Can't create a unix domain socket");

	/* keep this around for the unlink() when exiting */
	sockfn = xstrdup(fn);

	ssaddr.sun_family = AF_UNIX;
	snprintf(ssaddr.sun_path, sizeof(ssaddr.sun_path), "%s", sockfn);

	unlink(sockfn);

	/* group gets access so upsd can be a different user but same group */
	umask(0007);

	ret = bind(fd, (struct sockaddr *) &ssaddr, sizeof ssaddr);

	if (ret < 0)
		sock_fail(sockfn);

	ret = chmod(sockfn, 0660);

	if (ret < 0)
		fatal_with_errno(EXIT_FAILURE, "chmod(%s, 0660) failed", sockfn);

	ret = listen(fd, DS_LISTEN_BACKLOG);

	if (ret < 0)
		fatal_with_errno(EXIT_FAILURE, "listen(%d, %d) failed", fd, DS_LISTEN_BACKLOG);

	return fd;
}

static void conn_del(struct conn_t *target)
{
	struct	conn_t	*tmp, *last = NULL;

	tmp = connhead;

	while (tmp) {
		if (tmp == target) {

			if (last)
				last->next = tmp->next;
			else
				connhead = tmp->next;

			pconf_finish(&tmp->ctx);

			free(tmp);
			return;
		}

		last = tmp;
		tmp = tmp->next;
	}

	upslogx(LOG_ERR, "Tried to delete a bogus state connection");
}

static void send_to_all(const char *fmt, ...)
{
	int	ret;
	va_list	ap;
	char	buf[ST_SOCK_BUF_LEN];
	struct	conn_t	*tmp, *tmpnext;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	tmp = connhead;

	while (tmp) {
		tmpnext = tmp->next;

		ret = write(tmp->fd, buf, strlen(buf));

		if ((ret < 1) || (ret != (int) strlen(buf))) {
			upsdebugx(2, "write %d bytes to socket %d failed",
				strlen(buf), tmp->fd);

			close(tmp->fd);
			conn_del(tmp);
		}

		tmp = tmpnext;
	}
}

static int send_to_one(struct conn_t *conn, const char *fmt, ...)
{
	int	ret;
	va_list	ap;
	char	buf[ST_SOCK_BUF_LEN];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	ret = write(conn->fd, buf, strlen(buf));

	if ((ret < 1) || (ret != (int) strlen(buf))) {
		upsdebugx(2, "write to fd %d failed", conn->fd);

		close(conn->fd);
		conn_del(conn);

		return 0;	/* failed */
	}

	return 1;	/* OK */
}

static void conn_add(int fd)
{
	int	acc, ret;
	struct	conn_t	*tmp, *last;
	struct	sockaddr_un sa;
	socklen_t	salen;

	salen = sizeof(sa);
	acc = accept(fd, (struct sockaddr *) &sa, &salen);

	if (acc < 0) {
		upslog_with_errno(LOG_ERR, "accept on unix fd failed");
		return;
	}

	/* enable nonblocking I/O */

	ret = fcntl(acc, F_GETFL, 0);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "fcntl get on unix fd failed");
		close(acc);
		return;
	}

	ret = fcntl(acc, F_SETFL, ret | O_NDELAY);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "fcntl set O_NDELAY on unix fd failed");
		close(acc);
		return;
	}	

	tmp = last = connhead;

	while (tmp) {
		last = tmp;	
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(struct conn_t));
	tmp->fd = acc;
	tmp->next = NULL;

	pconf_init(&tmp->ctx, NULL);

	if (last)
		last->next = tmp;
	else
		connhead = tmp;

	upsdebugx(3, "new connection on fd %d", acc);
}

static int st_tree_dump_conn(struct st_tree_t *node, struct conn_t *conn)
{
	int	ret;
	struct	enum_t	*etmp;

	if (!node)
		return 1;	/* not an error */

	if (node->left) {
		ret = st_tree_dump_conn(node->left, conn);

		if (!ret)
			return 0;	/* write failed in the child */
	}

	if (!send_to_one(conn, "SETINFO %s \"%s\"\n", node->var, node->val))
		return 0;	/* write failed, bail out */

	/* send any enums */
	for (etmp = node->enum_list; etmp != NULL; etmp = etmp->next)
		if (!send_to_one(conn, "ADDENUM %s \"%s\"\n",
			node->var, etmp->val))
			return 0;

	/* provide any auxiliary data */
	if (node->aux != 0) {
		if (!send_to_one(conn, "SETAUX %s %d\n", node->var, node->aux))
			return 0;
	}

	/* finally report any flags */
	if (node->flags != 0) {
		char	flist[SMALLBUF];

		/* build the list */
		snprintf(flist, sizeof(flist), "%s", node->var);

		if (node->flags & ST_FLAG_RW)
			snprintfcat(flist, sizeof(flist), " RW");
		if (node->flags & ST_FLAG_STRING)
			snprintfcat(flist, sizeof(flist), " STRING");

		send_to_one(conn, "SETFLAGS %s\n", flist);
	}

	if (node->right)
		return st_tree_dump_conn(node->right, conn);

	return 1;	/* everything's OK here ... */
}

static int cmd_dump_conn(struct conn_t *conn)
{
	struct	cmdlist_t	*tmp;

	for (tmp = cmdhead; tmp != NULL; tmp = tmp->next) {
		if (!send_to_one(conn, "ADDCMD %s\n", tmp->name))
			return 0;
	}

	return 1;
}

static int sock_arg(struct conn_t *conn, int numarg, char **arg)
{
	if (numarg < 1)
		return 0;

	if (!strcasecmp(arg[0], "DUMPALL")) {

		/* first thing: the staleness flag */
		if (stale == 1)
			if (!send_to_one(conn, "DATASTALE\n"))
				return 1;

		if (!st_tree_dump_conn(dtree_root, conn))
			return 1;

		if (!cmd_dump_conn(conn))
			return 1;

		if (stale == 0)
			if (!send_to_one(conn, "DATAOK\n"))
				return 1;

		send_to_one(conn, "DUMPDONE\n");
		return 1;
	}

	if (!strcasecmp(arg[0], "PING")) {
		send_to_one(conn, "PONG\n");
		return 1;
	}

	if (numarg < 2)
		return 0;

	/* INSTCMD <cmdname> */			/* future: extra args */
	if (!strcasecmp(arg[0], "INSTCMD")) {

		/* try the new handler first if present */
		if (upsh.instcmd) {
			upsh.instcmd(arg[1], NULL);
			return 1;
		}

		upslogx(LOG_NOTICE, "Got INSTCMD, but driver lacks a handler");
		return 1;
	}

	if (numarg < 3)
		return 0;

	/* SET <var> <value> */
	if (!strcasecmp(arg[0], "SET")) {

		/* try the new handler first if present */
		if (upsh.setvar) {
			upsh.setvar(arg[1], arg[2]);
			return 1;
		}

		upslogx(LOG_NOTICE, "Got SET, but driver lacks a handler");
		return 1;
	}

	/* unknown */
	return 0;
}

static void log_unknown(int numarg, char **arg)
{
	int	i;

	upslogx(LOG_INFO, "Unknown command on socket: ");

	for (i = 0; i < numarg; i++)
		upslogx(LOG_INFO, "arg %d: %s", i, arg[i]);
}

static int sock_read(struct conn_t *conn)
{
	int	i, ret;
	char	ch;

	for (i = 0; i < DS_MAX_READ; i++) {

		ret = read(conn->fd, &ch, 1);

		if (ret < 1) {

			/* short read = no parsing, come back later */
			if ((ret == -1) && (errno == EAGAIN))
				return 0;

			/* some other problem */
			return -1;	/* error */
		}

		ret = pconf_char(&conn->ctx, ch);

		if (ret == 0)		/* nothing to parse yet */
			continue;

		if (ret == -1) {
			upslogx(LOG_NOTICE, "Parse error on sock: %s",
				conn->ctx.errmsg);

			return 0;	/* nothing parsed */
		}

		/* try to use it, and complain about unknown commands */
		if (!sock_arg(conn, conn->ctx.numargs, conn->ctx.arglist))
			log_unknown(conn->ctx.numargs, conn->ctx.arglist);

		return 1;	/* we did some work */
	}

	return 0;	/* fell out without parsing anything */
}

static void conn_close_all(void)
{
	struct	conn_t	*tmp, *next;

	tmp = connhead;

	while (tmp) {
		next = tmp->next;

		close(tmp->fd);
		free(tmp);

		tmp = next;
	}
}		

/* interface */

void dstate_init(const char *prog, const char *port)
{
	char	sockname[SMALLBUF];

	/* do this here for now */
	signal(SIGPIPE, SIG_IGN);

	if (port != NULL)
		snprintf(sockname, sizeof(sockname), "%s/%s-%s",
			dflt_statepath(), prog, port);
	else
		snprintf(sockname, sizeof(sockname), "%s/%s",
			dflt_statepath(), prog);

	sockfd = open_sock(sockname);

	upsdebugx(2, "dstate_init: sock %s open on fd %d", sockname, sockfd);
}

/* extrafd: provided for waking up based on the driver's UPS fd */
int dstate_poll_fds(int interval, int extrafd)
{
	int	ret, maxfd;
	fd_set	rfds;
	struct	timeval tv;
	struct	conn_t	*tmp, *tmpnext;

	tv.tv_sec = interval;
	tv.tv_usec = 0;

	FD_ZERO(&rfds);
	FD_SET(sockfd, &rfds);

	maxfd = sockfd;

	if (extrafd != -1) {
		FD_SET(extrafd, &rfds);

		if (extrafd > maxfd)
			maxfd = extrafd;
	}

	for (tmp = connhead; tmp != NULL; tmp = tmp->next) {
		FD_SET(tmp->fd, &rfds);

		if (tmp->fd > maxfd)
			maxfd = tmp->fd;
	}

	ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);

	if (ret == 0)
		return 0;

	if (ret < 0) {
		/* ignore interruptions from signals */
		if (errno != EINTR)
			upslog_with_errno(LOG_ERR, "select unix sockets failed");
		return 0;
	}

	if (FD_ISSET(sockfd, &rfds))
		conn_add(sockfd);

	tmp = connhead;

	while (tmp) {
		tmpnext = tmp->next;

		if (FD_ISSET(tmp->fd, &rfds)) {
			if (sock_read(tmp) < 0) {
				close(tmp->fd);
				conn_del(tmp);
			}
		}

		tmp = tmpnext;
	}

	/* tell the caller if that fd woke up */
	if ((extrafd != -1) && (FD_ISSET(extrafd, &rfds)))
		return 1;

	return 0;
}	

int dstate_setinfo(const char *var, const char *fmt, ...)
{
	int	ret;
	char	value[ST_MAX_VALUE_LEN];
	va_list	ap;

	va_start(ap, fmt);
	vsnprintf(value, sizeof(value), fmt, ap);
	va_end(ap);

	ret = state_setinfo(&dtree_root, var, value);

	if (ret == 1)
		send_to_all("SETINFO %s \"%s\"\n", var, value);

	return ret;
}

int dstate_addenum(const char *var, const char *fmt, ...)
{
	int	ret;
	char	value[ST_MAX_VALUE_LEN];
	va_list	ap;

	va_start(ap, fmt);
	vsnprintf(value, sizeof(value), fmt, ap);
	va_end(ap);

	ret = state_addenum(dtree_root, var, value);

	if (ret == 1)
		send_to_all("ADDENUM %s \"%s\"\n", var, value);

	return ret;
}

void dstate_setflags(const char *var, int flags)
{
	struct	st_tree_t	*sttmp;
	char	flist[SMALLBUF];

	/* find the dtree node for var */
	sttmp = state_tree_find(dtree_root, var);

	if (!sttmp) {
		upslogx(LOG_ERR, "dstate_setflags: base variable (%s) "
			"does not exist", var);
		return;
	}

	if (sttmp->flags == flags)
		return;		/* no change */

	sttmp->flags = flags;

	/* build the list */
	snprintf(flist, sizeof(flist), "%s", var);

	if (flags & ST_FLAG_RW)
		snprintfcat(flist, sizeof(flist), " RW");
	if (flags & ST_FLAG_STRING)
		snprintfcat(flist, sizeof(flist), " STRING");

	/* update listeners */
	send_to_all("SETFLAGS %s\n", flist);
}

void dstate_setaux(const char *var, int aux)
{
	struct	st_tree_t	*sttmp;

	/* find the dtree node for var */
	sttmp = state_tree_find(dtree_root, var);

	if (!sttmp) {
		upslogx(LOG_ERR, "dstate_setaux: base variable (%s) "
			"does not exist", var);
		return;
	}

	if (sttmp->aux == aux)
		return;		/* no change */

	sttmp->aux = aux;

	/* update listeners */
	send_to_all("SETAUX %s %d\n", var, aux);
}

const char *dstate_getinfo(const char *var)
{
	return state_getinfo(dtree_root, var);
}

void dstate_addcmd(const char *cmdname)
{
	int	ret;

	ret = state_addcmd(&cmdhead, cmdname);

	/* update listeners */
	if (ret == 1)
		send_to_all("ADDCMD %s\n", cmdname);
}

int dstate_delinfo(const char *var)
{
	int	ret;

	ret = state_delinfo(&dtree_root, var);

	/* update listeners */
	if (ret == 1)
		send_to_all("DELINFO %s\n", var);

	return ret;
}

int dstate_delenum(const char *var, const char *val)
{
	int	ret;

	ret = state_delenum(dtree_root, var, val);

	/* update listeners */
	if (ret == 1)
		send_to_all("DELENUM %s \"%s\"\n", var, val);

	return ret;
}

int dstate_delcmd(const char *cmd)
{
	int	ret;

	ret = state_delcmd(&cmdhead, cmd);

	/* update listeners */
	if (ret == 1)
		send_to_all("DELCMD %s\n", cmd);

	return ret;
}

void dstate_free(void)
{
	if (sockfd != -1) {
		close(sockfd);
		sockfd = -1;

		if (sockfn) {
			unlink(sockfn);
			free(sockfn);
			sockfn = NULL;
		}
	}

	state_infofree(dtree_root);
	dtree_root = NULL;
	
	state_cmdfree(cmdhead);
	cmdhead = NULL;

	conn_close_all();
}

const struct st_tree_t *dstate_getroot(void)
{
	return dtree_root;
}

const struct cmdlist_t *dstate_getcmdlist(void)
{
	return cmdhead;
}

void dstate_dataok(void)
{
	if (stale == 1) {
		stale = 0;
		send_to_all("DATAOK\n");
	}
}

void dstate_datastale(void)
{
	if (stale == 0) {
		stale = 1;
		send_to_all("DATASTALE\n");
	}
}

int dstate_is_stale(void)
{
	return stale;
}

/* ups.status management functions - reducing duplication in the drivers */

/* clean out the temp space for a new pass */
void status_init(void)
{
	memset(&status_buf, '\0', sizeof(status_buf));
}

/* add a status element */
void status_set(const char *buf)
{
	/* separate with a space if multiple elements are present */
	if (strlen(status_buf) != 0)
		snprintfcat(status_buf, sizeof(status_buf), " %s", buf);
	else
		snprintfcat(status_buf, sizeof(status_buf), "%s", buf);
}

/* write the status_buf into the externally visible dstate storage */
void status_commit(void)
{
	if (alarm_active)
		dstate_setinfo("ups.status", "ALARM %s", status_buf);
	else
		dstate_setinfo("ups.status", "%s", status_buf);
}

/* similar handlers for ups.alarm */

void alarm_init(void)
{
	memset(&alarm_buf, '\0', sizeof(alarm_buf));
}

void alarm_set(const char *buf)
{
	if (strlen(alarm_buf) != 0)
		snprintfcat(alarm_buf, sizeof(alarm_buf), " %s", buf);
	else
		snprintfcat(alarm_buf, sizeof(alarm_buf), "%s", buf);
}

/* write the status_buf into the info array */
void alarm_commit(void)
{
	if (strlen(alarm_buf) != 0) {
		dstate_setinfo("ups.alarm", "%s", alarm_buf);
		alarm_active = 1;
	} else {
		dstate_delinfo("ups.alarm");
		alarm_active = 0;
	}
}
