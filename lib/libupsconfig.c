/*  libupsconfig.c - API to manipulate NUT configuration

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

#include "libupsconfig.h"
#include "tree.h"
#include "nutparser.h"
#include "data_types.h"
#include "common.h"

struct {
	t_tree ups;
	t_rights rights;
	t_tree user;
	t_tree monitor_rule;
} current;

t_tree conf;

void save(t_tree tree, FILE* file, t_string tab);
void write_desc(t_string name, FILE* conf_file);


/***************************************************
 *              INITIALIZATION SECTION             *
 ***************************************************/


void new_config() {
	conf = new_node("nut", 0, 0);
	current.ups = 0;
	current.rights = admin_r;
	current.user = 0;
	current.monitor_rule = 0;
}

void load_config(t_string filename) {
	conf = parse_conf(filename, 0);
	current.ups = 0;
	current.rights = admin_r;
	current.user = 0;
	current.monitor_rule = 0;
}

/***************************************************
 *                 SAVING SECTION                  *
 ***************************************************/

void save_config(t_string directory, boolean single) {
	t_string nut_conf;
	t_string s;
	FILE* conf_file;
	t_modes mode;
	t_tree son;
	
	nut_conf = xmalloc(sizeof(char) * (strlen(directory) + 20));
	sprintf(nut_conf, "%s/nut.conf", directory);
	conf_file = fopen(nut_conf, "w");
	
	if (single == TRUE) {
		son = conf->son;
		while (son != 0) {
			save(son, conf_file, "");
			fwrite("\n", 1, 1, conf_file);
			son = son->next_brother;
		}
		fclose(conf_file);
		
		sprintf(nut_conf, "%s/ups.conf", directory);
		unlink(nut_conf);
		sprintf(nut_conf, "%s/users.conf", directory);
		unlink(nut_conf);
		sprintf(nut_conf, "%s/upsd.conf", directory);
		unlink(nut_conf);
		sprintf(nut_conf, "%s/upsmon.conf", directory);
		unlink(nut_conf);
		
		
	} else { // Not single file mode
		mode = string_to_mode((tree_search(conf, "nut.mode", 1))->value.string_value);
		switch (mode) {
			case standalone :
				s = xmalloc(sizeof(char) * ( 50 + strlen(directory)));
				fwrite("mode = \"standalone\"\n\n", 21, 1, conf_file);
				
				write_desc("nut.ups_header_only", conf_file);
				sprintf(s, "ups (\n\tinclude %s/ups.conf\n)\n\n", directory);
				fwrite(s, strlen(s), 1, conf_file);
				
				write_desc("nut.users_header_only", conf_file);
				sprintf(s, "users (\n\tinclude %s/users.conf\n)\n\n", directory);
				fwrite(s, strlen(s), 1, conf_file);
				
				write_desc("nut.upsd", conf_file);
				sprintf(s, "upsd (\n\tinclude %s/upsd.conf\n)\n\n", directory);
				fwrite(s, strlen(s), 1, conf_file);
				
				write_desc("nut.upsmon", conf_file);
				sprintf(s, "upsmon (\n\tinclude %s/upsmon.conf\n)\n\n", directory);
				fwrite(s, strlen(s), 1, conf_file);
				fclose(conf_file);
				
				sprintf(s, "%s/ups.conf", directory);
				conf_file = fopen( s, "w");
				write_desc("nut.ups", conf_file);
				son = (tree_search(conf, "nut.ups", 1))->son;
				while (son != 0) {
					save(son, conf_file, "");
					fwrite("\n", 1, 1, conf_file);
					son = son->next_brother;
				}
				fclose(conf_file);
				
				sprintf(s, "%s/users.conf", directory);
				conf_file = fopen( s, "w");
				write_desc("nut.users", conf_file);
				son = (tree_search(conf, "nut.users", 1))->son;
				while (son != 0) {
					save(son, conf_file, "");
					fwrite("\n", 1, 1, conf_file);
					son = son->next_brother;
				}
				fclose(conf_file);
				
				sprintf(s, "%s/upsd.conf", directory);
				conf_file = fopen( s, "w");
				write_desc("nut.upsd", conf_file);
				son = (tree_search(conf, "nut.upsd", 1))->son;
				while (son != 0) {
					save(son, conf_file, "");
					fwrite("\n", 1, 1, conf_file);
					son = son->next_brother;
				}
				fclose(conf_file);
				
				sprintf(s, "%s/upsmon.conf", directory);
				conf_file = fopen( s, "w");
				write_desc("nut.upsmon", conf_file);
				son = (tree_search(conf, "nut.upsmon", 1))->son;
				while (son != 0) {
					save(son, conf_file, "");
					fwrite("\n", 1, 1, conf_file);
					son = son->next_brother;
				}
				fclose(conf_file);
				
				free(s);
				break;
			case net_server :
				s = xmalloc(sizeof(char) * ( 50 + strlen(directory)));
				fwrite("mode = \"net_server\"\n\n", 21, 1, conf_file);
				
				write_desc("nut.ups_header_only", conf_file);
				sprintf(s, "ups (\n\tinclude %s/ups.conf\n)\n\n", directory);
				fwrite(s, strlen(s), 1, conf_file);
				
				write_desc("nut.users_header_only", conf_file);
				sprintf(s, "users (\n\tinclude %s/users.conf\n)\n\n", directory);
				fwrite(s, strlen(s), 1, conf_file);
				
				write_desc("nut.upsd", conf_file);
				sprintf(s, "upsd (\n\tinclude %s/upsd.conf\n)\n\n", directory);
				fwrite(s, strlen(s), 1, conf_file);
				
				write_desc("nut.upsmon", conf_file);
				sprintf(s, "upsmon (\n\tinclude %s/upsmon.conf\n)\n\n", directory);
				fwrite(s, strlen(s), 1, conf_file);
				fclose(conf_file);
				
				sprintf(s, "%s/ups.conf", directory);
				conf_file = fopen( s, "w");
				write_desc("nut.ups", conf_file);
				son = (tree_search(conf, "nut.ups", 1))->son;
				while (son != 0) {
					save(son, conf_file, "");
					fwrite("\n", 1, 1, conf_file);
					son = son->next_brother;
				}
				fclose(conf_file);
				
				sprintf(s, "%s/users.conf", directory);
				conf_file = fopen( s, "w");
				write_desc("nut.users", conf_file);
				son = (tree_search(conf, "nut.users", 1))->son;
				while (son != 0) {
					save(son, conf_file, "");
					fwrite("\n", 1, 1, conf_file);
					son = son->next_brother;
				}
				fclose(conf_file);
				
				sprintf(s, "%s/upsd.conf", directory);
				conf_file = fopen( s, "w");
				write_desc("nut.upsd", conf_file);
				son = (tree_search(conf, "nut.upsd", 1))->son;
				while (son != 0) {
					save(son, conf_file, "");
					fwrite("\n", 1, 1, conf_file);
					son = son->next_brother;
				}
				fclose(conf_file);
				
				sprintf(s, "%s/upsmon.conf", directory);
				conf_file = fopen( s, "w");
				write_desc("nut.upsmon", conf_file);
				son = (tree_search(conf, "nut.upsmon", 1))->son;
				while (son != 0) {
					save(son, conf_file, "");
					fwrite("\n", 1, 1, conf_file);
					son = son->next_brother;
				}
				fclose(conf_file);
				
				free(s);
				break;
			case net_client :
				s = xmalloc(sizeof(char) * ( 50 + strlen(directory)));
				fwrite("mode = \"standalone\"\n\n", 21, 1, conf_file);
				
				write_desc("nut.ups_header_only", conf_file);
				sprintf(s, "ups (\n\tinclude %s/ups.conf\n)\n\n", directory);
				fwrite(s, strlen(s), 1, conf_file);
				
				write_desc("nut.users_header_only", conf_file);
				sprintf(s, "users (\n\tinclude %s/users.conf\n)\n\n", directory);
				fwrite(s, strlen(s), 1, conf_file);
				
				write_desc("nut.upsmon", conf_file);
				sprintf(s, "upsmon (\n\tinclude %s/upsmon.conf\n)\n\n", directory);
				fwrite(s, strlen(s), 1, conf_file);
				fclose(conf_file);
				
				sprintf(s, "%s/ups.conf", directory);
				conf_file = fopen( s, "w");
				write_desc("nut.ups", conf_file);
				son = (tree_search(conf, "nut.ups", 1))->son;
				while (son != 0) {
					save(son, conf_file, "");
					fwrite("\n", 1, 1, conf_file);
					son = son->next_brother;
				}
				fclose(conf_file);
				
				sprintf(s, "%s/users.conf", directory);
				conf_file = fopen( s, "w");
				write_desc("nut.users", conf_file);
				son = (tree_search(conf, "nut.users", 1))->son;
				while (son != 0) {
					save(son, conf_file, "");
					fwrite("\n", 1, 1, conf_file);
					son = son->next_brother;
				}
				fclose(conf_file);
				
				sprintf(s, "%s/upsmon.conf", directory);
				conf_file = fopen( s, "w");
				write_desc("nut.upsmon", conf_file);
				son = (tree_search(conf, "nut.upsmon", 1))->son;
				while (son != 0) {
					save(son, conf_file, "");
					fwrite("\n", 1, 1, conf_file);
					son = son->next_brother;
				}
				fclose(conf_file);
				
				free(s);
				break;
			case pm :
				fwrite("mode = \"pm\"\n", 12, 1, conf_file);
			default : 
				exit(EXIT_FAILURE);
		}
	}
	free(nut_conf);
}


