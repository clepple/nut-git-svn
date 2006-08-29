/* print-ups-list : A tool to output the list of supported ups in diffrent format

   Copyright (C) 2001  Jonathan Dion <dion.jonathan@gmail.com>

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

#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "common.h"

#define LINE_BUFFER_SIZE 200

#define NAME_SIGN 1
#define VENDOR_SIGN 2
#define EXTRAINFO_SIGN 4
#define VENDORID_SIGN 8
#define PRODUCTID_SIGN 16
#define DESC_SIGN 32
#define TYPE_SIGN 64

typedef struct _ups_record {
	char* name;
	char* vendor;
	char* extrainfo;
	int vendorid;
	int productid;
	char* desc;
	char* driver;
	char* type; /* For genericups supported UPS */
	int sign;
	struct _ups_record* next;
} *ups_record;

typedef struct _id_list {
	int vendorid;
	int productid;
	struct _id_list* next;
} *id_list;

int fdi = 0;
int udev = 0;
int hotplug = 0;
int html = 0;
char* driver;
char* vendor;
char* color;
id_list id_already_printed = NULL;
ups_record record_list = 0, last = 0;

static char* usb_driver_list[] = {
	"newhidups",
	"tripplite_usb",
	"bcmxcp_usb",
	0 /* End */
};

static char* driver_list[] = {
	"al175",
	"apcsmart",
	"bcmxcp",
	"bcmxcp_usb",
	"belkin",
	"belkinunv",
	"bestfcom",
	"bestuferrups",
	"bestups",
	"blazer",
	"cpsups",
	"cyberpower",
	"energizerups",
	"esupssmart",
	"etapro",
	"everups",
	"fentonups",
	"gamatronic",
	"genericups",
	"ippon",
	"isbmex",
	"liebert",
	"masterguard",
	"megatec",
	"metasys",
	"mge-shut",
	"mge-utalk",
	"mustek",
	"newhidups",
	"nitram",
	"oneac",
	"optiups",
	"powercom",
	"rhino",
	"safenet",
	"sms",
	"snmp-ups",
	"solis",
	"tripplite",
	"tripplitesu",
	"tripplite_usb",
	"upscode2",
	"victronups",
	0 /* End */
};


static struct option long_options[] = {
	{"fdi-hal",          no_argument,       0, 'f'},
	{"udev-rules",       no_argument,       0, 'u'},
	{"hotplug-usermap",  no_argument,       0, 'p'},
	{"html-array",       no_argument,       0, 'a'},
	{"help",             no_argument,       0, 'h'}
};

int compare(ups_record ups1, ups_record ups2) {
	int ret;
	
	ret = strcmp(ups1->vendor, ups2->vendor);
	if (ret != 0) {
		return ret;
	}
	
	ret = strcmp(ups1->name, ups2->name);
	if (ret != 0) {
		return ret;
	}
	
	if (ups1->extrainfo == 0) return -1;
	ret = strcmp(ups1->extrainfo, ups2->extrainfo);
	if (ret != 0) {
		return ret;
	}
	
	ret = strcmp(ups1->driver, ups2->driver);
	return ret;
}

void print_tux();

void err_handler(const char* errtxt) {
	fprintf(stderr, errtxt);
}

void help() {
	printf("\nusage: print-ups-list [OPTIONS]\n\n");

	printf("  -f, --fdi-hal          - make the fdi output file for HAL\n");
	printf("  -u, --udev-rules       - make the udev rules output file for udev\n");
	printf("  -p, --hotplug-usermap  - make the hotplug usermap output file for hotplug\n");
	printf("  -h, --help             - display this help\n");
}


void print_headers() {
	
	if (fdi) {
		printf("<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?> <!-- -*- SGML -*- -->\n");
		printf("<deviceinfo version=\"0.2\">\n");
		printf("  <device>\n");
		printf("    <match key=\"info.bus\" string=\"usb_device\">\n\n");
	}
	
	if (udev) {
		printf("# udev rules for the NUT USB drivers\n");
		printf("SUBSYSTEM!=\"usb_device\", GOTO=\"nut-usbups_rules_end\"\n");
		printf("ACTION!=\"add\", GOTO=\"nut-usbups_rules_end\"\n\n");
	}
	
	if (hotplug) {
		printf("# This file is installed by the Network UPS Tools package.\n");
		printf("#\n");
		printf("# Sample entry (replace 0xVVVV and 0xPPPP with vendor ID and product ID respectively) :\n");
		printf("# usb module         match_flags idVendor idProduct bcdDevice_lo bcdDevice_hi bDeviceClass bDeviceSubClass bDeviceProtocol bInterfaceClass bInterfaceSubClass bInterfaceProtocol driver_info\n\n");
	}
	
	if (html) {
		printf("<TABLE BORDER=\"1\" CELLPADDING=\"5\" CELLSPACING=\"1\" ALIGN=\"CENTER\" BGCOLOR=\"#FFFFFF\">\n");

        /* print header */
        printf("<TR BGCOLOR=\"#959595\">\n");
        printf("<TH>Manufacturer</TH>\n");
        printf("<TH>Model</TH>\n");
        printf("<TH>NUT driver</TH>\n</TR>\n");
	}
}

