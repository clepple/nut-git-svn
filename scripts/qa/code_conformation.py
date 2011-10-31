#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#       code_conformance.py
#       
#       Copyright 2010 Chetan Agarwal <chetanagarwal@eaton.com>, 
#                      Prachi Gandhi <prachisgandhi@eaton.com>
#       
#       This program is free software; you can redistribute it and/or modify
#       it under the terms of the GNU General Public License as published by
#       the Free Software Foundation; either version 2 of the License, or
#       (at your option) any later version.
#       
#       This program is distributed in the hope that it will be useful,
#       but WITHOUT ANY WARRANTY; without even the implied warranty of
#       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#       GNU General Public License for more details.
#       
#       You should have received a copy of the GNU General Public License
#       along with this program; If not, see <http://www.gnu.org/licenses/>


# 2010-12-6 Prachi Gandhi / Chetan Agarwal
# 2011-10-25 Prachi Gandhi
#            Script to check the code conformance as per the NUT Coding guidelines.

# FIXME:	1. Add "function pointer" in 'MatchList' table for each check
#		2. Add Regex for 'Indentation' checking of files

# Required modules
import codecs
import re
import sys
import os
import glob
import math

# File name length
nMaxLen = 0
# Checking options byte
allByte=0xFF
memByte=0x1
commByte=0x2
gotoByte=0x04
printfByte=0x08
sysByte=0x10
timeByte=0x20
shebangByte=0x40
notmemByte=0xFE
notcommByte=0xFD
notgotoByte=0xFB
notprintfByte=0xF7
notsysByte=0xEF
nottimeByte=0xDF
notshebangByte=0xBF

DIR = "/NUT/trunk"

#Regex for pattern matching
MEMCALL_REGEX = '.*[^x]([mc]|re)alloc[\ ]*\(.*[;]+'
COMMENT_REGEX = "(?<!http:)\//[^%|^EN|^W3C|^DTD](?!www)"
GOTOCHK_REGEX = "\sgoto\s"
PRINTFCHK_REGEX = "\sprintf\("
SYSLOGCHK_REGEX = "\ssyslog\("
TIMEHCHK_REGEX = "\<.*time\.h\>"
SHEBANGCHK_REGEX = "[ ]*\#\!\`which"

CHECK		= 0
EXCEPTION	= 2
REGEX		= 3
RESULT		= 4
	
MatchList = [["memory", "/", "common.c|main.c", MEMCALL_REGEX, []],["comment", "/", "common.c", COMMENT_REGEX, []],["goto", "/", "common.c", GOTOCHK_REGEX, []],["printf", "/", "common.c|snprintf.c", PRINTFCHK_REGEX, []], ["syslog", "/", "common.c", SYSLOGCHK_REGEX, []], ["time", "/", "common.c", TIMEHCHK_REGEX, []], ["shebang", "/", "common.c", SHEBANGCHK_REGEX, []]]

def grep(exprlst, list):
	""" Function to find the match in the file for passed regex 
	exprlst	: Regular expression
	list	: List containing contents of opened file
	"""
	#seek to start in the passed file 
	list.seek(0)
	matchList = []
	expr = re.compile(exprlst)
	n = 0
	#check for the match as per regex passed in given file
	ind = 0
	for text in list:						
		n=n+1				
		if exprlst == MEMCALL_REGEX:
			match = expr.search(text)	
			if(match):
				tmp = text.split("=")	
				ind = iter(list)
				text1 = ind.next()
				if text1 == "\n":
					text1 = ind.next()
				varname = str(tmp[0]).lstrip()
				varname = str(varname).rstrip()
				varname = str(varname).replace("[","\[")
				varname = str(varname).replace("]","\]")
								
				expr1 = "if[\s]*\([[\!|\s]*|\!=\s*NULL|\==\s*NULL]" 
				#expr1 = "if[\s]*\([\!|\s]*"+varname+".*"  
							
				expr11 = re.compile(expr1)
				match1 = expr11.search(text1)	
				if(match1 == None):
					matchList.append([text,n])
		else:
			match = expr.search(text)
			if match != None:
				matchList.append([text,n])
									
	return matchList