/***************************************************
 *              GENERAL LEVEL SECTION              *
 ***************************************************/

t_modes get_mode() {
	t_tree t;
	
	t = tree_search(conf, "nut.mode", TRUE);
	if (t != 0 && t->has_value && t->type == string_type) {
		current.rights = t->right;
		return string_to_mode(t->value.string_value);
	}
	return -1;
}

void set_mode(t_modes mode) {
	t_string s;
	
	s = mode_to_string(mode);
	add_to_tree(conf, "nut.mode", s, string_type, current.rights);
	free(s);
}

t_rights get_rights() {
	return current.rights;
}

void set_rights(t_rights right) {
	current.rights = right;
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
			add_to_enum_string(enum_string, s);
			free(s);
			t = t->next_brother;
		}
		
		current.rights = -1;
		return enum_string;
	}
	current.rights = -1;
	return 0;
}

void add_ups(t_string upsname, t_string driver, t_string port) {
	t_string s;
	t_tree t;
	
	s = xmalloc(sizeof(char) * ( 31 + strlen(upsname)));
	sprintf(s, "nut.ups.%s.driver", upsname); 
	add_to_tree(conf, s, driver, string_type, current.rights);
	sprintf(s, "nut.ups.%s.driver.parameter.port", upsname);
	add_to_tree(conf, s, port, string_type, current.rights);
	sprintf(s, "nut.ups.%s", upsname);
	t = tree_search(conf, s, TRUE);
	current.ups = t;
	free(s);
}

int remove_ups(t_string upsname) {
	t_string s;
	int i;
	
	s = xmalloc(sizeof(char) * ( 9 + strlen(upsname)));
	sprintf(s, "nut.ups.%s", upsname);
	i = del_from_tree(conf, s);
	free(s);
	current.ups = 0;
	return i;
	
}

int search_ups(t_string upsname) {
	t_tree t;
	t_string s;
	
	s = xmalloc(sizeof(char) * ( 9 + strlen(upsname)));
	sprintf(s, "nut.ups.%s", upsname);
	t = tree_search(conf, s, TRUE);
	
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
	if (current.ups != 0) {
		free(current.ups->name);
		current.ups->name = string_copy(upsname);
	}	
}

t_string get_driver() {
	t_tree t;
	t_string s;
	
	if (current.ups != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + 8));
		sprintf( s, "%s.driver", current.ups->name);
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0) return 0;
		return string_copy(t->value.string_value);
	}
	return 0;
}

void set_driver(t_string driver) {
	t_string s;
	
	if (current.ups != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + 8));
		sprintf( s, "%s.driver", current.ups->name);
		add_to_tree(conf, s, driver, string_type, current.rights);
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
		if (t == 0) return 0;
		return string_copy(t->value.string_value);
	}
	return 0;
}

