#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "parseconf.h"
#include "../lib/libupsconfig.h"
#include "common.h"

static struct option long_options[] = {
	{"target_dir",      required_argument, 0, 't'},
	{"base_config_dir", required_argument, 0, 'b'},
	{"comments_dir",    required_argument, 0, 'c'},
	{"single",          no_argument,       0, 's'},
	{"quiet",           no_argument,       0, 'q'},
	{"mode",            required_argument, 0, 'm'},
	{"help",            no_argument,       0, 'h'}
};

char *target_dir = ".";
char *current_dir = CONFPATH;
char *comments_dir;
int single = 0;
int quiet = 0;
t_modes mode;
int mode_given = 0;
int error = 0;

void print_usage() {
	t_string s;
	
	s = malloc(sizeof(char) * (strlen(CONFPATH) + 50));
	sprintf(s, "%s/base_config/comments", CONFPATH);
	printf("usage : upsconfig [-t <target_dir>] [-c <current_config_dir>] [-m <mode>] [-s] [-h]\n");
	printf("-t, --target_dir          where to save the generated configuration. Default\n");
	printf("                          is current directory\n");
	printf("-b, --base_config_dir     the directory where are the current old way configuration\n");
	printf("                          files. Default is %s\n", CONFPATH);
	printf("-c, --comments_dir        the directory where is the comment file to use. Default\n");
	printf("                          is %s\n", s);
	printf("-s, --single              save the configuration in a single file\n");
	printf("-m, --mode                The mode of working of NUT in your computer. Valid \n");
	printf("                          value are standalone, net_server, net_client\n");
	printf("                          pm and none\n");
	printf("-q, --quiet               don't print anything apart errors\n");
	printf("-h, --help                display this help message\n");
	free(s);
}

/* called for fatal errors in parseconf like malloc failures */
static void err_handler(const char *errmsg)
{
	printf("Fatal error in nutparser: %s", errmsg);
}

/* Already exist in libupsconfig.c */
extern FILE* open_file(char* file_name, char* mode ,void errhandler(const char*));
	
int check_perms(const char *fn)
{
	FILE* ret;
	
	ret = fopen(fn, "r");
	if (ret == 0) {
		if (!quiet) {
			printf("%s : %s\n", fn, strerror(errno));
		}
		error = 1;
		return 0;
	}
	
	return 1;
}


/* callback during parsing of ups.conf */
void do_upsconf_args(char *upsname, char *var, char *val)
{
	t_string s;

	/* "global" stuff */
	if (!upsname) {
		s = xmalloc(sizeof(char) * (strlen(var) + 16));
		sprintf(s, "nut.ups.global.%s", var);
		set_variable(s, val, string_type);
		free(s);
		return;
	}

	/* Flags */
	if (val == 0) {
		s = xmalloc(sizeof(char) * (strlen(var) + strlen(upsname) + 22));
		sprintf(s, "nut.ups.%s.driver.flag.%s", upsname, var);
		set_variable(s, "enabled", string_type);
		free(s);
		free(upsname);
		return;
	}
	
	if (!strcmp(var, "driver")) {
		set_driver(val);
		free(upsname);
		return;
	}
	
	if (!strcmp(var, "desc")) {
		set_desc(val);
		free(upsname);
		return;
	}
	
	/* All the rest should be driver parameter */
	s = xmalloc(sizeof(char) * (strlen(var) + strlen(upsname) + 27));
	sprintf(s, "nut.ups.%s.driver.parameter.%s", upsname, var);
	set_variable(s, val, string_type);
	free(s);
	free(upsname);
	
}

