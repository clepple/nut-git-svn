#ifndef PARSECONF_H_
#define PARSECONF_H_

#include <stdio.h>
#include "tree.h"

/**
 * Print an error message and then exit
 * 
 * @param errtxt The string to display in the error message
 * 
 */
void pconf_fatal_error(char* errtxt);

/**
 * Print an error message without exiting
 * 
 * @param errtxt The string to display in the error message
 * 
 */
void pconf_error(char* errtxt);

/**
 * Main function of parse_conf. Parse a configuration file and return the corresponding tree
 * 
 * @param	filename	The name of the configuration file to open. Put 0 to open the default file
 * @param	errhandler	To specify a fonction to handle errors. Put 0 to use fprintf on stderr
 * 
 */
t_tree parse_conf(t_string filename, void errhandler(const char*));

char* pconf_encode(const char* src, char* dest, size_t destsize);

#endif /*PARSECONF_H_*/
