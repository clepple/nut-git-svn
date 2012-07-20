/* nutconf.c - configuration API

   Copyright (C)
	2012	Emilien Kia <emilienkia-guest@alioth.debian.org>

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

#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include "nutconf.h"

#define BOOL unsigned int
#define TRUE  ((BOOL)1)
#define FALSE ((BOOL)0)





/* Parse a string source for a CHARS and return its size if found or 0, if not.
 * CHARS     ::= CHAR+
 * CHAR      ::= __ASCIICHAR__ - ( __SPACES__ | '\\' | '\"' | '#' )
 *             | '\\' ( __SPACES__ | '\\' | '\"' | '#' )
 * TODO: accept "\t", "\s", "\r", "\n" ??
 */
int nutconf_parse_rule_CHARS(const char* src, unsigned int len)
{
	int l = 0;
	BOOL escaped = FALSE;

	if (src == NULL)
		return 0;	

	while (*src != 0 && len > 0)
	{
		char c = *src;
		if (escaped)
		{
			if (isspace(c) || c == '\\' || c == '"' || c == '#')
			{
				src++;
				len--;
				l += 2;
			}
			else
			{
				/* WTF ??? */
			}
			escaped = FALSE;
		}
		else
		{
			if (c == '\\')
			{
				escaped = TRUE;
				src++;
				len--;
			}
			else if (isgraph(c) && c != '\\' && c != '"' && c != '#')
			{
				src++;
				len--;
				l++;
			}
			else
			{
				break;
			}
		}
	}

	return l;
}

/* Parse a string source for a STRCHARS and return its size if found or 0, if not.
 * STRCHARS  ::= STRCHAR+
 * STRCHAR   ::= __ASCIICHAR__ - ( '\\' | '\"')
 *             | '\\' ( '\\' | '\"' )
 * TODO: validate size of l
 * TODO: accept "\t", "\s", "\r", "\n" ??
 */
int nutconf_parse_rule_STRCHARS(const char* src, unsigned int len)
{
	int l = 0;
	BOOL escaped = FALSE;
	
	if (src == NULL)
		return 0;	

	while (*src != 0 && len > 0)
	{
		char c = *src;
		if (escaped)
		{
			if (isspace(c) || c == '\\' || c == '"')
			{
				src++;
				len--;
				l += 2;
			}
			else
			{
				/* WTF ??? */
			}
			escaped = FALSE;
		}
		else
		{
			if (c == '\\')
			{
				escaped = TRUE;
				src++;
				len--;
			}
			else if (isprint(c) && c != '\\' && c != '"')
			{
				src++;
				len--;
				l++;
			}
			else
			{
				break;
			}
		}
	}

	return l;
}

/** Parse a string source for getting the next token, ignoring spaces.
 * \param src Begin of text source to parse.
 * \param len Size of text source to parse in byte.
 * \param[out] token Structure where store parsed token.
 * \return Token type.
 */
