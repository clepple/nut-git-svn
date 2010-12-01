/* upssched.h - supporting structures */

#include <parseconf.h>

#define SERIALIZE_INIT 1
#define SERIALIZE_SET  2
#define SERIALIZE_WAIT 3

/* track client connections */
typedef struct conn_s {
	int     fd;
	PCONF_CTX_t	ctx;
	struct conn_s	*next;
} conn_t;
