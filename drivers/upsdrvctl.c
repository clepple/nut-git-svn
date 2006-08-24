/* upsdrvctl.c - UPS driver controller

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "config.h"
#include "proto.h"
#include "version.h"
#include "common.h"
#include "../lib/libupsconfig.h"

typedef struct {
	char	*upsname;
	char	*driver;
	char	*port;
	int	sdorder;
	int	maxstartdelay;
	void	*next;
}	ups_t;

static	ups_t	*upstable = NULL;

static	int	maxsdorder = 0, testmode = 0, exec_error = 0;

	/* timer - keeps us from getting stuck if a driver hangs */
static	int	maxstartdelay = 45;

	/* Directory where driver executables live */
static	char	*driverpath = NULL;

	/* passthrough to the drivers: chroot path and new user name */
static	char	*pt_root = NULL, *pt_user = NULL;

static	sigset_t	nut_upsdrvctl_sigmask;
static	struct	sigaction	sa;

/* called for fatal errors in nutparser */
static void upsconf_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in configuration file : %s", errmsg);
}

/* called for variable values error */
static void variable_value_err(const char *varname)
{
	upslogx(LOG_ERR, "Error in configuration file : Variable %s : wrong type or no value. Ignoring it", varname);
}


void read_upsconf(void)
{
	char	fn[SMALLBUF];
	char 	tmp[SMALLBUF];
	t_typed_value value;
	t_enum_string enum_ups, enum_ups_begining;
	ups_t *ups;
	
	snprintf(fn, sizeof(fn), "%s/nut.conf", confpath());
	
	load_config(fn, upsconf_err);
	
	// Lets begin with global args
	value = get_variable("nut.ups.global.maxstartdelay");
	if (value.has_value && value.type == string_type) {
		maxstartdelay = atoi(value.value.string_value);
	}
	free_typed_value(value);
	
	value = get_variable("nut.ups.global.driverpath");
	if (value.has_value && value.type == string_type) {
		if (driverpath)
			free(driverpath);

		driverpath = value.value.string_value;
	}
	
	// Lets make the upstable 
	enum_ups_begining = enum_ups = get_ups_list();
	
	while (enum_ups != NULL) {
		ups = xmalloc(sizeof(ups_t));
		
		search_ups(enum_ups->value);
		
		ups->upsname = xstrdup(enum_ups->value);
		
		ups->driver = get_driver();
		if (ups->driver == 0) {
			sprintf(tmp, "nut.ups.%s.driver.name", enum_ups->value);
			variable_value_err(tmp);
			enum_ups = enum_ups->next_value;
			continue;
		}
		
		ups->port = get_port();
		if (ups->port == 0) {
			sprintf(tmp, "nut.ups.%s.driver.parameter.port", enum_ups->value);
			variable_value_err(tmp);
			enum_ups = enum_ups->next_value;
			continue;
		}
		
		ups->next = upstable;
		
		value = get_ups_variable("sdorder");
		if (value.has_value && value.type == string_type) {
			ups->sdorder = atoi(value.value.string_value);
		} else {
			ups->sdorder = 0;
		}
		free_typed_value(value);
		
		value = get_ups_variable("maxstartdelay");
		if (value.has_value && value.type == string_type) {
			ups->maxstartdelay = atoi(value.value.string_value);
		} else {
			ups->maxstartdelay = -1;
		}
		free_typed_value(value);
		
		upstable = ups;

		enum_ups = enum_ups->next_value;
	}
	free_enum_string(enum_ups_begining);
	
}


