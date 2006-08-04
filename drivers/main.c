/* main.c - Network UPS Tools driver core

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>

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

#include "main.h"
#include "dstate.h"
#include "../lib/libupsconfig.h"

	/* data which may be useful to the drivers */	
	int	upsfd = -1;
	char	*device_path = NULL;
	const	char	*progname = NULL, *upsname = NULL, 
			*device_name = NULL;

	/* may be set by the driver to wake up while in dstate_poll_fds */
	int	extrafd = -1;

	/* for ser_open */
	int	do_lock_port = 1;

	/* set by the drivers */
	int	experimental_driver = 0;
	int	broken_driver = 0;

	/* for detecting -a values that don't match anything */
	static	int	upsname_found = 0;

	static vartab_t	*vartab_h = NULL;

	/* variables possibly set by the global part of ups.conf */
	static unsigned int	poll_interval = 2;
	static char		*chroot_path = NULL, *user = NULL;

	/* signal handling */
	int	exit_flag = 0;
	static	sigset_t		main_sigmask;
	static	struct	sigaction	main_sa;

	/* everything else */
	static	char	*pidfn = NULL;

/* power down the attached load immediately */
static void forceshutdown(void)
{
	upslogx(LOG_NOTICE, "Initiating UPS shutdown");

	/* the driver must not block in this function */
	upsdrv_shutdown();
	exit(EXIT_SUCCESS);
}

static void help(void)
{
	vartab_t	*tmp;

	printf("\nusage: %s [OPTIONS] [<device>]\n\n", progname);

	printf("  -V             - print version, then exit\n");
	printf("  -L             - print parseable list of driver variables\n");
	printf("  -a <id>        - autoconfig using ups.conf section <id>\n");
	printf("                 - note: -x after -a overrides ups.conf settings\n");
	printf("  -D             - raise debugging level\n");
	printf("  -h             - display this help\n");
	printf("  -k             - force shutdown\n");
	printf("  -i <int>       - poll interval\n");
	printf("  -r <dir>       - chroot to <dir>\n");
	printf("  -u <user>      - switch to <user> (if started as root)\n");
	printf("  -x <var>=<val> - set driver variable <var> to <val>\n");
	printf("                 - example: -x cable=940-0095B\n\n");

	printf("  <device>       - /dev entry corresponding to UPS port\n");
	printf("                 - Only optional when using -a!\n\n");

	if (vartab_h) {
		tmp = vartab_h;

		printf("Acceptable values for -x or ups.conf in this driver:\n\n");

		while (tmp) {
			if (tmp->vartype == VAR_VALUE)
				printf("%40s : -x %s=<value>\n", 
					tmp->desc, tmp->var);
			else
				printf("%40s : -x %s\n", tmp->desc, tmp->var);
			tmp = tmp->next;
		}
	}

	upsdrv_help();

	exit(EXIT_SUCCESS);
}

/* store these in dstate as driver.(parameter|flag) */
static void dparam_setinfo(const char *var, const char *val)
{
	char	vtmp[SMALLBUF];

	/* store these in dstate for debugging and other help */
	if (val) {
		snprintf(vtmp, sizeof(vtmp), "driver.parameter.%s", var);
		dstate_setinfo(vtmp, "%s", val);
		return;
	}

	/* no value = flag */

	snprintf(vtmp, sizeof(vtmp), "driver.flag.%s", var);
	dstate_setinfo(vtmp, "enabled");
}

/* cram var [= <val>] data into storage */
static void storeval(const char *var, char *val)
{
	vartab_t	*tmp, *last;

	tmp = last = vartab_h;

	while (tmp) {
		last = tmp;

		/* sanity check */
		if (!tmp->var) {
			tmp = tmp->next;
			continue;
		}

		/* later definitions overwrite earlier ones */
		if (!strcasecmp(tmp->var, var)) {
			if (tmp->val)
				free(tmp->val);

			if (val)
				tmp->val = xstrdup(val);

			/* don't keep things like SNMP community strings */
			if ((tmp->vartype & VAR_SENSITIVE) == 0)
				dparam_setinfo(var, val);

			tmp->found = 1;
			return;
		}

		tmp = tmp->next;
	}

	/* try to help them out */
	printf("\nFatal error: '%s' is not a valid %s for this driver.\n", var,
		val ? "variable name" : "flag");
	printf("\n");
	printf("Look in the man page or call this driver with -h for a list of\n");
	printf("valid variable names and flags.\n");

	exit(EXIT_SUCCESS);
}

