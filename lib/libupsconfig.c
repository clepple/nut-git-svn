/*
   Copyright (C) 2006 Jonathan Dion <dion.jonathan@gmail.com>

   This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h> 
#include <grp.h>

#include "libupsconfig.h"
#include "tree.h"
#include "nutparser.h"
#include "data_types.h"
#include "common.h"

struct {
	t_tree ups;
	t_rights rights_in; /* The right to give to value when we set it */
	t_rights rights_out; /* The right value of the last variable get */
	t_tree user;
	t_tree monitor_rule;
} current;

t_tree conf;

void save(t_tree tree, FILE* file, t_string tab, FILE* comm_file);
void write_desc(t_string name, FILE* conf_file, FILE* comm_file);

void libupsconfig_print_error(const t_string errtxt) {
	fprintf(stderr, "Libupsconfig error : %s.\n", errtxt);
}

FILE* open_file(t_string file_name, t_string mode ,void errhandler(const char*)) {
	FILE* file;
	void (*errfunction)(const char*) = errhandler;
	t_string s;
	
	if (errfunction == 0) {
		errfunction = libupsconfig_print_error;
	}
	file = fopen(file_name, mode);
	if (file == 0) {
		s = xmalloc(sizeof(char) * (32 + strlen(file_name)));
		switch (errno) {
			case EACCES : 
				sprintf(s, "Permission denied to access to %s", file_name);
				errfunction(s);
				break;
			case ENOENT :
				sprintf(s, "%s does not exist", file_name);
				errfunction(s);
				break;
			default :
				sprintf(s, "Did not reach to open %s", file_name);
				errfunction(s);
		}
		return 0;
	}
	return file;
}


/***************************************************
 *              INITIALIZATION SECTION             *
 ***************************************************/


void new_config() {
	conf = new_node("nut", 0, 0);
	current.ups = 0;
	current.rights_in = admin_r;
	current.user = 0;
	current.monitor_rule = 0;
}

int load_config(t_string filename, void errhandler(const char*)) {
	conf = parse_conf(filename, errhandler);
	current.ups = 0;
	current.rights_in = admin_r;
	current.user = 0;
	current.monitor_rule = 0;
	if (conf == 0) return 0;
	return 1;
}

void drop_config() {
	if (conf != 0) {
		free_tree(conf);	
	}
	conf = 0;
	current.ups = 0;
	current.rights_in = admin_r;
	current.user = 0;
	current.monitor_rule = 0;
}

/***************************************************
 *                 SAVING SECTION                  *
 ***************************************************/

static void write_section( t_string section, t_string filename, FILE* comm_file, boolean is_root_user, void errhandler(const char*)) {	
	FILE* conf_file;
	t_tree son;
	struct group* grp;
	
	conf_file = open_file( filename, "w", errhandler);
			
	if (conf_file == 0) {
		return;
	}
			
	son = tree_search(conf, section, 1);
	if (son == 0) return;
	son = son->son;
	write_desc( section, conf_file, comm_file);
	while (son != 0) {
		save(son, conf_file, "", comm_file);
		fwrite("\n", 1, 1, conf_file);
		son = son->next_brother;
	}
	fclose(conf_file);
	if (is_root_user) {
		grp = getgrnam(RUN_AS_USER);
		if (grp == 0) {
			grp = getgrnam("root");
		}
		chown(filename, getpwnam("root")->pw_uid, grp->gr_gid);
	}
	chmod(filename, S_IRUSR | S_IWUSR | S_IRGRP);
}

int is_fatal_error(t_string fn, t_string new_fn, void errhandler(const char*)) {
	char s[300];
	
	switch (errno) {
		case ENOENT : return 0;
		case EEXIST : 
			sprintf(s, "file %s : %m", new_fn);
			errhandler(s);
			return 1;
		default :
			sprintf(s, "file %s, trying to save as %s : %m", fn, new_fn);
			errhandler(s);
			return 1;
	}
}

int save_existant_files(t_string directory, void errhandler(const char*)) {
	t_string fn, new_fn;
	int error = 0;
	int ret;
	struct stat *st = malloc(sizeof(struct stat));
	
	fn = xmalloc(sizeof(char) * (strlen(directory) + 50));
	new_fn = xmalloc(sizeof(char) * (strlen(directory) + 50));
	
	
	sprintf(fn, "%s/nut.conf", directory);
	sprintf(new_fn, "%s/nut.conf.save", directory);
	
	ret = stat(fn, st);
	if (ret == 0 || errno != ENOENT) {
		if (link(fn, new_fn) == -1 && is_fatal_error(fn, new_fn, errhandler)) {
			error = 1;
		} else if (unlink(fn) == -1 && is_fatal_error(fn, new_fn, errhandler)) {
			error = 1;
		}
	}
	
	sprintf(fn, "%s/ups.conf", directory);
	sprintf(new_fn, "%s/ups.conf.save", directory);
	ret = stat(fn, st);
	if (ret == 0 || errno != ENOENT) {
		if (link(fn, new_fn) == -1 && is_fatal_error(fn, new_fn, errhandler)) {
			error = 1;
		} else if (unlink(fn) == -1 && is_fatal_error(fn, new_fn, errhandler)) {
			error = 1;
		}
	}
	
	sprintf(fn, "%s/users.conf", directory);
	sprintf(new_fn, "%s/users.conf.save", directory);
	ret = stat(fn, st);
	if (ret == 0 || errno != ENOENT) {
		if (link(fn, new_fn) == -1 && is_fatal_error(fn, new_fn, errhandler)) {
			error = 1;
		} else if (unlink(fn) == -1 && is_fatal_error(fn, new_fn, errhandler)) {
			error = 1;
		}
	}
	
	sprintf(fn, "%s/upsd.conf", directory);
	sprintf(new_fn, "%s/upsd.conf.save", directory);
	ret = stat(fn, st);
	if (ret == 0 || errno != ENOENT) {
		if (link(fn, new_fn) == -1 && is_fatal_error(fn, new_fn, errhandler)) {
			error = 1;
		} else if (unlink(fn) == -1 && is_fatal_error(fn, new_fn, errhandler)) {
			error = 1;
		}
	}
	
	sprintf(fn, "%s/upsmon.conf", directory);
	sprintf(new_fn, "%s/upsmon.conf.save", directory);
	ret = stat(fn, st);
	if (ret == 0 || errno != ENOENT) {
		if (link(fn, new_fn) == -1 && is_fatal_error(fn, new_fn, errhandler)) {
			error = 1;
		} else if (unlink(fn) == -1 && is_fatal_error(fn, new_fn, errhandler)) {
			error = 1;
		}
	}
	
	if (error) {
		errhandler("Unable to successfully save your old configurations files.");
		return 0;
	}
	return 1;
}

