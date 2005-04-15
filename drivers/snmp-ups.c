/*  snmp-ups.c - NUT Meta SNMP driver (support different MIBS)
 *
 *  Based on NetSNMP API (Simple Network Management Protocol V1-2)
 *
 *  Copyright (C) 2002-2004 
 *  			Arnaud Quette <arnaud.quette@free.fr>
 *  			Dmitry Frolov <frolov@riss-telecom.ru>
 *  			J.W. Hoogervorst <jeroen@hoogervorst.net>
 *  			Niels Baggesen <niels@baggesen.net>
 *
 *  Sponsored by MGE UPS SYSTEMS <http://opensource.mgeups.com/>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* NUT SNMP common functions */
#include "snmp-ups.h"
#include "main.h"
#include "parseconf.h"

/* include all known mib2nut lookup tables */
#include "apccmib.h"
#include "ietfmib.h"
#include "mgemib.h"
#include "netvisionmib.h"

/* pointer to the Snmp2Nut lookup table */
snmp_info_t *snmp_info;

time_t lastpoll;

/* ---------------------------------------------
 * driver functions implementations
 * --------------------------------------------- */
void upsdrv_initinfo(void)
{
	snmp_info_t *su_info_p;

	upsdebugx(1, "SNMP UPS driver : entering upsdrv_initinfo()");

	dstate_setinfo("driver.version.internal", DRIVER_VERSION);
	
	/* add instant commands to the info database. */
	for (su_info_p = &snmp_info[0]; su_info_p->info_type != NULL ; su_info_p++)			
		if (su_info_p->flags & SU_TYPE_CMD)
			dstate_addcmd(su_info_p->info_type);

	/* setup handlers for instcmd and setvar functions */
	upsh.setvar = su_setvar;
	upsh.instcmd = su_instcmd;

	if (testvar("notransferoids"))
		disable_transfer_oids();

	/* initialize all other INFO_ fields from list */
	if (snmp_ups_walk(SU_WALKMODE_INIT))
		dstate_dataok();
	else		
		dstate_datastale();

	/* store timestamp */
	lastpoll = time(NULL);
}

void upsdrv_updateinfo(void)
{
	upsdebugx(1,"SNMP UPS driver : entering upsdrv_updateinfo()");
	
	/* only update every pollfreq */
	if (time(NULL) > (lastpoll + pollfreq)) {

		/* update all dynamic info fields */
		if (snmp_ups_walk(SU_WALKMODE_UPDATE))
			dstate_dataok();
		else		
			dstate_datastale();	
	
		/* store timestamp */
		lastpoll = time(NULL);
	}
}

void upsdrv_shutdown(void)
{
	/* TODO: su_shutdown_ups(); */
	
	/* replace with a proper shutdown function */
	fatalx("shutdown not supported");
}

void upsdrv_help(void)
{
  upsdebugx(1, "entering upsdrv_help");
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
  upsdebugx(1, "entering upsdrv_makevartable()");

  addvar(VAR_VALUE, SU_VAR_MIBS, "Set MIB compliance (default=ietf, allowed mge,apcc,netvision)");
  addvar(VAR_VALUE | VAR_SENSITIVE, SU_VAR_COMMUNITY, "Set community name (default=public)");
  addvar(VAR_VALUE, SU_VAR_VERSION, "Set SNMP version (default=v1, allowed v2c)");
  addvar(VAR_VALUE, SU_VAR_POLLFREQ, "Set polling frequency in seconds, to reduce network flow (default=30)");
  addvar(VAR_FLAG, "notransferoids", "Disable transfer OIDs (use on APCC Symmetras)");
}

void upsdrv_banner(void)
{
	upslogx(1,"Network UPS Tools - Multi-MIBS SNMP UPS driver %s (%s)", 
		DRIVER_VERSION, UPS_VERSION);
	
	experimental_driver = 1;
}