PARSING_TOKEN_e nutconf_parse_token(const char* src, unsigned int len,
	LEXTOKEN_t* token)
{
	char c;
	LEXPARSING_STATE_e state = LEXPARSING_STATE_DEFAULT;
	BOOL escaped = FALSE;

    LEXTOKEN_set(token, NULL, NULL, TOKEN_NONE);
	if (src == NULL)
		return TOKEN_NONE;

	while (	*src != 0 && len > 0)
	{
		c = *src;
		switch (state)
		{
			case LEXPARSING_STATE_DEFAULT: /* Wait for a non-space char */
			{
				if (c == ' ' || c == '\t')
				{
					/* Space : do nothing */
				}
				else if (c == '[')
				{
                    LEXTOKEN_set(token, src, src + 1, TOKEN_BRACKET_OPEN);
					return TOKEN_BRACKET_OPEN;
				}
				else if (c == ']')
				{
                    LEXTOKEN_set(token, src, src + 1, TOKEN_BRACKET_CLOSE);
					return TOKEN_BRACKET_CLOSE;
				}
				else if (c == ':')
				{
                    LEXTOKEN_set(token, src, src + 1, TOKEN_COLON);
					return TOKEN_COLON;
				}
				else if (c == '=')
				{
                    LEXTOKEN_set(token, src, src + 1, TOKEN_EQUAL);
					return TOKEN_EQUAL;
				}
				else if (c == '\r' || c == '\n')
				{
                    LEXTOKEN_set(token, src, src + 1, TOKEN_EOL);
					return TOKEN_EOL;
				}
				else if (c == '#')
				{
                    token->begin = src + 1;
					state = LEXPARSING_STATE_COMMENT;
				}
				else if (c == '"')
				{
                    token->begin = src + 1;
					state = LEXPARSING_STATE_QUOTED_STRING;
				}
				else if (c == '\\')
				{
                    token->begin = src;
					/* Begin of STRING with escape */
					state = LEXPARSING_STATE_STRING;
					escaped = TRUE;
				}
				else if (isgraph(c))
				{
                    token->begin = src;
					state = LEXPARSING_STATE_STRING;
				}
				else
				{
                    LEXTOKEN_set(token, src, src + 1, TOKEN_UNKNOWN);
					return TOKEN_UNKNOWN;
				}
				break;
			}
			case LEXPARSING_STATE_QUOTED_STRING:
			{
				if (c == '"')
				{
					if (escaped == TRUE)
					{
						escaped = FALSE;
					}
					else
					{
						token->end = src;
                        token->type  = TOKEN_QUOTED_STRING;
						return TOKEN_QUOTED_STRING;
					}
				}
				else if (c == '\\')
				{
					if (escaped == TRUE)
					{
						escaped = FALSE;
					}
					else
					{
						escaped = TRUE;
					}
				}
				else if (c == ' ' || c == '\t')
				{
					/* Space : do nothing.*/
				}
				/*else if (c == '\r' || c == '\n') */
				/* TODO What about EOL in a quoted string ?? */
				else if (!isgraph(c))
				{
					token->end = src + 1;
                    token->type  = TOKEN_UNKNOWN;
					return TOKEN_UNKNOWN;
				}
				/* TODO What about other escaped character ? */
				break;
			}
			case LEXPARSING_STATE_STRING:
			{
				if (c == '"' || c == '#' || c == '[' || c == ']' || c == ':' || c == '=')
				{
					if (escaped == TRUE)
					{
						escaped = FALSE;
					}
					else
					{
						token->end = src;
                        token->type  = TOKEN_STRING;
						return TOKEN_STRING;
					}
				}
				else if (c == '\\')
				{
					if (escaped == TRUE)
					{
						escaped = FALSE;
					}
					else
					{
						escaped = TRUE;
					}
				}
				/* else if (c == '\r' || c == '\n') */
				else if (!isgraph(c))
				{
					token->end = src;
                    token->type  = TOKEN_STRING;
					return TOKEN_STRING;
				}
				/* TODO What about escaped character ? */
				break;
			}
			case LEXPARSING_STATE_COMMENT:
			{
				if (c == '\r' || c == '\n')
				{
					token->end = src;
                    token->type  = TOKEN_COMMENT;
					return TOKEN_COMMENT;
				}
				break;
			}
			default:
				/* Must not occur. */
				break;
		}
		src++;
		len--;
	}
	switch (state)
	{
		case LEXPARSING_STATE_DEFAULT:
			token->begin = token->end = src - 1;
            token->type  = TOKEN_NONE;
			return TOKEN_NONE;
		case LEXPARSING_STATE_QUOTED_STRING:
			token->end = src;
			/* TODO Should be permissive with not ended quoted string ? */
            token->type  = TOKEN_QUOTED_STRING;
			return TOKEN_QUOTED_STRING; 
		case LEXPARSING_STATE_STRING:
			token->end = src;
            token->type  = TOKEN_STRING;
			return TOKEN_STRING;
		case LEXPARSING_STATE_COMMENT:
			token->end = src;
            token->type  = TOKEN_COMMENT;
			return TOKEN_COMMENT;		
		default:
			/* Must not occur. */
			return TOKEN_NONE;
	}
}