int save_config(t_string directory_dest, t_string comm_filename, boolean single, void errhandler(const char*)) {
	t_string nut_conf;
	t_string s;
	FILE *conf_file, *comm_file = 0;
	t_modes mode;
	t_tree son;
	boolean is_root_user = FALSE;
	struct group* grp;
	
	if (errhandler == 0) {
		errhandler = libupsconfig_print_error;
	}
	
	if (!save_existant_files(directory_dest, errhandler)) {
		return 0;
	}
	
	nut_conf = xmalloc(sizeof(char) * (strlen(directory_dest) + 20));
	sprintf(nut_conf, "%s/nut.conf", directory_dest);
	conf_file = open_file(nut_conf, "w", errhandler);
	
	if (conf_file == 0) {
		return 0;
	}
	
	if (comm_filename != 0) {
		comm_file = open_file(comm_filename, "r", errhandler);
	}
	
	is_root_user = (getuid() == getpwnam("root")->pw_uid);
	
	if (single == TRUE) {
		son = conf->son;
		write_desc("nut", conf_file, comm_file);
		while (son != 0) {
			save(son, conf_file, "", comm_file);
			fwrite("\n", 1, 1, conf_file);
			son = son->next_brother;
		}
		fclose(conf_file);
		if (is_root_user) {
			grp = getgrnam(RUN_AS_USER);
			if (grp == 0) {
				grp = getgrnam("root");
			}
			chown(nut_conf, getpwnam("root")->pw_uid, grp->gr_gid);
		}
		chmod(nut_conf, S_IRUSR | S_IWUSR | S_IRGRP);
		
		
	} else { /* Not single file mode*/
		mode = string_to_mode((tree_search(conf, "nut.mode", 1))->value.string_value);
		switch (mode) {
			case standalone :
				write_desc("nut", conf_file, comm_file);
				s = xmalloc(sizeof(char) * ( 50 + strlen(directory_dest)));
				write_desc("nut.mode", conf_file, comm_file);
				fwrite("mode = \"standalone\"\n\n", 21, 1, conf_file);
				
				write_desc("nut.ups_header_only", conf_file, comm_file);
				sprintf(s, "ups (\n\tinclude \"%s/ups.conf\"\n)\n\n", directory_dest);
				fwrite(s, strlen(s), 1, conf_file);
				
				write_desc("nut.users_header_only", conf_file, comm_file);
				sprintf(s, "users (\n\tinclude \"%s/users.conf\"\n)\n\n", directory_dest);
				fwrite(s, strlen(s), 1, conf_file);
				
				write_desc("nut.upsd", conf_file, comm_file);
				sprintf(s, "upsd (\n\tinclude \"%s/upsd.conf\"\n)\n\n", directory_dest);
				fwrite(s, strlen(s), 1, conf_file);
				
				write_desc("nut.upsmon", conf_file, comm_file);
				sprintf(s, "upsmon (\n\tinclude \"%s/upsmon.conf\"\n)\n\n", directory_dest);
				fwrite(s, strlen(s), 1, conf_file);
				fclose(conf_file);
				if (is_root_user) {
					grp = getgrnam(RUN_AS_USER);
					if (grp == 0) {
						grp = getgrnam("root");
					}
					chown(nut_conf, getpwnam("root")->pw_uid, grp->gr_gid);
				}
				chmod(nut_conf, S_IRUSR | S_IWUSR | S_IRGRP);
				
				sprintf(s, "%s/ups.conf", directory_dest);
				write_section("nut.ups", s, comm_file, is_root_user, errhandler);
				
				sprintf(s, "%s/users.conf", directory_dest);
				write_section("nut.users", s, comm_file, is_root_user, errhandler);

				sprintf(s, "%s/upsd.conf", directory_dest);
				write_section("nut.upsd", s, comm_file, is_root_user, errhandler);

				sprintf(s, "%s/upsmon.conf", directory_dest);
				write_section("nut.upsmon", s, comm_file, is_root_user, errhandler);

				
				free(s);
				break;
			case net_server :
				write_desc("nut", conf_file, comm_file);
				s = xmalloc(sizeof(char) * ( 50 + strlen(directory_dest)));
				write_desc("nut.mode", conf_file, comm_file);
				fwrite("mode = \"net_server\"\n\n", 21, 1, conf_file);
				
				write_desc("nut.ups_header_only", conf_file, comm_file);
				sprintf(s, "ups (\n\tinclude \"%s/ups.conf\"\n)\n\n", directory_dest);
				fwrite(s, strlen(s), 1, conf_file);
				
				write_desc("nut.users_header_only", conf_file, comm_file);
				sprintf(s, "users (\n\tinclude \"%s/users.conf\"\n)\n\n", directory_dest);
				fwrite(s, strlen(s), 1, conf_file);
				
				write_desc("nut.upsd", conf_file, comm_file);
				sprintf(s, "upsd (\n\tinclude \"%s/upsd.conf\"\n)\n\n", directory_dest);
				fwrite(s, strlen(s), 1, conf_file);
				
				write_desc("nut.upsmon", conf_file, comm_file);
				sprintf(s, "upsmon (\n\tinclude \"%s/upsmon.conf\"\n)\n\n", directory_dest);
				fwrite(s, strlen(s), 1, conf_file);
				fclose(conf_file);
				if (is_root_user) {
					grp = getgrnam(RUN_AS_USER);
					if (grp == 0) {
						grp = getgrnam("root");
					}
					chown(nut_conf, getpwnam("root")->pw_uid, grp->gr_gid);
				}
				chmod(nut_conf, S_IRUSR | S_IWUSR | S_IRGRP);
				
				sprintf(s, "%s/ups.conf", directory_dest);
				write_section("nut.ups", s, comm_file, is_root_user, errhandler);
				
				sprintf(s, "%s/users.conf", directory_dest);
				write_section("nut.users", s, comm_file, is_root_user, errhandler);

				sprintf(s, "%s/upsd.conf", directory_dest);
				write_section("nut.upsd", s, comm_file, is_root_user, errhandler);

				sprintf(s, "%s/upsmon.conf", directory_dest);
				write_section("nut.upsmon", s, comm_file, is_root_user, errhandler);
				
				
				free(s);
				break;
				
				
				
			case net_client :
				write_desc("nut", conf_file, comm_file);
				s = xmalloc(sizeof(char) * ( 50 + strlen(directory_dest)));
				write_desc("nut.mode", conf_file, comm_file);
				fwrite("mode = \"net_client\"\n\n", 21, 1, conf_file);
				
				write_desc("nut.users_header_only", conf_file, comm_file);
				sprintf(s, "users (\n\tinclude \"%s/users.conf\"\n)\n\n", directory_dest);
				fwrite(s, strlen(s), 1, conf_file);
				
				write_desc("nut.upsmon", conf_file, comm_file);
				sprintf(s, "upsmon (\n\tinclude \"%s/upsmon.conf\"\n)\n\n", directory_dest);
				fwrite(s, strlen(s), 1, conf_file);
				fclose(conf_file);
				if (is_root_user) {
					grp = getgrnam(RUN_AS_USER);
					if (grp == 0) {
						grp = getgrnam("root");
					}
					chown(nut_conf, getpwnam("root")->pw_uid, grp->gr_gid);
				}
				chmod(nut_conf, S_IRUSR | S_IWUSR | S_IRGRP);
				
				sprintf(s, "%s/users.conf", directory_dest);
				write_section("nut.users", s, comm_file, is_root_user, errhandler);

				sprintf(s, "%s/upsmon.conf", directory_dest);
				write_section("nut.upsmon", s, comm_file, is_root_user, errhandler);
				
				free(s);
				break;
				
			case pm :
				write_desc("nut", conf_file, comm_file);
				s = xmalloc(sizeof(char) * ( 50 + strlen(directory_dest)));
				write_desc("nut.mode", conf_file, comm_file);
				fwrite("mode = \"pm\"\n\n", 13, 1, conf_file);
				fclose(conf_file);
				if (is_root_user) {
					grp = getgrnam(RUN_AS_USER);
					if (grp == 0) {
						grp = getgrnam("root");
					}
					chown(nut_conf, getpwnam("root")->pw_uid, grp->gr_gid);
				}
				chmod(nut_conf, S_IRUSR | S_IWUSR | S_IRGRP);
				
				sprintf(s, "%s/ups.conf", directory_dest);
				write_section("nut.ups", s, comm_file, is_root_user, errhandler);
				
				free(s);
				break;
				
			case no_mode :
				write_desc("nut", conf_file, comm_file);
				write_desc("nut.mode", conf_file, comm_file);
				fwrite("mode = \"none\"\n\n", 15, 1, conf_file);
				fclose(conf_file);
				if (is_root_user) {
					grp = getgrnam(RUN_AS_USER);
					if (grp == 0) {
						grp = getgrnam("root");
					}
					chown(nut_conf, getpwnam("root")->pw_uid, grp->gr_gid);
				}
				chmod(nut_conf, S_IRUSR | S_IWUSR | S_IRGRP);
				
			default : 
				return 0;
		}
	}
	free(nut_conf);
	return 1;
}