def usage():
	""" Function to display the usage of this script
	"""
	print "\nusage: code_conformation.py [folder location] [checking option]\n"
	print "usage: code_conformation.py\n"
	print "\t *** Default folder location = current NUT directory, Default option = all ***\n"
	print "usage: code_confromation.py [folder location]\n"
	print "\t *** Default option = all ***\n"
	print "usage: code_confromation.py [checking option]\n"
	print "\t *** Default folder location = current NUT directory ***\n"
	print "checking options: " + "mem " + "or all or " + "\"mem|comm|goto|printf|sys|time|shebang\"" + " or " + "\"all|~printf\"" + " or " + "\"all|~printf|~goto\"\n"
	print "default option: all\n"
	print "help: -h --help"

# 
def getfilelist(source):	
	""" Function to get .cpp or .c filename from source folder location specified in options
	source     : Folder name to search for the source code files
	"""
	p = re.compile('.*\.[cC]+[pP]*$')

	# Function to get filename from source folder location specified in options
	for root, dirs, files in os.walk(source, topdown=False):
	#source = source + "*.c"
	#print source
	#for filename in glob.glob('/NUT/trunk/drivers/*.c'):
		for name in files:
			memcnt=0
			if p.match(name):
				filename = os.path.join(root, name)
				# call function to check code conformance specified in options in file found.
				checkfile(filename);
	return

# 
def checkfile(filename):
	""" Function to check code conformance specified in options in file passed.
	filename : File name being scanned
	"""

	global CodeConfByte
		
	# Check byte and call 'getResult' function	
	if CodeConfByte & memByte == memByte:		
		getResult(memByte,filename)		
	if CodeConfByte & commByte == commByte:		
		getResult(commByte,filename)					
	if CodeConfByte & gotoByte == gotoByte:
		getResult(gotoByte,filename)
	if CodeConfByte & printfByte == printfByte:
		getResult(printfByte,filename)
	if CodeConfByte & sysByte == sysByte:	
		getResult(sysByte,filename)
	if CodeConfByte & timeByte == timeByte:
		getResult(timeByte,filename)
	if CodeConfByte & shebangByte == shebangByte:
		getResult(shebangByte,filename)
		
	return

def getResult(checkbyte, filename):
	""" Function for checking exception file for checkbyte option and get result matchlist.
	checkbyte 	: Checking option byte for requested check
	filename	: File name being scanned
	"""
	
	global nMaxLen
	index = 0
	
	#get filename from the full file path
	file = filename.split("/")
	#get index of 'MatchList' corresponding to passed checkbyte		
	index = int(math.log(checkbyte,EXCEPTION))
	#get the list of exception files if any for passed checkbyte	
	exceptionlist = (MatchList[index][EXCEPTION]).split("|")
	
	#check filename for exception file corresponding to passed check byte	
	for no in xrange(len(exceptionlist)):
		if(file[(len(file))-1] == exceptionlist[no]):
			return
			
	# Open file passed to check code conformance
	fd = codecs.open(filename, encoding='utf-8')
	#call 'grep' function to find the match corresponding to regex in the file passed
	matchlist = grep(MatchList[index][REGEX], fd)
	
	#get maximum file legth
	if nMaxLen < len(filename):
		nMaxLen = len(filename)	
					
	#append matched result of checkbyte in 'MatchList'
	for line in matchlist:						
		MatchList[index][4].append([line[0], line[1], filename])

	
def Display(List,index):
	""" Function to print file path, invalid count and invalid statement in passed Invalid list
	List     : List to display
	"""
	#Get list of invalid data like filename, line no, invalid data
	loglist = List[RESULT]
	
	for no in xrange(len(loglist)):
		print "%s | %s | %s" %(loglist[no][2].ljust(nMaxLen),str(loglist[no][1]).ljust(6), loglist[no][0].lstrip())
	print str('-'*120)
		

