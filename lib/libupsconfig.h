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
 
 /* TODO
  * 
  * The saving of rights notion have been disabled for the 
  * moment, because no application use it for the moment,
  * and we don't want users to wonder what it is and/or
  * what it does.
  * 
  * If you want to enable it back, define the flag CONFIG_USE_RIGHT
  * 
  * This notion of right have been made for a future evolution
  * with a unified tree for data and configuration, to be
  * able for instance to modify configuration "in live" and to
  * protect the acces of data in this tree (like password, or 
  * variable you need right to set, and so on)
  * 
  */

#include "data_types.h"
#include "tree.h"

typedef struct _t_typed_value {
	boolean has_value;
	t_value value;
	t_types type;
} t_typed_value;
 
/* *************************************************
 *              INITIALIZATION SECTION             *
 ***************************************************/
 
/**
 * Create a new (empty) configuration tree.
 * 
 * @note Initialize the "current" values to null and current.right to admin_r
 */
void new_config();

/**
 * Load a configuration from a files.
 * 
 * @param filename The name of the file to load the configuration from
 * @param errhandler A function to use to manage errors. Put 0 to use fprintf on sdterr
 * 
 * @return 1 if the configuration is succesfully loaded, 0 if error
 * 
 * @note Initialize the "current" values to null and current.right to admin_r
 */
int load_config(t_string filename, void errhandler(const char*));

/**
 * Drop the current configuration and free associated memory
 */
void drop_config();


/* *************************************************
 *                 SAVING SECTION                  *
 ***************************************************/

/**
 * Save the current configuration in configuration file
 * 
 * @param directory_dest The directory where to save the files in
 * @param comm_filename  The comments template file to use to comment the output file
 * @param single A boolean to choose to save in the single or multi file format
 * @param errhandler The function to use to manage error. Put 0 to use fprintf on stderr
 * 
 * @return 0 if an error occured, else 1
 * 
 * @note Don't drop the configuration. Use drop_config to free memory
 */
int save_config(t_string directory_dest, t_string comm_filename, boolean single, void errhandler(const char*));


/* *************************************************
 *              GENERAL LEVEL SECTION              *
 ***************************************************/
 
/**
 * Return the mode of the configuration
 * 
 * @return The mode if it was set, -1 else
 */
t_modes get_mode();

/**
 * Set the mode of the configuration
 * 
 * @param mode The mode to set. Valid values are : standalone, net_server, net_client and pm
 */
void set_mode(t_modes mode);

/**
 * Return the right of the last variable got
 * 
 * @return The right of the last variable got by a get_* function
 */
t_rights get_rights();

/**
 * Set the right to give to variables
 * 
 * @param right The right to give to variables
 * 
 * @note All subsequent calls to functions set_* will set the right of the set variable to this right
 */
void set_rights(t_rights right);

/**
 * Return the list of variable that is under the given path
 * 
 * @param path The path to list the variable of
 * 
 * @return the list of found variables
 */
t_enum_string get_variable_list(t_string path);

/**
 * Return the value of a variable in the NUT tree
 * 
 * @param varname The name of the variable to return the value of
 * 
 * @return The value of the variable
 * 
 * @note varname must be the absolute path in the tree to the variable
 * @note See structure t_typed_value to know more about return value
 */
t_typed_value get_variable(t_string varname);

/**
 * Set the value of a variable in the NUT tree
 * 
 * @param varname The name of the variable to set the value of
 * @param value The value
 * @param type the type of the value
 * 
 * @note varname must be the absolute path in the tree to the variable
 */
void set_variable(t_string varname, void* value, t_types type);


/* *************************************************
 *                   UPS SECTION                   *
 ***************************************************/
 
/**
 * Return the list of declared UPSes in the current configuration
 * 
 * @return The list of declared UPSes in the current configuration
 * 
 * @note See the t_enum_string structure in data_types.h to know how to use it
 */
t_enum_string get_ups_list();

/**
 * Add an UPS to the configuration
 * 
 * @param upsname The name to give to the UPS
 * @param driver The driver to use to manage this UPS
 * @param port In which port is this UPS
 * 
 * @note If an UPS nammed upsname already exist, it will overwrite it
 */