/***************************************************
 *              GENERAL LEVEL SECTION              *
 ***************************************************/

t_modes get_mode() {
	t_tree t;
	
	t = tree_search(conf, "nut.mode", TRUE);
	if (t != 0 && t->has_value && t->type == string_type) {
		current.rights_out = t->right;
		return string_to_mode(t->value.string_value);
	}
	return -1;
}

void set_mode(t_modes mode) {
	t_string s;
	
	if (current.rights_in != invalid_right) {
		s = mode_to_string(mode);
		add_to_tree(conf, "nut.mode", s, string_type, current.rights_in);
	}
	
}

t_rights get_rights() {
	return current.rights_out;
}

void set_rights(t_rights right) {
	current.rights_in = right;
}

t_typed_value get_variable(t_string varname) {
	t_tree t;
	t_typed_value value;
	
	if (varname != 0) {
		t = tree_search(conf, varname, TRUE);
		if (t == 0) {
			value.has_value = FALSE;
			value.type = string_type; /* To avoid a warning; */
			return value;
		}
		value.has_value = t->has_value;
		value.type = t->type;
		if (value.has_value) {
			value.value = t->value;
		}
		current.rights_out = t->right;
		return value;
	}
	value.has_value = FALSE;
	value.type = string_type; /* To avoid a warning; */
	return value;
}

void set_variable(t_string varname, void* value, t_types type) {
	if (current.rights_in != invalid_right && varname != 0) {
		if (value == 0) {
			del_from_tree(conf, varname);
		} else {
			add_to_tree(conf, varname, value, type, current.rights_in);
		}
	}	
}

t_enum_string get_variable_list(t_string path) {
	t_enum_string enum_string;
	t_tree t;
	t_string s;
	
	t = tree_search(conf, path, TRUE);
	if (t != 0) {
		t = t->son;
		enum_string = 0;
		while (t != 0) {
			s = extract_last_part(t->name);
			enum_string = add_to_enum_string(enum_string, s);
			free(s);
			t = t->next_brother;
		}
		
		return enum_string;
	}
	return 0;
}

t_string get_comments_template(t_string directory) {
	t_string s;
	t_string comments_dir;
	t_string comm_file;
	FILE* test;
	
	if (directory == 0) {
		comments_dir = xmalloc(sizeof(char) * (strlen(CONFPATH) + 34));
		sprintf(comments_dir, "%s/base_config/comments", CONFPATH);
	} else {
		comments_dir = directory;
	}
	
	/* Generate the name of the comments file to use */
	s = xmalloc(sizeof(char) * 20);
	strcpy(s, getenv("LANG"));
	s[5] = 0;

	comm_file = xmalloc(sizeof(char) * (strlen(comments_dir) + strlen(s) + 30));
	sprintf(comm_file, "%s/conf.comments.%s", comments_dir, s);
	
	test = fopen(comm_file, "r");
	
	if (test == 0) {
		s[2] = 0;
		sprintf(comm_file, "%s/conf.comments.%s", comments_dir, s);
		test = fopen(comm_file, "r");
		if (test == 0) {
			sprintf(comm_file, "%s/conf.comments.C", comments_dir);
			test = fopen(comm_file, "r");
			if (test == 0) {
				free(comm_file);
				comm_file = 0;
			}
		}
	}
	free(s);
	if (test != 0) {
		fclose(test);
	}
	if (directory == 0) {
		free(comments_dir);
	}
	
	return comm_file;
}

/***************************************************
 *                   UPS SECTION                   *
 ***************************************************/


t_enum_string get_ups_list() {
	t_enum_string enum_string;
	t_tree t;
	t_string s;
	
	t = tree_search(conf, "nut.ups", TRUE);
	if (t != 0) {
		t = t->son;
		enum_string = 0;
		while (t != 0) {
			s = extract_last_part(t->name);
			if (strcmp(s, "global") == 0) {
				t = t->next_brother;
				continue;
			}
			enum_string = add_to_enum_string(enum_string, s);
			free(s);
			t = t->next_brother;
		}
		
		return enum_string;
	}
	return 0;
}

void add_ups(t_string upsname, t_string driver, t_string port) {
	t_string s;
	
	s = xmalloc(sizeof(char) * ( 31 + strlen(upsname)));
	sprintf(s, "nut.ups.%s.driver.name", upsname); 
	add_to_tree(conf, s, driver, string_type, current.rights_in);
	sprintf(s, "nut.ups.%s.driver.parameter.port", upsname);
	add_to_tree(conf, s, port, string_type, current.rights_in);
	sprintf(s, "nut.ups.%s", upsname);
	current.ups = tree_search(conf, s, TRUE);
	free(s);
}

int remove_ups(t_string upsname) {
	t_string s;
	int i;
	
	s = xmalloc(sizeof(char) * ( 9 + strlen(upsname)));
	sprintf(s, "nut.ups.%s", upsname);
	if (tree_search(conf,s, TRUE) == current.ups) current.ups = 0;
	i = del_from_tree(conf, s);
	free(s);
	return i;
	
}

int search_ups(t_string upsname) {
	t_tree t;
	t_string s;
	
	s = xmalloc(sizeof(char) * ( 9 + strlen(upsname)));
	sprintf(s, "nut.ups.%s", upsname);
	t = tree_search(conf, s, TRUE);
	free(s);
	
	if (t != 0) {
		current.ups = t;
		return 1;
	} else {
		current.ups = 0;
		return 0;
	}
}