void upsdrv_initups(void)
{
	snmp_info_t *su_info_p;
	char model[SU_INFOSIZE];
	bool status;

	upsdebugx(1, "SNMP UPS driver : entering upsdrv_initups()");

	/* Load the SNMP to NUT translation data */
	/* read_mibconf(SU_VAR_MIBS) ? getval(SU_VAR_MIBS) : "ietf"); */
	load_mib2nut(testvar(SU_VAR_MIBS) ? getval(SU_VAR_MIBS) : "ietf");
	
	/* init SNMP library, etc... */
	nut_snmp_init(progname, device_path,
		(testvar(SU_VAR_COMMUNITY) ? getval(SU_VAR_COMMUNITY) : "public"));

	/* init polling frequency */
	if (getval(SU_VAR_POLLFREQ))
		pollfreq = atoi(getval(SU_VAR_POLLFREQ));
	else
		pollfreq = DEFAULT_POLLFREQ;
	
  	/* Get UPS Model node to see if there's a MIB */
	su_info_p = su_find_info("ups.model");
	status = nut_snmp_get_str(su_info_p->OID, model, sizeof(model));

	if (status == TRUE)
		upslogx(0, "detected %s on host %s", model, device_path);
	else
		fatalx("%s MIB wasn't found on %s", testvar(SU_VAR_MIBS) ? getval(SU_VAR_MIBS) : "ietf",
			g_snmp_sess.peername);   
}

void upsdrv_cleanup(void)
{
	nut_snmp_cleanup();
}

/* -----------------------------------------------------------
 * SNMP functions.
 * ----------------------------------------------------------- */

void nut_snmp_init(const char *type, const char *hostname, const char *community)
{  
	upsdebugx(2, "SNMP UPS driver : entering nut_snmp_init(%s, %s, %s)",
		type, hostname, community);

	/* Initialize the SNMP library */
	init_snmp(type);

	/* Initialize session */
	snmp_sess_init(&g_snmp_sess);

	g_snmp_sess.peername = xstrdup(hostname);
	g_snmp_sess.community = xstrdup(community);
	g_snmp_sess.community_len = strlen(community);
	g_snmp_sess.version = SNMP_VERSION_1;

	/* Open the session */
	SOCK_STARTUP;
	g_snmp_sess_p = snmp_open(&g_snmp_sess);	/* establish the session */
	if (g_snmp_sess_p == NULL) {
		nut_snmp_perror(&g_snmp_sess, 0, NULL, "nut_snmp_init: snmp_open");
		exit(EXIT_FAILURE);
	}
}

void nut_snmp_cleanup(void)
{
	/* close snmp session. */
	if (g_snmp_sess_p) {
		snmp_close(g_snmp_sess_p);
		g_snmp_sess_p = NULL;
	}
	SOCK_CLEANUP;
}

static int oid_compare(const char *oid_req_in, long *oid_ans, size_t oid_len)
{
	unsigned int	ctr;
	long	val;
	char	*tmp, *ptr, *oid_req;

	/* sanity check */
	if (oid_req_in[0] != '.')
		return 0;	/* failed: invalid input */

	/* keep a local copy */
	oid_req = xstrdup(oid_req_in);

	ctr = 0;
	tmp = &oid_req[1];
	ptr = strchr(tmp, '.');

	while (ptr) {
		*ptr++ = '\0';

		val = strtol(tmp, (char **) NULL, 10);

		if (val != oid_ans[ctr]) {
			free(oid_req);
			return 0;	/* failed: mismatched octet */
		}

		tmp = ptr;
		ptr = strchr(tmp, '.');

		ctr++;

		if (ctr > (oid_len - 1)) {
			free(oid_req);
			return 0;	/* failed: answer is too short */
		}
	}

	/* check the final octet */
	val = strtol(tmp, (char **) NULL, 10);

	/* done with this now */
	free(oid_req);

	if (val != oid_ans[ctr])
		return 0;	/* failed: mismatch on the last octet */

	/* answer OID must be req OID plus one more element (the index) */
	if (oid_len > (ctr + 2)) 
		return 0;	/* failed: ans is too long */

	return 1;	/* success */
}