void add_ups(t_string upsname, t_string driver, t_string port);

/**
 * Remove an UPS from the configuration
 * 
 * @param upsname The name of the UPS to remove.
 * 
 * @return 0 if an error occured, else 1
 */
int remove_ups(t_string upsname);

/**
 * Set current.ups to the desired UPS
 * 
 * @param upsname The name of the UPS to set current.ups to
 * 
 * @return 0 if the UPS was not found, else 1
 * 
 * @note if it returned 0, current.ups is set to Null. Then function that use this
 * variable will not work until current.ups value is Null
 */
int search_ups(t_string upsname);

/**
 * Return the name of the current UPS
 * 
 * @return A t_string containing the name, or 0 if current.ups is Null
 */
t_string get_ups_name();

/*
 * Set the name of the current ups
 * 
 * @param upsname The new name to give to the current ups
 * 
 * NOT IMPLEMENTED YET
 * It need to modify the name of each sons of the ups.
 * Use add_ups then remove_ups for the moment
 */
/*void set_ups_name(t_string upsname); */

/**
 * Return the name of the driver of the current UPS
 * 
 * @return The name of the driver of the current UPS
 */
t_string get_driver();

/**
 * Set the driver to use for the current UPS
 * 
 * @param driver The name of the driver to use
 */
void set_driver(t_string driver);

/**
 * Return the port of the current UPS
 * 
 * @return The port of the current UPS
 */
t_string get_port();

/**
 * Set the port to search the current UPS on
 * 
 * @param port The port to search the current UPS on
 */
void set_port(t_string port);

/**
 * Return the description of the current UPS
 * 
 * @return The description of the current UPS
 */
t_string get_desc();

/**
 * Set the description of the current UPS
 * 
 * @param desc The description of the current UPS
 */
void set_desc(t_string desc);

/**
 * Return the list of driver parameters of the current UPS
 * 
 * @return The list of driver parameters
 * 
 * @note See the t_enum_string structure in data_types.h to know how to manage it
 */
t_enum_string get_driver_parameter_list();

/**
 * Return the value of a driver parameter
 * 
 * @param paramname The name of the driver parameter to return the value of
 * 
 * @return The value of the driver parameter
 * 
 * @note See structure t_typed_value to know more about return value
 */
t_typed_value get_driver_parameter(t_string paramname);

/**
 * Set the value of a driver parameter
 * 
 * @param paramname The name of the driver parameter to set the value of
 * @param value The value
 * @param type the type of the value
 */
void set_driver_parameter(t_string paramname, void* value, t_types type);

/**
 * Return the list of driver flag of the current UPS
 * 
 * @return The list of driver flag
 * 
 * @note See the t_enum_string structure in data_types.h to know how to manage it
 */
t_enum_string get_driver_flag_list();

/**
 * Enable a driver parameter
 * 
 * @param flagname The name of the driver flag to enable
 */
void enable_driver_flag(t_string flagname);

/**
 * Disable a driver parameter
 * 
 * @param flagname The name of the driver flag to disable
 */
void disable_driver_flag(t_string flagname);

/**
 * Return the list of variable of the current UPS
 * 
 * @return The list of variable
 * 
 * @note See the t_enum_string structure in data_types.h to know how to manage it
 */
t_enum_string get_ups_variable_list();

/**
 * Return the list of sub variable of a variable of the current UPS
 * 
 * @return The list of variable
 * 
 * @note See the t_enum_string structure in data_types.h to know how to manage it
 */
t_enum_string get_ups_subvariable_list(t_string varname);

/**
 * Return the value of an UPS variable
 * 
 * @param varname The name of the UPS variable to return the value of
 * 
 * @return The value of the UPS variable
 * 
 * @note See structure t_typed_value to know more about return value
 */
t_typed_value get_ups_variable(t_string varname);

/**
 * Set the value of an UPS variable
 * 
 * @param varname The name of UPS variable to set the value of
 * @param value The value
 * @param type the type of the value
 */