t_string get_ups_name() {
	if (current.ups != 0) {
		return extract_last_part(current.ups->name);
	}	
	return 0;
}

void set_ups_name(t_string upsname) {
	if (current.ups != 0 && current.rights_in != invalid_right) {
		
		free(current.ups->name);
		current.ups->name = string_copy(upsname);
	}	
}

t_string get_driver() {
	t_tree t;
	t_string s;
	
	if (current.ups != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + 13));
		sprintf( s, "%s.driver.name", current.ups->name);
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0 || t->type != string_type) return 0;
		return string_copy(t->value.string_value);
	}
	return 0;
}

void set_driver(t_string driver) {
	t_string s;
	
	if (current.ups != 0 && current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + 13));
		sprintf( s, "%s.driver.name", current.ups->name);
		add_to_tree(conf, s, driver, string_type, current.rights_in);
		free(s);
	}
}

t_string get_port() {
	t_tree t;
	t_string s;
	
	if (current.ups != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + 23));
		sprintf( s, "%s.driver.parameter.port", current.ups->name);
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0 || t->type != string_type) return 0;
		current.rights_out = t->right;
		return string_copy(t->value.string_value);
	}
	return 0;
}

void set_port(t_string driver) {
	t_string s;
	
	if (current.ups != 0 && current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + 23));
		sprintf( s, "%s.driver.parameter.port", current.ups->name);
		add_to_tree(conf, s, driver, string_type, current.rights_in);
		free(s);
	}
}

t_string get_desc() {
	t_tree t;
	t_string s;
	
	if (current.ups != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + 6));
		sprintf( s, "%s.desc", current.ups->name);
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0 || t->type != string_type) return 0;
		current.rights_out = t->right;
		return string_copy(t->value.string_value);
	}
	return 0;
}

void set_desc(t_string desc) {
	t_string s;
	
	if (current.ups != 0 && current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + 6));
		sprintf( s, "%s.desc", current.ups->name);
		if (desc == 0) {
			del_from_tree(conf, s);
		} else {
			add_to_tree(conf, s, desc, string_type, current.rights_in);
		}
		free(s);
	}
}


t_enum_string get_driver_parameter_list() {
	t_enum_string enum_string;
	t_tree t;
	t_string s;
	
	
	if (current.ups != 0) {
		enum_string = 0;
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + 20));
		sprintf(s, "%s.driver.parameter", current.ups->name);
		t =tree_search(conf, s, TRUE);
		free(s);
		if (t == 0) return 0;
		t = t->son;
		while (t !=0) {
			enum_string = add_to_enum_string(enum_string, extract_last_part(t->name));
			t = t->next_brother;
		}
		return enum_string;
		
	}
	return 0;
}

t_typed_value get_driver_parameter(t_string paramname) {
	t_tree t;
	t_string s;
	t_typed_value value;
	
	if (current.ups != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + strlen(paramname) + 21));
		sprintf( s, "%s.driver.parameter.%s", current.ups->name, paramname);
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0) {
			value.has_value = FALSE;
			value.type = string_type; /* To avoid a warning */
			return value;
		}
		value.has_value = t->has_value;
		if (value.has_value) {
			switch (t->type) {
				case string_type : 
					value.value.string_value = string_copy(t->value.string_value);
					break;
				case enum_string_type :
					value.value.enum_string_value = enum_string_copy(t->value.enum_string_value);
					break;
				default : value.has_value = FALSE;
			}
		}
		value.type = t->type;
		current.rights_out = t->right;
		return value;
	}
	value.has_value = FALSE;
	value.type = string_type; /* To avoid a warning; */
	return value;
}

void set_driver_parameter(t_string paramname, void* value, t_types type) {
	t_string s;
	
	if (current.ups != 0 && current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * (strlen(paramname) + strlen(current.ups->name) + 21));
		sprintf(s, "%s.driver.parameter.%s", current.ups->name, paramname);
		if (value == 0) {
			del_from_tree(conf, s);
		} else {
			add_to_tree(conf, s, value, type, current.rights_in);
		}
		free(s);
	}	
}

t_enum_string get_driver_flag_list() {
	t_enum_string enum_string;
	t_tree t;
	t_string s;
	
	if (current.ups != 0) {
		enum_string = 0;
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + 20));
		sprintf(s, "%s.driver.flag", current.ups->name);
		t =tree_search(conf, s, TRUE);
		free(s);
		if (t == 0) return 0;
		t = t->son;
		while (t !=0) {
			enum_string = add_to_enum_string(enum_string, extract_last_part(t->name));
			t = t->next_brother;
		}
		return enum_string;	
	}
	return 0;
}
	
void enable_driver_flag(t_string flagname) {
	t_string s;
	
	if (current.ups != 0 && current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * (strlen(flagname) + strlen(current.ups->name) + 21));
		sprintf(s, "%s.driver.flag.%s", current.ups->name, flagname);
		add_to_tree(conf, s, "enabled", string_type, current.rights_in);
		free(s);
	}	
}

void disable_driver_flag(t_string flagname) {
	t_string s;
	
	if (current.ups != 0 && current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * (strlen(flagname) + strlen(current.ups->name) + 21));
		sprintf(s, "%s.driver.flag.%s", current.ups->name, flagname);
		del_from_tree(conf, s);
		free(s);
	}	
}

t_enum_string get_ups_variable_list() {
	t_enum_string enum_string;
	t_tree t;
	
	if (current.ups != 0) {
		enum_string = 0;
		t =tree_search(conf, current.ups->name, TRUE);
		if (t == 0 ) return 0;
		t = t->son;
		while (t !=0) {
			enum_string = add_to_enum_string(enum_string, extract_last_part(t->name));
			t = t->next_brother;
		}
		return enum_string;	
	}
	return 0;
}

t_enum_string get_ups_subvariable_list(t_string varname) {
	t_enum_string enum_string;
	t_tree t;
	t_string s;
	
	if (current.ups != 0) {
		enum_string = 0;
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + strlen(varname) + 2));
		sprintf(s, "%s.%s", current.ups->name, varname);
		t =tree_search(conf, s, TRUE);
		free(s);
		if (t == 0) return 0;
		t = t->son;
		while (t !=0) {
			enum_string = add_to_enum_string(enum_string, extract_last_part(t->name));
			t = t->next_brother;
		}
		return enum_string;	
	}
	return 0;
}

t_typed_value get_ups_variable(t_string varname) {
	t_tree t;
	t_string s;
	t_typed_value value;
	
	if (current.ups != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + strlen(varname) + 2));
		sprintf( s, "%s.%s", current.ups->name, varname);
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0) {
			value.has_value = FALSE;
			value.type = string_type; /* To avoid a warning; */
			return value;
		}
		value.has_value = t->has_value;
		value.type = t->type;
		if (value.has_value) {
			value.value = t->value;
		}
		current.rights_out = t->right;
		return value;
	}
	value.has_value = FALSE;
	value.type = string_type; /* To avoid a warning; */
	return value;
}