struct snmp_pdu *nut_snmp_get(const char *OID)
{
	int status;
	struct snmp_pdu *pdu, *response = NULL;
	oid name[MAX_OID_LEN];
	size_t name_len = MAX_OID_LEN;
	static unsigned int numerr = 0;

	/* create and send request. */
	if (!read_objid(OID, name, &name_len)) {
		upslogx(LOG_ERR, "[%s] nut_snmp_get: %s: %s",
			upsname, OID, snmp_api_errstring(snmp_errno));
		return NULL;
	}

	pdu = snmp_pdu_create(SNMP_MSG_GETNEXT);
	
	if (pdu == NULL)
		fatalx("Not enough memory");
	
	snmp_add_null_var(pdu, name, name_len);

	status = snmp_synch_response(g_snmp_sess_p, pdu, &response);

	if (!response)
		return NULL;

	/* getnext can take us too far, so make sure we got what we want */
	if (!oid_compare(OID, response->variables->name, 
		response->variables->name_length)) {
		return NULL;
	}
	
	if (!((status == STAT_SUCCESS) && (response->errstat == SNMP_ERR_NOERROR)))
	{
		numerr++;

		if ((numerr == SU_ERR_LIMIT) || ((numerr % SU_ERR_RATE) == 0))
			upslogx(LOG_WARNING, "[%s] Warning: excessive poll "
				"failures, limiting error reporting",
				upsname);

		if ((numerr < SU_ERR_LIMIT) || ((numerr % SU_ERR_RATE) == 0))
			nut_snmp_perror(g_snmp_sess_p, status, response, 
				"nut_snmp_get: %s", OID);

/*		snmp_free_pdu(pdu);
		snmp_free_pdu(response);
*/		response = NULL;
	} else {
		numerr = 0;
	}

	return response;
}

bool nut_snmp_get_str(const char *OID, char *buf, size_t buf_len)
{
	size_t len = 0;
	struct snmp_pdu *pdu;

	/* zero out buffer. */
	memset(buf, 0, buf_len);

	pdu = nut_snmp_get(OID);
	if (pdu == NULL)
		return FALSE;

	switch (pdu->variables->type) {
	case ASN_OCTET_STR:
	case ASN_OPAQUE:
		len = pdu->variables->val_len > buf_len - 1 ?
			buf_len - 1 : pdu->variables->val_len;
		memcpy(buf, pdu->variables->val.string, len);
		buf[len] = '\0';
		break;
	case ASN_INTEGER:
	case ASN_GAUGE:
		len = snprintf(buf, buf_len, "%ld", *pdu->variables->val.integer);
		break;
	case ASN_TIMETICKS:
		/* convert timeticks to seconds */
		len = snprintf(buf, buf_len, "%ld", *pdu->variables->val.integer / 100);
		break;
	default:
		upslogx(LOG_ERR, "[%s] unhandled ASN 0x%x recieved from %s",
			upsname, pdu->variables->type, OID);
		return FALSE;
		break;
	}

	snmp_free_pdu(pdu);

	return TRUE;
}

bool nut_snmp_get_int(const char *OID, long *pval)
{
	struct snmp_pdu *pdu;
	long value;
	char *buf;

	pdu = nut_snmp_get(OID);
	if (pdu == NULL)
		return FALSE;

	switch (pdu->variables->type) {
	case ASN_OCTET_STR:
	case ASN_OPAQUE:
		buf = xmalloc(pdu->variables->val_len + 1);
		memcpy(buf, pdu->variables->val.string, pdu->variables->val_len);
		buf[pdu->variables->val_len] = '\0';
		value = strtol(buf, NULL, 0);
		free(buf);
		break;
	case ASN_INTEGER:
	case ASN_GAUGE:
		value = *pdu->variables->val.integer;
		break;
	case ASN_TIMETICKS:
		/* convert timeticks to seconds */
		value = *pdu->variables->val.integer / 100;
		break;
	default:
		upslogx(LOG_ERR, "[%s] unhandled ASN 0x%x recieved from %s",
			upsname, pdu->variables->type, OID);
		return FALSE;
		break;
	}

	snmp_free_pdu(pdu);

	if (pval != NULL)
		*pval = value;

	return TRUE;
}

