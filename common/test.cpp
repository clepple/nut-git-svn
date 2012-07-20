
#include <iostream>
#include <string.h>
#include <stdio.h>

using namespace std;

#include "nutconf.h"



/* Test functions */
int TEST_nutconf_parse_token();
int TEST_nutconf_parse_line();
int TEST_nutconf_parse_memory();

int TEST_nutconf_parse_rule_CHARS();
int TEST_nutconf_parse_rule_STRCHARS();


/* Test helpers */

#define ASSERT_EQUAL(expected, test, str) {if((test)!=(expected)){std::cout<<"Failing assert "<<str<<" : "<<test<<" ("<<expected<<" expected)"<<std::endl;return -1;}}

/* Tests */

void print_token(LEXTOKEN_t* token)
{
	if (token == NULL)
	{
		printf("--null--");
		return;
	}
	switch (token->type)
	{
	case TOKEN_UNKNOWN: printf("-unknown-"); break;
	case TOKEN_NONE: printf("-none-"); break;
	case TOKEN_STRING: printf("-string-"); break;
	case TOKEN_QUOTED_STRING: printf("-quoted-string-"); break;
	case TOKEN_COMMENT: printf("-comment-"); break;
	case TOKEN_BRACKET_OPEN: printf("-bracket-open-"); break;
	case TOKEN_BRACKET_CLOSE: printf("-bracket-close-"); break;
	case TOKEN_EQUAL: printf("-equal-"); break;
	case TOKEN_COLON: printf("-colon-"); break;
	case TOKEN_EOL: printf("-eol-"); break;
	default: printf("-default-"); break;
	}
	if(token->begin==NULL)
		printf("NULL-");
	else if(token->end==token->begin)
		printf("EMPTY-");
	else

		printf("\"%*s\"-", (int)(token->end-token->begin), token->begin);
}

void nutconf_parse_line_test_callback(SYNLINE_t* line, void* user_data)
{
	printf("nutconf_parse_test_callback : ");
	switch (line->line_type)
	{
    case SYNPARSING_LINETYPE_COMMENT:
		printf("comment : %d : %*s\n", (int)(line->comment.end-line->comment.begin), (int)(line->comment.end-line->comment.begin), line->comment.begin);
		break;
    case SYNPARSING_LINETYPE_SECTION:
		printf("section\n");
		break;
    case SYNPARSING_LINETYPE_DIRECTIVE_COLON:
		printf("directive colon\n");
		break;
    case SYNPARSING_LINETYPE_DIRECTIVE_EQUAL:
		printf("directive equal\n");
		break;
    case SYNPARSING_LINETYPE_DIRECTIVE_NOSEP:
		printf("directive nosep\n");
		break;
    case SYNPARSING_LINETYPE_UNKNWON:
	default:
		printf("unknown\n");
		break;
	}
}

const char* content = 
"toto:bidule\n"
"[truc] # new section\n"
"bidule=15\n"
"\n"
"#plop\n"
"toto\n";

int TEST_nutconf_parse_memory()
{
	nutconf_parse_memory(content, strlen(content),
		nutconf_parse_line_test_callback, NULL);

	return 0;
}

int TEST_nutconf_parse_line()
{
	const char* src = " [toto] # A bidule is a truc !!";
	int len;
	const char *rend;
	SYNLINE_t line;
	LEXTOKEN_t args[50];

	line.args = args;
	line.nb_args = 50;	

	len = strlen(src);
	nutconf_parse_line(src, len, &rend, &line);

	return 0;
}