/* retrieve the value of variable <var> if possible */
char *getval(const char *var)
{
	vartab_t	*tmp = vartab_h;

	while (tmp) {
		if (!strcasecmp(tmp->var, var))
			return(tmp->val);
		tmp = tmp->next;
	}

	return NULL;
}

/* see if <var> has been defined, even if no value has been given to it */
int testvar(const char *var)
{
	vartab_t	*tmp = vartab_h;

	while (tmp) {
		if (!strcasecmp(tmp->var, var))
			return tmp->found;
		tmp = tmp->next;
	}

	return 0;	/* not found */
}

/* callback from driver - create the table for -x/conf entries */
void addvar(int vartype, const char *name, const char *desc)
{
	vartab_t	*tmp, *last;

	tmp = last = vartab_h;

	while (tmp) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(vartab_t));

	tmp->vartype = vartype;
	tmp->var = xstrdup(name);
	tmp->val = NULL;
	tmp->desc = xstrdup(desc);
	tmp->found = 0;
	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		vartab_h = tmp;
}	

/* handle -x / ups.conf config details that are for this part of the code */
static int main_arg(char *var, char *val)
{
	/* flags for main: just 'nolock' for now */

	if (!strcmp(var, "nolock")) {
		do_lock_port = 0;
		dstate_setinfo("driver.flag.nolock", "enabled");
		return 1;	/* handled */
	}

	/* any other flags are for the driver code */
	if (!val)
		return 0;

	/* variables for main: port */

	if (!strcmp(var, "port")) {
		device_path = xstrdup(val);
		device_name = xbasename(device_path);
		dstate_setinfo("driver.parameter.port", "%s", val);
		return 1;	/* handled */
	}

	if (!strcmp(var, "sddelay")) {
		upslogx(LOG_INFO, "Obsolete value sddelay found in ups.conf");
		return 1;	/* handled */
	}

	/* only for upsdrvctl - ignored here */
	if (!strcmp(var, "sdorder"))
		return 1;	/* handled */

	/* only for upsd (at the moment) - ignored here */
	if (!strcmp(var, "desc"))
		return 1;	/* handled */

	return 0;	/* unhandled, pass it through to the driver */
}

/* called for fatal errors in nutparser */
static void upsconf_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in configuration file : %s", errmsg);
}