bool nut_snmp_set(const char *OID, char type, const char *value)
{
	int status;
	bool ret = FALSE;
	struct snmp_pdu *pdu, *response = NULL;
	oid name[MAX_OID_LEN];
	size_t name_len = MAX_OID_LEN;
	char *setOID = (char *) xmalloc (strlen(OID) + 3);
	
	/* append ".0" suffix to reach the right OID */
	sprintf(setOID, "%s.0", OID);
	
	if (!read_objid(setOID, name, &name_len)) {
		upslogx(LOG_ERR, "[%s] nut_snmp_set: %s: %s",
			upsname, setOID, snmp_api_errstring(snmp_errno));
		free(setOID);
		return FALSE;
	}

	/* TODO: confirm we're setting the right OID ! */
	pdu = snmp_pdu_create(SNMP_MSG_SET);
	if (pdu == NULL)
		fatalx("Not enough memory");

	if (snmp_add_var(pdu, name, name_len, type, value)) {
		upslogx(LOG_ERR, "[%s] nut_snmp_set: %s: %s",
			upsname, OID, snmp_api_errstring(snmp_errno));
		
		free(setOID);
		return FALSE;
	}

	status = snmp_synch_response(g_snmp_sess_p, pdu, &response);

	if ((status == STAT_SUCCESS) && (response->errstat == SNMP_ERR_NOERROR))
		ret = TRUE;
	else
		nut_snmp_perror(g_snmp_sess_p, status, response,
			"nut_snmp_set: can't set %s", OID);

	snmp_free_pdu(response);
	free(setOID);
	return ret;
}

bool nut_snmp_set_str(const char *OID, const char *value)
{
	return nut_snmp_set(OID, 's', value);
}

bool nut_snmp_set_int(const char *OID, long value)
{
	char buf[SU_BUFSIZE];

	snprintf(buf, sizeof(buf), "%ld", value);
	return nut_snmp_set(OID, 'i', buf);
}

bool nut_snmp_set_time(const char *OID, long value)
{
	char buf[SU_BUFSIZE];

	snprintf(buf, SU_BUFSIZE, "%ld", value * 100);
	return nut_snmp_set(OID, 't', buf);
}

/* log descriptive SNMP error message. */
void nut_snmp_perror(struct snmp_session *sess, int status,
	struct snmp_pdu *response, const char *fmt, ...)
{
	va_list va;
	int cliberr, snmperr;
	char *snmperrstr;
	char buf[SU_LARGEBUF];

	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	if (response == NULL) {
		snmp_error(sess, &cliberr, &snmperr, &snmperrstr);
		upslogx(LOG_ERR, "[%s] %s: %s",
			upsname, buf, snmperrstr);
		free(snmperrstr);
	} else if (status == STAT_SUCCESS) {
		if (response->errstat != SNMP_ERR_NOERROR)
			upslogx(LOG_ERR, "[%s] %s: Error in packet: %s",
				upsname, buf, snmp_errstring(response->errstat));
	} else if (status == STAT_TIMEOUT) {
		upslogx(LOG_ERR, "[%s] %s: Timeout: no response from %s",
			upsname, buf, sess->peername);
	} else {
		snmp_sess_error(sess, &cliberr, &snmperr, &snmperrstr);
		upslogx(LOG_ERR, "[%s] %s: %s",
			upsname, buf, snmperrstr);
		free(snmperrstr);
	}
}

/* -----------------------------------------------------------
 * utility functions.
 * ----------------------------------------------------------- */

