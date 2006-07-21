/* data_type.c - Definition of types of data and accessors to thoses types

   Copyright (C) 2006 Jonathan Dion <dion.jonathan@gmail.com>

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
#include <string.h>

#include "data_types.h"
#include "common.h"

t_string string_copy(t_string string) {
	t_string string2;
	
	string2 = (t_string)xmalloc(sizeof(char) * (strlen(string) + 1));
	
	strcpy(string2, string);
	
	return string2;
}


t_enum_string new_enum_string(t_string value) {
	t_enum_string enum_string;
	enum_string = (t_enum_string)xmalloc(sizeof(struct _t_enum_string));
	
	enum_string->value = string_copy(value);
	enum_string->next_value = 0;
	
	return enum_string;
}

t_enum_string add_to_enum_string(t_enum_string enum_string, t_string value) {
	
	t_enum_string save;
	
	if (enum_string == 0) {
		return new_enum_string(value);
	}
	
	save = enum_string;
	
	while(enum_string->next_value != 0) {
		enum_string = enum_string->next_value; 
	}
	// end of the enum string reached, the new value is to be added here
	enum_string->next_value = new_enum_string(value);
	if (enum_string->next_value == 0) {
		// There was an error
		return 0;
	}
	return save;
}

void del_from_enum_string(t_enum_string* p_enum_string, t_string value) {
	t_enum_string enum_string2;
	
	if (*p_enum_string == 0) {
		return;
	}
	
	if (strcmp((*p_enum_string)->value, value) == 0) {
		// The string to delete is at the first place
		enum_string2 = (*p_enum_string)->next_value;
		(*p_enum_string)->next_value = 0;
		free_enum_string(*p_enum_string);
		*p_enum_string = enum_string2;
		del_from_enum_string(p_enum_string, value);
	}
	
	del_from_enum_string(&((*p_enum_string)->next_value), value);
}
	
t_enum_string search_in_enum_string(t_enum_string enum_string, t_string value) {
	if (enum_string == 0) {
		return 0;
	}
	
	while (enum_string != 0) {
		if (strcmp(enum_string->value, value) == 0) {
			return enum_string;
		}
		enum_string = enum_string->next_value;
	}
	
	return 0;
}


t_enum_string enum_string_copy(t_enum_string string) {
	t_enum_string string2;
	
	if (string == 0) return 0;
	
	string2 = (t_enum_string)xmalloc(sizeof(struct _t_enum_string));
	string2->value = string_copy(string->value);
	
	string2->next_value = enum_string_copy(string->next_value);
	
	return string2;
}

void free_enum_string(t_enum_string enum_string) {
	t_enum_string next;
	
	if (enum_string == 0) return;
	next = enum_string->next_value;
	free(enum_string->value);
	free(enum_string);
	free_enum_string(next);
}

t_string enum_string_to_string(t_enum_string enum_string) {
	t_enum_string enum_string_save;
	t_string string;
	int string_size;
	
	if (enum_string == 0) {
		return string_copy("");
	}
	
	// We go through the list, so we use save the head pointer
	enum_string_save = enum_string;
	
	//
	// Calculate the size of the string to create
	//
	
	// +2 is for the quotes it add around the value
	string_size = strlen(enum_string->value) + 2;
	enum_string = enum_string->next_value;
	
	while(enum_string !=0) {
		// +1 for the space and +2 for quotes
		string_size += strlen(enum_string->value) + 1 + 2;
		enum_string = enum_string->next_value;
	}
	// Allocate this size (+1 because its a string)
	string = (t_string)xmalloc(sizeof(char) * (string_size + 1));
	
	//
	// Create the concatenation of the strings (+ added quotes and spaces)
	//
	
	enum_string = enum_string_save;
	
	string[0] = '\"';
	strcpy(string + 1, enum_string->value);
	string_size = strlen(enum_string->value) + 2;
	string[string_size - 1] = '\"';
	string[string_size] = 0;
	enum_string = enum_string->next_value;
	
	while(enum_string !=0) {
		*(string + string_size++) = ' ';
		string[string_size++ ] = '\"';
		strcpy(string + string_size, enum_string->value);
		string_size += strlen(enum_string->value) + 1;
		string[string_size - 1] = '\"';
		string[string_size] = 0;
		enum_string = enum_string->next_value;
	}
	
	// done ^_^
	return string;
}
