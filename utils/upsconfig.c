#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "data_types.h"
#include "common.h"
#include "../lib/libupsconfig.h"
#include "nutparser.h"
#include <sys/types.h>
#include <pwd.h> 

void print_error(const char* errtxt) {
	fprintf(stderr, "Upsconfig error : %s. Aborting\n", errtxt);
}

void print_usage() {
	printf("usage : upsconfig -d <driver> [-m <mode>] [-p <port>] [-t <target_dir>]\n");
	printf("                  [-b <base_config_dir>] [-u <upsd_server>] [-s] [-h]\n");
	printf("-d, --driver            The driver which support the ups\n");
	printf("-m, --mode              The mode to run NUT in. standalone | net_server |\n");
	printf("                        net_client | pm.\n");
	printf("-p, --port              The port in which the ups is plugged. Needed if the\n");
	printf("                        driver don't support \"auto\". default is \"auto\"\n");
	printf("-t, --target_dir        Where to save the generated configuration. Default\n");
	printf("                        is the default nut configuration dir\n");
	printf("-b, --base_config_dir   The directory where are the templates and comments\n");
	printf("                        files. Default is the default base configuration\n");
	printf("                        directory of nut\n");
	printf("-u, --upsd_server       Where is the upsd server, in host[:port] format\n");
	printf("                        Use it for net_client configuration. Default is\n");
	printf("                        \"localhost\"\n");
	printf("-s, --single            Save the configuration in a single file\n");
	printf("-h, --help              Show this help message\n");
}