/* deal with APCC weirdness on Symmetras */
static void disable_transfer_oids(void)
{
	snmp_info_t *su_info_p;

	upslogx(LOG_INFO, "Disabling transfer OIDs");

	for (su_info_p = &snmp_info[0]; su_info_p->info_type != NULL ; su_info_p++) {
		if (!strcasecmp(su_info_p->info_type, "input.transfer.low")) {
			su_info_p->flags &= ~SU_FLAG_OK;
			continue;
		}

		if (!strcasecmp(su_info_p->info_type, "input.transfer.high")) {
			su_info_p->flags &= ~SU_FLAG_OK;
			continue;
		}
	}
}

/* universal function to add or update info element. */
void su_setinfo(const char *type, const char *value, int flags, int auxdata)
{
	snmp_info_t *su_info_p;
	
	upsdebugx(1, "SNMP UPS driver : entering su_setinfo(%s)", type);
			
	su_info_p = su_find_info(type);
	
	if (su_info_p->flags & SU_TYPE_CMD)
		return;

	if (strcasecmp(type, "ups.status")) {
		dstate_setinfo(type, value);
		dstate_setflags(type, flags);
		dstate_setaux(type, auxdata);
	}
}

void su_status_set(snmp_info_t *su_info_p, long value)
{	
	const char *info_value = NULL;

	upsdebugx(2, "SNMP UPS driver : entering su_status_set()");

	if ((info_value = su_find_infoval(su_info_p->oid2info, value)) != NULL)
	{
		if (strcmp(info_value, "")) {
			status_init();
			status_set(info_value);
			status_commit();
		}
	}
	/* TODO: else */
}

/* find info element definition in my info array. */
snmp_info_t *su_find_info(const char *type)
{
	snmp_info_t *su_info_p;

	for (su_info_p = &snmp_info[0]; su_info_p->info_type != NULL ; su_info_p++)
		if (!strcasecmp(su_info_p->info_type, type))
			return su_info_p;
		
	fatalx("nut_snmp_find_info: unknown info type: %s", type);
	return NULL;
}

/* Load the right snmp_info_t structure matching mib parameter */
void load_mib2nut(const char *mib)
{
	upsdebugx(2, "SNMP UPS driver : entering load_mib2nut(%s)", mib);
	
/*	read_mibconf(mib); */
	
	if (!strcmp(mib, "apcc")) {
		upsdebugx(1, "load_mib2nut: using apcc mib");
		snmp_info = &apcc_mib[0];
		OID_pwr_status = APCC_OID_POWER_STATUS;
	} else if (!strcmp(mib, "mge")) {
		upsdebugx(1, "load_mib2nut: using mge (MGE UPS SYSTEMS) mib");
		snmp_info = &mge_mib[0];
		OID_pwr_status = "";
/*		read_mibconf("mgemib"); */
       } else if (!strcmp(mib, "netvision")) {
               upsdebugx(1, "load_mib2nut: using netvision (SOCOMEC SICON UPS) mib");
               snmp_info = &netvision_mib[0];
               OID_pwr_status = "";
/*             read_mibconf("netvisionmib"); */
	} else {
		upsdebugx(1, "load_mib2nut: using ietf (RFC 1628) mib");
		snmp_info = &ietf_mib[0];
		OID_pwr_status = IETF_OID_POWER_STATUS;
/*		read_mibconf("ietfmib"); */
	}
}

/* find the OID value matching that INFO_* value */
long su_find_valinfo(info_lkp_t *oid2info, char* value)
{
	info_lkp_t *info_lkp;

	for (info_lkp = oid2info; (info_lkp != NULL) &&
		(strcmp(info_lkp->info_value, "NULL")); info_lkp++) {
			
		if (!(strcmp(info_lkp->info_value, value))) {
			upsdebugx(1, "su_find_valinfo: found %s (value: %s)",
					info_lkp->info_value, value);
			
			return info_lkp->oid_value;
		}
	}
	upsdebugx(1, "su_find_infoval: no matching INFO_* value for this OID value (%s)", value);
	return -1;
}