void set_ups_variable(t_string varname, void* value, t_types type);


/* *************************************************
 *                  USERS SECTION                  *
 ***************************************************/
 
/**
 * Return the list of declared users in the current configuration
 * 
 * @return The list of declared users in the current configuration
 * 
 * @note See the t_enum_string structure in data_types.h to know how to use it
 */ 
t_enum_string get_users_list();

/**
 * Add an user to the configuration
 * 
 * @param username The name to give to the new user
 * @param type The type of the new user. Valid values are : admin, upsmon_master, upsmon_slave and custom.
 * @param password The password of the user
 * 
 * @note If an user nammed username already exist, it will overwrite it
 * @note /!\ password should be given different rights. At least dont be readable
 * by others than admin. It is recommended to use set_right then set_password to
 * set the password.
 */
void add_user(t_string username, t_user_types type, t_string password);

/**
 * Remove an user from the configuration
 * 
 * @param username The name of the user to remove.
 * 
 * @return 0 if an error occured, else 1
 */
int remove_user(t_string username);

/**
 * Set current.user to the desired user
 * 
 * @param username The name of the user to set current.user to
 * 
 * @return 0 if the user was not found, else 1
 * 
 * @note if it returned 0, current.user is set to Null. Then function that use this
 * variable will not work while current.user value is Null
 */
int search_user(t_string username);

/**
 * Return the name of the current user
 * 
 * @return A t_string containing the name, or 0 if current.user is Null
 */
t_string get_name();

/*
 * Set the name of the current user
 * 
 * @param username The new name to give to the current user
 * 
 * NOT IMPLEMENTED YET
 * It need to modify the name of each sons of the user.
 * Use add_user then remove_user for the moment
 */
/*void set_name(t_string username); */

/**
 * Return the type of the current user
 * 
 * @return The type of the current user
 */
t_user_types get_type();

/**
 * Set the type of the current user
 * 
 * @param type The new type to give to the current user
 */
void set_type(t_user_types type);

/**
 * Return the password of the current user
 * 
 * @return The password of the current user
 */
t_string get_password();

/**
 * Set the password of the current user
 * 
 * @param type The new password to give to the current user
 */
void set_password(t_string password);

/**
 * Return the allowfrom list of value of the current user
 * 
 * @return The allowfrom list of value of the current user, or 0 if not defined
 */
t_enum_string get_allowfrom();

/**
 * Set the allowfrom list of value of the current user
 * 
 * @param type The new allowfrom list of value to give to the current user
 */
void set_allowfrom(t_enum_string acllist);

/**
 * Return the list of actions of the current user
 * 
 * @return The list of actions of the current user, or 0 if not defined
 */
t_enum_string get_actions();

/**
 * Set the list of actions of the current user
 * 
 * @param type The new list of actions to give to the current user
 */
void set_actions(t_enum_string actionlist);

/**
 * Return the list of instant commands of the current user
 * 
 * @return The list of instant commands of the current user, or 0 if not defined
 */
t_enum_string get_instcmds();

/**
 * Set the list of instant commands of the current user
 * 
 * @param type The new list of instant commands to give to the current user
 */
void set_instcmds(t_enum_string instcmdlist);


/* *************************************************
 *                   UPSD SECTION                  *
 ***************************************************/

/**
 * Return the list of declared acl in the current configuration
 * 
 * @return The list of declared acl in the current configuration
 * 
 * @note See the t_enum_string structure in data_types.h to know how to use it
 */ 
t_enum_string get_acl_list();

/**
 * Remove an acl declaration from the configuration
 * 
 * @param aclname The name of the acl to remove.
 * 
 * @return 0 if an error occured, else 1
 */
int remove_acl(t_string aclname);

/**
 * Return the value of an acl
 * 
 * @param aclname The name of the acl to return the value of
 * 
 * @return The value of the acl, 0 if not defined
 */
t_string get_acl_value(t_string aclname);

/**
 * Set an acl declaration in the configuration
 * 
 * @param aclname The name to give to the new acl
 * @param value The value of the acl, in standart format ("192.168.0.3/32" for instance)
 * 
 * @note If no acl nammed aclname already exist, it will will create it
 */
