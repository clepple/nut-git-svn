/*  nutparser.c - The new configuration parser (and possibly network protocol)

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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "config.h"
#include "data_types.h"
#include "nutparser.h"
#include "tree.h"
#include "stack.h"

// Define the max size of words
#define BUFFER_SIZE 200
// Define the max depth of imbrication of the configuration file
#define STACK_SIZE  50

/**
 * Structure of a context 
 * Used to parse the configurations files
 */
typedef struct {
	t_tree tree;
	char* full_path;
	FILE* conf_file;
	void (*error_handler)(const char*);
	t_string buffer;
	int line_number;
	t_string filename;
	t_rights rights;
} t_context;

// Definition of the type lex_types
// List of all type of lexeme in the grammar
typedef enum {
	OPEN_BRACE,
	CLOSE_BRACE,
	OPEN_BRACKET,
	CLOSE_BRACKET,
	DOT,
	EOL,
	INCLUDE,
	EQUAL,
	QUOTE,
	EoF,	// Not EOF because it is already defined.
	WORD,
	COMM,
	COMMA,
	INVALID
} lex_types;

// Global variables
t_context *parser_ctx;
t_stack context_stack;

// preDeclaration of some function for crossed reference problemes.
void parse_include();
void parse_word ();

// Convert a lexeme to a string, for explicit error messages
t_string lex_to_string(lex_types lex) {
	switch (lex) {
		case OPEN_BRACE : return "open brace";
		case CLOSE_BRACE : return "close brace";
		case OPEN_BRACKET : return "open bracket";
		case CLOSE_BRACKET : return "close bracket";
		case DOT : return "dot";
		case EOL : return "end of line";
		case INCLUDE : return "\"include\" reserved word";
		case EQUAL : return "equal";
		case QUOTE : return "quote";
		case EoF : return "end of file";
		case WORD : return "word";
		case COMM : return "\"#\"";
		case COMMA : return "comma";
		default : return "invalid character";
	}
}

// Copy a context. Some value, as filename are not
// copied but passed by pointer.
t_context* context_copy(t_context* ctx) {
	t_context* new_ctx = (t_context*)xmalloc(sizeof(t_context));
	new_ctx->buffer = (t_string)xmalloc(sizeof(char)* BUFFER_SIZE );
	new_ctx->conf_file = ctx->conf_file;
	new_ctx->error_handler = ctx->error_handler;
	new_ctx->filename = ctx->filename;
	new_ctx->full_path = string_copy(ctx->full_path);
	new_ctx->line_number = ctx->line_number;
	new_ctx->rights = ctx->rights;
	new_ctx->tree = ctx->tree;
	return new_ctx;
}

// Free a context structure.
// The value (as filename) are not freed because they are not
// copied by context_copy bu passed by pointer
// Don't forget to free thoses value when needed (and only when needed)
void free_context(t_context* ctx) {
	free(ctx->buffer);
	free(ctx->full_path);
	free(ctx);
}

// Make an error message and exit
void pconf_fatal_error(char* errtxt) {
	if (parser_ctx == 0 || parser_ctx->error_handler == 0)
     	fprintf(stderr, "nutparser : fatal error : %s\n", errtxt);
	else
		parser_ctx->error_handler(errtxt);
		
		exit(EXIT_FAILURE);
}

// Make an error message without exiting
void pconf_error(char* errtxt) {
	if (parser_ctx == 0 || parser_ctx->error_handler == 0)
     	fprintf(stderr, "nutparser : error : %s\n", errtxt);
	else
		parser_ctx->error_handler(errtxt);
}

// Make a syntax error message
// Syntax error are considered fatal, so it use pconf_fatal_error
// (and thus exit)
void pconf_syntax_error(lex_types lex) {
	t_string s = (t_string)xmalloc(sizeof(char)* (100 + strlen(parser_ctx->filename) + strlen(parser_ctx->filename)));
	if (lex == WORD) {
		sprintf(s, "syntax error : In %s, line %d : Unexpected word \"%s\" found. Stoping parsing", 
				parser_ctx->filename, 
				parser_ctx->line_number, 
				parser_ctx->buffer);
	} else {
		sprintf(s, "syntax error : In %s, line %d : Unexpected %s found. Stoping parsing", 
				parser_ctx->filename, 
				parser_ctx->line_number, 
				lex_to_string(lex));
	}
	pconf_fatal_error(s);
}

