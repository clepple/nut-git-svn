/*! 
 * @file libupsconfig.h
 * @brief an API to manipulate configuration of NUT
 * 
 * @author Copyright (C) 2006 Jonathan Dion <dion.jonathan@gmail.com>
 *
 * This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "data_types.h"
#include "tree.h"
 

void new_config();
void load_config(t_string filename);

void save_config(t_string directory, boolean single);

t_modes get_mode();
void set_mode(t_modes mode);
t_rights get_rights();
void set_rights(t_rights right);

t_enum_string get_ups_list();
void add_ups(t_string upsname, t_string driver, t_string port);
int remove_ups(t_string upsname);
int search_ups(t_string upsname);
t_string get_ups_name();
void set_ups_name(t_string upsname);
t_string get_driver();
void set_driver(t_string driver);
t_string get_port();
void set_port(t_string port);
t_string get_desc();
void set_desc(t_string desc);
t_enum_string get_driver_parameter_list();
t_string get_driver_parameter(t_string paramname);
void set_driver_parameter(t_string paramname, void* value, t_types type);
t_string get_ups_variable(t_string varname);
void set_ups_variable(t_string varname, void* value, t_types type);

t_enum_string get_users_list();
void add_user(t_string username, t_user_types type, t_string password);
int remove_user(t_string username);
int search_user(t_string username);
t_string get_name();
void set_name(t_string username);
t_user_types get_type();
void set_type(t_user_types type);
t_string get_password();
void set_password(t_string password);
t_enum_string get_allowfrom();
void set_allowfrom(t_enum_string acllist);
t_enum_string get_actions();
void set_actions(t_enum_string actionlist);
t_enum_string get_instcmds();
void set_instcmds(t_enum_string instcmdlist);

t_enum_string get_acl_list();
void add_acl(t_string aclname, t_string value);
int remove_acl(t_string aclname);
t_string get_acl_value(t_string aclname);
void set_acl_value(t_string aclname, t_string value);
t_enum_string get_accept();
void set_accept(t_enum_string acllist);
t_enum_string get_reject();
void set_reject(t_enum_string acllist);
int get_maxage();
void set_maxage(int value);

int get_monitor_rules_number();
void add_monitor_rule(t_string upsname, t_string host, int powervalue, t_string username);
int remove_monitor_rule(int rulenumber);
int search_monitor_rule(int rulenumber);
t_string get_monitor_ups();
t_string get_minotor_host();
int get_monitor_powervalue();
t_string get_monitor_user();
t_string get_shutdown_command();
void set_shutdown_command(t_string command);
t_flags get_notify_flag(t_notify_events event);
void set_notify_flag(t_notify_events event, t_flags flag);
t_string get_notify_message(t_notify_events event);
void set_notify_message(t_notify_events event, t_string message);
t_string get_notify_command();
void set_notify_command(t_string command);
int get_deadtime();
void set_deadtime(int value);
int get_finaldelay();
void set_finaldelay(int value);
int get_hostsync();
void set_hostsync(int value);
int get_minsupplies();
void set_minsupplies(int value);
int get_nocommwarntime();
void set_nocommwarntime(int value);
int get_rbwarntime();
void set_rbwarntime(int value);
int get_pollfreq();
void set_pollfreq(int value);
int get_pollfreqalert();
void set_pollfreqalert(int value);
t_string get_powerdownflag();
void set_powerdownflag(t_string filename);
t_string get_runasuser();
void set_runasuser(t_string username);