/* handle sending the signal */
static void stop_driver(const ups_t *ups)
{
	char	pidfn[SMALLBUF];
	int	ret;
	struct	stat	fs;

	upsdebugx(1, "Stopping UPS: %s", ups->upsname);

	snprintf(pidfn, sizeof(pidfn), "%s/%s-%s.pid", altpidpath(),
		ups->driver, xbasename(ups->port));
	ret = stat(pidfn, &fs);

	if (ret != 0) {
		snprintf(pidfn, sizeof(pidfn), "%s/%s-%s.pid", altpidpath(),
			ups->driver, ups->upsname);
		ret = stat(pidfn, &fs);
	}

	if (ret != 0) {
		upslog_with_errno(LOG_ERR, "Can't open %s", pidfn);
		exec_error++;
		return;
	}

	upsdebugx(2, "Sending signal to %s", pidfn);

	if (testmode)
		return;

	ret = sendsignalfn(pidfn, SIGTERM);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "Stopping %s failed", pidfn);
		exec_error++;
		return;
	}
}

static void waitpid_timeout(const int sig)
{
	/* do nothing */
	return;
}

static void forkexec(const char *prog, char **argv, const ups_t *ups)
{
	int	ret;
	pid_t	pid;

	pid = fork();

	if (pid < 0)
		fatal_with_errno("fork");

	if (pid != 0) {			/* parent */
		int	wstat;

		sigemptyset(&nut_upsdrvctl_sigmask);
		sa.sa_mask = nut_upsdrvctl_sigmask;
		sa.sa_flags = 0;
		sa.sa_handler = waitpid_timeout;
		sigaction(SIGALRM, &sa, NULL);

		if (ups->maxstartdelay != -1)
			alarm(ups->maxstartdelay);
		else
			alarm(maxstartdelay);

		ret = waitpid(pid, &wstat, 0);

		signal(SIGALRM, SIG_IGN);
		alarm(0);

		if (ret == -1) {
			upslogx(LOG_WARNING, "Startup timer elapsed, continuing...");
			exec_error++;
			return;
		}

		if (WIFEXITED(wstat) == 0) {
			upslogx(LOG_WARNING, "Driver exited abnormally");
			exec_error++;
			return;
		}

		if (WEXITSTATUS(wstat) != 0) {
			upslogx(LOG_WARNING, "Driver failed to start"
			" (exit status=%d)", WEXITSTATUS(wstat));
			exec_error++;
			return;
		}

		/* the rest only work when WIFEXITED is nonzero */

		if (WIFSIGNALED(wstat)) {
			upslog_with_errno(LOG_WARNING, "Driver died after signal %d",
				WTERMSIG(wstat));
			exec_error++;
		}

		return;
	}

	/* child */

	ret = execv(prog, argv);

	/* shouldn't get here */
	fatal_with_errno("execv");
}

static void start_driver(const ups_t *ups)
{
	char	dfn[SMALLBUF], *argv[8];
	int	ret, arg = 0;
	struct	stat	fs;

	upsdebugx(1, "Starting UPS: %s", ups->upsname);

	snprintf(dfn, sizeof(dfn), "%s/%s", driverpath, ups->driver);
	ret = stat(dfn, &fs);

	if (ret < 0)
		fatal_with_errno("Can't start %s", dfn);

	upsdebugx(2, "exec: %s -a %s", dfn, ups->upsname);

	argv[arg++] = dfn;
	argv[arg++] = "-a";
	argv[arg++] = ups->upsname;

	/* stick on the chroot / user args if given to us */
	if (pt_root) {
		argv[arg++] = "-r";
		argv[arg++] = pt_root;
	}

	if (pt_user) {
		argv[arg++] = "-u";
		argv[arg++] = pt_user;
	}

	if (testmode)
		return;

	/* tie it off */
	argv[arg++] = NULL;

	forkexec(dfn, argv, ups);
}

static void help(const char *progname)
{
	printf("Starts and stops UPS drivers via ups.conf.\n\n");
	printf("usage: %s [OPTIONS] (start | stop | shutdown) [<ups>]\n\n", progname);

	printf("  -h			display this help\n");
	printf("  -r <path>		drivers will chroot to <path>\n");
	printf("  -t			testing mode - prints actions without doing them\n");
	printf("  -u <user>		drivers started will switch from root to <user>\n");
	printf("  -D            	raise debugging level\n");
	printf("  start			start all UPS drivers in ups.conf\n");
	printf("  start	<ups>		only start driver for UPS <ups>\n");
	printf("  stop			stop all UPS drivers in ups.conf\n");
	printf("  stop <ups>		only stop driver for UPS <ups>\n");
	printf("  shutdown		shutdown all UPS drivers in ups.conf\n");
	printf("  shutdown <ups>	only shutdown UPS <ups>\n");

	exit(EXIT_SUCCESS);
}