void set_ups_variable(t_string varname, void* value, t_types type) {
	t_string s;
	
	if (current.ups != 0 && current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * (strlen(varname) + strlen(current.ups->name) + 2));
		sprintf(s, "%s.%s", current.ups->name, varname);
		if (value == 0) {
			del_from_tree(conf, s);
		} else {
			add_to_tree(conf, s, value, type, current.rights_in);
		}
		free(s);
	}	
}







/***************************************************
 *                  USERS SECTION                  *
 ***************************************************/

t_enum_string get_users_list() {
	t_enum_string enum_string;
	t_tree t;
	t_string s;
	
	t = tree_search(conf, "nut.users", TRUE);
	if (t != 0) {
		t = t->son;
		enum_string = 0;
		while (t != 0) {
			s = extract_last_part(t->name);
			enum_string = add_to_enum_string(enum_string, s);
			free(s);
			t = t->next_brother;
		}
		return enum_string;
	}
	return 0;
}
	
	

void add_user(t_string username, t_user_types type, t_string password) {
	t_string s;
	
	s = xmalloc(sizeof(char) * ( 20 + strlen(username)));
	sprintf(s, "nut.users.%s.type", username);
	add_to_tree(conf, s, user_type_to_string(type), string_type, current.rights_in);
	sprintf(s, "nut.users.%s.password", username);
	add_to_tree(conf, s, password, string_type, current.rights_in);
	sprintf(s, "nut.users.%s", username);
	current.user = tree_search(conf, s, TRUE);
	free(s);	
}
	
	
int remove_user(t_string username) {
	t_string s;
	int i;
	
	s = xmalloc(sizeof(char) * ( 11 + strlen(username)));
	sprintf(s, "nut.users.%s", username);
	if (current.user == tree_search(conf,s,TRUE)) current.user = 0;
	i = del_from_tree(conf, s);
	free(s);
	current.user = 0;
	return i;	
}
	
int search_user(t_string username) {
	t_tree t;
	t_string s;
	
	s = xmalloc(sizeof(char) * ( 11 + strlen(username)));
	sprintf(s, "nut.users.%s", username);
	t = tree_search(conf, s, TRUE);
	free(s);
	
	if (t != 0) {
		current.user = t;
		return 1;
	} else {
		current.user = 0;
		return 0;
	}
}


t_string get_name() {
	if (current.user != 0) {
		return extract_last_part(current.user->name);
	}	
	return 0;
}


/* NIY : It need to modify the name of each sons of the user. Use add_user then remove_user for the moment
void set_name(t_string username) {
	if (current.user != 0 && current.rights_in != invalid_right) {
		free(current.user->name);
		current.user->name = string_copy(username);
	}	
} */


t_user_types get_type() {
	t_tree t;
	t_string s;
	
	if (current.user != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 6));
		sprintf( s, "%s.type", current.user->name);
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0  || t->type != string_type) return 0;
		current.rights_out = t->right;
		return string_to_user_type(t->value.string_value);
	}
	return 0;
}


void set_type(t_user_types type) {
	t_string s;
	
	if (current.user != 0 && current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 6));
		sprintf( s, "%s.type", current.user->name);
		add_to_tree(conf, s, user_type_to_string(type), string_type, current.rights_in);
		free(s);
	}
}



t_string get_password() {
	t_tree t;
	t_string s;
	
	if (current.user != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 10));
		sprintf( s, "%s.password", current.user->name);
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0  || t->type != string_type) return 0;
		return string_copy(t->value.string_value);
	}
	return 0;
}


void set_password(t_string password) {
	t_string s;
	
	if (current.user != 0 && current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 10));
		sprintf( s, "%s.password", current.user->name);
		add_to_tree(conf, s, password, string_type, current.rights_in);
		free(s);
	}
}


t_enum_string get_allowfrom() {
	t_tree t;
	t_string s;
	
	if (current.user != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 11));
		sprintf( s, "%s.allowfrom", current.user->name);
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0  || t->type != enum_string_type) return 0;
		if (t->type == enum_string_type) {
			current.rights_out = t->right;
			return enum_string_copy(t->value.enum_string_value);
		}
	}
	return 0;
}


void set_allowfrom(t_enum_string acllist) {
	t_string s;
	
	if (current.user != 0 && current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 11));
		sprintf( s, "%s.allowfrom", current.user->name);
		if (acllist == 0) {
			del_from_tree(conf, s);
		} else {
			add_to_tree(conf, s, acllist, enum_string_type, current.rights_in);
		}
		free(s);
	}
}


t_enum_string get_actions() {
	t_tree t;
	t_string s;
	
	if (current.user != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 9));
		sprintf( s, "%s.actions", current.user->name);
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0  || t->type != enum_string_type) return 0;
		if (t->type == enum_string_type) {
			current.rights_out = t->right;
			return enum_string_copy(t->value.enum_string_value);
		}
	}
	return 0;
}


void set_actions(t_enum_string actionlist) {
	t_string s;
	
	if (current.user != 0 && current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 9));
		sprintf( s, "%s.actions", current.user->name);
		if (actionlist == 0) {
			del_from_tree(conf, s);
		} else {
			add_to_tree(conf, s, actionlist, enum_string_type, current.rights_in);
		}
		free(s);
	}
}


t_enum_string get_instcmds() {
	t_tree t;
	t_string s;
	
	if (current.user != 0 && current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 10));
		sprintf( s, "%s.instcmds", current.user->name);
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0  || t->type != enum_string_type) return 0;
		if (t->type == enum_string_type) {
			current.rights_out = t->right;
			return enum_string_copy(t->value.enum_string_value);
		}
	}
	return 0;
}


void set_instcmds(t_enum_string instcmdlist) {
	t_string s;
	
	if (current.user != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 10));
		sprintf( s, "%s.instcmds", current.user->name);
		if (instcmdlist == 0) {
			del_from_tree(conf, s);
		} else {
			add_to_tree(conf, s, instcmdlist, enum_string_type, current.rights_in);
		}
		free(s);
	}
}


/***************************************************
 *                   UPSD SECTION                  *
 ***************************************************/

t_enum_string get_acl_list() {
	t_enum_string enum_string;
	t_tree t;
	t_string s;
	
	t = tree_search(conf, "nut.upsd.acl", TRUE);
	if (t != 0) {
		t = t->son;
		enum_string = 0;
		while (t != 0) {
			s = extract_last_part(t->name);
			enum_string = add_to_enum_string(enum_string, s);
			free(s);
			t = t->next_brother;
		}
		return enum_string;
	}
	return 0;
}	
	
int remove_acl(t_string aclname) {
	t_string s;
	int i;
	
	s = xmalloc(sizeof(char) * ( 14 + strlen(aclname)));
	sprintf(s, "nut.upsd.acl.%s", aclname);
	i = del_from_tree(conf, s);
	free(s);
	return i;	
}


t_string get_acl_value(t_string aclname) {
	t_tree t;
	t_string s;
	
	s = xmalloc(sizeof(char) * (strlen(aclname) + 14));
	sprintf( s, "nut.upsd.acl.%s", aclname);
	t = tree_search(conf, s, TRUE);
	free(s);
	if (t == 0  || t->type != string_type) return 0;
	current.rights_out = t->right;
	return string_copy(t->value.string_value);
}