// Read a character is the stream
// As configuration file must end with an empty line and as
// get_lex don't use this function, getting an EOF here is an error
char get_char(FILE * file) {
	char c;
	c = fgetc(file);
	if (c == EOF) {
		pconf_syntax_error(EoF);
	}
	return c;
}

// Remove char from the stream until it reach end of line
void eat_comm() {
	char c = get_char(parser_ctx->conf_file);
	while (c != '\n') {
		c = get_char(parser_ctx->conf_file);
	}
}

int is_a_letter(char c) {
	return ((c >= 'A') && (c<= 'Z')) || ((c >= 'a') && (c<= 'z'));
}

int is_a_number(char c) {
	return (c >= '0') && (c <= '9');
}

int is_valid_char_for_word(char c) {
	return is_a_letter(c) || is_a_number(c) || c == '@' || c == '_' || c == '-';
}

int is_valid_char_for_rights(char c) {
	return c == 's' || c == '*' ;
}

// The base function.
// Return the next lexeme in the stream.
// you pass it a fonction as parameter depending of the notion
// of word you are in. For instance, node name words can contain letters, numbers
// '@', '_' or '-', while rights words can only contain 's' or '*'
//
// ffe : make a function is_valid_char_for_numerical ( numbers and '.' for instance)
// to be able to parse numerical value
lex_types get_lex(int (*is_valid_char)(char)) {
	int end = 0, i = 0;
	int c;
	
	c = fgetc(parser_ctx->conf_file);
	
	// Pass the spaces and tabulations
	while((c == ' ') || (c == '\t')) {
		c = fgetc(parser_ctx->conf_file);
	}
	
	// Case of 1 caracter lexeme
	switch (c) {
		case '{'  : return OPEN_BRACE;
		case '}'  : return CLOSE_BRACE;
		case '('  : return OPEN_BRACKET;
		case ')'  : return CLOSE_BRACKET;
		case '.'  : return DOT;
		case '\n' : return EOL;
		case '#'  : eat_comm(); return EOL;
		case '='  : return EQUAL;
		case '\"' : return QUOTE;
		case ','  : return COMMA;
		case EOF  : return EoF;
	}
	
	if (!is_valid_char(c)) {
		return INVALID;
	}
	
	// Others cases : INCLUDE or name
	while (!end) {
		parser_ctx->buffer[i] = c;
		c = get_char(parser_ctx->conf_file);
		i++;
		
		if ( !is_valid_char(c)) {
			// It is not anymore a letter. End the current word and put the 
			// caracter back in the stream
			parser_ctx->buffer[i] = 0;
			end = 1;
			ungetc(c, parser_ctx->conf_file);
		}		
	}
	
	
	if ((i == 7) && (strncmp(parser_ctx->buffer, "include", i)) == 0) {
		return INCLUDE;
	}
	
	return WORD;
}

// Increase the fuul_path of the context by a level.
// For instance if it was nut.ups and increase_context("my_ups") is called
// full_path will become nut.ups.my_ups
void increase_context(t_string s) {
	// Allocate enough memory
	parser_ctx->full_path = xrealloc(parser_ctx->full_path, strlen(parser_ctx->full_path) + strlen(s) + 2);
	// Concatenate
	sprintf(parser_ctx->full_path + strlen(parser_ctx->full_path), ".%s", s);
}

// Open a file
void open_file(t_string filename) {
	t_string complete_filename, s;
	
	// If the filename is not an absolute name, considers the path
	// is the default configuration path
	if (filename[0] != '/') {
		complete_filename = (t_string)xmalloc(sizeof(char)*(strlen(CONFPATH) + strlen(filename) + 2));
		sprintf(complete_filename, "%s/%s", CONFPATH, filename);
	} else {
		complete_filename = string_copy(filename);
	}
	
	// Open the file
	parser_ctx->conf_file = fopen(complete_filename, "r");
	
	if (parser_ctx->conf_file == 0) {
		// If there were an error, signal it
		s = (t_string)xmalloc(sizeof(char)*(strlen(complete_filename) + 100));
		sprintf(s,"Unable to open the configuration file (\"%s\").", complete_filename);
		free(complete_filename);
		free(s);
		pconf_error(s);
	}
	
	// Update the filename in the context structure
	parser_ctx->filename = string_copy(complete_filename);
	free(complete_filename);
}