static void shutdown_driver(const ups_t *ups)
{
	char	*argv[7], dfn[SMALLBUF];
	int	arg = 0;

	upsdebugx(1, "Shutdown UPS: %s", ups->upsname);

	snprintf(dfn, sizeof(dfn), "%s/%s", driverpath, ups->driver);

	upsdebugx(2, "exec: %s -a %s -k", dfn, ups->upsname);

	if (testmode)
		return;

	argv[arg++] = dfn;
	argv[arg++] = "-a";
	argv[arg++] = ups->upsname;
	argv[arg++] = "-k";
	argv[arg++] = NULL;

	forkexec(dfn, argv, ups);
}

static void send_one_driver(void (*command)(const ups_t *), const char *upsname)
{
	ups_t	*ups = upstable;

	if (!ups)
		fatalx("Error: no UPS definitions found in ups.conf!\n");

	while (ups) {
		if (!strcmp(ups->upsname, upsname)) {
			command(ups);
			return;
		}

		ups = ups->next;
	}

	fatalx("UPS %s not found in ups.conf", upsname);
}

/* walk UPS table and send command to all UPSes according to sdorder */
static void send_all_drivers(void (*command)(const ups_t *))
{
	ups_t	*ups;
	int	i;

	if (!upstable)
		fatalx("Error: no UPS definitions found in ups.conf");

	if (command != &shutdown_driver) {
		ups = upstable;

		while (ups) {
			command(ups);

			ups = ups->next;
		}

		return;
	}

	for (i = 0; i <= maxsdorder; i++) {
		ups = upstable;

		while (ups) {
			if (ups->sdorder == i)
				command(ups);
			
			ups = ups->next;
		}
	}
}

static void exit_cleanup(void)
{
	ups_t	*tmp, *next;

	tmp = upstable;

	while (tmp) {
		next = tmp->next;

		if (tmp->driver)
			free(tmp->driver);

		if (tmp->port)
			free(tmp->port);

		if (tmp->upsname)
			free(tmp->upsname);
		free(tmp);

		tmp = next;
	}

	if (driverpath)
		free(driverpath);
}

int main(int argc, char **argv)
{
	int	i;
	char	*prog;
	void	(*command)(const ups_t *) = NULL;

	printf("Network UPS Tools - UPS driver controller %s\n",
		UPS_VERSION);

	prog = argv[0];
	while ((i = getopt(argc, argv, "+htu:r:DV")) != EOF) {
		switch(i) {
			case 'r':
				pt_root = optarg;
				break;

			case 't':
				testmode = 1;
				break;

			case 'u':
				pt_user = optarg;
				break;

			case 'V':
				exit(EXIT_SUCCESS);

			case 'D':
				nut_debug_level++;
				break;

			case 'h':
			default:
				help(prog);
				break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		help(prog);

	if (testmode) {
		printf("*** Testing mode: not calling exec/kill\n");

		if (nut_debug_level < 2)
			nut_debug_level = 2;
	}

	if (!strcmp(argv[0], "start"))
		command = &start_driver;

	if (!strcmp(argv[0], "stop"))
		command = &stop_driver;

	if (!strcmp(argv[0], "shutdown"))
		command = &shutdown_driver;

	if (!command)
		fatalx("Error: unrecognized command [%s]", argv[0]);

	driverpath = xstrdup(DRVPATH);	/* set default */

	atexit(exit_cleanup);

	read_upsconf();

	if (argc == 1)
		send_all_drivers(command);
	else
		send_one_driver(command, argv[1]);

	if (exec_error)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
