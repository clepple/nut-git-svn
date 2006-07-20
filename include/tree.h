#ifndef TREE_H_
#define TREE_H_

/*
 * 		Implementation of a tree of data with access rights
 * 		(More details follow)
 * 
 *  Copyright (C) 2006  Jonathan Dion <dion.jonathan@gmail.com>
 *
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * 
*/

/*
 *  Here is a sample of tree that can be managed with this program
 * 
 * nut
 * 	|->ups
 * 	|	|->myups
 * 	|		|->driver
 * 	|		|	|->name = newhidups
 * 	|		|	|->port = auto
 * 	|		|->battery
 * 	|			|->charge = 98
 * 	|				|->low = 30 s*
 * 	|->users
 * 		|->myadmin
 * 		|	|->type = admin
 * 		|	|->passwd = mypass s*
 * 		|->monuser
 * 			|->type = upsmon_master
 * 			|->passwd = titi s*
 * 
 * 
 * Notes : - the s after a value means this value can be modified.
 *         - the * after a value means this value is protected and need
 *           "admin" rights to access to 
 * 		   - Path in the tree are in this format : nut.ups.myups.battery.charge
 *         - Each node's name is the complete path to this node
 * 
 * 	Types defined
 * 
 * t_tree			=> implement a node of a tree or a tree
 * t_types			=> define the types of data that can be contained in the tree
 * 						Only chain and enumeration of chain are implemented for
 * 						the moment
 * t_enum_chain		=> One of those types : enumeration of chain
 * 
 *  List of functions :
 * 
 * t_tree new_node(char* name, void* value, t_types type );
 * int add_to_tree(t_tree tree, char* name, void* value, t_types type, t_rights right);
 * t_tree tree_search(t_tree tree, char* name, int admin);
 * int modify_node_value(t_tree node, void* new_value, t_types new_type, int admin);
 * int del_from_tree(t_tree tree, char* name);
 * void add_tree_to_tree(t_tree tree1, t_tree tree2);
 */

#include "data_types.h"

typedef enum {
	all_rw,
	all_r,
	admin_rw,
	admin_r
} t_rights;

/**
 * Structure that represente a tree. 
 * 
 * @note In name is the full path to the current node
 * @note Each node only have a pointer to his first son. To enumerate the sons
 * 		 use the next_brother pointer of the first son.
 * @note The union variable value enable to add others types in future modifications
 */
typedef struct _t_tree {
	char* name;
	int has_value;
	union {
		char* string_value;
		t_enum_string enum_string_value;
	} value;
	t_types type;
	t_rights right;
	struct _t_tree * son;
	struct _t_tree * father;
	struct _t_tree * next_brother;
	struct _t_tree * previous_brother;
} *t_tree;


/**
 * Make a tree node
 * 
 * @param name	Name to give to the node
 * @param value	Pointer to the value to give to the node. Null pointer if none
 * @param type	Type of the value. If value = 0, it don't matter.
 * @return 		The new node.
 * 
 * @note 		All the pointer of the tree structure are initialized to Null
 * @note		protected is initialized to false
 */
t_tree new_node(char* name, void* value, t_types type );

/**
 * add a normal (not protected) value to a tree
 * 
 * @param tree  The tree to add the value to
 * @param name  The name of the variable to add. Must represente the full 
 *                 path to the variable from the first node of tree
 * @param value The value to give at the variable. If not value, give a null pointer
 * @param type  The type of the value
 * @return 		1 if no problem, else 0
 * 
 * @note		The the variable already exist, it will be overwritten if not 
 * @note		a protected variable. If it is a protected varible, it will
 * @note		remains unchanged.
 */
int add_to_tree(t_tree tree, char* name, void* value, t_types type, t_rights right);


/**
 * Search on a tree for a variable
 * 
 * @param tree	The tree to search in
 * @param name	The full path to the variable to search
 * @param admin	A boolean. Do the search with admin rights ?
 * @return		The node of the tree that represente the variable. If not found or
 * 				if it was a protected variable, null pointer is returned
 */
t_tree tree_search(t_tree tree, char* name, int admin);

/**
 * Modify the value of the node of a tree
 * 
 * @param node		The variable to modify the value of
 * @param new_value	The new value
 * @param new_type	The new type
 * @return			0 if problem, else 1
 */
int modify_node_value(t_tree node, void* new_value, t_types new_type, int admin);

/**
 * Delete a node from a tree
 * 
 * @param tree	The tree to delete the node from
 * @param name	The full path to the node to delete
 * @return		1 if deleted without errot, else 0			
 */
int del_from_tree(t_tree tree, char* name);

/**
 * Add all the variable of a tree to another tree
 * 
 * @param tree1	The tree to add to the second tree
 * @param tree2	The tree to add the first tree to
 * @return		1 if no error, else 0
 * 
 * @note		The variables and value of tree1 will overwrite variables and
 * 				values of tree2
 */
void add_tree_to_tree(t_tree tree1, t_tree tree2);


void free_tree(t_tree tree);


// FOR DEBUG USE ONLY
void print_tree(t_tree tree);


#endif /*TREE_H_*/