/** Parse a string source for a line.
 * \param src Begin of text source to parse.
 * \param len Size of text source to parse in byte.
 * \param[out] rend Pointer where store end of line (address of byte following
 * the last character of the line, so the size of line is end-src).
 * \param[out] parsed_line Parsed line result.
 * TODO Handle end-of-file
 */
void nutconf_parse_line(const char* src, unsigned int len,
	const char** rend, SYNLINE_t* parsed_line)
{
	PARSING_TOKEN_e    directive_separator = TOKEN_NONE;
    SYNPARSING_STATE_e state = SYNPARSING_STATE_INIT;
    SYNPARSING_LINETYPE_e line_type = SYNPARSING_LINETYPE_UNKNWON;

	LEXTOKEN_t current;

/* Little macros (state-machine sub-functions) to reuse in this function: */

/* Add current token to arg list.*/
#define nutconf_parse_line__PUSH_ARG() \
{\
    if (parsed_line->arg_count < parsed_line->nb_args-1)\
    {\
        LEXTOKEN_copy(&parsed_line->args[parsed_line->arg_count], &current);\
        parsed_line->arg_count++;\
    }\
    /* TODO Add args overflow handling. */\
}
/* Set comment and end state machine: */
#define nutconf_parse_line__SET_COMMENT_AND_END_STM() \
{\
    LEXTOKEN_copy(&parsed_line->comment, &current);\
    state = SYNPARSING_STATE_FINISHED;\
}

	if (parsed_line == NULL)
	{
		return; /* TODO add error code. */
	}
	/* Init returned values */
	parsed_line->line_type = SYNPARSING_LINETYPE_UNKNWON;
	parsed_line->directive_separator = TOKEN_NONE;
	parsed_line->arg_count = 0;
	LEXTOKEN_set(&parsed_line->comment, NULL, NULL, TOKEN_NONE);

	/* Lets parse */
    while (TRUE)
    {
        nutconf_parse_token(src, len, &current);
        switch (state)
        {
        case SYNPARSING_STATE_INIT:
            switch (current.type)
            {
            case TOKEN_COMMENT: /* Line with only a comment. */
                line_type = SYNPARSING_LINETYPE_COMMENT;
                nutconf_parse_line__SET_COMMENT_AND_END_STM();
                break;
            case TOKEN_BRACKET_OPEN: /* Begin of a section. */
                state = SYNPARSING_STATE_SECTION_BEGIN;
                break;
            case TOKEN_STRING:  /* Begin of a directive line. */
            case TOKEN_QUOTED_STRING:
                nutconf_parse_line__PUSH_ARG();
                state = SYNPARSING_STATE_DIRECTIVE_BEGIN;
                break;
            default:
                /* Must not occur. */
                /* TODO WTF ? .*/
                break;
            }
            break;
        case SYNPARSING_STATE_SECTION_BEGIN:
            switch (current.type)
            {
            case TOKEN_BRACKET_CLOSE: /* Empty section. */
                state = SYNPARSING_STATE_SECTION_END;
                break;
            case TOKEN_STRING:  /* Section name. */
            case TOKEN_QUOTED_STRING:
                nutconf_parse_line__PUSH_ARG();
                state = SYNPARSING_STATE_SECTION_NAME;
                break;
            default:
                /* Must not occur. */
                /* TODO WTF ? .*/
                break;            
            }
            break;
        case SYNPARSING_STATE_SECTION_NAME:
            switch (current.type)
            {
            case TOKEN_BRACKET_CLOSE: /* End of named section. */
                state = SYNPARSING_STATE_SECTION_END;
                break;
            default:
                /* Must not occur. */
                /* TODO WTF ? .*/
                break;            
            }
            break;
        case SYNPARSING_STATE_SECTION_END:
            switch (current.type)
            {
            case TOKEN_COMMENT:
                line_type = SYNPARSING_LINETYPE_SECTION;
                nutconf_parse_line__SET_COMMENT_AND_END_STM();
                break;
            case TOKEN_EOL:
                line_type = SYNPARSING_LINETYPE_SECTION;
                state = SYNPARSING_STATE_FINISHED;
                break;
            default:
                /* Must not occur. */
                /* TODO WTF ? .*/
                break;            
            }
            break;
        case SYNPARSING_STATE_DIRECTIVE_BEGIN:
            switch (current.type)
            {
            case TOKEN_COLON:   /* Directive with ':'.*/
            case TOKEN_EQUAL:   /* Directive with '='.*/
                directive_separator = current.type;
                state = SYNPARSING_STATE_DIRECTIVE_ARGUMENT;
                break;
            case TOKEN_STRING:  /* Directive direct argument, no separator. */
            case TOKEN_QUOTED_STRING:
                nutconf_parse_line__PUSH_ARG();
                state = SYNPARSING_STATE_DIRECTIVE_ARGUMENT;
                break;
            case TOKEN_COMMENT:
                line_type = SYNPARSING_LINETYPE_DIRECTIVE_NOSEP;
                nutconf_parse_line__SET_COMMENT_AND_END_STM();
                break;
            case TOKEN_EOL:
                line_type = SYNPARSING_LINETYPE_DIRECTIVE_NOSEP;
                state = SYNPARSING_STATE_FINISHED;
                break;
            default:
                /* Must not occur. */
                /* TODO WTF ? .*/
                break;            
            }
            break;
        case SYNPARSING_STATE_DIRECTIVE_ARGUMENT:
            switch (current.type)
            {
            case TOKEN_STRING:  /* Directive argument. */
            case TOKEN_QUOTED_STRING:
                nutconf_parse_line__PUSH_ARG();
                /* Keep here, in SYNPARSING_STATE_DIRECTIVE_ARGUMENT state.*/
                break;
            case TOKEN_COMMENT:
                /* TODO signal directive with comment */
                nutconf_parse_line__SET_COMMENT_AND_END_STM();
                break;
            case TOKEN_EOL:
                line_type = SYNPARSING_LINETYPE_DIRECTIVE_NOSEP;
                state = SYNPARSING_STATE_FINISHED;
                break;
            default:
                /* Must not occur. */
                /* TODO WTF ? .*/
                break;
            }
            break;
        default:
            /* Must not occur. */
            /* TODO WTF ? .*/
            break;
        }

		src = *rend = current.end; 
        if (state == SYNPARSING_STATE_FINISHED)
            break; /* Go out infinite while loop. */
    }

    if (line_type == SYNPARSING_LINETYPE_DIRECTIVE_NOSEP)
    {
        if (directive_separator == TOKEN_COLON)
        {
            line_type = SYNPARSING_LINETYPE_DIRECTIVE_COLON;
        }
        else if (directive_separator == TOKEN_EQUAL)
        {
            line_type = SYNPARSING_LINETYPE_DIRECTIVE_EQUAL;
        }
    }

    /* End of process : save data for returning */
	parsed_line->line_type = line_type;
	parsed_line->directive_separator = directive_separator;

#undef nutconf_parse_line__PUSH_ARG
#undef nutconf_parse_line__SET_COMMENT_AND_END_STM
}