void print_footers() {
	
	if (fdi) {
		printf("    </match>\n");
		printf("  </device>\n");
		printf("</deviceinfo>\n");
	}
	
	if (udev) {
		printf("\nLABEL=\"nut-usbups_rules_end\"\n");
	}
	
	/* no footer for hotplug */
	
	if (html) {
		printf("</TABLE>\n");
	}
}

int is_already_printed(int vendorid, int productid, id_list *list) {
	if (*list == NULL) {
		*list = malloc(sizeof(struct _id_list));
		(*list)->vendorid = vendorid;
		(*list)->productid = productid;
		(*list)->next = NULL;
		return 0;
	}
	if ( (*list)->vendorid == vendorid && (*list)->productid == productid ) {
		return 1;
	}
	return is_already_printed(vendorid, productid, &((*list)->next));
}

void print_record(ups_record record) {
	int sign = record->sign;
	int i = 1;
	ups_record aux;
	int changed_vendor = 0;
	
	if (fdi) {
		/* Ignore record that don't have both vendorid and productid */
		if ((sign | (VENDORID_SIGN + PRODUCTID_SIGN)) == 0 ) return; 
		/* Ignore id pairs that were already printed */
		if (is_already_printed(record->vendorid, record->productid, &id_already_printed)) return;
		printf("      <match key=\"usb_device.vendor_id\" int=\"0x%04x\">\n", record->vendorid);
		printf("        <match key=\"usb_device.product_id\" int=\"0x%04x\">\n", record->productid);
		printf("          <append key=\"info.category\" type=\"string\">battery</append>\n");
		printf("          <merge key=\"info.capabilities\" type=\"strlist\">battery</merge>\n");
		printf("          <merge key=\"info.addons\" type=\"strlist\">hald-addon-usbhid-ups</merge>\n");
		printf("          <merge key=\"battery.type\" type=\"string\">ups</merge>\n");
        printf("        </match>\n");
		printf("      </match>\n\n");
	}
	
	if (udev) {
		/* Ignore record that don't have both vendorid and productid */
		if ((sign | (VENDORID_SIGN + PRODUCTID_SIGN)) == 0 ) return; 
		/* Ignore id pairs that were already printed */
		if (is_already_printed(record->vendorid, record->productid, &id_already_printed)) return;
		printf("SYSFS{idVendor}==\"0x%04x\", SYSFS{idProduct}==\"0x%04x\", MODE=\"660\", GROUP=\"@RUN_AS_USER@\"\n",record->vendorid, record->productid );
	}
	
	if (hotplug) {
		/* Ignore record that don't have both vendorid and productid */
		if ((sign | (VENDORID_SIGN + PRODUCTID_SIGN)) == 0 ) return; 
		/* Ignore id pairs that were already printed */
		if (is_already_printed(record->vendorid, record->productid, &id_already_printed)) return;
		printf("libhidups\t0x0003\t0x%04x\t0x%04x\t0x0000\t0x0000\t0x00\t0x00\t0x00\t0x00\t0x00\t0x00\t0x00000000\n", record->vendorid, record->productid);
	}
	
	if (html) {
		if (vendor == 0 || strcmp(record->vendor, vendor) != 0) {
			if (color == 0) {
				color = "#E0E0E0";
			} else if (strcmp(color,"#F0F0F0") == 0) {
				color = "#E0E0E0";
			} else if (strcmp(color,"#E0E0E0") == 0) {
				color = "F0F0F0";
			} else {
				color = "#E0E0E0";
			}
			printf("<TR BGCOLOR=\"%s\">\n", color);
			
			vendor = record->vendor;
			changed_vendor = 1;
			aux = record;
			while( aux != 0 && strcmp(aux->vendor, vendor) == 0) {
				i++;
				aux = aux->next;
			}
			printf("<TH ROWSPAN=\"%d\">%s</TH>\n", i, vendor);
		}
		printf("<TR BGCOLOR=\"%s\">\n", color);
		printf("<TD>%s", record->name);
		if ((sign & EXTRAINFO_SIGN) != 0 ) {
			printf("<BR>\n%s", record->extrainfo);
		}
		printf("</TD>\n");
		
		
		if (driver == 0 || strcmp(record->driver, driver) != 0 || changed_vendor) {
			driver = record->driver;
			aux = record;
			i = 0;
			while( aux != 0 && strcmp(aux->vendor, vendor) == 0 && strcmp(aux->driver, driver) == 0) {
				i++;
				aux = aux->next;
			}
			
			if ((sign & TYPE_SIGN) != 0 ) {
				printf("<TD ROWSPAN=\"%d\">%s type = %s</TD>\n", i, record->driver, record->type);
			} else {
				printf("<TD ROWSPAN=\"%d\">%s</TD>\n", i, record->driver);
			}
		}
		printf("</TR>\n");
	}
}

