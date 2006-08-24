/*! 
 * @file nutparser.h
 * @brief The new configuration parser (and possibly network protocol)
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
 

#ifndef NUTPARSER_H_
#define NUTPARSER_H_

#include <stdio.h>
#include "tree.h"

/* Define the max size of words */
#define BUFFER_SIZE 200
/* Define the max depth of imbrication of the configuration file */
#define STACK_SIZE  50

/**
 * Print an error message and then exit
 * 
 * @param errtxt The string to display in the error message
 */
void pconf_fatal_error(char* errtxt);

/**
 * Print an error message without exiting
 * 
 * @param errtxt The string to display in the error message
 */
void pconf_error(char* errtxt);

/**
 * Main function of parse_conf. Parse a configuration file and return the corresponding tree
 * 
 * @param	filename	The name of the configuration file to open. Put 0 to open the default file
 * @param	errhandler	To specify a fonction to handle errors. Put 0 to use fprintf on stderr
 * 
 * @return A tree that contains the parsed configuration
 */
t_tree parse_conf(t_string filename, void errhandler(const char*));

#endif /*NUTPARSER_H_*/