/* handle arguments separated by parseconf */
static void ups_args(int numargs, char **arg)
{
	char	*ep;

	if (numargs < 1)
		return;

	/* look for section headers - [upsname] */
	if ((arg[0][0] == '[') && (arg[0][strlen(arg[0])-1] == ']')) {
		
		arg[0][strlen(arg[0])-1] = '\0';
		
		add_ups(&arg[0][1], "foo", "auto");
		return;
	}

	/* handle 'foo=bar' (compressed form) */
	ep = strchr(arg[0], '=');
	if (ep) {
		*ep = '\0';

		do_upsconf_args(get_ups_name(), arg[0], ep+1);
		return;
	}

	/* handle 'foo' (flag) */
	if (numargs == 1) {
		do_upsconf_args(get_ups_name(), arg[0], NULL);
		return;
	}

	if (numargs < 3)
		return;

	/* handle 'foo = bar' (split form) */
	if (!strcmp(arg[1], "=")) {
		do_upsconf_args(get_ups_name(), arg[0], arg[2]);
		return;
	}
}

/* open the ups.conf, parse it, and call back do_upsconf_args() */
int parse_ups() {
	char	fn[SMALLBUF];
	PCONF_CTX	ctx;

	snprintf(fn, sizeof(fn), "%s/ups.conf", current_dir);

	if (!check_perms(fn)) {
		return 0;
	}

	pconf_init(&ctx, err_handler);

	if (!pconf_file_begin(&ctx, fn)) {
		pconf_finish(&ctx);
		return 0;
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			printf("Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		ups_args(ctx.numargs, ctx.arglist);
	}

	pconf_finish(&ctx);
	return 1;
}

/* actually do something with the variable + value pairs */
static void parse_var(char *var, char *val)
{
	t_enum_string enum_string;
	
	if (!strcasecmp(var, "password")) {
		set_rights(admin_rw);
		set_password(val);
		set_rights(all_r_admin_rw);
		return;
	}

	if (!strcasecmp(var, "instcmds")) {
		enum_string = get_instcmds();
		enum_string = add_to_enum_string(enum_string, val);
		set_instcmds(enum_string);
		free_enum_string(enum_string);
		return;
	}

	if (!strcasecmp(var, "actions")) {
		enum_string = get_actions();
		enum_string = add_to_enum_string(enum_string, val);
		set_actions(enum_string);
		free_enum_string(enum_string);
		return;
	}

	if (!strcasecmp(var, "allowfrom")) {
		enum_string = get_allowfrom();
		enum_string = add_to_enum_string(enum_string, val);
		set_allowfrom(enum_string);
		free_enum_string(enum_string);
		return;
	}

	/* someone did 'upsmon = type' - allow it anyway */
	if (!strcasecmp(var, "upsmon")) {
		if (strcmp(val, "master") == 0) {
			set_type(upsmon_master);
		} else if (strcmp(val, "slave") == 0) {
			set_type(upsmon_slave);
		}
		return;
	}

	printf("Unrecognized user setting %s", var);
}

/* parse first var+val pair, then flip through remaining vals */
static void parse_rest(char *var, char *fval, char **arg, int next, int left)
{
	int	i;
	t_string s;

	/* no globals supported yet, so there's no sense in continuing */
	s = get_name();
	if (s == 0)
		return;
		
	free(s);

	parse_var(var, fval);

	if (left == 0)
		return;

	for (i = 0; i < left; i++)
		parse_var(var, arg[next + i]);
}


static void user_parse_arg(int numargs, char **arg)
{
	char	*ep;

	if ((numargs == 0) || (!arg))
		return;

	/* ignore old file format */
	if (!strcasecmp(arg[0], "user"))
		return;

	/* handle 'foo=bar' (compressed form) */

	ep = strchr(arg[0], '=');
	if (ep) {
		*ep = '\0';

		/* parse first var/val, plus subsequent values (if any) */

		/*      0       1       2  ... */
		/* foo=bar <rest1> <rest2> ... */

		parse_rest(arg[0], ep+1, arg, 1, numargs - 1);
		return;
	}

	/* look for section headers - [username] */
	if ((arg[0][0] == '[') && (arg[0][strlen(arg[0])-1] == ']')) {
		arg[0][strlen(arg[0])-1] = '\0';
		add_user(&arg[0][1], custom, "");
		set_rights(admin_rw);
		set_password("!");
		set_rights(all_r_admin_rw);
		return;
	}

	if (numargs < 2)
		return;

	if (!strcasecmp(arg[0], "upsmon")) {
		if (strcmp(arg[1], "master") == 0) {
			set_type(upsmon_master);
		} else if (strcmp(arg[1], "slave") == 0) {
			set_type(upsmon_slave);
		}
	}

	/* everything after here needs arg[1] and arg[2] */
	if (numargs < 3)
		return;

	/* handle 'foo = bar' (split form) */
	if (!strcmp(arg[1], "=")) {

		/*   0 1   2      3       4  ... */
		/* foo = bar <rest1> <rest2> ... */

		/* parse first var/val, plus subsequent values (if any) */
		
		parse_rest(arg[0], arg[2], arg, 3, numargs - 3);
		return;
	}

	/* ... unhandled ... */
}



int parse_users() {
	char	fn[SMALLBUF];
	PCONF_CTX	ctx;

	snprintf(fn, sizeof(fn), "%s/upsd.users", current_dir);

	if (!check_perms(fn)) {
		return 0;
	}
	
	pconf_init(&ctx, err_handler);

	if (!pconf_file_begin(&ctx, fn)) {
		pconf_finish(&ctx);
		return 0;
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			printf("Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		user_parse_arg(ctx.numargs, ctx.arglist);
	}

	pconf_finish(&ctx);
	return 1;
}

static int parse_upsd_conf_args(int numargs, char **arg, t_enum_string *accept_list, t_enum_string *reject_list)
{
	t_enum_string enum_string;
	int i;
	
	/* everything below here uses up through arg[1] */
	if (numargs < 2)
		return 0;

	/* MAXAGE <seconds> */
	if (!strcmp(arg[0], "MAXAGE")) {
		set_maxage(atoi(arg[1]));
		return 1;
	}

	/* STATEPATH <dir> */
	if (!strcmp(arg[0], "STATEPATH")) {
		set_variable("nut.upsd.statepath",arg[1], string_type);
		return 1;
	}

	/* DATAPATH <dir> */
	if (!strcmp(arg[0], "DATAPATH")) {
		set_variable("nut.upsd.datapath", arg[1], string_type);
		return 1;
	}

	/* CERTFILE <dir> */
	if (!strcmp(arg[0], "CERTFILE")) {
		set_variable("nut.upsd.certfile", arg[1], string_type);
		return 1;
	}

	/* ACCEPT <aclname> [<aclname>...] */
	if (!strcmp(arg[0], "ACCEPT")) {
		enum_string = get_accept();
		for (i = 1; i < numargs; i++) {
			*accept_list = add_to_enum_string(*accept_list, arg[i]);
		}
		return 1;
	}

	/* REJECT <aclname> [<aclname>...] */
	if (!strcmp(arg[0], "REJECT")) {
		enum_string = get_reject();
		for (i = 1; i < numargs; i++) {
			if ( strcmp(arg[i], "all") == 0 ) continue; 
			*reject_list = add_to_enum_string(*reject_list, arg[i]);
		}
		return 1;
	}

	/* everything below here uses up through arg[2] */
	if (numargs < 3)
		return 0;

	/* ACL <aclname> <ip block> */
	if (!strcmp(arg[0], "ACL")) {
		set_acl_value(arg[1], arg[2]);
		return 1;
	}

	if (numargs < 4)
		return 0;

	if (!strcmp(arg[0], "ACCESS")) {
		printf("ACCESS in upsd.conf is no longer supported - switch to ACCEPT/REJECT");
		return 1;
	}

	/* not recognized */
	return 0;
}


int parse_upsd() {
	char	fn[SMALLBUF];
	PCONF_CTX	ctx;
	t_enum_string accept_list = 0, reject_list = 0;

	snprintf(fn, sizeof(fn), "%s/upsd.conf", current_dir);

	if (!check_perms(fn)) {
		return 0;
	}

	pconf_init(&ctx, err_handler);

	if (!pconf_file_begin(&ctx, fn)) {
		pconf_finish(&ctx);
		return 0;
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			printf("Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		if (ctx.numargs < 1)
			continue;

		if (!parse_upsd_conf_args(ctx.numargs, ctx.arglist, &accept_list, &reject_list)) {
			unsigned int	i;
			char	errmsg[SMALLBUF];

			snprintf(errmsg, sizeof(errmsg), 
				"upsd.conf: invalid directive");

			for (i = 0; i < ctx.numargs; i++)
				snprintfcat(errmsg, sizeof(errmsg), " %s", 
					ctx.arglist[i]);

			printf("%s", errmsg);
		}

	}
	if (reject_list) {
		set_reject(reject_list);
		free_enum_string(reject_list);
	}
	if (accept_list) {
		set_accept(accept_list);
		free_enum_string(accept_list);
	}

	pconf_finish(&ctx);
	return 1;		
}


/* returns 1 if used, 0 if not, so we can complain about bogus configs */
static int parse_upsmon_arg(int numargs, char **arg)
{
	char* ep;
	
	/* using up to arg[1] below */
	if (numargs < 2)
		return 0;

	/* SHUTDOWNCMD <cmd> */
	if (!strcmp(arg[0], "SHUTDOWNCMD")) {
		set_shutdown_command(arg[1]);
		return 1;
	}

	/* POWERDOWNFLAG <fn> */
	if (!strcmp(arg[0], "POWERDOWNFLAG")) {
		set_powerdownflag(arg[1]);
		return 1;
	}		

	/* NOTIFYCMD <cmd> */
	if (!strcmp(arg[0], "NOTIFYCMD")) {
		set_notify_command(arg[1]);
		return 1;
	}

	/* POLLFREQ <num> */
	if (!strcmp(arg[0], "POLLFREQ")) {
		set_pollfreq(atoi(arg[1]));
		return 1;
	}

	/* POLLFREQALERT <num> */
	if (!strcmp(arg[0], "POLLFREQALERT")) {
		set_pollfreqalert(atoi(arg[1]));
		return 1;
	}

	/* HOSTSYNC <num> */
	if (!strcmp(arg[0], "HOSTSYNC")) {
		set_hostsync(atoi(arg[1]));
		return 1;
	}

	/* DEADTIME <num> */
	if (!strcmp(arg[0], "DEADTIME")) {
		set_deadtime(atoi(arg[1]));
		return 1;
	}

	/* MINSUPPLIES <num> */
	if (!strcmp(arg[0], "MINSUPPLIES")) {
		set_minsupplies(atoi(arg[1]));
		return 1;
	}

	/* RBWARNTIME <num> */
	if (!strcmp(arg[0], "RBWARNTIME")) {
		set_rbwarntime(atoi(arg[1]));
		return 1;
	}

	/* NOCOMMWARNTIME <num> */
	if (!strcmp(arg[0], "NOCOMMWARNTIME")) {
		set_nocommwarntime(atoi(arg[1]));
		return 1;
	}

	/* FINALDELAY <num> */
	if (!strcmp(arg[0], "FINALDELAY")) {
		set_finaldelay(atoi(arg[1]));
		return 1;
	}

	/* RUN_AS_USER <userid> */
 	if (!strcmp(arg[0], "RUN_AS_USER")) {
		set_run_as_user(arg[1]);
		return 1;
	}

	/* CERTPATH <path> */
	if (!strcmp(arg[0], "CERTPATH")) {
		set_variable("nut.upsd.certpath", arg[1], string_type);
		return 1;
	}

	/* CERTVERIFY (0|1) */
	if (!strcmp(arg[0], "CERTVERIFY")) {
		set_variable("nut.upsd.certverify", arg[1], string_type);
		return 1;
	}

	/* FORCESSL (0|1) */
	if (!strcmp(arg[0], "FORCESSL")) {
		set_variable("nut.upsd.forcessl", arg[1], string_type);
		return 1;
	}

	/* using up to arg[2] below */
	if (numargs < 3)
		return 0;

	/* NOTIFYMSG <notify type> <replacement message> */
	if (!strcmp(arg[0], "NOTIFYMSG")) {
		set_notify_message(string_to_event(arg[1]), arg[2]);
		return 1;
	}

	/* NOTIFYFLAG <notify type> <flags> */
	if (!strcmp(arg[0], "NOTIFYFLAG")) {
		set_notify_flag(string_to_event(arg[1]), string_to_flag(arg[2]));
		return 1;
	}	

	/* using up to arg[4] below */
	if (numargs < 5)
		return 0;

	if (!strcmp(arg[0], "MONITOR")) {

		/* original style: no username (only 5 args) */
		if (numargs == 5) {
			printf("Unable to use old-style MONITOR line without a username");
			printf("Convert it and add a username to upsd.users - see the documentation");

			printf("Fatal error: unusable configuration");
			exit(EXIT_FAILURE);
		}
		
		/* <sys> <pwrval> <user> <pw> ("master" | "slave") */
		ep = strchr(arg[1], '@');
		if (ep) {
			*ep = '\0';
			add_monitor_rule(arg[1], ep+1, atoi(arg[2]), arg[3]);
			if (strcmp(arg[5], "master") == 0) {
				add_user(arg[3], upsmon_master, "!");
				set_rights(admin_rw);
				set_password(arg[4]);
				set_rights(all_r_admin_rw);
			} else {
				add_user(arg[3], upsmon_slave, "!");
				set_rights(admin_rw);
				set_password(arg[4]);
				set_rights(all_r_admin_rw);
			}
			return 1;
		}
	}

	/* didn't parse it at all */
	return 0;
}


int parse_upsmon() {
	char	fn[SMALLBUF];
	PCONF_CTX	ctx;

	snprintf(fn, sizeof(fn), "%s/upsmon.conf", current_dir);

	if (!check_perms(fn)) {
		return 0;
	}

	pconf_init(&ctx, err_handler);

	if (!pconf_file_begin(&ctx, fn)) {
		pconf_finish(&ctx);
		return 0;
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			printf("Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		if (ctx.numargs < 1)
			continue;

		if (!parse_upsmon_arg(ctx.numargs, ctx.arglist)) {
			unsigned int	i;
			char	errmsg[SMALLBUF];

			snprintf(errmsg, sizeof(errmsg), 
				"upsmon.conf line %d: invalid directive",
				ctx.linenum);

			for (i = 0; i < ctx.numargs; i++)
				snprintfcat(errmsg, sizeof(errmsg), " %s", 
					ctx.arglist[i]);

			printf("%s\n", errmsg);
		}
	}

	pconf_finish(&ctx);
	return 1;		
}


int main (int argc, char** argv) {
	int option_index = 0, i;
	int ups, users, upsd, upsmon;
	t_string comm_file;
	
	if (argc == 1) {
		print_usage();
		exit(0);
	}
	
	comments_dir = malloc(sizeof(char) * (strlen(CONFPATH) + 23));
	sprintf(comments_dir, "%s/base_config/comments/", CONFPATH);
	
	while ((i = getopt_long(argc, argv, "+t:b:c:sqm:h", long_options, &option_index)) != -1) {
		switch (i) {
			case 't':
				target_dir = strdup(optarg);
				break;
			case 'b':
				current_dir = strdup(optarg);
				break;
			case 'c':
				free(comments_dir);
				comments_dir = strdup(optarg);
				break;
			case 's':
				single = 1;
				break;
			case 'q':
				quiet = 1;
				break;
			case 'm':
				mode_given = 1;
				mode = string_to_mode(optarg);
				break;
			default:
				print_usage();
				return 1;
				break;
		}
	}
	new_config();
	
	set_rights(all_r_admin_rw);
	
	set_mode(mode);
	
	ups = parse_ups();
	upsmon = parse_upsmon();
	users = parse_users();
	upsd = parse_upsd();
	
	/* Generate the name of the comments file to use */
	comm_file = get_comments_template(comments_dir);
		
	save_config(target_dir, comm_file, single, 0);
	
	drop_config();
	return 0;
}