void set_port(t_string driver) {
	t_string s;
	
	if (current.ups != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + 23));
		sprintf( s, "%s.driver.parameter.port", current.ups->name);
		add_to_tree(conf, s, driver, string_type, current.rights);
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
		if (t == 0) return 0;
		return string_copy(t->value.string_value);
	}
	return 0;
}

void set_desc(t_string driver) {
	t_string s;
	
	if (current.ups != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + 6));
		sprintf( s, "%s.desc", current.ups->name);
		add_to_tree(conf, s, driver, string_type, current.rights);
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
		if (t ==0) return 0;
		t = t->son;
		while (t !=0) {
			add_to_enum_string(enum_string, t->name);
			t = t->next_brother;
		}
		return enum_string;
		
	}
	return 0;
}

t_string get_driver_parameter(t_string paramname) {
	t_tree t;
	t_string s;
	
	if (current.ups != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + strlen(paramname) + 17));
		sprintf( s, "%s.desc.parameter.%s", current.ups->name, paramname);
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0) return 0;
		return string_copy(t->value.string_value);
	}
	return 0;
}

void set_driver_parameter(t_string paramname, void* value, t_types type) {
	t_string s;
	
	if (current.ups != 0) {
		s = xmalloc(sizeof(char) * (strlen(paramname) + strlen(current.ups->name) + 19));
		sprintf(s, "%s.driver.parameter.%s", current.ups->name, paramname);
		add_to_tree(conf, s, value, type, current.rights);
		free(s);
	}	
}

t_string get_ups_variable(t_string varname) {
	t_tree t;
	t_string s;
	
	if (current.ups != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.ups->name) + strlen(varname) + 2));
		sprintf( s, "%s.%s", current.ups->name, varname);
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0) return 0;
		return string_copy(t->value.string_value);
	}
	return 0;
}

void set_ups_variable(t_string varname, void* value, t_types type) {
	t_string s;
	
	if (current.ups != 0) {
		s = xmalloc(sizeof(char) * (strlen(varname) + strlen(current.ups->name) + 2));
		sprintf(s, "%s.%s", current.ups->name, varname);
		add_to_tree(conf, s, value, type, current.rights);
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
			add_to_enum_string(enum_string, s);
			free(s);
			t = t->next_brother;
		}
		current.rights = -1;
		return enum_string;
	}
	current.rights = -1;
	return 0;
}
	
	

void add_user(t_string username, t_user_types type, t_string password) {
	t_string s;
	t_tree t;
	
	s = xmalloc(sizeof(char) * ( 20 + strlen(username)));
	sprintf(s, "nut.users.%s.type", username);
	add_to_tree(conf, s, user_type_to_string(type), string_type, current.rights);
	sprintf(s, "nut.users.%s.password", username);
	add_to_tree(conf, s, password, string_type, current.rights);
	sprintf(s, "nut.users.%s", username);
	t = tree_search(conf, s, TRUE);
	current.user = t;
	free(s);	
}
	
	
int remove_user(t_string username) {
	t_string s;
	int i;
	
	s = xmalloc(sizeof(char) * ( 11 + strlen(username)));
	sprintf(s, "nut.users.%s", username);
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


void set_name(t_string username) {
	if (current.user != 0) {
		free(current.user->name);
		current.user->name = string_copy(username);
	}	
}


t_user_types get_type() {
	t_tree t;
	t_string s;
	
	if (current.user != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 6));
		sprintf( s, "%s.type", current.user->name);
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0) return 0;
		return string_to_user_type(t->value.string_value);
	}
	return 0;
}


void set_type(t_user_types type) {
	t_string s;
	
	if (current.user != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 6));
		sprintf( s, "%s.type", current.user->name);
		add_to_tree(conf, s, user_type_to_string(type), string_type, current.rights);
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
		if (t == 0) return 0;
		return string_copy(t->value.string_value);
	}
	return 0;
}


void set_password(t_string password) {
	t_string s;
	
	if (current.user != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 10));
		sprintf( s, "%s.password", current.user->name);
		add_to_tree(conf, s, password, string_type, current.rights);
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
		if (t == 0) return 0;
		if (t->type == enum_string_type) {
			return enum_string_copy(t->value.enum_string_value);
		}
	}
	return 0;
}


void set_allowfrom(t_enum_string acllist) {
	t_string s;
	
	if (current.user != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 11));
		sprintf( s, "%s.allowfrom", current.user->name);
		add_to_tree(conf, s, acllist, enum_string_type, current.rights);
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
		if (t == 0) return 0;
		if (t->type == enum_string_type) {
			return enum_string_copy(t->value.enum_string_value);
		}
	}
	return 0;
}


void set_actions(t_enum_string actionlist) {
	t_string s;
	
	if (current.user != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 9));
		sprintf( s, "%s.actions", current.user->name);
		add_to_tree(conf, s, actionlist, enum_string_type, current.rights);
		free(s);
	}
}


t_enum_string get_instcmds() {
	t_tree t;
	t_string s;
	
	if (current.user != 0) {
		s = xmalloc(sizeof(char) * (strlen(current.user->name) + 10));
		sprintf( s, "%s.instcmds", current.user->name);
		t = tree_search(conf, s, TRUE);
		free(s);
		if (t == 0) return 0;
		if (t->type == enum_string_type) {
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
		add_to_tree(conf, s, instcmdlist, enum_string_type, current.rights);
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
			add_to_enum_string(enum_string, s);
			free(s);
			t = t->next_brother;
		}
		return enum_string;
	}
	return 0;
}


void add_acl(t_string aclname, t_string value) {
	t_string s;
	
	s = xmalloc(sizeof(char) * ( 14 + strlen(aclname)));
	sprintf(s, "nut.upsd.acl.%s", aclname);
	add_to_tree(conf, s, value, string_type, current.rights);
	free(s);	
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
	if (t == 0) return 0;
	return string_copy(t->value.string_value);
}


void set_acl_value(t_string aclname, t_string value) {
	t_string s;
	
	s = xmalloc(sizeof(char) * (strlen(aclname) + 14));
	sprintf( s, "nut.upsd.acl.%s", aclname);
	add_to_tree(conf, s, value, string_type, current.rights);
	free(s);
}


t_enum_string get_accept() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsd.accept", TRUE);
	if (t == 0) return 0;
	if (t->type == enum_string_type) {
		return enum_string_copy(t->value.enum_string_value);
	}
	return 0;
}