void set_acl_value(t_string aclname, t_string value) {
	t_string s;
	
	if (current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * (strlen(aclname) + 14));
		sprintf( s, "nut.upsd.acl.%s", aclname);
		add_to_tree(conf, s, value, string_type, current.rights_in);
		free(s);
	}
}


t_enum_string get_accept() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsd.accept", TRUE);
	if (t == 0  || t->type != enum_string_type) return 0;
	if (t->type == enum_string_type) {
		current.rights_out = t->right;
		return enum_string_copy(t->value.enum_string_value);
	}
	return 0;
}


void set_accept(t_enum_string acllist) {
	if (current.rights_in != invalid_right) {
		add_to_tree(conf, "nut.upsd.accept", acllist, enum_string_type, current.rights_in);
	}
}


t_enum_string get_reject() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsd.reject", TRUE);
	if (t == 0  || t->type != enum_string_type) return 0;
	if (t->type == enum_string_type) {
		current.rights_out = t->right;
		return enum_string_copy(t->value.enum_string_value);
	}
	return 0;
}


void set_reject(t_enum_string acllist) {
	if (current.rights_in != invalid_right) {
		add_to_tree(conf, "nut.upsd.reject", acllist, enum_string_type, current.rights_in);
	}
}


int get_maxage() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsd.maxage", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		current.rights_out = t->right;
		return atoi(t->value.string_value);
	}
	return 0;
}


void set_maxage(int value) {
	t_string s;
	
	if (current.rights_in != invalid_right) {
		/* an int can need 10 char to be represented (without the sign)
		 I put 15 to be sure */
		s = xmalloc(sizeof(char) * 15);
		sprintf(s, "%d", value);
		add_to_tree(conf, "nut.upsd.maxage", s, string_type, current.rights_in);
		free(s);
	}
}


/***************************************************
 *                 UPSMON SECTION                  *
 ***************************************************/


int get_number_of_monitor_rules() {
	t_tree t;
	int i = 0;
	
	t = tree_search(conf, "nut.upsmon.monitor", TRUE);
	if (t != 0) {
		t = t->son;
		while (t != 0) {
			i++;
			t = t->next_brother;
		}
		return i;
	}
	return 0;
}

/* There is a little problem if the host of a monitor rule*/ 
/* Contains dot (interpreted as tree node) This function  */
/* pass the node until it arrive to the node that contains*/
/* powervalue and user informations                       */

/* The parameter is the node to begin the search from     */
t_tree find_monitor_rule(t_tree begining) {
	t_string s;
	t_tree t;
	
	if (begining == 0) return 0;
	
	t = begining->son;
	
	if (t == 0) return 0;
	
	s = extract_last_part(t->name);
	if (strcmp(s, "powervalue") == 0 )  {
		if (t->next_brother == 0) return find_monitor_rule(t);
		free(s);
		s = extract_last_part(t->next_brother->name);
		if (strcmp(s, "user") == 0) {
			free(s);
			return begining;
		}
	}
	free(s);
	return find_monitor_rule(t);
}

/* created in last. */
void add_monitor_rule(t_string upsname, t_string host, int powervalue, t_string username) {
	t_string s, s2;
	
	s = xmalloc(sizeof(char) * ( 32 + strlen(upsname) + strlen(host)));
	sprintf(s, "nut.upsmon.monitor.%s@%s.powervalue", upsname, host);
	s2 = xmalloc(sizeof(char) * 15);
	sprintf(s2, "%d", powervalue);
	add_to_tree(conf, s, s2, string_type, current.rights_in);
	free(s2);
	sprintf(s, "nut.upsmon.monitor.%s@%s.user", upsname, host);
	add_to_tree(conf, s, username, string_type, current.rights_in);
	sprintf(s, "nut.upsmon.monitor.%s@%s", upsname, host);
	current.monitor_rule = tree_search(conf, s, TRUE);
	free(s);
}
	
	
int remove_monitor_rule(int rulenumber) {
	t_tree t, temp;
	int i = 0;
	
	t = tree_search(conf, "nut.upsmon.monitor", TRUE);
	if (t != 0) {
		t = t->son;
		while (t != 0) {
			i++;
			if (i == rulenumber) {
				temp = find_monitor_rule(t);
				if (temp == current.monitor_rule) current.monitor_rule = 0;
				i = del_from_tree(conf, t->name);	
				return i;
			}
			t = t->next_brother;
		}
		return 0;
	}
	return 0;
}


int search_monitor_rule(int rulenumber) {
	t_tree t;
	int i = 0;
	
	t = tree_search(conf, "nut.upsmon.monitor", TRUE);
	if (t != 0) {
		t = t->son;
		while (t != 0) {
			i++;
			if (i == rulenumber) {
				current.monitor_rule = find_monitor_rule(t);
				return 1;
			}
			t = t->next_brother;
		}
		return 0;
	}
	return 0;
}

t_string get_monitor_system() {
	if (current.monitor_rule != 0) {
		return string_copy(current.monitor_rule->name + 19);
	}
	return 0;
}

t_string get_monitor_ups() {
	t_string s, s2;
	int i = 0;
	
	if (current.monitor_rule != 0) {
		s = xmalloc(sizeof(char) * strlen(current.monitor_rule->name));
		s = extract_last_part(current.monitor_rule->name);
		s2 = xmalloc(sizeof(char) * (strlen(s) + 1));
		while ( s[i] != '@' ) {
			s2[i] = s[i];
			i++;
		}
		s2[i] = 0;
		free(s);
		return s2;
	}
	return 0;
}

t_string get_monitor_host(){
	t_string s, s2;
	int i = 0;
	
	if (current.monitor_rule != 0) {
		s = xmalloc(sizeof(char) * strlen(current.monitor_rule->name));
		s = extract_last_part(current.monitor_rule->name);
		s2 = xmalloc(sizeof(char) * (strlen(s) + 1));
		while ( s[i] != '@' ) {
			i++;
		}
		i++;
		while ( s[i] != 0 ) {
			s2[i] = s[i];
			i++;
		}
		s2[i] = 0;
		free(s);
		return s2;
	}
	return 0;
}

int get_monitor_powervalue() {
	t_tree t;
	t_string s;
	
	if (current.monitor_rule != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.monitor_rule->name) + 12));
		sprintf(s, "%s.powervalue", current.monitor_rule->name );
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0) return 0;
		if (t->type == string_type) {
			current.rights_out = t->right;
			return atoi(t->value.string_value);
		}
		return -1;
	}
	return -1;
}

void set_monitor_powervalue(int value) {
	t_string s, s2;

	if (current.monitor_rule != 0 && current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * (strlen(current.monitor_rule->name) + 12));
		sprintf( s, "%s.powervalue", current.monitor_rule->name);
		s2 = xmalloc(sizeof(char) * 15);
		sprintf(s2, "%d", value);
		
		add_to_tree(conf, s, s2, string_type, current.rights_in);
		free(s);
		free(s2);
	}
}