/* Parse a string source, memory mapping of a conf file.
 * End the parsing at the end of file (ie null-char, specified size
 * or error).
 */
void nutconf_parse_memory(const char* src, int len,
	nutconf_parse_line_callback cb, void* user_data)
{
	const char* rend = src;
	SYNLINE_t    parsed_line;
	LEXTOKEN_t   tokens[16];

	parsed_line.args = tokens;
	parsed_line.nb_args = 16;

	while (len > 0)
	{
		nutconf_parse_line(src, len, &rend, &parsed_line);

		cb(&parsed_line, user_data);

		len -= rend - src;
		src = rend;
	}
}




typedef struct {
	NUTCONF_t* conf;
	NUTCONF_SECTION_t* current_section;
	NUTCONF_ARG_t* current_arg;
}NUTCONF_CONF_PARSE_t;

static void nutconf_conf_parse_callback(SYNLINE_t* line, void* user_data)
{
	NUTCONF_CONF_PARSE_t* parse = (NUTCONF_CONF_PARSE_t*)user_data;
	int num;

	/* Verify parameters */
	if (parse==NULL)
	{
		return;
	}

	/* Parsing state treatment */
	switch (line->line_type)
	{
	case SYNPARSING_LINETYPE_SECTION:
		if (parse->current_section == NULL)
		{
			/* No current section - begin of the parsing.*/
			/* Use conf as section .*/
			parse->current_section = parse->conf;
		}
		else
		{
			/* Already have a section, add new one to chain. */
			parse->current_section->next = malloc(sizeof(NUTCONF_SECTION_t));
			parse->current_section = parse->current_section->next;
			memset(parse->current_section, 0, sizeof(NUTCONF_SECTION_t));
			parse->current_arg = NULL;
		}
		/* Set the section name. */
		if (line->arg_count > 0)
		{
			parse->current_section->name = LEXTOKEN_chralloc(line->args[0]);
		}
		break;
	case SYNPARSING_LINETYPE_DIRECTIVE_COLON:
	case SYNPARSING_LINETYPE_DIRECTIVE_EQUAL:
	case SYNPARSING_LINETYPE_DIRECTIVE_NOSEP:
		if (line->arg_count < 1)
		{
			/* No directive if no argument. */
			break;
		}

		if (parse->current_section == NULL)
		{
			/* No current section - begin of the parsing.*/
			/* Use conf as section .*/
			parse->current_section = parse->conf;
		}

		/* Add a new argument. */
		if (parse->current_arg != NULL)
		{
			parse->current_arg->next = malloc(sizeof(NUTCONF_ARG_t));
			parse->current_arg = parse->current_arg->next;
		}
		else
		{
			parse->current_arg = malloc(sizeof(NUTCONF_ARG_t));
		}
		memset(parse->current_arg, 0, sizeof(NUTCONF_ARG_t));

		/* Set directive name. */
		parse->current_arg->name = LEXTOKEN_chralloc(line->args[0]);
		
		/* Set directive type. */
		switch(line->line_type)
		{
		case SYNPARSING_LINETYPE_DIRECTIVE_COLON:
			parse->current_arg->type = NUTCONF_ARG_COLON;
			break;
		case SYNPARSING_LINETYPE_DIRECTIVE_EQUAL:
			parse->current_arg->type = NUTCONF_ARG_EQUAL;
			break;
		default:
			parse->current_arg->type = NUTCONF_ARG_NONE;
			break;
		}

		/* TODO Add directive values.*/
		for(num=1; num<line->arg_count; num++)
		{
			/* ... */
		}

		break;
	default:
		/* Do nothing (unknown or comment. */
		break;
	}
}

NUTCONF_t* nutconf_conf_parse(const char* src, int len)
{
	NUTCONF_CONF_PARSE_t parse;
	
	/* Validate parameters */
	if (src==NULL || len <=0)
	{
		return NULL;
	}

	/* Initialize working structures */
	memset(&conf, 0, sizeof(NUTCONF_CONF_PARSE_t));
	parse.conf = malloc(sizeof(NUTCONF_t));
	memset(parse.conf, 0, sizeof(NUTCONF_t));
	
	/* Do the parsing. */
	nutconf_parse_memory(src, len, nutconf_conf_parse_callback, &parse);

	/* TODO Test for successfull parsing. */
	return parse.conf;
}