void set_accept(t_enum_string acllist) {
	add_to_tree(conf, "nut.upsd.accept", acllist, enum_string_type, current.rights);
}


t_enum_string get_reject() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsd.reject", TRUE);
	if (t == 0) return 0;
	if (t->type == enum_string_type) {
		return enum_string_copy(t->value.enum_string_value);
	}
	return 0;
}


void set_reject(t_enum_string acllist) {
	add_to_tree(conf, "nut.upsd.reject", acllist, enum_string_type, current.rights);
}


int get_maxage() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsd.maxage", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		return atoi(t->value.string_value);
	}
	return 0;
}


void set_maxage(int value) {
	t_string s;
	
	// an int can need 10 char to be represented (without the sign)
	// I put 15 to be sure
	s = xmalloc(sizeof(char) * 15);
	sprintf(s, "%d", value);
	add_to_tree(conf, "nut.upsd.reject", s, string_type, current.rights);
	free(s);
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


// created in last.
void add_monitor_rule(t_string upsname, t_string host, int powervalue, t_string username) {
	t_string s, s2;
	
	s = xmalloc(sizeof(char) * ( 32 + strlen(upsname) + strlen(host)));
	sprintf(s, "nut.upsmon.monitor.%s@%s.powervalue", upsname, host);
	s2 = xmalloc(sizeof(char) * 15);
	sprintf(s2, "%d", powervalue);
	add_to_tree(conf, s, s2, string_type, current.rights);
	free(s2);
	sprintf(s, "nut.upsmon.monitor.%s@%s.user", upsname, host);
	add_to_tree(conf, s, username, string_type, current.rights);
	free(s);
}
	
	
int remove_monitor_rule(int rulenumber) {
	t_tree t;
	int i = 0;
	
	t = tree_search(conf, "nut.upsmon.monitor", TRUE);
	if (t != 0) {
		t = t->son;
		while (t != 0) {
			i++;
			if (i == rulenumber) {
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
				current.monitor_rule = t;
				return 1;
			}
			t = t->next_brother;
		}
		return 0;
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


t_string get_minotor_host(){
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
			return atoi(t->value.string_value);
		}
		return -1;
	}
	return -1;
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
			return string_copy(t->value.string_value);
		}
		return 0;
	}
	return 0;
}


t_string get_shutdown_command() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.shutdowncmd", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		return string_copy(t->value.string_value);
	}
	return 0;
}


void set_shutdown_command(t_string command) {
	add_to_tree(conf, "nut.upsmon.shutdowncmd", command, string_type, current.rights);
}


t_flags get_notify_flag(t_notify_events event) {
	t_string s, s2;
	t_tree t;
	
	s2 = event_to_string(event);
	s = xmalloc(sizeof(char) * (strlen(s2) + 23));
	sprintf(s, "nut.upsmon.notifyflag.%s", s2);
	t = tree_search(conf, s, TRUE);
	free(s);
	if (t != 0) {
		return string_to_flag(t->value.string_value);
	}
	return IGNORE;
}


void set_notify_flag(t_notify_events event, t_flags flag) {
	t_string s, s2;
	
	s2 = event_to_string(event);
	s = xmalloc(sizeof(char) * (strlen(s2) + 23));
	sprintf(s, "nut.upsmon.notifyflag.%s", s2);
	add_to_tree(conf, s, flag_to_string(flag), string_type, current.rights);
	free(s);
}

t_string get_notify_message(t_notify_events event) {
	t_string s, s2;
	t_tree t;
	
	s2 = event_to_string(event);
	s = xmalloc(sizeof(char) * (strlen(s2) + 22));
	sprintf(s, "nut.upsmon.notifymsg.%s", s2);
	t = tree_search(conf, s, TRUE);
	free(s);
	if (t != 0) {
		return string_copy(t->value.string_value);
	}
	return 0;
}


void set_notify_message(t_notify_events event, t_string message) {
	t_string s, s2;
	
	s2 = event_to_string(event);
	s = xmalloc(sizeof(char) * (strlen(s2) + 22));
	sprintf(s, "nut.upsmon.notifymsg.%s", s2);
	add_to_tree(conf, s, message, string_type, current.rights);
	free(s);
}


t_string get_notify_command() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.notifycmd", TRUE);
	if (t != 0 || t->type != string_type) {
		return string_copy(t->value.string_value);
	}
	return 0;
}


void set_notify_command(t_string command) {
	add_to_tree(conf, "nut.upsmon.notifycmd", command, string_type, current.rights);
}


int get_deadtime() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.deadtime", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		return atoi(t->value.string_value);
	}
	return 0;
}


void set_deadtime(int value) {
	t_string s;
	
	s = xmalloc(sizeof(char) * 15);
	sprintf(s, "%d", value);
	add_to_tree(conf, "nut.upsmon.deadtime", s, string_type, current.rights);
	free(s);
}


int get_finaldelay() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.finaldelay", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		return atoi(t->value.string_value);
	}
	return 0;
}


void set_finaldelay(int value) {
	t_string s;
	
	s = xmalloc(sizeof(char) * 15);
	sprintf(s, "%d", value);
	add_to_tree(conf, "nut.upsmon.finaldelay", s, string_type, current.rights);
	free(s);
}


int get_hostsync() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.hostsync", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		return atoi(t->value.string_value);
	}
	return 0;
}


void set_hostsync(int value) {
	t_string s;
	
	s = xmalloc(sizeof(char) * 15);
	sprintf(s, "%d", value);
	add_to_tree(conf, "nut.upsmon.hostsync", s, string_type, current.rights);
	free(s);
}


int get_minsupply() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.minsupply", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		return atoi(t->value.string_value);
	}
	return 0;
}


void set_minsupply(int value) {
	t_string s;
	
	s = xmalloc(sizeof(char) * 15);
	sprintf(s, "%d", value);
	add_to_tree(conf, "nut.upsmon.minsupply", s, string_type, current.rights);
	free(s);
}


int get_nocommwarntime() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.nocommwarntime", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		return atoi(t->value.string_value);
	}
	return 0;
}