// Parse a string value
t_string parse_string_value() {
	t_string s;
	char c;
	int string_max_size;
	int i = 0;
	
	// Lets begin with buffer_size
	s = (t_string)xmalloc(sizeof(char)*BUFFER_SIZE);
	string_max_size = BUFFER_SIZE;
	c = get_char(parser_ctx->conf_file);
	while (1) {
		
		// If not enough space, we double it
		if (i >= string_max_size) {
			s = xrealloc(s, string_max_size*2);
			string_max_size *= 2;
		}
		
		/* another " means we're done with this word */
		if (c == '"') {
			s[i] = 0;
			break;
		}
		
		// A '\\' mean we accept the next even if it is a '"'
		if (c == '\\') {
			c = get_char(parser_ctx->conf_file);
			if (c == '\n') {
				c = get_char(parser_ctx->conf_file);
			}
		}
		s[i] = c;
		c = get_char(parser_ctx->conf_file);
		i++;
	}
	return s;
}

// Parse an enumeartion of string, create the structure en return it
t_enum_string parse_enum_string_value() {
	t_enum_string enum_string = 0;
	t_string s;
	lex_types lex = get_lex(is_valid_char_for_word);
	
	// An enumaration ends with CLOSE_BRACE
	while (lex != CLOSE_BRACE) {
	
		// Can be on more than one line
		if (lex == EOL) {
			lex = get_lex(is_valid_char_for_word);
			parser_ctx->line_number++;
			continue;
		}
	
		// If not an EOL, have to be a quote indicating the begining of a string
		if (lex != QUOTE) {
			pconf_syntax_error(lex);
		}
	
		// Get the string
		s = parse_string_value();
		
		// Add it to the enum_string structure
		enum_string = add_to_enum_string(enum_string, s);
		
		// To avoid memory leaks ;-)
		free(s);
		
		lex = get_lex(is_valid_char_for_word);
	}
	// Finally return the enum_string structure
	return enum_string;
}

// Parse the rights of an affectation
void parse_rights() {
	// we get a lex with the is_valid_char_for_rights function
	lex_types lex = get_lex(is_valid_char_for_rights);
	
	// Valid rights are : "", "s", "*" and "s*"
	// In the "" case, OPEN_BRACKET or EOL can be found
	// it is not the role of this function to treat them
	// so it put them back in the stream
	switch (lex) {
		case OPEN_BRACKET : 
			ungetc('(', parser_ctx->conf_file);
			parser_ctx->rights = all_r;
			break;
		case WORD :
			if (strcmp(parser_ctx->buffer, "s") == 0) {
				parser_ctx->rights = all_rw;
				break;
			}
			if (strcmp(parser_ctx->buffer, "*") == 0) {
				parser_ctx->rights = admin_r;
				break;
			}
			if (strcmp(parser_ctx->buffer, "s*") == 0) {
				parser_ctx->rights = admin_rw;
				break;
			}
			pconf_syntax_error(lex);
		case EOL :
			parser_ctx->rights = all_r;
			ungetc('\n', parser_ctx->conf_file);
			break;
		default :
			pconf_syntax_error(lex);
	}
}

// Parse an affectation
// For the moment, only string and enum of string are accepted as values
void parse_equal() {
	t_enum_string enum_string;
	t_string s;
	lex_types lex;
	lex = get_lex(is_valid_char_for_word);
	
	switch (lex) {
		// An affection accept as value :
		// - a string (begin by a QUOTE)
		// - an enum (of string only for the moment) (begin by an OPEN_BRACE)
		case QUOTE : 
			// Get the string
			s = parse_string_value();
			// Get the rights to give to the node
			parse_rights();
			// Add a node to the tree
			add_to_tree(parser_ctx->tree, 
						parser_ctx->full_path, 
						s,
						string_type,
						parser_ctx->rights);
			free(s);
			break;
		case OPEN_BRACE :
			// Get enumeration (of string ony for the moment)
			// ffe, call a parse_enum function that will parse both 
			// string enumeration and numerical value enumeration.
			// This function will have to "return" the value and the type
			enum_string = parse_enum_string_value();
			// Get the rights to give to the node
			parse_rights();
			// Add a node to the tree
			add_to_tree(parser_ctx->tree, 
						parser_ctx->full_path, 
						enum_string,
						enum_string_type,
						parser_ctx->rights);
			break;
		default :
			pconf_syntax_error(lex);
	}
	
	
}