void set_acl_value(t_string aclname, t_string value);

/**
 * Return the list of accepted acl of upsd
 * 
 * @return The list of accepted acl.
 * 
 * @note See the t_enum_string structure in data_types.h to know how to use it
 */
t_enum_string get_accept();

/**
 * Set the list of accepted acl of upsd
 * 
 * @param acllist The list of acl that upsd will accept
 */
void set_accept(t_enum_string acllist);

/**
 * Return the list of rejected acl of upsd
 * 
 * @return The list of rejected acl.
 * 
 * @note See the t_enum_string structure in data_types.h to know how to use it
 */
t_enum_string get_reject();

/**
 * Set the list of rejected acl of upsd
 * 
 * @param acllist The list of acl that upsd will reject
 */
void set_reject(t_enum_string acllist);

/**
 * Return the max time for an ups value before being declared staled if not updated
 * 
 * @return The maxage value of upsd
 */
int get_maxage();

/**
 * Set the max time for an ups value before being declared staled if not updated
 * 
 * @value The maxage value for upsd
 */
void set_maxage(int value);


/* *************************************************
 *                 UPSMON SECTION                  *
 ***************************************************/

/**
 * Return the number of declared monitor rules in upsmon configuration
 * 
 * @return The number of declared monitor rules
 */
int get_number_of_monitor_rules();

/**
 * Add a monitor rule to the upsmon configuration
 * 
 * @param upsname The name of the ups to monitor
 * @param host The host on which the ups is connected (the address and eventualy port to use to connect to upsd server)
 * @param powervalue The power value of the UPS for this computer.
 * @param username The user to use to connect to upsd
 */
void add_monitor_rule(t_string upsname, t_string host, int powervalue, t_string username);

/**
 * Remove a monitor rule from the configuration
 * 
 * @param rulenumber The number of the rule to remove.
 * 
 * @return 0 if an error occured, else 1
 */
int remove_monitor_rule(int rulenumber);

/**
 * Set current.monitor_rule to the desired rule
 * 
 * @param rulenumber The number of the rule to set current.monitor_rule to
 * 
 * @return 0 if the monitor rule was not found, else 1
 * 
 * @note if it returned 0, current.monitor_rule is set to Null. Then function that use this
 * variable will not work while current.monitor_rule value is Null
 */
int search_monitor_rule(int rulenumber);

/**
 * Return the name of the system monitored (that is upsname@host)
 * 
 * @return The name of the system monitored by the current monitor rule
 */
t_string get_monitor_system();

/**
 * Return the name of the ups monitored by the current monitor rule
 * 
 * @return The name of the ups monitored by the current monitor rule
 */
t_string get_monitor_ups();

/**
 * Return the host value of the current monitor rule
 * 
 * @return The host value of the current monitor rule
 */
t_string get_monitor_host();

/**
 * Return the power value of the current monitor rule
 * 
 * @return The power value of the current monitor rule
 */
int get_monitor_powervalue();

/**
 * Set the power value of the current monitor rule
 * 
 * @param value the new power value to set
 */
void set_monitor_powervalue(int value);

/**
 * Return the user name of the current monitor rule
 * 
 * @return The user name of the current monitor rule
 */
t_string get_monitor_user();

/**
 * Set the user name of the current monitor rule
 * 
 * @param username the new user name to set
 * 
 * @note username must match an user in the users section for the monitor rule to work
 */
void set_monitor_user(t_string username);

/**
 * Return the shutdown command used by upsmon
 * 
 * @return The shutdown command used by upsmon
 */
t_string get_shutdown_command();

/**
 * Set the shutdown command used by upsmon
 * 
 * @param The shutdown command to be used by upsmon
 * 
 * @note /!\ If an invalid command or a command that don't shutdown the computer is given
 * your computer will not shutdown even if UPS goes in critical state.
 */
void set_shutdown_command(t_string command);

/**
 * Return the flag associated to an event
 * 
 * @param event The event to see the flag associated to
 * 
 * @return The flag associated to an event
 * 
 * @see t_flags
 * @see t_notify_event
 */