/* find the INFO_* value matching that OID value */
const char *su_find_infoval(info_lkp_t *oid2info, long value)
{
	info_lkp_t *info_lkp;
		
	for (info_lkp = oid2info; (info_lkp != NULL) &&
		(strcmp(info_lkp->info_value, "NULL")); info_lkp++) {
			
		if (info_lkp->oid_value == value) {
			upsdebugx(1, "su_find_infoval: found %s (value: %ld)",
					info_lkp->info_value, value);
			
			return info_lkp->info_value;
		}
	}
	upsdebugx(1, "su_find_infoval: no matching INFO_* value for this OID value (%ld)", value);
	return NULL;
}

/* walk ups variables and set elements of the info array. */
bool snmp_ups_walk(int mode)
{
	static unsigned long iterations = 0;
	snmp_info_t *su_info_p;
	bool status = FALSE;
	
	for (su_info_p = &snmp_info[0]; su_info_p->info_type != NULL ; su_info_p++) {

		/* skip instcmd. */
		if (su_info_p->info_flags & SU_TYPE_CMD) {
			upsdebugx(1, "SU_CMD_MASK => %s", su_info_p->OID);
			continue;
		}
		/* skip elements we shouldn't show. */
		if (!(su_info_p->flags & SU_FLAG_OK))
			continue;
		
		/* skip static elements in update mode. */
		if (mode == SU_WALKMODE_UPDATE && 
				su_info_p->flags & SU_FLAG_STATIC)
			continue;

		/* set default value if we cannot fetch it */
		/* and set static flag on this element. */
		if (su_info_p->flags & SU_FLAG_ABSENT) {
			if (mode == SU_WALKMODE_INIT) {
				if (su_info_p->dfl) {
					/* Set default value if we cannot fetch it from ups. */
					su_setinfo(su_info_p->info_type, su_info_p->dfl,
						su_info_p->info_flags, su_info_p->info_len);
				}
				su_info_p->flags |= SU_FLAG_STATIC;
			}
			continue;
		}

		/* check stale elements only on each PN_STALE_RETRY iteration. */
		if ((su_info_p->flags & SU_FLAG_STALE) &&
				(iterations % SU_STALE_RETRY) != 0)
			continue;

		/* ok, update this element. */
		status = su_ups_get(su_info_p);
		
		/* set stale flag if data is stale, clear if not. */
		if (status == TRUE) {
			if (su_info_p->flags & SU_FLAG_STALE) {
				upslogx(LOG_INFO, "[%s] snmp_ups_walk: data resumed for %s",
					upsname, su_info_p->info_type);
			}
			su_info_p->flags &= ~SU_FLAG_STALE;
			dstate_dataok();
		} else {
			if (mode == SU_WALKMODE_INIT) {
				/* handle unsupported vars */
				su_info_p->flags &= ~SU_FLAG_OK;
			} else	{
				if (!(su_info_p->flags & SU_FLAG_STALE)) {
					upslogx(LOG_INFO, "[%s] snmp_ups_walk: data stale for %s",
						upsname, su_info_p->info_type);
				}
				su_info_p->flags |= SU_FLAG_STALE;
				dstate_datastale();
			}
		}
	}	/* for (su_info_p... */

	iterations++;
	
	return status;	
}