int TEST_nutconf_parse_token()
{
	const char* src = "ceci \"est un\" test.";
	const char* src2 = "\"est\\\\ \\\"un\"";
	const char* src3 = "toto[bidule\\[turc chose]autre";

	int len;

	const char *begin, *end;

    LEXTOKEN_t token;

	len = strlen(src);
	ASSERT_EQUAL(1, nutconf_parse_token(src, len, &token), "\"ceci\"");
	ASSERT_EQUAL(4, token.end-token.begin, "\"ceci\"");

	ASSERT_EQUAL(2, nutconf_parse_token(src+4, len-4, &token), "\" \"est un\"");
	ASSERT_EQUAL(6, token.end-token.begin, "\" \"est un\"");

	ASSERT_EQUAL(1, nutconf_parse_token(src+14, len+14, &token), "\"test.\"");
	ASSERT_EQUAL(5, token.end-token.begin, "\"test.\"");

	len = strlen(src2);
	ASSERT_EQUAL(2, nutconf_parse_token(src2, len, &token), src2);
	ASSERT_EQUAL(10, token.end-token.begin, src2);

	len = strlen(src3);
	ASSERT_EQUAL(1, nutconf_parse_token(src3, len, &token), src3);
	ASSERT_EQUAL(4, token.end-token.begin, src3);
	ASSERT_EQUAL(1, nutconf_parse_token(src3+5, len-5, &token), src3+5);
	ASSERT_EQUAL(12, token.end-token.begin, src3+5);
	ASSERT_EQUAL(1, nutconf_parse_token(src3+18, len-18, &token), src3+18);
	ASSERT_EQUAL(5, token.end-token.begin, src3+18);

	return 0;
}

int TEST_nutconf_parse_rule_CHARS()
{
	const char* src = "ceci est un test.";
	const char* src2 = "toto#titi\"tata";
	const char* src3 = "toto\\#titi\\\"tata\\ tutu";
	int len;

	len = strlen(src);
	ASSERT_EQUAL(4, nutconf_parse_rule_CHARS(src, len), "\"ceci\"");
	ASSERT_EQUAL(3, nutconf_parse_rule_CHARS(src+1, len-1), "\"eci\"");
	ASSERT_EQUAL(0, nutconf_parse_rule_CHARS(src+4, len-4), "\" \"");
	ASSERT_EQUAL(3, nutconf_parse_rule_CHARS(src+5, len-5), "\"est\"");
	ASSERT_EQUAL(5, nutconf_parse_rule_CHARS(src+12, len-12), "\"test.\"");

	len = strlen(src2);
	ASSERT_EQUAL(4, nutconf_parse_rule_CHARS(src2, len), "\"toto#\"");
	ASSERT_EQUAL(4, nutconf_parse_rule_CHARS(src2+5, len+5), "\"titi\"\"");

	len = strlen(src3);
	ASSERT_EQUAL(22, nutconf_parse_rule_CHARS(src3, len), "toto\\#titi\\\"tata\\ tutu");

	return 0;
}

int TEST_nutconf_parse_rule_STRCHARS()
{
	const char* src = "ceci est un test.";
	const char* src2 = "toto#titi\"tata";
	const char* src3 = "toto\\\\titi\\\"tata";
	int len;

	len = strlen(src);
	ASSERT_EQUAL(17, nutconf_parse_rule_STRCHARS(src, len), "\"ceci est un test.\"");
	ASSERT_EQUAL(16, nutconf_parse_rule_STRCHARS(src+1, len-1), "\"eci est un test.\"");

	len = strlen(src2);
	ASSERT_EQUAL(9, nutconf_parse_rule_STRCHARS(src2, len), "\"toto#titi\"tata\"");

	len = strlen(src3);
	ASSERT_EQUAL(16, nutconf_parse_rule_STRCHARS(src3, len), "toto\\\\titi\\\"tata");

	return 0;
}


#define CALL_TEST(name, fct) {std::cout<<"Test "<<name<<" : ";if(fct()){std::cout<<"FAILED"<<std::endl;return -1;}std::cout<<"SUCCESS"<<std::endl;}

/* Main */
int main()
{
	cout << "testnutconf" << endl;

    CALL_TEST("nutconf_parse_rule_CHARS", TEST_nutconf_parse_rule_CHARS)
    CALL_TEST("nutconf_parse_rule_STRCHARS", TEST_nutconf_parse_rule_STRCHARS)
    CALL_TEST("nutconf_parse_token", TEST_nutconf_parse_token)
    CALL_TEST("nutconf_parse_line", TEST_nutconf_parse_line)
    CALL_TEST("nutconf_parse_memory", TEST_nutconf_parse_memory)

	return 0;
}