int main (int argc, char** argv)  {
	t_modes mode = standalone;
	t_string driver = 0;
	t_string port = "auto";
	t_string target_dir = 0;
	t_string base_config_dir;
	t_string server = 0;
	boolean single = FALSE;
	int i;
	t_string conf_file, comm_file, s;
	FILE* test;
	
	
	// Value by default
	target_dir = CONFPATH;
	base_config_dir = xmalloc(sizeof(char) * (strlen(CONFPATH) + 13));
	sprintf(base_config_dir, "%s/base_config", CONFPATH);
	
	
	
	// Parse the parameters
	for (i = 1; i <= (argc - 1); i++ ) {
		if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mode") == 0) {
			if (i != argc - 1) {
				mode = string_to_mode(argv[++i]);
				if (mode == (unsigned int)-1) {
					print_error("bad parameter of option \"-m\"");
					print_usage();
					exit(EXIT_FAILURE);
				}
			} else {
				print_error("\"mode\" option used without parameter");
				print_usage();
				exit(EXIT_FAILURE);
			}
			continue;
		}
		if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--driver") == 0) {
			if (i != argc - 1) {
				driver = string_copy(argv[++i]);
			} else {
				print_error("\"driver\" option used without parameter");
				print_usage();
				exit(EXIT_FAILURE);
			}
			continue;
		}
		if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
			if (i != argc - 1) {
				port = string_copy(argv[++i]);
			} else {
				print_error("\"port\" option used without parameter");
				print_usage();
				exit(EXIT_FAILURE);
			}
			continue;
		}
		if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--target_dir") == 0) {
			if (i != argc - 1) {
				target_dir = string_copy(argv[++i]);
			} else {
				print_error("\"target_dir\" option used without parameter");
				print_usage();
				exit(EXIT_FAILURE);
			}
			continue;
		}
		if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--base_config_dir") == 0) {
			if (i != argc - 1) {
				free(base_config_dir);
				base_config_dir = string_copy(argv[++i]);
			} else {
				print_error("\"base_config_dir\" option used without parameter");
				print_usage();
				exit(EXIT_FAILURE);
			}
			continue;
		}
		if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--upds_server") == 0) {
			if (i != argc - 1) {
				server = string_copy(argv[++i]);
			} else {
				print_error("\"upsd_server\" option used without parameter");
				print_usage();
				exit(EXIT_FAILURE);
			}
			continue;
		}
		if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--single") == 0) {
			single = TRUE;
			continue;
		}
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			print_usage();
			exit(0);
		}
	}
	
	// Some parameters are mandatory
	if (driver == 0) {
		print_error("\"driver\" option is mandatory");
		print_usage();
		exit(EXIT_FAILURE);
	}
	
	if (mode == net_client && server == 0) {
		print_error("\"upsd_server\" option is mandatory when in net_client mode");
		print_usage();
		exit(EXIT_FAILURE);
	}
	
	
	// Generate the name of the template file to use
	conf_file = xmalloc(sizeof(char) * (strlen(base_config_dir) + 10));
	sprintf(conf_file, "%s/nut.conf", base_config_dir);
	
	// Generate the name of the comments file to use
	s = xmalloc(sizeof(char) * 20);
	strcpy(s, getenv("LANG"));
	s[5] = 0;
	
	comm_file = xmalloc(sizeof(char) * (strlen(base_config_dir) + strlen(s) + 30));
	sprintf(comm_file, "%s/comments/conf.comm.%s", base_config_dir, s);
	
	test = fopen(comm_file, "r");
		
	if (test == 0) {
		s[2] = 0;
		sprintf(comm_file, "%s/comments/conf.comm.%s", base_config_dir, s);
		test = fopen(comm_file, "r");
		if (test == 0) {
			sprintf(comm_file, "%s/comments/conf.comm.C", base_config_dir);
			test = fopen(comm_file, "r");
			if (test == 0) {
				free(comm_file);
				comm_file = 0;
			}
		}
	}

	free(base_config_dir);
	free(s);

	if (test != 0) {
		fclose(test);
	}
	
	// Load and modify the configuration from the template
	printf("\nLoading the base configuration from template %s\n\n", conf_file);
	
	if (!load_config(conf_file, 0)) {
		// An errors occured, aborting
		exit(EXIT_FAILURE);
	}
	
	// Set the mode
	set_mode(mode);
	
	// Fill the ups section with the good values
	search_ups("myups");
	set_driver(driver);
	set_port(port);
	
	set_runasuser(RUN_AS_USER);
	
	if (mode == net_client) {
		// net_client configuration don't need some part of the tree
		remove_user("nutadmin");
		remove_user("monmaster");
		search_user("monslave");
		// To remove the allowfrom variable :
		set_allowfrom(0);
		// Want to modify the host of the monitor rule.
		// Create the one wanted then delete the first
		search_monitor_rule(1);
		s = get_monitor_ups();
		add_monitor_rule(s, server, get_monitor_powervalue(), "monslave");
		free(s);
		remove_monitor_rule(1);
	}
	
	if (mode == standalone) {
		remove_user("monslave");
	}
	
	
	// Save the generated configuration
	printf("Saving your configuration in %s\n", target_dir);
	
	if (comm_file == 0) {
		printf("I did not find any comments template file. Your configuration files will not be commented\n\n");
	} else {
		printf("I'll use the following comments template file to comment your configuration file : %s\n\n", comm_file);
	}
	
	if (!save_config(target_dir, comm_file, single, print_error)) {
		exit(EXIT_FAILURE);
	}
	
	// Print some comments about the configuration
	if (mode == net_server) {
		if (single) {
			printf("#   You are in network server configuration\n\n");
			printf("# Don't forget to modify the acl (in upsd.acl section),\n");
			printf("# the accept and reject variable (both in upsd section)\n");
			printf("# and the allowfrom variable of user monslave (in \n");
			printf("# users.monslave section) to allow him to acces to the.\n");
			printf("# upsd server.\n\n");
		} else {
			printf("#   You are in network server configuration\n\n");
			printf("# Don't forget to modify the acl (in upsd.conf file),\n");
			printf("# the accept and reject variable (also in upsd.conf file)\n");
			printf("# and the allowfrom variable of user monslave (in \n");
			printf("# users.conf, monslave section) to allow him to acces to.\n");
			printf("# the upsd server.\n\n");
		}
	}
	
	if (getuid() != getpwnam("root")->pw_uid) {
		printf("#   You are not root\n");
		printf("# I am not able to modify the ownership and the group of the \n");
		printf("# created file(s).\n\n");
	}
	
	if (single) {
		printf("# !!                                                    #\n");
		printf("# !! DON'T FORGET TO MODIFY PASSWORDS IN USERS SECTION  #\n");
		printf("# !!                                                    #\n");
	} else {
		printf("# !!                                                      #\n");
		printf("# !! DON'T FORGET TO MODIFY PASSWORDS IN users.conf FILE  #\n");
		printf("# !!                                                      #\n");
	}
	
	// free memory
	if (server != 0) free(server);
	if (comm_file != 0) free(comm_file);
	free(conf_file);
	if (target_dir != CONFPATH) free(target_dir);
	free(driver);
	drop_config();
	
	return 0;
}