bool su_ups_get(snmp_info_t *su_info_p)
{
	static char buf[SU_INFOSIZE];
	bool status;
	long value;

	upsdebugx(2, "su_ups_get: %s", su_info_p->OID);
	
	if (!strcasecmp(su_info_p->info_type, "ups.status")) {

		status = nut_snmp_get_int(su_info_p->OID, &value);
		if (status == TRUE)
		{
			su_status_set(su_info_p, value);
			upsdebugx(2, "=> value: %ld", value);
		}

		return status;
	}

	/* another special case */
	if (!strcasecmp(su_info_p->info_type, "ambient.temperature")) {
		status = nut_snmp_get_int(su_info_p->OID, &value);

		/* only do this if using the IEM sensor */
		if (!strcmp(su_info_p->OID, APCC_OID_IEM_TEMP)) {
			int	su;
			long	units;

			su = nut_snmp_get_int(APCC_OID_IEM_TEMP_UNIT, &units);

			/* no response, or units == F */
			if ((su == FALSE) || (units == TEMP_UNIT_FAHRENHEIT))
				value = (value - 32) / 1.8;
		}

		if (status == TRUE)
			dstate_setinfo("ambient.temperature", "%ld", value);

		return status;
	}			

	if (su_info_p->info_flags == 0) {
		status = nut_snmp_get_int(su_info_p->OID, &value);
		if (status == TRUE) {
			sprintf(buf, "%05.1f", value * su_info_p->info_len);
		}
	} else {
		status = nut_snmp_get_str(su_info_p->OID, buf, sizeof(buf));
	}
		
	if (status == TRUE) {
		su_setinfo(su_info_p->info_type, buf,
			su_info_p->info_flags, su_info_p->info_len);
		
		upsdebugx(2, "=> value: %s", buf);
	}

	return status;
}

/* set r/w INFO_ element to a value. */
int su_setvar(const char *varname, const char *val)
{
	snmp_info_t *su_info_p;
	bool ret;

	upsdebugx(2, "entering su_setvar()");

	su_info_p = su_find_info(varname);

	if (su_info_p == NULL || su_info_p->info_type == NULL ||
		!(su_info_p->flags & SU_FLAG_OK))
	{
		upslogx(LOG_ERR, "su_setvar: info element unavailable %s", varname);
		return STAT_SET_UNKNOWN;
	}

	if (!(su_info_p->info_flags & ST_FLAG_RW) || su_info_p->OID == NULL) {
		upslogx(LOG_ERR, "su_setvar: not writable %s", varname);
		return STAT_SET_UNKNOWN; /* STAT_SET_UNHANDLED would be better */
	}

	/* set value. */
	if (SU_TYPE(su_info_p) == SU_TYPE_STRING) {
		ret = nut_snmp_set_str(su_info_p->OID, val);
	} else {
		ret = nut_snmp_set_int(su_info_p->OID, strtol(val, NULL, 0));
	}

	if (ret == FALSE)
		upslogx(LOG_ERR, "su_setvar: cannot set value %s for %s", val, su_info_p->OID);
	else
		upsdebugx(1, "su_setvar: sucessfully set %s to \"%s\"", su_info_p->info_type, val);

	/* update info array. */
	su_setinfo(varname, val, su_info_p->info_flags, su_info_p->info_len);

	/* TODO: check su_setinfo() retcode */
	return STAT_SET_HANDLED;
}

/* process instant command and take action. */
int su_instcmd(const char *cmdname, const char *extradata)
{
	snmp_info_t *su_info_p;
	int status;

	upsdebugx(2, "entering su_instcmd()");

	su_info_p = su_find_info(cmdname);

	if ((su_info_p->info_type == NULL) || !(su_info_p->flags & SU_FLAG_OK) ||
		(su_info_p->OID == NULL))
	{
		upslogx(LOG_ERR, "su_instcmd: %s unavailable", cmdname);
		return STAT_INSTCMD_UNKNOWN;
	}

	/* set value. */
	if (su_info_p->info_flags & ST_FLAG_STRING) {
		status = nut_snmp_set_str(su_info_p->OID, su_info_p->dfl);
	} else {
		status = nut_snmp_set_int(su_info_p->OID, su_info_p->info_len);
	}

	if (status == FALSE) {
		upslogx(LOG_ERR, "su_instcmd: cannot set value for %s", cmdname);
		return STAT_INSTCMD_UNKNOWN;
	} else {
		upsdebugx(1, "su_instcmd: successfully sent command %s", cmdname);
		return STAT_INSTCMD_HANDLED;
	}	
}

