/*! 
 * @file data_type.h
 * @brief Definition of types of data and accessors to thoses types
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
*/

#ifndef DATA_TYPES_H_
#define DATA_TYPES_H_

// Some basic types
typedef int boolean;
typedef char* t_string;

#define TRUE 1
#define FALSE 0

/**
 * Structure to represente enumeration of chain
 */
typedef struct _t_enum_string {
	t_string value;
	struct _t_enum_string * next_value; 
} *t_enum_string;

/**
 * Enumeration of the rights possible for a tree node
 */
typedef enum {
	invalid_right,
	all_rw,
	all_r,
	all_r_admin_rw,
	admin_rw,
	admin_r,
} t_rights;

/**
 * Enumeration of NUT mode of functionment
 */
typedef enum {
	standalone,
	net_server,
	net_client,
	pm
} t_modes;


/**
 * Enumeration of the possible type of a user
 */
typedef enum {
	admin,
	upsmon_master,
	upsmon_slave,
	custom
} t_user_types;

/**
 * Enumeration of the possible notify flags
 */
typedef enum {
	SYSLOG,
	SYSLOGWALL,
	SYSLOGEXEC,
	SYSLOGWALLEXEC,
	WALL,
	WALLEXEC,
	EXEC,
	IGNORE
} t_flags;

/**
 * Enumeration of the possible notify event
 */
typedef enum {
	ONLINE,
	ONBATT,
	LOWBATT,
	FSD,
	COMMOK,
	COMMBAD,
	SHUTDOWN,
	REPLBATT,
	NOCOMM
} t_notify_events;

/**
 * Enumeration of possible type of data for the tree
 * 
 * @note Add here the new types in future modifications
 */
typedef enum {
	string_type,
	enum_string_type
} t_types;

/*
 * Functions to manage t_string
 */

/**
 * Copy a string
 * 
 * @param string The string to copy
 * 
 * @return A copy of the string
 * 
 * @note This function allocate place for the new string and use strcpy to duplicate a string
 */
t_string string_copy(t_string string);


/*
 * Functions to manage t_enum_string
 * 
 * Notes :
 * 		- An empty enumeration of string is represented by a null pointer
 * 		- Thus, an t_enum_string which is not a null pointer at least contains one value
 * 		  (this is why new_enum_string need a parameter)
 */
 
/**
 * Create a new enumeration of string initialized with the given string
 * 
 * @param value The string to initialize the enumeration with
 * 
 * @return The new enumeration of string.
 * 
 * @note This function copies the given string
 */
t_enum_string new_enum_string(t_string value);

/**
 * Add a value to an enumeration of string.
 * 
 * @param enum_string The enumeration of string to add a string to
 * @param value The string to add
 * 
 * @return A pointer to the enumeration of string
 * 
 * @note If enum_string is null, create a new enumeration of string
 * @note The new value is added at the end of the enumeration
 */
t_enum_string add_to_enum_string(t_enum_string enum_string, t_string value);

/**
 * Remove a string from an enumeration of string
 * 
 * @param p_enum_string A pointer to an enumeration of string
 * @param value the value of the string to remove
 * 
 * @note Remove all occurence of the given string in the enumeration
 * @note Only remove strings that perfectly match the given string.
 */
void del_from_enum_string(t_enum_string* p_enum_string, t_string value);

/**
 * Make a copy of an emumeration of string
 * 
 * @param enum_string The enumeration of string to copy
 * 
 * @return The copy of the given enumeration of string
 * 
 * @note Each string is copied
 */
t_enum_string enum_string_copy(t_enum_string enum_string);

/**
 * Free an enumeration of string
 * 
 * @param enum_string The enumeration of string to free
 * 
 * @note Free all the structure and the strings
 */
void free_enum_string(t_enum_string enum_string);

/**
 * Search a string in an enumeration of string
 * 
 * @param enum_string The enumeration to search in
 * @param value The string to search
 * 
 * @return The sub-enumeration begining at the first occurence of the searchd string. Null if not found
 */
t_enum_string search_in_enum_string(t_enum_string enum_string, t_string value);

/**
 * Convert an enumeration of string into a string (for display for instance)
 * 
 * @param enum_string The enumeration of string to convert
 * 
 * @return A string represented the enumeration of string on the format : "\"string1\" \"string2\" ..."
 */
t_string enum_string_to_string(t_enum_string enum_string);

/**
 * Convert right to string
 * 
 * @param right The right to convert
 * 
 * @return The string that represent the right
 */
t_string right_to_string(t_rights right);

/**
 * Convert string to right
 * 
 * @param s The string to convert
 * 
 * @return The right represented by the string. return invalid_right if the string does not match any other right
 */
t_rights string_to_right(t_string s);

/**
 * Convert user_type to string
 * 
 * @param type The user_type to convert
 * 
 * @return The string that represent the user_type
 */
t_string user_type_to_string(t_user_types type);

/**
 * Convert string into user_type
 * 
 * @param s The string to convert
 * 
 * @return The user_type, or -1 if the string does not match any
 */
t_user_types string_to_user_type(t_string s);

/**
 * Convert string into mode
 * 
 * @param s The string to convert
 * 
 * @return The corresponding mode, or -1 if not any
 */
t_modes string_to_mode(t_string s);

/**
 * Convert mode to string
 * 
 * @param mode The mode to convert
 * 
 * @return A string that represent the mode
 */
t_string mode_to_string(t_modes mode);

/**
 * Convert a string into a notify event
 * 
 * @param s The string to convert
 * 
 * @return The corresponding notify event, or -1 if not any
 */
t_notify_events string_to_event(t_string s);

/**
 * Convert a notify event into a string
 * 
 * @param event The notify event to convert
 * 
 * @return A string that represent the notify event
 */
t_string event_to_string(t_notify_events event);

/**
 * Convert a string into a notify flag
 * 
 * @param s The string to convert
 * 
 * @return A notify flag that correspond to he string, or -1 if not any
 */
t_flags string_to_flag(t_string s);

/**
 * Convert a notify flag into a string
 * 
 * @param flag The notify flag to convert
 * 
 * @return A string representing the flag
 */
t_string flag_to_string(t_flags flag);

#endif /*DATA_TYPES_H_*/
