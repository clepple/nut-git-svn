/* stype.h - server data definitions for upsd

   Copyright (C) 2007  Arjen de Korte <arjen@de-korte.org>

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

#ifndef STYPE_H_SEEN
#define STYPE_H_SEEN 1

#include <netdb.h>

#ifndef NI_MAXHOST
#define NI_MAXHOST      1025
#endif

#ifndef NI_MAXSERV
#define NI_MAXSERV      32
#endif

typedef struct {
	char	*addr;
	char	*port;
	int	sock_fd;
	void	*next;
}	stype_t;

#endif	/* STYPE_H_SEEN */