/* TODO: complete rewrite */
void su_shutdown_ups(void)
{
	int sdtype = 0;
	long pwr_status;

	if (nut_snmp_get_int(OID_pwr_status, &pwr_status) == FALSE)
		fatalx("cannot determine UPS status");

	if (testvar(SU_VAR_SDTYPE))
		sdtype = atoi(getval(SU_VAR_SDTYPE));

	/* logic from newapc.c */
	switch (sdtype) {
	case 3:		/* shutdown with grace period */
		upslogx(LOG_INFO, "sending delayed power off command to UPS");
		su_instcmd("shutdown.stayoff", "0");
		break;
	case 2:		/* instant shutdown */
		upslogx(LOG_INFO, "sending power off command to UPS");
		su_instcmd("load.off", "0");
		break;
	case 1:
		/* Send a combined set of shutdown commands which can work better */
		/* if the UPS gets power during shutdown process */
		/* Specifically it sends both the soft shutdown 'S' */
		/* and the powerdown after grace period - '@000' commands */
/*		upslogx(LOG_INFO, "UPS - sending shutdown/powerdown");
		if (pwr_status == g_pwr_battery)
			su_ups_instcmd(CMD_SOFTDOWN, 0, 0);
		su_ups_instcmd(CMD_SDRET, 0, 0);
		break;
*/	
	default:
		/* if on battery... */
/*		if (pwr_status == su_find_valinfo(info_lkp_t *oid2info, "OB")) {
			upslogx(LOG_INFO,
				"UPS is on battery, sending shutdown command...");
			su_ups_instcmd(CMD_SOFTDOWN, 0, 0);
		} else {
			upslogx(LOG_INFO, "UPS is online, sending shutdown+return command...");
			su_ups_instcmd(CMD_SDRET, 0, 0);
		}
*/
		break;
	}
}

/* return 1 if usable, 0 if not */
static int parse_mibconf_args(int numargs, char **arg)
{
	bool ret;
	
	/* everything below here uses up through arg[1] */
	if (numargs < 6)
		return 0;

	/* <info type> <info flags> <info len> <OID name> <default value> <value lookup> */

	/* special case for setting some OIDs value at driver startup */
	if (!strcmp(arg[0], "init")) {
		/* set value. */
		if (!strcmp(arg[1], "str")) {
			ret = nut_snmp_set_str(arg[3], arg[4]);
		} else {
			ret = nut_snmp_set_int(arg[3], strtol(arg[4], NULL, 0));
		}
	
		if (ret == FALSE)
			upslogx(LOG_ERR, "su_setvar: cannot set value %s for %s", arg[4], arg[3]);
		else
			upsdebugx(1, "su_setvar: sucessfully set %s to \"%s\"", arg[0], arg[4]);

		return 1;
	}

	/* TODO: create the lookup table */
	upsdebugx(2, "%s, %s, %s, %s, %s, %s", arg[0], arg[1], arg[2], arg[3], arg[4], arg[5]);

	return 1;
}
/* called for fatal errors in parseconf like malloc failures */
static void mibconf_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf (*mib.conf): %s", errmsg);
}
/* load *mib.conf into an snmp_info_t structure */
void read_mibconf(char *mib)
{
	char	fn[SMALLBUF];
	PCONF_CTX	ctx;

	upsdebugx(2, "SNMP UPS driver : entering read_mibconf(%s)", mib);
	
	snprintf(fn, sizeof(fn), "%s/snmp/%s.conf", CONFPATH, mib);

	pconf_init(&ctx, mibconf_err);

	if (!pconf_file_begin(&ctx, fn))
		fatalx("%s", ctx.errmsg);

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		if (ctx.numargs < 1)
			continue;

		if (!parse_mibconf_args(ctx.numargs, ctx.arglist)) {
			unsigned int	i;
			char	errmsg[SMALLBUF];

			snprintf(errmsg, sizeof(errmsg), 
				"mib.conf: invalid directive");

			for (i = 0; i < ctx.numargs; i++)
				snprintfcat(errmsg, sizeof(errmsg), " %s", 
					ctx.arglist[i]);

			upslogx(LOG_WARNING, "%s", errmsg);
		}
	}
	pconf_finish(&ctx);		
}
