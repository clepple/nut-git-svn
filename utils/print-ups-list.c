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

#include "parseconf.h"

int fdi = 0;
int udev = 0;
int hotplug = 0;
int html = 0;

static struct option long_options[] = {
	{"fdi-hal",          no_argument,       0, 'f'},
	{"udev-rules",       no_argument,       0, 'u'},
	{"hotplug-usermap",  no_argument,       0, 'p'},
	{"html-array",       no_argument,       0, 'a'},
	{"help",             no_argument,       0, 'h'}
};

void print_tux();

void err_handler(char* errtxt) {
	fprintf(stderr, errtxt);
}

void help() {
	printf("\nusage: print-ups-list [OPTIONS]\n\n");

	printf("  -f, --fdi-hal          - make the fdi output file for HAL\n");
	printf("  -u, --udev-rules       - make the udev rules output file for udev\n");
	printf("  -p, --hotplug-usermap  - make the hotplug usermap output file for hotplug\n");
	printf("  -h, --help             - display this help\n");
}

FILE* open_file(char* file_name, char* mode ,void errhandler(const char*)) {
	FILE* file;
	void (*errfunction)(const char*) = errhandler;
	char* s;
	
	if (errfunction == 0) {
		errfunction = &printf;
	}
	file = fopen(file_name, mode);
	if (file == 0) {
		s = malloc(sizeof(char) * (32 + strlen(file_name)));
		switch (errno) {
			case EACCES : 
				sprintf(s, "Permission denied to access to %s", file_name);
				errfunction(s);
				break;
			case ENOENT :
				sprintf(s, "%s does not exist", file_name);
				errfunction(s);
				break;
			default :
				sprintf(s, "Did not reach to open %s", file_name);
				errfunction(s);
		}
		return 0;
	}
	return file;
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
	
	// no footer for hotplug
}

void print_ups(char* vendorID, char* productID) {
	char* s;
	if (fdi) {
		s = malloc(200);
		printf("      <match key=\"usb_device.vendor_id\" int=\"%s\">\n", vendorID);
		printf("        <match key=\"usb_device.product_id\" int=\"%s\">\n", productID);
		printf("          <append key=\"info.category\" type=\"string\">battery</append>\n");
		printf("          <merge key=\"info.capabilities\" type=\"strlist\">battery</merge>\n");
		printf("          <merge key=\"info.addons\" type=\"strlist\">hald-addon-usbhid-ups</merge>\n");
		printf("          <merge key=\"battery.type\" type=\"string\">ups</merge>\n");
        printf("        </match>\n");
		printf("      </match>\n\n");
	}
	
	if (udev) {
		printf("SYSFS{idVendor}==\"%s\", SYSFS{idProduct}==\"%s\", MODE=\"660\", GROUP=\"@RUN_AS_USER@\"\n",vendorID + 2, productID + 2 );
	}
	
	if (hotplug) {
		printf("libhidups\t0x0003\t%s\t%s\t0x0000\t0x0000\t0x00\t0x00\t0x00\t0x00\t0x00\t0x00\t0x00000000\n", vendorID, productID);
	}
}

void parse(FILE* pipe) {
	PCONF_CTX* ctx = malloc(sizeof(PCONF_CTX));
	char *vendorID = 0;
	char *productID = 0;
	int new_entry = 0;
	
	pconf_init(ctx, err_handler);
	
	ctx->f = pipe;
	
	while (pconf_file_next(ctx) != 0) {
		
		
		if (strcmp(ctx->arglist[0], "===") == 0) {
			if (new_entry == 1) {
				vendorID = 0;
				productID = 0;
			}
			new_entry = 1;
		}
		if (strcmp(ctx->arglist[0], "VendorID") == 0) {
			if (!new_entry) continue;
			vendorID = strdup(ctx->arglist[2]);
			if (productID != 0) {
				print_ups(vendorID, productID);
				free(productID);
				free(vendorID);
				productID = 0;
				vendorID = 0;
				new_entry = 0;
			}
		}
		if (strcmp(ctx->arglist[0], "ProductID") == 0) {
			if (!new_entry) continue;
			productID = strdup(ctx->arglist[2]);
			if (vendorID != 0) {
				print_ups(vendorID, productID);
				free(productID);
				free(vendorID);
				productID = 0;
				vendorID = 0;
				new_entry = 0;
			}
		}
	}
}

int main(int argc, char** argv) {
	int option_index = 0, i;
	FILE* pipe;
	
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
	
	if ((fdi + udev + hotplug + html) == 0) {
		printf("Only use one output format\n");
		help();
	}
	
	
	if (html) {
		printf("This command is not implemented yet.\n");
		printf("But I can draw a nice ascii art for you ! :\n");
		print_tux();
	}
	
	pipe = popen("../drivers/newhidups -l", "r");
	
	if (pipe == 0) {
		printf("Error : Unable to launch newhidups\n"); 
		return 1;
	}
	
	print_headers();
	
	parse(pipe);
		
	print_footers();
	
	pclose(pipe);

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
