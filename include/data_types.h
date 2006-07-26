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

typedef enum {
	standalone,
	net_server,
	net_client,
	pm
} t_modes;

typedef enum {
	admin,
	upsmon_master,
	upsmon_slave,
	custom
} t_user_types;

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

t_string user_type_to_string(t_user_types type);
t_user_types string_to_user_type(t_string s);
t_modes string_to_mode(t_string s);
t_string mode_to_string(t_modes mode);
t_notify_events string_to_event(t_string s);
t_string event_to_string(t_notify_events event);
t_flags string_to_flag(t_string s);
t_string flag_to_string(t_flags flag);

#endif /*DATA_TYPES_H_*/
