/* tree.c : Implementation of a tree of data with access rights
	(More details follow)
	
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "tree.h"
#include "nutparser.h"
#include "data_types.h"
#include "common.h"

t_tree new_node(char* name, void* value, t_types type) {
	t_tree node;
	node = (t_tree)xmalloc(sizeof(struct _t_tree));
	if (value != 0) {
		node->has_value = 1;
		node->type = type;
		switch(type) {
			case string_type :
				node->value.string_value = *(char**)value;
				break;
			case enum_string_type :
				node->value.enum_string_value = *(t_enum_string*)value;
		}
	} else {
		node->has_value = 0;	
	}
	
	node->father = 0;
	node->son = 0;
	node->next_brother = 0;
	node->previous_brother = 0;
	node->right = all_r;
	node->name = string_copy(name);
	return node;
}


t_string extract_last_part(t_string string) {
	int i;
	int j;
	int size;
	t_string buffer;
	
	i = 0; j= 0;
	buffer = xmalloc(sizeof(char) * BUFFER_SIZE);
	
	size = strlen(string);
	
	while ( i < size ) {
		if (string[i] == '.') {
			j = 0;
		} else {
			buffer[j++] = string[i];
		}
		i++;
	}
	buffer[j] = 0;
	return buffer;
}

/*
 * Extract the next part of a path in a tree
 * 
 * For instance, if name is "nut.ups.myups1.driver" 
 * and current_name is "nut.ups"
 * 
 * the function will return "nut.ups.myups1"
 * 
 * Return 0 if the begining of name don't matches current_name
 * 
 * Supposes that name and current name are in a valid path format
 */
char* extract_next_part(char* name, char* current_name) {
	int first_size = strlen(current_name);
	int next_size;
	char* next_part;
	
	if (strncmp(name, current_name, first_size) != 0) {
		// the first_size first bytes of name don't match current_name
		return 0;
	}
	
	if (name[first_size] != '.') {
		// name is not an extension of current_name
		return 0;
	}
	next_size = first_size +1;
	
	// Calculates the length of the next part
	while((name[next_size] != '.') && (name[next_size] != 0)) {
		next_size++;
	}
	
	// Allocates required memory
	next_part = (char *)xmalloc(sizeof(char) * (next_size + 1));
	
	// Sets the string to its value
	memcpy(next_part, name, next_size);
	next_part[next_size] = 0;
	
	return next_part;
	
}

t_tree go_to_node(t_tree tree, char* name) {
	char* next_step;
	t_tree next_tree;
	
	if (tree == 0) {
		// Non initialized tree
		pconf_fatal_error("in go_to_node : Non initialized tree");
	}
	
	if (strcmp(tree->name, name) == 0) {
		// tree is the searched node
		return tree;
	}
	
	// else we search throught the sons of tree
	
	// get the name of the son to go to
	next_step = extract_next_part(name, tree->name);
	if (next_step == 0) {
		// paths don't match
		pconf_fatal_error("in go_to_node : path don't match");
	}
	
	next_tree = tree->son;
	if (next_tree == 0) {
		// There is no sons to search through
		free(next_step);
		return 0;
	}
	
	// itterate through the sons
	while(strcmp(next_tree->name, next_step) != 0) {
		next_tree = next_tree->next_brother;
		if (next_tree == 0) {
			// There is no other sons to search through
			free(next_step);
			return 0;
		}	
	}
	// Here next_tree->name == next_name, we progressed of one step in the tree
	// Let's make the next step :
	free(next_step);
	return go_to_node(next_tree, name);
	
}

t_tree go_to_node_or_create(t_tree tree, char* name) {
	char* next_step;
	t_tree next_tree;
	
	if (tree == 0) {
		// Non initialized tree
		pconf_fatal_error("in go_to_node_or_create : Non initialized tree");
	}
	
	if (strcmp(tree->name, name) == 0) {
		// tree is the searched node
		return tree;
	}
	
	// else we search throught the sons of tree
	
	// get the name of the son to go to
	next_step = extract_next_part(name, tree->name);
	if (next_step == 0) {
		// paths don't match
		pconf_fatal_error("in go_to_node_or_create : paths don't match");
	}
	
	next_tree = tree->son;
	if (next_tree == 0) {
		// There is no sons to search through, we will create it
		tree->son = new_node(next_step, 0, 0);
		tree->son->father = tree;
		// then let's make the next step
		free(next_step);
		return go_to_node_or_create(tree->son, name);
	}
	
	// itterate through the sons
	while(strcmp(next_tree->name, next_step) != 0) {
		
		if (next_tree->next_brother == 0) {
			// There is no more sons to search through, we will create it
			next_tree->next_brother = new_node(next_step, 0, 0);
			next_tree->next_brother->previous_brother = next_tree;
			next_tree->next_brother->father = next_tree->father;
			// then let's make the next step
			free(next_step);
			return go_to_node_or_create(next_tree->next_brother, name);
		}	
		next_tree = next_tree->next_brother;
	}
	// Here next_tree->name == next_name, we progressed of one step in the tree
	// Let's make the next step :
	free(next_step);
	return go_to_node_or_create(next_tree, name);
	
}