void read_upsconf(void) 
{
	char	fn[SMALLBUF];
	char	tmp[SMALLBUF];
	t_string s;
	t_enum_string enum_ups, enum_ups_begining;
	t_enum_string enum_parameter, enum_parameter_begining;
	t_enum_string enum_flag, enum_flag_begining;
	t_typed_value value;

	snprintf(fn, sizeof(fn), "%s/nut.conf", confpath());
	
	load_config(fn, upsconf_err);
	
	// Lets begin with global args
	value = get_variable("nut.ups.pollinterval");
	if (value.has_value && value.type == string_type) {
		poll_interval = atoi(value.value.string_value);
	}
	free_typed_value(value);
	
	value = get_variable("nut.ups.chroot");
	if (value.has_value && value.type == string_type) {
		if (chroot_path)
			free(chroot_path);

		chroot_path = value.value.string_value;
	}
	
	value = get_variable("nut.ups.user");
	if (value.has_value && value.type == string_type) {
		if (user)
			free(user);

		user = value.value.string_value;
	}
	
	// Go through the list of UPSs
	enum_ups_begining = enum_ups = get_ups_list();
	
	while (enum_ups != NULL) {
		// Is that ups for us ?
		if (strcmp(enum_ups->value, upsname) != 0) {
			// Not for us, lets go to the next
			enum_ups = enum_ups->next_value;
			continue;
		}
		
		upsname_found = 1;
		search_ups(enum_ups->value);
		
		/* don't let the users shoot themselves in the foot */
		s = get_driver();
		if (strcmp(s, progname) != 0) {
			fatalx("Error: UPS [%s] is for driver %s, but I'm %s!\n",
				enum_ups->value, s, progname);
		}
		free(s);
		
		enum_parameter_begining = enum_parameter = get_driver_parameter_list();
		while (enum_parameter != NULL) {
			
			value = get_driver_parameter(enum_parameter->value);
			
			// Only strings value are handled for the moment
			if (value.type != string_type) {
				// Ignore it and go to the next one
				enum_parameter = enum_parameter->next_value;
				continue;
			}
			
			// It is a arg for main ?
			if (main_arg(enum_parameter->value, value.value.string_value)) {
				// It was ! go to the next
				enum_parameter = enum_parameter->next_value;
				continue;
			}
			
			/* allow per-driver overrides of the global setting */
			if (strcmp(enum_parameter->value, "pollinterval") == 0) {
				poll_interval = atoi(value.value.string_value);
				dparam_setinfo("pollinterval", value.value.string_value);
			}
			
			/* everything else must be for the driver */
			storeval(enum_parameter->value, value.value.string_value);
			
			free_typed_value(value);
			
			// Let's go to the next parameter
			enum_parameter = enum_parameter->next_value;
		}
		free_enum_string(enum_parameter_begining);
		
		// The flags now
		enum_flag_begining = enum_flag = get_driver_flag_list();
		
		while ( enum_flag != NULL) {
			
			// It is a flag for main ?
			if (main_arg(enum_flag->value, NULL)) {
				// It was ! go to the next
				enum_flag = enum_flag->next_value;
				continue;
			}
			
			// If not, it is for the driver
			snprintf(tmp, sizeof(tmp), "driver.flag.%s", enum_flag->value);
			dstate_setinfo(tmp, "enabled");

			storeval(enum_flag->value, NULL);
		}
		
		enum_ups = enum_ups->next_value;
	}
	free_enum_string(enum_ups_begining);
	
	drop_config();
}

/* split -x foo=bar into 'foo' and 'bar' */
static void splitxarg(char *inbuf)
{
	char	*eqptr, *val, *buf;

	/* make our own copy - avoid changing argv */
	buf = xstrdup(inbuf);

	eqptr = strchr(buf, '=');

	if (!eqptr)
		val = NULL;
	else {
		*eqptr++ = '\0';
		val = eqptr;
	}

	/* see if main handles this first */
	if (main_arg(buf, val))
		return;

	/* otherwise store it for later */
	storeval(buf, val);
}

/* dump the list from the vartable for external parsers */
static void listxarg(void)
{
	vartab_t	*tmp;

	tmp = vartab_h;

	if (!tmp)
		return;

	while (tmp) {

		switch (tmp->vartype) {
			case VAR_VALUE: printf("VALUE"); break;
			case VAR_FLAG: printf("FLAG"); break;
			default: printf("UNKNOWN"); break;
		}

		printf(" %s \"%s\"\n", tmp->var, tmp->desc);

		tmp = tmp->next;
	}
}

static void vartab_free(void)
{
	vartab_t	*tmp, *next;

	tmp = vartab_h;

	while (tmp) {
		next = tmp->next;

		if (tmp->var)
			free(tmp->var);
		if (tmp->val)
			free(tmp->val);
		if (tmp->desc)
			free(tmp->desc);
		free(tmp);

		tmp = next;
	}
}

static void exit_cleanup(void)
{
	upsdrv_cleanup();

	if (chroot_path)
		free(chroot_path);
	if (device_path)
		free(device_path);
	if (user)
		free(user);
	if (pidfn) {
		unlink(pidfn);
		free(pidfn);
	}

	dstate_free();
	vartab_free();
}

static void set_exit_flag(int sig)
{
	exit_flag = sig;
}

static void setup_signals(void)
{
	sigemptyset(&main_sigmask);
	main_sa.sa_mask = main_sigmask;
	main_sa.sa_flags = 0;

	main_sa.sa_handler = set_exit_flag;
	sigaction(SIGTERM, &main_sa, NULL);
	sigaction(SIGINT, &main_sa, NULL);
	sigaction(SIGQUIT, &main_sa, NULL);

	main_sa.sa_handler = SIG_IGN;
	sigaction(SIGHUP, &main_sa, NULL);
	sigaction(SIGPIPE, &main_sa, NULL);
}