// Parse a bloc opened by an open bracket
void parse_new_bloc() {
	int line_number;
	
	// An OPEN_BRACKET must be followed by an end of line (EOL)
	lex_types lex = get_lex(is_valid_char_for_word);
	if (lex != EOL) {
		pconf_syntax_error(lex);
	}
	parser_ctx->line_number++;
	
	// The current context become the base context until an close bracket
	// is found. So it is pushed into the stack
	if (!stack_is_full(context_stack)) {
		stack_push((void*)context_copy(parser_ctx),context_stack);
	} else {
		pconf_fatal_error("Too many level of recursion");
	}
	
	// A bloc ends with a CLOSE_BRACKET
	lex = get_lex(is_valid_char_for_word);
	while(lex != CLOSE_BRACKET) {
		// In a bloc one can do :
		// - Declaration (begins with a WORD)
		// - Inclusion (begins with "include")
		// - Pass lines (EOL at the begining of line)
		switch (lex) {
			case WORD :
				increase_context(parser_ctx->buffer);
				parse_word();
				break;
			case INCLUDE :
				parse_include();
				break;
			case EOL :
				// End of line. restore the last base context
				// and increase the line number.
				line_number = parser_ctx->line_number;
				free_context(parser_ctx);
				parser_ctx = context_copy((t_context*)stack_front(context_stack));
				parser_ctx->line_number = line_number + 1;
				break;
			default :
				pconf_syntax_error(lex);
		}
		lex = get_lex(is_valid_char_for_word);
	}
	lex = get_lex(is_valid_char_for_word);
	if (lex != EOL) {
		pconf_syntax_error(lex);
	}
	line_number = parser_ctx->line_number;
	free_context(parser_ctx);
	parser_ctx = stack_pop(context_stack);
	free_context(parser_ctx);
	parser_ctx = context_copy((t_context*)stack_front(context_stack));
	parser_ctx->line_number = line_number + 1;
}

// Parse a word
void parse_word () {
	lex_types lex;
	lex = get_lex(is_valid_char_for_word);
	
	
	// A word can be followed by
	// - a dot
	// - an equal sign
	// - an open bracket
	switch (lex) {
		case DOT :
			lex = get_lex(is_valid_char_for_word);
			if (lex != WORD) {
				pconf_syntax_error(lex);
			}
			increase_context(parser_ctx->buffer);
			parse_word();
			break;
		case EQUAL :
			// parse the equal and the value after it
			parse_equal();
			lex = get_lex(is_valid_char_for_word);
			if (lex != EOL && lex != OPEN_BRACKET) {
				pconf_syntax_error(lex);
			}
			// The next lex has to be a EOL or an OPEN_BRACKET
			if (lex == EOL) {
				ungetc('\n', parser_ctx->conf_file);
				break;
			}
			if (lex != OPEN_BRACKET) {
				pconf_syntax_error(lex);
			}
		case OPEN_BRACKET :
			parse_new_bloc();
			break;
		default :
			pconf_syntax_error(lex);
	}
}