void set_nocommwarntime(int value) {
	t_string s;
	
	s = xmalloc(sizeof(char) * 15);
	sprintf(s, "%d", value);
	add_to_tree(conf, "nut.upsmon.nocommwarntime", s, string_type, current.rights);
	free(s);
}


int get_rbwarntime() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.rbwarntime", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		return atoi(t->value.string_value);
	}
	return 0;
}


void set_rbwarntime(int value) {
	t_string s;
	
	s = xmalloc(sizeof(char) * 15);
	sprintf(s, "%d", value);
	add_to_tree(conf, "nut.upsmon.rbwarntime", s, string_type, current.rights);
	free(s);
}


int get_pollfreq() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.pollfreq", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		return atoi(t->value.string_value);
	}
	return 0;
}


void set_pollfreq(int value) {
	t_string s;
	
	s = xmalloc(sizeof(char) * 15);
	sprintf(s, "%d", value);
	add_to_tree(conf, "nut.upsmon.pollfreq", s, string_type, current.rights);
	free(s);
}


int get_pollfreqalert() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.pollfreqalert", TRUE);
	if (t == 0) return 0;
	if (t->type == string_type) {
		return atoi(t->value.string_value);
	}
	return 0;
}


void set_pollfreqalert(int value) {
	t_string s;
	
	s = xmalloc(sizeof(char) * 15);
	sprintf(s, "%d", value);
	add_to_tree(conf, "nut.upsmon.pollfreqalert", s, string_type, current.rights);
	free(s);
}


t_string get_powerdownflag() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.powerdownflag", TRUE);
	if (t != 0 || t->type != string_type) {
		return string_copy(t->value.string_value);
	}
	return 0;
}


void set_powerdownflag(t_string filename) {
	add_to_tree(conf, "nut.upsmon.powerdownflag", filename, string_type, current.rights);
}


t_string get_runasuser() {
	t_tree t;
	
	t = tree_search(conf, "nut.upsmon.runasuser", TRUE);
	if (t != 0 || t->type != string_type) {
		return string_copy(t->value.string_value);
	}
	return 0;
}


void set_runasuser(t_string username) {
	add_to_tree(conf, "nut.upsmon.", username, string_type, current.rights);
}















t_string node_to_string(t_tree tree) {
	t_string last_part, rights, s, s2;
	int len;
	
	last_part = extract_last_part(tree->name);
	
	if (tree->has_value) {
		
		switch(tree->right) {
			case all_rw :
				rights = string_copy("s");
				break;
			case all_r :
				rights = string_copy("");
				break;
			case admin_rw :
				rights = string_copy("s*");
				break;
			case admin_r :
				rights = string_copy("*");
		}
		
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
		return s;
	} else {
		return last_part;
	}
}


void save(t_tree tree, FILE* file, t_string tab) {
	t_string node_string, new_tab;
	
	node_string = node_to_string(tree);
	write_desc(tree->name, file);
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
		save(tree, file, new_tab);
		while (tree->next_brother != 0) {
			tree = tree->next_brother;
			save(tree, file, new_tab);
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
			save(tree, file, new_tab);
			while (tree->next_brother != 0) {
				tree = tree->next_brother;
				save(tree, file, new_tab);
			}
			fwrite(tab, strlen(tab), 1, file);
			fwrite(")\n", 2, 1, file);
			free(new_tab);
			return;
		}
	}
}


void write_desc(t_string name, FILE* conf_file) {
	t_string s;
	
	if (strcmp(name, "nut.ups") == 0 ) {
		write_desc("nut.ups_header_only", conf_file);
		write_desc("nut.ups_desc", conf_file);
		return;
	}
	if (strcmp(name, "nut.ups_header_only") == 0 ) {
		s = "#################\n## UPS SECTION ##\n#################\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.users") == 0 ) {
		write_desc("nut.users_header_only", conf_file);
		write_desc("nut.users_desc", conf_file);
		return;
	}
	if (strcmp(name, "nut.users_header_only") == 0 ) {
		s = "###################\n## USERS SECTION ##\n###################\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsd") == 0 ) {
		s = "##################\n## UPSD SECTION ##\n##################\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon") == 0 ) {
		s = "####################\n## UPSMON SECTION ##\n####################\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.mode") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# mode = \"<mode>\" - In which mode to run NUT\