def main():
	""" Function main 
	"""
	global nMaxLen	
	global CodeConfByte 

	CodeConfByte = 0
		
	# Get length of argument list
	argscnt = len(sys.argv)
	
	# Display usage and exit script if folder location is not mentioned
	#if argscnt < 2:
	#	usage()
	#	exit()		
	
	# Display usage and exit script only if "help" option is specified not the folder location
	#if argscnt == 2:		
	#	if sys.argv[1] == "-h" or sys.argv[1] == "--help":
	#		usage()
	#		exit()
	#	else:
	#		print "Default option : all"
	# Display only if folder location is mentioned make default checking option as "all"
	#		CodeConfByte = CodeConfByte | allByte
		
	# Default check option is "all" otherwise create list of all options
	#ArgumentList = "all"
	if argscnt > 2:
		ArgumentList = []		
		ArgumentList = sys.argv[2].split("|")
		sourcename = sys.argv[1]
	else:
		# check if 1st argument is source location or exception option
		if argscnt > 1:
			argument = sys.argv[1]			
			p = re.compile("\/")
			match = p.search(argument)
			if match:
				sourcename = argument
				ArgumentList = []
				ArgumentList = "all"
				CodeConfByte = CodeConfByte | allByte				
			else:
				folders = sys.argv[0].split("/")
				#if folders[0] == scripts:
				if len(folders) > 1:
					sourcename = "."
				else:
					sourcename = "../.."
				ArgumentList = []
				ArgumentList = sys.argv[1].split("|")				
		else:
			folders = sys.argv[0].split("/")
			sourcename = "../.."
			if folders[0]== "." and folders[1] == "code_conformation.py":
				if len(folders) < 2:
					sourcename = "../.."
			else:
				if len(folders) > 1:
                        		sourcename = "."
	                        else:
					sourcename = "../.."
			ArgumentList = "all"
			CodeConfByte = CodeConfByte | allByte
			
	# As per all options creat code conformance byte	
	for ListItem in ArgumentList:								
		if ListItem == "all":
			CodeConfByte = CodeConfByte | allByte		
		if ListItem == "~mem":
			CodeConfByte = CodeConfByte & notmemByte			
		if ListItem == "mem":
			CodeConfByte = CodeConfByte | memByte
		if ListItem == "~comm":
			CodeConfByte = CodeConfByte & notcommByte			
		if ListItem == "comm":
			CodeConfByte = CodeConfByte | commByte
		if ListItem == "~goto":
			CodeConfByte = CodeConfByte & notgotoByte			
		if ListItem == "goto":
			CodeConfByte = CodeConfByte | gotoByte
		if ListItem == "~printf":
			CodeConfByte = CodeConfByte & notprintfByte			
		if ListItem == "printf":
			CodeConfByte = CodeConfByte | printfByte
		if ListItem == "~sys":
			CodeConfByte = CodeConfByte & notsysByte			
		if ListItem == "sys":
			CodeConfByte = CodeConfByte | sysByte
		if ListItem == "time":
			CodeConfByte = CodeConfByte | timeByte
		if ListItem == "~time":
			CodeConfByte = CodeConfByte & nottimeByte
		if ListItem == "shebang":
			CodeConfByte = CodeConfByte | shebangByte
		if ListItem == "~shebang":
			CodeConfByte = CodeConfByte & notshebangByte
		if ListItem == "-h" or ListItem =="-help" or ListItem =="--help":
			usage()
			exit()		
					
	# Get all invalid logs as per options mentioned from passed source location
	#getfilelist(sys.argv[1])
	getfilelist(sourcename)
			
	# Log header
	print "%s | %s | %s" %("\nFileName".ljust(nMaxLen),"Line#".ljust(6),"Code")
	print str('-'*120)
	
	# Display all Invalid logs with full file path, line number and invalid statement
	for no in xrange(len(MatchList)):
		if no != 3: 
		# Do not display printf statements
			Display(MatchList[no],no)	
			
	# Display Code conformance summary
	totallog = 0
	print "Code Conformance Summary:\n"	
	for no in xrange(len(MatchList)):
		totallog = totallog + len(MatchList[no][RESULT])
		strTemp = "Invalid "
		matchIndex = MatchList[no][CHECK]
		if no == 2:
			strTemp = "Warning "
		if no == 3:
			strTemp = "Warning "
			
		print strTemp + "%s count	: %s \n" %(MatchList[no][CHECK], len(MatchList[no][RESULT]))
	
	# Display total invalid logs count
	print "\nTotal Invalid log count	: %s" %(str(totallog))
	

if __name__ == '__main__':
	
	main()


	