t_flags get_notify_flag(t_notify_events event);

/**
 * Set the flag associated to an event
 * 
 * @param event The event to set the flag associated to
 * @param flag The flag to associate to the event
 * 
 * @see t_flags
 * @see t_notify_event
 */
void set_notify_flag(t_notify_events event, t_flags flag);

/**
 * Return the notify message associated to an event
 * 
 * @param event The event to see the notify message of
 * 
 * @return The notify message
 * 
 * @see t_notify_event
 */
t_string get_notify_message(t_notify_events event);

/**
 * Set the notify message associated to an event
 * 
 * @param event The event to set the flag associated to
 * @param message The notify message to associate to the event
 * 
 * @see t_notify_event
 */
void set_notify_message(t_notify_events event, t_string message);

/**
 * Return the notify command used when a flag EXEC is found
 * 
 * @return The notify command
 */
t_string get_notify_command();

/**
 * Set the notify command to use when a flag EXEC is found
 * 
 * @param command The notify command
 */
void set_notify_command(t_string command);

/**
 * Return the time upsmon wait before declaring an UPS "dead" when it cannot connect to upsd
 * 
 * @return The time
 */
int get_deadtime();

/**
 * Set the time upsmon wait before declaring an UPS "dead" when it cannot connect to upsd
 * 
 * @param value The time
 */
void set_deadtime(int value);

/**
 * Return the last sleep interval before shutting down the system
 *  
 * @return The time
 */
int get_finaldelay();

/**
 * Set the last sleep interval to wait before shutting down the system
 *  
 * @param value The time
 */
void set_finaldelay(int value);

/**
 * Return how long upsmon will wait before giving up on another upsmon
 *  
 * @return The time
 */
int get_hostsync();

/**
 * Set how long upsmon will wait before giving up on another upsmon
 *  
 * @param value The time
 */
void set_hostsync(int value);

/**
 * Return the minimum number power supplies needed to run the computer
 *  
 * @return The minimum number of power supplies
 */
int get_minsupplies();

/**
 * Set the minimum number power supplies needed to run the computer
 *  
 * @param value The minimum number of power supplies
 */
void set_minsupplies(int value);

/**
 * Return the time upsmon wait between two warning of "no communication"
 *  
 * @return The time
 */
int get_nocommwarntime();

/**
 * Set the time upsmon wait between two warning of "no communication"
 *  
 * @param value The time
 */
void set_nocommwarntime(int value);

/**
 * Return the time upsmon wait between two warning of "replace battery"
 *  
 * @return The time
 */
int get_rbwarntime();

/**
 * Set the time upsmon wait between two warning of "replace battery"
 *  
 * @param value The time
 */
void set_rbwarntime(int value);

/**
 * Return the time between two polling in normal activity
 *  
 * @return The time
 */
int get_pollfreq();

/**
 * Set the time between two polling in normal activity
 *  
 * @param value The time
 */
void set_pollfreq(int value);

/**
 * Return the time between two polling when UPS is on battery
 *  
 * @return The time
 */
int get_pollfreqalert();

/**
 * Set the time between two polling when UPS is on batery
 *  
 * @param value The time
 */
void set_pollfreqalert(int value);

/**
 * Return the flag file used for forcing UPS shutdown on the master system
 *  
 * @return The file name
 */
t_string get_powerdownflag();

/**
 * Set the flag file used for forcing UPS shutdown on the master system
 *  
 * @param filename The file name
 */
void set_powerdownflag(t_string filename);

/**
 * Return the name of the user upsmon run as
 *  
 * @return The name of the user
 */
t_string get_run_as_user();

/**
 * Return the name of the user to run upsmon as
 *  
 * @param username The name of the user
 */
void set_run_as_user(t_string username);

t_string get_cert_path();
void set_cert_path(t_string filename);
boolean get_cert_verify();
void set_cert_verify(boolean value);
boolean get_force_ssl();
void set_force_sll(boolean value);

void free_typed_value(t_typed_value value);