t_string get_monitor_user() {
	t_tree t;
	t_string s;
	
	if (current.monitor_rule != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.monitor_rule->name) + 6));
		sprintf(s, "%s.user", current.monitor_rule->name );
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0) return 0;
		if (t->type == string_type) {
			current.rights_out = t->right;
			return string_copy(t->value.string_value);
		}
		return 0;
	}
	return 0;
}

void set_monitor_user(t_string username) {
	t_string s;

	if (current.monitor_rule != 0 && current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * (strlen(current.monitor_rule->name) + 6));
		sprintf( s, "%s.user", current.monitor_rule->name);
		add_to_tree(conf, s, username, string_type, current.rights_in);
		free(s);
	}
}


t_string get_shutdown_command() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.shutdowncmd", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		current.rights_out = t->right;
		return string_copy(t->value.string_value);
	}
	return 0;
}


void set_shutdown_command(t_string command) {
	if (current.rights_in != invalid_right) {
		add_to_tree(conf, "nut.upsmon.shutdowncmd", command, string_type, current.rights_in);
	}
}


t_flags get_notify_flag(t_notify_events event) {
	t_string s, s2;
	t_tree t;
	
	s2 = event_to_string(event);
	s = xmalloc(sizeof(char) * (strlen(s2) + 23));
	sprintf(s, "nut.upsmon.notifyflag.%s", s2);
	t = tree_search(conf, s, TRUE);
	free(s);
	if ((t != 0)  && (t->type == string_type)) {
		current.rights_out = t->right;
		return string_to_flag(t->value.string_value);
	}
	return IGNORE;
}


void set_notify_flag(t_notify_events event, t_flags flag) {
	t_string s, s2;
	
	if (current.rights_in != invalid_right) {
		s2 = event_to_string(event);
		s = xmalloc(sizeof(char) * (strlen(s2) + 23));
		sprintf(s, "nut.upsmon.notifyflag.%s", s2);
		add_to_tree(conf, s, flag_to_string(flag), string_type, current.rights_in);
		free(s);
	}
}

t_string get_notify_message(t_notify_events event) {
	t_string s, s2;
	t_tree t;
	
	s2 = event_to_string(event);
	s = xmalloc(sizeof(char) * (strlen(s2) + 22));
	sprintf(s, "nut.upsmon.notifymsg.%s", s2);
	t = tree_search(conf, s, TRUE);
	free(s);
	if (t != 0  && t->type == string_type) {
		current.rights_out = t->right;
		return string_copy(t->value.string_value);
	}
	return 0;
}


void set_notify_message(t_notify_events event, t_string message) {
	t_string s, s2;
	
	s2 = event_to_string(event);
	s = xmalloc(sizeof(char) * (strlen(s2) + 22));
	sprintf(s, "nut.upsmon.notifymsg.%s", s2);
	if (message == 0) {
			del_from_tree(conf, s);
	} else {
		if (current.rights_in != invalid_right) {
			add_to_tree(conf, s, message, string_type, current.rights_in);
		}
	}
	free(s);
}


t_string get_notify_command() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.notifycmd", TRUE);
	if (t != 0 && t->type == string_type) {
		current.rights_out = t->right;
		return string_copy(t->value.string_value);
	}
	return 0;
}


void set_notify_command(t_string command) {
	if (command == 0) {
			del_from_tree(conf,"nut.upsmon.notifycmd" );
	} else {
		if (current.rights_in != invalid_right) {
			add_to_tree(conf, "nut.upsmon.notifycmd", command, string_type, current.rights_in);
		}
	}
}


int get_deadtime() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.deadtime", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		current.rights_out = t->right;
		return atoi(t->value.string_value);
	}
	return -1;
}


void set_deadtime(int value) {
	t_string s;
	
	if (current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * 15);
		sprintf(s, "%d", value);
		add_to_tree(conf, "nut.upsmon.deadtime", s, string_type, current.rights_in);
		free(s);
	}
}


int get_finaldelay() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.finaldelay", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		current.rights_out = t->right;
		return atoi(t->value.string_value);
	}
	return -1;
}


void set_finaldelay(int value) {
	t_string s;
	
	if (current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * 15);
		sprintf(s, "%d", value);
		add_to_tree(conf, "nut.upsmon.finaldelay", s, string_type, current.rights_in);
		free(s);
	}
}


int get_hostsync() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.hostsync", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		current.rights_out = t->right;
		return atoi(t->value.string_value);
	}
	return -1;
}


void set_hostsync(int value) {
	t_string s;
	
	if (current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * 15);
		sprintf(s, "%d", value);
		add_to_tree(conf, "nut.upsmon.hostsync", s, string_type, current.rights_in);
		free(s);
	}
}


int get_minsupplies() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.minsupplies", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		current.rights_out = t->right;
		return atoi(t->value.string_value);
	}
	return -1;
}


void set_minsupplies(int value) {
	t_string s;
	
	if (current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * 15);
		sprintf(s, "%d", value);
		add_to_tree(conf, "nut.upsmon.minsupply", s, string_type, current.rights_in);
		free(s);
	}
}


int get_nocommwarntime() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.nocommwarntime", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		current.rights_out = t->right;
		return atoi(t->value.string_value);
	}
	return -1;
}


void set_nocommwarntime(int value) {
	t_string s;
	
	if (current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * 15);
		sprintf(s, "%d", value);
		add_to_tree(conf, "nut.upsmon.nocommwarntime", s, string_type, current.rights_in);
		free(s);
	}
}


int get_rbwarntime() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.rbwarntime", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		current.rights_out = t->right;
		return atoi(t->value.string_value);
	}
	return -1;
}


void set_rbwarntime(int value) {
	t_string s;
	
	if (current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * 15);
		sprintf(s, "%d", value);
		add_to_tree(conf, "nut.upsmon.rbwarntime", s, string_type, current.rights_in);
		free(s);
	}
}


int get_pollfreq() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.pollfreq", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		current.rights_out = t->right;
		return atoi(t->value.string_value);
	}
	return -1;
}


void set_pollfreq(int value) {
	t_string s;
	
	if (current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * 15);
		sprintf(s, "%d", value);
		add_to_tree(conf, "nut.upsmon.pollfreq", s, string_type, current.rights_in);
		free(s);
	}
}


int get_pollfreqalert() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.pollfreqalert", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		current.rights_out = t->right;
		return atoi(t->value.string_value);
	}
	return -1;
}


void set_pollfreqalert(int value) {
	t_string s;
	
	if (current.rights_in != invalid_right) {
		s = xmalloc(sizeof(char) * 15);
		sprintf(s, "%d", value);
		add_to_tree(conf, "nut.upsmon.pollfreqalert", s, string_type, current.rights_in);
		free(s);
	}
}


t_string get_powerdownflag() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.powerdownflag", TRUE);
	if (t != 0 && t->type == string_type) {
		current.rights_out = t->right;
		return string_copy(t->value.string_value);
	}
	return 0;
}


void set_powerdownflag(t_string filename) {
	if (current.rights_in != invalid_right) {
		add_to_tree(conf, "nut.upsmon.powerdownflag", filename, string_type, current.rights_in);
	}
}