int add_to_tree(t_tree tree, char* name, void* value, t_types type, t_rights right ) {
	t_tree node;
	int res;
	
	node = go_to_node_or_create(tree, name);
	
	res = modify_node_value(node, value, type, 1);
	
	node->right = right;
	
	return res;
}


int modify_node_value(t_tree node, void* new_value, t_types new_type, int admin) {
	if (node == 0) {
		pconf_fatal_error("in modify_node_value : Trying to modify null pointer");
	}
	if (new_value != 0) {
		node->has_value = 1;
		switch(new_type) {
			case string_type :
				node->value.string_value = string_copy((char*)new_value);
				break;
			case enum_string_type :
				node->value.enum_string_value = enum_string_copy((t_enum_string)new_value);
		}
		node->type = new_type;
	} else {
		node->has_value = 0;	
	}
	return 1;
}


t_tree tree_search(t_tree tree, char* name, int admin) {
	t_tree node;
	
	node = go_to_node(tree, name);
	
	if (node == 0) {
		return 0;
	}
	
	if (!admin && node->right >= admin_rw) {
		return 0;
	}
	
	return node;
}


void free_tree(t_tree tree) {
	t_tree son1, son2;
	
	if (tree == 0) {
		return;
	}
	
	// Kill all the sons
	son1 = tree->son;
	while (son1 != 0) {
		son2 = son1->next_brother;
		free_tree(son1);
		son1 = son2;
	}
	
	// Then commit suicide
	free(tree->name);
	if (tree->has_value) {
		switch (tree->type) {
			case string_type :
				free(tree->value.string_value);
				break;
			case enum_string_type :	
				free_enum_string(tree->value.enum_string_value);
				break;
		}
	}
	free(tree);
}

int del_from_tree(t_tree tree, char* name) {
	t_tree tree2;
	
	tree2 = go_to_node(tree, name);
	if (tree2 == 0) {
		return 0;
	}
	
	if (tree2->father->son == tree2) {
		tree2->father->son = tree2->next_brother;
	} else {
		tree2->previous_brother->next_brother = tree2->next_brother;
		if (tree2->next_brother != 0) {
			tree2->next_brother->previous_brother = tree2->previous_brother;
		}
	}
	
	free_tree(tree2);
	return 1;
}

void add_tree_to_tree(t_tree tree1, t_tree tree2) {
	void* value;
	t_tree son;
	
	if (tree1 == 0) return;
	if (tree1->has_value) {
		switch (tree1->type) {
			case string_type :
				value = string_copy(tree1->value.string_value);
				add_to_tree(tree2, tree1->name, value, tree1->type, tree1->right);
				free(value);
				break;
			case enum_string_type :
				value = enum_string_copy(tree1->value.enum_string_value);
				add_to_tree(tree2, tree1->name, value, tree1->type, tree1->right);
				free_enum_string(value);
				break;
		}
	}
	son = tree1->son;
	while (son != 0) {
		add_tree_to_tree(son, tree2);
		son = son->next_brother;
	}
	
}

// FOR DEBUG USE ONLY
void print_tree(t_tree tree){
	t_tree son;
	t_string rights, s;
	
	if (tree == 0) return;
	
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
				printf("%s = \"%s\" %s\n", tree->name, tree->value.string_value, rights);
				break;
			case enum_string_type :
				s = enum_string_to_string(tree->value.enum_string_value);
				strlen(s);
				printf("%s = { %s } %s\n", tree->name, s, rights);
				free(s);
		}
		free(rights);
	} else {
		printf("%s\n", tree->name);
	}
	son = tree->son;
	while (son != 0) {
		print_tree(son);
		son = son->next_brother;
	}
}