\n#\
\n# Possible values are :\
\n#\
\n# - standalone  : The standard case : one UPS (or more) protect one computer on \
\n#                 which NUT is running without any monitoring by network.\
\n# - net-server  : The current computer monitor one or more UPS that are\
\n#                 physically connected to this computer via USB or serial\
\n#                 cable. Other computers can access to UPS status via network\
\n# - net_client  : The current computer monitor one or more UPS that is NOT\
\n#                 physically connected to this computer via USB or serial\
\n#                 cable. Another computers provide UPS status via network\
\n# - pm          : for unified power management use, or custom use. Note that\
\n#                 NUT will not start any service by itself if this value is\
\n#                 given\
\n# - none        : NUT is not configured yet\
\n#\
\n# Note that this value determine which service NUT will start (for instance\
\n# in net_client mode, upsd is not started)\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon.runasuser") == 0 ) {
		s = " \n# --------------------------------------------------------------------------\
\n# runasuser <username>\
\n#\
\n# By default, upsmon splits into two processes.  One stays as root and\
\n# waits to run the SHUTDOWNCMD.  The other one switches to another userid\
\n# and does everything else.\
\n#\
\n# The default nonprivileged user is set at compile-time with\
\n#       'configure --with-user=...'.\
\n#\
\n# You can override it with '-u <user>' when starting upsmon, or just\
\n# define it here for convenience.\
\n#\
\n# Note: if you plan to use the reload feature, this file (upsmon.conf)\
\n# must be readable by this user!  Since it contains passwords, DO NOT\
\n# make it world-readable.  Also, do not make it writable by the upsmon\
\n# user, since it creates an opportunity for an attack by changing the\
\n# SHUTDOWNCMD to something malicious.\
\n#\
\n# For best results, you should create a new normal user like \"nutmon\",\
\n# and make it a member of a \"nut\" group or similar.  Then specify it\
\n# here and grant read access to the upsmon.conf for that group.\
\n#\
\n# This user should not have write access to upsmon.conf.\
\n#\
\n# runasuser \"monuser\"\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon.monitor") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# monitor.<system> (\
\n# 		powervalue = \"<powervalue>\"\
\n#		user = \"<username>\"\
\n# )\
\n#\
\n# List systems you want to monitor.  Not all of these may supply power\
\n# to the system running upsmon, but if you want to watch it, it has to\
\n# be in this section.\
\n#\
\n# You must have at least one of these declared.\
\n#\
\n# <system> is a UPS identifier in the form <upsname>@<hostname>[:<port>]\
\n# like ups@localhost, su700@mybox, etc.\
\n#\
\n# Examples:\
\n#\
\n#  - \"su700@mybox\" means a UPS called \"su700\" on a system called \"mybox\"\
\n#\
\n#  - \"fenton@bigbox:5678\" is a UPS called \"fenton\" on a system called\
\n#    \"bigbox\" which runs upsd on port \"5678\".\
\n#\
\n# The UPS names like \"su700\" and \"fenton\" are set in the ups section\
\n# ( in ups trunk )\
\n#\
\n# If the ups.conf on host \"doghouse\" has a section called \"snoopy\", the\
\n# identifier for it would be \"snoopy@doghouse\".\
\n#\
\n# <powervalue> is an integer - the number of power supplies that this UPS\
\n# feeds on this system.  Most computers only have one power supply, so this\
\n# is normally set to 1.  You need a pretty big or special box to have any\
\n# other value here.\
\n#\
\n# You can also set this to 0 for a system that doesn't supply any power,\
\n# but you still want to monitor.  Use this when you want to hear about\
\n# changes for a given UPS without shutting down when it goes critical,\
\n# unless <powervalue> is 0.\
\n#\
\n# <username> must match an entry in that system's users section ( in users\
\n# trunk) that is of type upsmon_maser or upsmon_slave.\
\n# If your upsmon user name is monmaster and he is of type upsmon_master\
\n# you should have in your users section something like\
\n# monmaster (\
\n#		type = upsmon_master\
\n#		password = \"your_user_password\"\
\n# )\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon.minsupplies") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# minsupplies \"<num>\"\
\n#\
\n# Give the number of power supplies that must be receiving power to keep\
\n# this system running.  Most systems have one power supply, so you would\
\n# put \"1 \" in this field.\
\n#\
\n# Large/expensive server type systems usually have more, and can run with\
\n# a few missing.  The HP NetServer LH4 can run with 2 out of 4, for example,\
\n# so you'd set that to 2.  The idea is to keep the box running as long\
\n# as possible, right?\
\n#\
\n# Obviously you have to put the redundant supplies on different UPS circuits\
\n# for this to make sense!  See big-servers.txt in the docs subdirectory\
\n# for more information and ideas on how to use this feature.\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon.shutdowncmd") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# shutdowncmd \"<command>\"\
\n#\
\n# upsmon runs this command when the system needs to be brought down.\
\n#\
\n# This should work just about everywhere ... if it doesn't, well, change it.\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon.notifycmd") == 0 ) {
		s = "# --------------------------------------------------------------------------\
\n# notifycmd \"<command>\"\
\n#\
\n# upsmon calls this to send messages when things happen\
\n#\
\n# This command is called with the full text of the message as one argument.\
\n# The environment string NOTIFYTYPE will contain the type string of\
\n# whatever caused this event to happen.\
\n#\
\n# Note that this is only called for NOTIFY events that have EXEC set with\
\n# notifyflag.  See notifyflag below for more details.\
\n#\
\n# Making this some sort of shell script might not be a bad idea.  For more\
\n# information and ideas, see pager.txt in the docs directory.\
\n#\
\n# Example:\
\n# notifycmd \"/usr/local/ups/bin/notifyme\"\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon.pollfreq") == 0 ) {
		s = "# --------------------------------------------------------------------------\
\n# pollfreq \"<n>\"\
\n#\
\n# Polling frequency for normal activities, measured in seconds.\
\n#\
\n# Adjust this to keep upsmon from flooding your network, but don't make\
\n# it too high or it may miss certain short-lived power events.\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon.pollfrealert") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# pollfreqalert \"<n>\"\
\n#\
\n# Polling frequency in seconds while UPS on battery.\
\n#\
\n# You can make this number lower than pollfreq, which will make updates\
\n# faster when any UPS is running on battery.  This is a good way to tune\
\n# network load if you have a lot of these things running.\
\n#\
\n# The default is 5 seconds for both this and pollfreq.\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon.hostsync") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# hostsync \"<n>\"\
\n#\
\n# How long upsmon will wait before giving up on another upsmon\
\n#\
\n# The master upsmon process uses this number when waiting for slaves to\
\n# disconnect once it has set the forced shutdown (FSD) flag.  If they\
\n# don't disconnect after this many seconds, it goes on without them.\
\n#\
\n# Similarly, upsmon slave processes wait up to this interval for the\
\n# master upsmon to set FSD when a UPS they are monitoring goes critical -\
\n# that is, on battery and low battery.  If the master doesn't do its job,\
\n# the slaves will shut down anyway to avoid damage to the file systems.\
\n#\
\n# This \"wait for FSD\" is done to avoid races where the status changes\
\n# to critical and back between polls by the master.\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon.deadtime") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# deadtime \"<n>\"\
\n#\
\n# Interval to wait before declaring a stale ups \"dead\"\
\n#\
\n# upsmon requires a UPS to provide status information every few seconds\
\n# (see pollfreq and pollfreqalert) to keep things updated.  If the status\
\n# fetch fails, the UPS is marked stale.  If it stays stale for more than\
\n# deadtime seconds, the UPS is marked dead.\
\n#\
\n# A dead UPS that was last known to be on battery is assumed to have gone\
\n# to a low battery condition.  This may force a shutdown if it is providing\
\n# a critical amount of power to your system.\
\n#\
\n# Note: deadtime should be a multiple of pollfreq and pollfreqalert.\
\n# Otherwise you'll have \"dead\" UPSes simply because upsmon isn't polling\
\n# them quickly enough.  Rule of thumb: take the larger of the two\
\n# pollfreq values, and multiply by 3.\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon.powerdownflag") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# powerdownflag \"<n>\"\
\n#\
\n# Flag file for forcing UPS shutdown on the master system\
\n#\
\n# upsmon will create a file with this name in master mode when it's time\
\n# to shut down the load.  You should check for this file's existence in\
\n# your shutdown scripts and run 'upsdrvctl shutdown' if it exists.\
\n#\
\n# See the shutdown.txt file in the docs subdirectory for more information.\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon.notifymsg") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# notifymsg - change messages sent by upsmon when certain events occur\
\n#\
\n# You can change the stock messages to something else if you like.\
\n#\
\n# notifymsg.<notify type> = \"message\"\
\n#\
\n# notifymsg (\
\n# 	online = \"UPS %s is getting line power\"\
\n# 	onbatt = \"Someone pulled the plug on %s\"\
\n# )\
\n#\
\n# Note that %s is replaced with the identifier of the UPS in question.\
\n#\
\n# Possible values for <notify type>:\
\n#\
\n# online   : UPS is back online\
\n# onbatt   : UPS is on battery\
\n# lowbatt  : UPS has a low battery (if also on battery, it's \"critical\")\
\n# fsd      : UPS is being shutdown by the master (FSD = \"Forced Shutdown\")\
\n# commok   : Communications established with the UPS\
\n# commbad  : Communications lost to the UPS\
\n# shutdown : The system is being shutdown\
\n# replbatt : The UPS battery is bad and needs to be replaced\
\n# nocomm   : A UPS is unavailable (can't be contacted for monitoring)\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon.notifyflag") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# notifyflag - change behavior of upsmon when NOTIFY events occur\
\n#\
\n# By default, upsmon sends walls (global messages to all logged in users)\
\n# and writes to the syslog when things happen.  You can change this.\
\n#\
\n# notifyflag.<notify type> = \"<flag>[+<flag>][+<flag>]\"\
\n#\
\n# notifyflag ( \
\n# 	online = \"SYSLOG\"\
\n# 	onbatt = \"SYSLOG+WALL+EXEC\"\
\n# )\
\n#\
\n# Possible values for the flags:\
\n#\
\n# SYSLOG - Write the message in the syslog\
\n# WALL   - Write the message to all users on the system\
\n# EXEC   - Execute NOTIFYCMD (see above) with the message\
\n# IGNORE - Don't do anything\
\n#\
\n# If you use IGNORE, don't use any other flags on the same line.\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon.rbwarntime") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# rbwarntime \"<n>\" - replace battery warning time in seconds\
\n#\
\n# upsmon will normally warn you about a battery that needs to be replaced\
\n# every 43200 seconds, which is 12 hours.  It does this by triggering a\
\n# NOTIFY_REPLBATT which is then handled by the usual notify structure\
\n# you've defined in the notifyflag section.\
\n#\
\n# If this number is not to your liking, override it here.\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# nocommwarntime \"<n>\" - no communications warning time in seconds\
\n#\
\n# upsmon will let you know through the usual notify system if it can't\
\n# talk to any of the UPS entries that are defined in this file.  It will\
\n# trigger a NOTIFY_NOCOMM by default every 300 seconds unless you\
\n# change the interval with this directive.\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsmon") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# finaldealy \"<n>\" - last sleep interval before shutting down the system\
\n#\
\n# On a master, upsmon will wait this long after sending the NOTIFY_SHUTDOWN\
\n# before executing your SHUTDOWNCMD.  If you need to do something in between\
\n# those events, increase this number.  Remember, at this point your UPS is\
\n# almost depleted, so don't make this too high.\
\n#\
\n# Alternatively, you can set this very low so you don't wait around when\
\n# it's time to shut down.  Some UPSes don't give much warning for low\
\n# battery and will require a value of 0 here for a safe shutdown.\
\n#\
\n# Note: If FINALDELAY on the slave is greater than HOSTSYNC on the master,\
\n# the master will give up waiting for the slave to disconnect.\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsd.acl") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# Access Control Lists (ACLs)\
\n#\
\n# acl.<name> = \"<ipblock>\"\
\n#\
\n# acl (\
\n# 	localhost = \"10.0.0.1/32\"\
\n#	all = \"0.0.0.0/0\"\
\n#)\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsd.accept") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# accept = { \"<aclname>\" [\"<aclname>\"] ... }\
\n#\
\n# Define lists of hosts or networks with ACL definitions.\
\n#\
\n# accept use ACL definitions to control whether a host is\
\n# allowed to connect to upsd.\
\n#\
\n# refer to acl section for defined acl name\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsd.reject") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# reject = { \"<aclname>\" [\"<aclname>\"] ... }\
\n#\
\n# Define lists of hosts or networks with ACL definitions.\
\n#\
\n# reject use ACL definitions to control whether a host is\
\n# not allowed to connect to upsd.\
\n#\
\n# refer to acl section for defined acl name\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
	if (strcmp(name, "nut.upsd.maxage") == 0 ) {
		s = "\n# --------------------------------------------------------------------------\
\n# maxage = \"<seconds>\"\
\n# maxage = \"15\"\
\n#\
\n# This defaults to 15 seconds.  After a UPS driver has stopped updating\
\n# the data for this many seconds, upsd marks it stale and stops making\
\n# that information available to clients.  After all, the only thing worse\
\n# than no data is bad data.\
\n#\
\n# You should only use this if your driver has difficulties keeping\
\n# the data fresh within the normal 15 second interval.  Watch the syslog\
\n# for notifications from upsd about staleness.\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}

	if (strcmp(name, "nut.users_desc") == 0 ) {
		s = "\n# Network UPS Tools: users section\
\n#\
\n# This section sets the permissions for upsd - the UPS network daemon.\
\n# Users are defined here, are given passwords, and their privileges are\
\n# controlled here too. \
\n\
\n# --------------------------------------------------------------------------\
\n\
\n# Each user gets a section in the users section.\
\n# The username is case-sensitive, so admin and AdMiN are two different users.\
\n#\
\n# Example for a user nammed \"myuser\"\
\n#\
\n# <myuser> (\
\n#	type = \"admin\" \
\n#	password = \"mypass\"\
\n#	allowfrom = { \"localhost\" \"adminbox\" }\
\n# )\
\n\
\n# Possible settings:\
\n#\
\n# password: The user's password.  This is case-sensitive.\
\n#\
\n# --------------------------------------------------------------------------\
\n#\
\n# allowfrom: ACL names that this user may connect from.  ACLs are\
\n#            defined in upsd.conf.\
\n#\
\n# It is a list, so put the values between brace\
\n#\
\n# --------------------------------------------------------------------------\
\n#\
\n# actions: Let the user do certain things with upsd.\
\n#\
\n# Valid actions are:\
\n#\
\n# SET   - change the value of certain variables in the UPS\
\n# FSD   - set the \"forced shutdown\" flag in the UPS\
\n#\
\n# It is a list, so put the values between brace\
\n#\
\n# --------------------------------------------------------------------------\
\n#\
\n# instcmds: Let the user initiate specific instant commands.  Use \"ALL\"\
\n# to grant all commands automatically.  There are many possible\
\n# commands, so use 'upscmd -l' to see what your hardware supports.  Here\
\n# are a few examples:\
\n#\
\n# test.panel.start      - Start a front panel test\
\n# test.battery.start    - Start battery test\
\n# test.battery.stop     - Stop battery test\
\n# calibrate.start       - Start calibration\
\n# calibrate.stop        - Stop calibration\
\n#\
\n# It is a list, so put the values between brace\
\n#\
\n# --------------------------------------------------------------------------\
\n\
\n#\
\n# --- Configuring for upsmon\
\n#\
\n# To add a user for your upsmon, use this example:\
\n#\
\n# <monuser> (\
\n#	type = \"upsmon_master\" (or \"upsmon_slave\")\
\n# 	password  = \"pass\"\
\n#   allowfrom = \"bigserver\"\
\n#\
\n\
\n# The matching monitor section in your upsmon section would look like this:\
\n#\
\n# monitor.myups@myhost (\
\n#	powervalue = \"1\"\
\n#	user = \"monuser\"\
\n# )\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
if (strcmp(name, "nut.ups_desc") == 0 ) {
		s = "\n# Network UPS Tools: example ups.conf\
\n#\
\n# --- SECURITY NOTE ---\
\n#\
\n# If you use snmp-ups and set a community string in here, you\
\n# will have to secure this file to keep other users from obtaining\
\n# that string.  It needs to be readable by upsdrvctl and any drivers,\
\n# and by upsd.\
\n#\
\n# ---\
\n#\
\n# This is where you configure all the UPSes that this system will be\
\n# monitoring directly.  These are usually attached to serial ports, but\
\n# USB devices and SNMP devices are also supported.\
\n#\
\n# This file is used by upsdrvctl to start and stop your driver(s), and\
\n# is also used by upsd to determine which drivers to monitor.  The\
\n# drivers themselves also read this file for configuration directives.\
\n#\
\n# The general form is:\
\n#\
\n# <upsname> (\
\n# 	driver (\
\n#		name = \"<drivername>\"\
\n# 		port = \"<portname>\"\
\n#	)\
\n#	[desc = \"UPS description\"]\
\n#	.\
\n#	.\
\n# )\
\n#\
\n# The name od the ups (<upsname]> can be just about anything as long as\
\n# it is a single word inside containing only letters, number, '-' and '_'.\
\n#  upsd uses this to uniquely identify a UPS on this system.\
\n#\
\n# If you have a UPS called snoopy on a system called \"doghouse\", \
\n# the section in your upsmon section to monitor it would look something\
\n# like this:\
\n#\
\n# monitor.snoopy@doghouse (\
\n# 	powervalue = \"1\" \
\n# 	user = \"upsmonuser\"\
\n# )\
\n#\
\n\
\n# Configuration directives\
\n# ------------------------\
\n#\
\n# These directives are common to all drivers that support ups section:\
\n#\
\n#  driver.name: REQUIRED.  Specify the program to run to talk to this UPS.\
\n#          apcsmart, fentonups, bestups, and sec are some examples.\
\n#\
\n#  driver.port: REQUIRED.  The serial port where your UPS is connected.\
\n#          /dev/ttyS0 is usually the first port on Linux boxes, for example.\
\n#\
\n#  sdorder: optional.  When you have multiple UPSes on your system, you\
\n#          usually need to turn them off in a certain order.  upsdrvctl\
\n#          shuts down all the 0s, then the 1s, 2s, and so on.  To exclude\
\n#          a UPS from the shutdown sequence, set this to -1.\
\n#\
\n#          The default value for this parameter is 0.\
\n#\
\n#  nolock: optional, and not recommended for use in this file.\
\n#\
\n#          If you put nolock in here, the driver will not lock the\
\n#          serial port every time it starts.  This may allow other\
\n#          processes to seize the port if you start more than one by\
\n#          mistake.\
\n#\
\n#          This is only intended to be used on systems where locking\
\n#          absolutely must be disabled for the software to work.\
\n#\
\n# maxstartdelay: optional.  This can be set as a global variable\
\n#                above your first UPS definition and it can also be\
\n#                set in a UPS section.  This value controls how long\
\n#                upsdrvctl will wait for the driver to finish starting.\
\n#                This keeps your system from getting stuck due to a\
\n#                broken driver or UPS.\
\n#\
\n#                The default is 45 seconds.\
\n#\
\n#\
\n# Anything else is passed through to the hardware-specific part of\
\n# the driver.\
\n#\
\n# Examples\
\n# --------\
\n#\
\n# A simple example for a UPS called \"powerpal\" that uses the fentonups\
\n# driver on /dev/ttyS0 is:\
\n#\
\n# powerpal (\
\n# 	driver (\
\n# 		name = \"fentonups\"\
\n#       parameter.port = \"/dev/ttyS0\"\
\n#	)\
\n#	desc = \"Web server\"\
\n# )\
\n#\
\n# If your UPS driver requires additional settings, you can specify them\
\n# in the driver.parameter section. For example, if it supports a setting\
\n# of \"1234\" for the variable \"cable\", it would look like this:\
\n#\
\n# myups (\
\n# 	driver (\
\n# 		name = \"mydriver\"\
\n# 		parameter (\
\n#				port = \"/dev/ttyS1\"\
\n#				cable = \"1234\"\
\n#		)\
\n#	)\
\n#	desc = \"Something descriptive\"\
\n# )\
\n#\
\n# To find out if your driver supports any extra settings, start it with\
\n# the -h option and/or read the driver's documentation.\
\n# --------------------------------------------------------------------------\n\n";
		fwrite( s, strlen(s), 1, conf_file);
		return;
	}
}