t_string get_run_as_user() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.run_as_user", TRUE);
	if (t != 0 && t->type == string_type) {
		current.rights_out = t->right;
		return string_copy(t->value.string_value);
	}
	return 0;
}


void set_run_as_user(t_string username) {
	if (current.rights_in != invalid_right) {
		add_to_tree(conf, "nut.upsmon.run_as_user", username, string_type, current.rights_in);
	}
}

t_string get_cert_path() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.security.cert_path", TRUE);
	if (t != 0 && t->type == string_type) {
		current.rights_out = t->right;
		return string_copy(t->value.string_value);
	}
	return 0;
}

void set_cert_path(t_string filename) {
	if (current.rights_in != invalid_right) {
		add_to_tree(conf, "nut.upsmon.security.cert_path", filename, string_type, current.rights_in);
	}
}

boolean get_cert_verify() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.security.cert_verify", TRUE);
	if (t != 0 && t->type == string_type) {
		current.rights_out = t->right;
		return atoi(t->value.string_value);
	}
	return FALSE;
}

void set_cert_verify(boolean value) {
	if (current.rights_in != invalid_right) {
		if (value == FALSE) {
			add_to_tree(conf, "nut.upsmon.security.cert_verify", "0", string_type, current.rights_in);	
		} else {
			add_to_tree(conf, "nut.upsmon.security.cert_verify", "1", string_type, current.rights_in);	
		}
	}
}

boolean get_force_ssl() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.security.force_ssl", TRUE);
	if (t != 0 && t->type == string_type) {
		current.rights_out = t->right;
		return atoi(t->value.string_value);
	}
	return FALSE;
}

void set_force_sll(boolean value) {
	if (current.rights_in != invalid_right) {
		if (value == FALSE) {
			add_to_tree(conf, "nut.upsmon.security.force_ssl", "0", string_type, current.rights_in);	
		} else {
			add_to_tree(conf, "nut.upsmon.security.force_ssl", "1", string_type, current.rights_in);	
		}
	}
}














t_string node_to_string(t_tree tree) {
	t_string last_part, s = 0, s2;
	t_string rights;
	int len;
	
	last_part = extract_last_part(tree->name);
	
	if (tree->has_value) {
		
/* Use this flag to enable the saving of rights in the 
 * configuration file. Disabled for the moment because
 * not used yet (and we don't want the user to wonder
 * about what it is) */
#ifdef CONFIG_USE_RIGHT
		rights = right_to_string(tree->right);
#else
		rights = "";
#endif
		
		switch(tree->type) {
			case string_type :
				len = strlen(tree->value.string_value) + strlen(rights) + strlen(last_part) + 6;
				s = xmalloc(sizeof(char) * (len + 1));
				sprintf(s, "%s = \"%s\" %s",last_part, tree->value.string_value, rights);
				break;
			case enum_string_type :
				s2 = enum_string_to_string(tree->value.enum_string_value);
				len = strlen(s2) + strlen(rights) + strlen(last_part) + 8;
				s = xmalloc(sizeof(char) * (len + 1));
				sprintf(s,"%s = { %s } %s", last_part, s2, rights);
				free(s2);
		}
		free(last_part);
		return s;
	} else {
		return last_part;
	}
}


void save(t_tree tree, FILE* file, t_string tab, FILE* comm_file) {
	t_string node_string, new_tab;
	
	node_string = node_to_string(tree);
	write_desc(tree->name, file, comm_file);
	fwrite(tab, strlen(tab), 1, file);
	fwrite(node_string, strlen(node_string), 1, file);
	
	free(node_string);
	
	if (tree->son == 0) {
		fwrite("\n", 1, 1, file);
		return;
	}
	
	if (tree->has_value || tree->son->next_brother != 0) {
		fwrite(" (\n", 3, 1, file);
		new_tab = xmalloc(sizeof(char) * (strlen(tab) + 2));
		new_tab[0] = '\t';
		strcpy(new_tab + 1, tab);
		tree = tree->son;
		save(tree, file, new_tab, comm_file);
		while (tree->next_brother != 0) {
			tree = tree->next_brother;
			save(tree, file, new_tab, comm_file);
		}
		fwrite(tab, strlen(tab), 1, file);
		fwrite(")\n", 2, 1, file);
		free(new_tab);
		return;
	}
	
	// tree don't have value and have only one son
	while (1) {
		fwrite(".", 1, 1, file);
		tree = tree->son;
		node_string = node_to_string(tree);
		fwrite(node_string, strlen(node_string), 1, file);
		free(node_string);
		
		if (tree->son == 0) {
			fwrite("\n", 1, 1, file);
			return;
		}
		
		if (tree->has_value || tree->son->next_brother != 0) {
			fwrite(" (\n", 3, 1, file);
			new_tab = xmalloc(sizeof(char) * (strlen(tab) + 2));
			new_tab[0] = '\t';
			strcpy(new_tab + 1, tab);
			tree = tree->son;
			save(tree, file, new_tab, comm_file);
			while (tree->next_brother != 0) {
				tree = tree->next_brother;
				save(tree, file, new_tab, comm_file);
			}
			fwrite(tab, strlen(tab), 1, file);
			fwrite(")\n", 2, 1, file);
			free(new_tab);
			return;
		}
	}
}

void write_desc(t_string name, FILE* conf_file, FILE* comm_file) {
	char c;
	int i = 0;
	int name_length;
	boolean match = FALSE;
	
	if (comm_file == 0) {
		return;
	}
	
	/* Two special cases : */
	if (strcmp(name, "nut.ups") == 0 ) {
		write_desc("nut.ups_header_only", conf_file, comm_file);
		write_desc("nut.ups_desc", conf_file, comm_file);
		return;
	}
	if (strcmp(name, "nut.users") == 0 ) {
		write_desc("nut.users_header_only", conf_file, comm_file);
		write_desc("nut.users_desc", conf_file, comm_file);
		return;
	}
	
	fseek(comm_file, 0, SEEK_SET);
	c = getc(comm_file);
	while ( c != '\\') {
		c = getc(comm_file);
	}
	while(c != EOF) {
		name_length = strlen(name);
		c = getc(comm_file);
		for ( i = 0; i < name_length; i++) {
			match = TRUE;
			if (name[i] != c) {
				match = FALSE;
				break;
			}
			c = fgetc(comm_file);
		} 
		/* It matchs for the name_length first char. Does it end here ? */
		if (match && c == '\n') {
			c = fgetc(comm_file);
			while (c != '\\' && c != EOF) {
				fputc(c, conf_file);
				c = fgetc(comm_file);
			}
			return;
		}
		/* It didn't match, lets try the next */
		c = getc(comm_file);
		while ( c != '\\' && c != EOF) {
			c = getc(comm_file);
		}
	}
	
}

void free_typed_value(t_typed_value value) {
	if (value.has_value) {
		switch (value.type) {
			case string_type : 
				free(value.value.string_value);
				break;
			case enum_string_type : 
				free_enum_string(value.value.enum_string_value);
				break;
			default : ;
		}
	}
}