int main(int argc, char **argv)
{
	struct	passwd	*new_uid = NULL;
	int	i, do_forceshutdown = 0;

	/* pick up a default from configure --with-user */
	user = xstrdup(RUN_AS_USER);	/* xstrdup: this gets freed at exit */

	upsdrv_banner();

	if (experimental_driver) {
		printf("Warning: This is an experimental driver.\n");
		printf("Some features may not function correctly.\n\n");
	}

	progname = xbasename(argv[0]);
	open_syslog(progname);

	/* build the driver's extra (-x) variable table */
	upsdrv_makevartable();

	while ((i = getopt(argc, argv, "+a:kDhx:Lr:u:Vi:")) != EOF) {
		switch (i) {
			case 'a':
				upsname = optarg;

				read_upsconf();

				if (!upsname_found)
					upslogx(LOG_WARNING, "Warning: Section %s not found in ups.conf",
						optarg);
				break;
			case 'D':
				nut_debug_level++;
				break;
			case 'i':
				poll_interval = atoi(optarg);
				break;
			case 'k':
				do_lock_port = 0;
				do_forceshutdown = 1;
				break;
			case 'L':
				listxarg();
				exit(EXIT_SUCCESS);
			case 'r':
				chroot_path = xstrdup(optarg);
				break;
			case 'u':
				user = xstrdup(optarg);
				break;
			case 'V':
				/* already printed the banner, so exit */
				exit(EXIT_SUCCESS);
			case 'x':
				splitxarg(optarg);
				break;
			case 'h':
			default:
				help();
				break;
		}
	}

	argc -= optind;
	argv += optind;
	optind = 1;

	/* we need to get the port from somewhere */
	if (argc < 1) {
		if (!device_path) {
			fprintf(stderr, "Error: You must specify a port name in ups.conf or on the command line.\n");
			help();
		}
	}

	/* allow argv to override the ups.conf entry if specified */
	else {
		device_path = xstrdup(argv[0]);
		device_name = xbasename(device_path);
	}

	pidfn = xmalloc(SMALLBUF);

	if (upsname_found)
		snprintf(pidfn, SMALLBUF, "%s/%s-%s.pid", 
			altpidpath(), progname, upsname);
	else
		snprintf(pidfn, SMALLBUF, "%s/%s-%s.pid", 
			 altpidpath(), progname, device_name);

	upsdebugx(1, "debug level is '%d'", nut_debug_level);

	new_uid = get_user_pwent(user);
	
	if (chroot_path)
		chroot_start(chroot_path);

	become_user(new_uid);

	/* Only switch to statepath if we're not powering off */
	/* This avoid case where ie /var is umounted */
	if (!do_forceshutdown)
		if (chdir(dflt_statepath()))
			fatal_with_errno("Can't chdir to %s", dflt_statepath());

	setup_signals();

	/* clear out callback handler data */
	memset(&upsh, '\0', sizeof(upsh));

	upsdrv_initups();

	/* now see if things are very wrong out there */
	if (broken_driver) {
		printf("Fatal error: broken driver. It probably needs to be converted.\n");
		printf("Search for 'broken_driver = 1' in the source for more details.\n");
		exit(EXIT_FAILURE);
	}

	if (do_forceshutdown)
		forceshutdown();

	/* get the base data established before allowing connections */
	upsdrv_initinfo();
	upsdrv_updateinfo();

	/* now we can start servicing requests */
	if (upsname_found)
		dstate_init(upsname, NULL);
	else
		dstate_init(progname, device_name);

	/* publish the top-level data: version number, driver name */
	dstate_setinfo("driver.version", "%s", UPS_VERSION);
	dstate_setinfo("driver.name", "%s", progname);

	if (nut_debug_level == 0) {
		background();
		writepid(pidfn);
	}

	/* safe to do this now that the parent has exited */
	atexit(exit_cleanup);

	while (exit_flag == 0) {
		upsdrv_updateinfo();

		dstate_poll_fds(poll_interval, extrafd);
	}

	/* if we get here, the exit flag was set by a signal handler */

	upslogx(LOG_INFO, "Signal %d: exiting", exit_flag);

	exit(EXIT_SUCCESS);
}