void sort() {
	ups_record aux, aux2, sorted_list ;
	
	if (record_list != 0) {
		sorted_list = record_list;
		record_list = record_list->next;
		sorted_list->next = 0;
	} else {
		return;
	}
	
	while (record_list != 0) {
		aux = record_list;
		record_list = record_list->next;
		aux->next = 0;
		
		if (compare(aux, sorted_list) < 0) {
			aux->next = sorted_list;
			sorted_list = aux;
			continue;
		}
		aux2 = sorted_list;
		while(aux2->next != 0) {
			if (compare(aux, aux2->next) < 0) break;
			aux2 = aux2->next;
		}
		aux->next = aux2->next;
		aux2->next = aux;
		
	}
	
	record_list = sorted_list;
}

int hex_to_int (char c) {
	unsigned int y;
	
	if      ((y = c - '0') <= '9'-'0') return y;
    else if ((y = c - 'a') <= 'f'-'a') return y+10;
    else if ((y = c - 'A') <= 'F'-'A') return y+10;
    else return -1;
}

int parse_hex(char* value) {
	
	int len = strlen(value);
	int i, aux;
	int res = 0;
	
	for (i = 0; i < 4 && len - i > 0 && value[len - i - 1] != 'x' ; i++ ) {
		aux = hex_to_int(value[len - i - 1]);
		if (aux == -1) return -1;
		res += (aux << (4*i));
	}
	
	return res;

}


/* Remove the space and tab at the begining or at the end of a string*/
char* strip(char* str) {
	if (str[0] == ' ' || str[0] == '\t') return strip(&(str[1]));
	
	if (str[strlen(str) - 1 ] == ' ' 
		|| str[strlen(str) - 1 ] == '\t'
		|| str[strlen(str) - 1 ] == '\n' ) {
			
		str[strlen(str) - 1 ] = 0;
		return strip(str);
	}
	return str;
}

/* if line represents a key-value pair, return 1 and let key and value
  point to key and value. "line" is updated destructively, and key
  and value may point inside line. If not a key-value-pair, return 0
  and leave line unchanged. */
int key_value(char *line, char **key, char **value) {
	char *p;

	p = strchr(line, ':');
	if (!p) {
		return 0;
	}
	*p = 0;
	*key = strip(line);
	*value = strip(p + 1);
   	return 1;
}

