/* user-data.h - structures for user.c

   Copyright (C) 2001  Russell Kroll <rkroll@exploits.org>

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

typedef struct {
	char	*aclname;
	void	*next;
} acllist_t;

typedef struct {
	char	*cmd;
	void	*next;
} instcmdlist_t;

typedef struct {
	char	*action;
	void	*next;
} actionlist_t;

typedef struct {
	char	*username;
	acllist_t *firstacl;
	char	*password;
	instcmdlist_t *firstcmd;
	actionlist_t  *firstaction;
	void	*next;
} ulist_t;
