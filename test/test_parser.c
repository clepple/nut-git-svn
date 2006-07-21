#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tree.h"
#include "stack.h"
#include "nutparser.h"


int main(int argc, char** argv) {
	t_tree tree2;
	
	if (argc == 1) {
		fprintf(stderr,"Please tell me what file to parse : test_parser file_name\n");
		exit(EXIT_FAILURE);
	}
	
	if (argc > 2 ) {
		fprintf(stderr,"Too much parameters. Only the first will be kept\n");
	}
	
	printf("\nI begin the parsing of %s...\n...\n", argv[1]);
	tree2 = parse_conf(argv[1], 0);
	printf("I have finished the parsing. I now print the tree I created.\n\n");
	
	print_tree(tree2);
	
	free_tree(tree2);
	
	return 0;
}