ups_record parse(FILE* pipe) {
	
	char line[LINE_BUFFER_SIZE];
	char *p, *key, *value;
	int l = 0;
	int have_record = 0;
	ups_record record = malloc(sizeof(struct _ups_record));
	int first_line_found = 0;
	int sign = 0;
	
	record->next = 0;
	record->driver = driver;

	while (fgets(line, LINE_BUFFER_SIZE, pipe) > 0) {
		l++;
		p = strip(line);
		if (*p == 0 || *p == '#') { /* comment, blank line */
			continue;
		}
		if (*p == '=') {  /* any line starting with '=' */
			first_line_found = 1;
			if (have_record) {
				record->sign = sign;
				if (record_list == 0) {
					record_list = record;
					last = record_list;
				} else {
					last->next = record;
					last = last->next;
				}
				last = record;
				record = malloc(sizeof(struct _ups_record));
				record->next = 0;
				record->driver = driver;
				
				//print_record(record, sign);
				have_record = 0;
				sign = 0;
			}
		} else if (first_line_found && key_value(p, &key, &value)) {
			if (strcasecmp(key, "ProductID") == 0) {
				record->productid = parse_hex(value);
				if (record->productid != -1) sign += PRODUCTID_SIGN;
			} else if (strcasecmp(key, "VendorID") == 0) {
				record->vendorid = parse_hex(value);
				if (record->vendorid != -1) sign += VENDORID_SIGN;
			} else if (strcasecmp(key, "Name") == 0) {
				record->name = strdup(value);
				sign += NAME_SIGN;
			} else if (strcasecmp(key, "Vendor") == 0) {
				record->vendor = strdup(value);
				sign += VENDOR_SIGN;
			} else if (strcasecmp(key, "ExtraInfo") == 0) {
				record->extrainfo = strdup(value);
				sign += EXTRAINFO_SIGN;
			} else if (strcasecmp(key, "Desc") == 0) {
				record->desc = strdup(value);
				sign += DESC_SIGN;
			} else if (strcasecmp(key, "Type") == 0) {
				record->type = strdup(value);
				sign += TYPE_SIGN;	
			} else {
				fprintf(stderr,"Warning: unknown key %s in line %d\n", key, l);
			}
			have_record = 1;
			continue;
		} 
	}
    if (have_record) {
    	record->sign = sign;
    	if (record_list == 0) {
			record_list = record;
			last = record_list;
		} else {
			last->next = record;
			last = last->next;
		}
		
        //print_record(record, sign);
    }
    return record_list;
}


int main(int argc, char** argv) {
	int option_index = 0, i;
	FILE* pipe;
	char* s;
	char c;
	char** list;
	
	while ((i = getopt_long(argc, argv, "+fupah", long_options, &option_index)) != -1) {
		switch (i) {
			case 'f':
				fdi = 1;
				break;
			case 'u':
				udev = 1;
				break;
			case 'p':
				hotplug = 1;
				break;
			case 'a':
				html = 1;
				break;
			default:
				help();
				return 1;
				break;
		}
	}
	
	if ((fdi | udev | hotplug | html) == 0) {
		printf("You didn't told me what to do\n");
		printf("So I'll draw some ascii art! :\n");
		print_tux();
		printf("Perhaps you want some help ?\n");
		help();
	}
	
	if ((fdi + udev + hotplug + html) > 1) {
		printf("Only use one output format\n");
		help();
		exit(0);
	}
	
	
	if (html) {
		list = driver_list;
	} else {
		list = usb_driver_list;
	}
	
	
	print_headers();
	
	i = 0;
	
	while (list[i] != 0) {
		
		driver = strdup(list[i]);
		
		s = malloc(sizeof(char) * (strlen(list[i]) + 15));
		sprintf(s, "../drivers/%s -l", list[i]);
		pipe = popen(s, "r");
	
		if ( (c = fgetc(pipe)) == EOF) {
			s = malloc(sizeof(char) * (strlen(DRVPATH) + strlen(list[i]) + 5));
			sprintf(s, "%s/newhidups -l", DRVPATH);
			pipe = popen(s, "r");
			free(s);
			if ((c = fgetc(pipe)) == EOF) {
				fprintf(stderr, "Error : Unable to launch %s\n", list[i]); 
				i++;
				continue;
			}
		}
	
		ungetc(c, pipe);
	
		parse(pipe);
		
		pclose(pipe);
	
		i++;
	
	}
	
	if (html) {
		sort();
		vendor = 0;
		driver = 0;
	}
	
	while(record_list != 0) {
		print_record(record_list);
		record_list = record_list->next;
	}
	
	print_footers();
	

	return 0;
}








void print_tux() {
		printf("             .-\"\"\"-.\n");
		printf("           '       \n");
		printf("          |,.  ,-.  |\n");
		printf("          |()L( ()| |\n");
		printf("          |,'  `\".| |\n");
		printf("          |.___.',| `\n");
		printf("         .j `--\"' `  `.\n");
		printf("        / '        '   \\\n");
		printf("       / /          `   `.\n");
		printf("      / /            `    .\n");
		printf("     / /              l   |\n");
		printf("    . ,               |   |\n");
		printf("    ,\"`.             .|   |\n");
		printf(" _.'   ``.   o     | `..-'l\n");
		printf("|       `.`,        |      `.\n");
		printf("|         `.    __.j         )\n");
		printf("|__        |--\"\"___|      ,-'\n");
		printf("   `\"--...,+\"\"\"\"   `._,.-' \n");
}