// Manage the parsing of include files.
void parse_include () {
	t_string s;
	lex_types lex = get_lex(is_valid_char_for_word);
	int line_number;
	
	// filename must be quoted
	switch (lex) {
		case QUOTE :
			s = parse_string_value() ;
			break;
		default :
			pconf_syntax_error(lex);
			// We never arrive here, but to don't get a warning about s that
			// not be initialized :
			return;
	}
	
	lex = get_lex(is_valid_char_for_word);
	
	// and followed by an end of line
	if (lex != EOL) {
		pconf_syntax_error(lex);
	}
	
	// Save the current line number
	((t_context*)stack_front(context_stack))->line_number = parser_ctx->line_number + 1;
	
	// Try to open the file
	open_file(s);
	
	free(s);
	
	if (parser_ctx->conf_file == 0) {
		//
		// Unable to open the file
		//
		
		// Error message
		s = (t_string)xmalloc(sizeof(char)*(strlen(((t_context*)(stack_front(context_stack)))->filename) + 200));
		sprintf(s,"In %s, line %d : Unable to execute include directive. Ignoring it.\n", ((t_context*)(stack_front(context_stack)))->filename, parser_ctx->line_number);
		pconf_error(s);
		free(s);
		free_context(parser_ctx);
		
		// Restore the context and ignore the include directive
		parser_ctx = context_copy((t_context*)stack_front(context_stack));
		return;
	}

	// Updating new values.	
	parser_ctx->line_number = 1;

	// Save the new base context
	if (!stack_is_full(context_stack)) {
		stack_push((void*)context_copy(parser_ctx),context_stack);
	} else {
		pconf_fatal_error("Too many level of recursion");
	}
	
	lex = get_lex(is_valid_char_for_word);

	while(lex != EoF) {
		switch (lex) {
			// In the first level of an include file, one can do :
			// - Declaration (begins with a WORD)
			// - Inclusion (begins with "include")
			// - Pass lines (EOL at the begining of line)
			case WORD :
				increase_context(parser_ctx->buffer);
				parse_word();
				break;
			case INCLUDE :
				parse_include();
				break;
			case EOL :
				// End of line. restore the last base context
				// and increase the line number,
				line_number = parser_ctx->line_number;
				free_context(parser_ctx);
				parser_ctx = context_copy((t_context*)stack_front(context_stack));
				parser_ctx->line_number = line_number + 1;
				break;
			default :
				pconf_syntax_error(lex);
		}
		lex = get_lex(is_valid_char_for_word);
	}
	
	// We finished the include file. 
	fclose(parser_ctx->conf_file);
	
	//
	// Restore the context
	//
	
	// free the current context
	free_context(parser_ctx);
	
	// pop and free the base context of the include file
	parser_ctx = stack_pop(context_stack);
	free(parser_ctx->filename);
	free_context(parser_ctx);
	
	// get back the base context of the previous file
	 parser_ctx = context_copy((t_context*)stack_front(context_stack));
	
}

// Launch the parsing of the main file. Return when the parsing is finished
void begin_parse() {
	lex_types lex;
	lex = get_lex(is_valid_char_for_word);
	int line_number;
	
	
	while(lex != EoF) {
	
		switch (lex) {
			// In the first level of the main file, one can do :
			// - Declaration (begins with a WORD)
			// - Inclusion (begins with "include")
			// - Pass lines (EOL at the begining of line)
			case WORD :
				increase_context(parser_ctx->buffer);
				parse_word();
				break;
			case INCLUDE :
				parse_include();
				break;
			case EOL :
				// End of line. restore the last base context
				// and increase the line number,
				line_number = parser_ctx->line_number;
				free_context(parser_ctx);
				parser_ctx = context_copy((t_context*)stack_front(context_stack));
				parser_ctx->line_number = line_number + 1;
				break;
			default :
				pconf_syntax_error(lex);
		}
		lex = get_lex(is_valid_char_for_word);
	}
}


// Entry of the parser
t_tree parse_conf(t_string filename, void errhandler(const char*)) {
	t_tree conf_tree;
	t_string s;
	
	//
	// Initializations
	//
	
	// Allocation of the base context structure.
	parser_ctx = (t_context*)(xmalloc(sizeof(t_context)));
	
	parser_ctx->error_handler = errhandler;
	
	// Opening configuration file
	if (filename != 0) {
		open_file(filename);
	} else { 
		// If no filename provided, open default configuration file
		s = (t_string)xmalloc(sizeof(char)*(strlen(CONFPATH)+ 10));
		sprintf(s,"%s/nut.conf", CONFPATH);
		open_file(s);
		free(s);
	}
	
	// Was the opening a succes ?
	if (parser_ctx->conf_file == 0) {
		pconf_fatal_error("No configuration file opened. Aborting");
	}
	
	conf_tree = new_node("nut",0,0);
	parser_ctx->full_path = string_copy(conf_tree->name);
	parser_ctx->tree = conf_tree;
	parser_ctx->buffer = (t_string)xmalloc(sizeof(char)* BUFFER_SIZE );
	parser_ctx->line_number = 1;
	
	// Save the base context
	context_stack = new_stack(STACK_SIZE);
	if (!stack_is_full(context_stack)) {
		stack_push((void*)context_copy(parser_ctx),context_stack);
	} else {
		pconf_fatal_error("Too many level of recursion");
	}
	
	//
	// Launch the parsing and the construction of the tree
	//
	begin_parse();
	
	//
	// Free memory
	//
	
	fclose(parser_ctx->conf_file);
	free_context(parser_ctx);
	parser_ctx = stack_pop(context_stack);
	free(parser_ctx->filename);
	free_context(parser_ctx);
	free_stack(context_stack);
	
	//
	// Return the tree
	//
	
	return conf_tree;
	
}
