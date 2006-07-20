#ifndef TYPES_H_
#define TYPES_H_

typedef char* t_string;

/**
 * Structure to represente enumeration of chain
 */
typedef struct _t_enum_string {
	t_string value;
	struct _t_enum_string * next_value; 
} *t_enum_string;

/**
 * Add here the new types in future modifications
 */
typedef enum {
	string_type,
	enum_string_type
} t_types;

t_string string_copy(t_string string);

t_enum_string new_enum_string(t_string value);
t_enum_string add_to_enum_string(t_enum_string enum_string, t_string value);
void del_from_enum_string(t_enum_string* p_enum_string, t_string value);
t_enum_string enum_string_copy(t_enum_string enum_string);
void free_enum_string(t_enum_string enum_string);
t_enum_string search_in_enum_string(t_enum_string enum_string, t_string value);
t_string enum_string_to_string(t_enum_string enum_string);

#endif /*TYPES_H_*/
