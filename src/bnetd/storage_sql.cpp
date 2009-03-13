#include "stdafx.h";
/*
 * Copyright (C) 2002 TheUndying
 * Copyright (C) 2002 zap-zero
 * Copyright (C) 2002,2003 Mihai RUSU (dizzy@roedu.net)
 * Copyright (C) 2002 Zzzoom
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "common/setup_before.h"
#ifdef WITH_SQL
#include <stdio.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#include "compat/strdup.h"
#include "compat/strcasecmp.h"
#include "compat/strncasecmp.h"
#include "compat/strtoul.h"

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "common/eventlog.h"
#include "prefs.h"
#include "common/util.h"

#define CLAN_INTERNAL_ACCESS
#define TEAM_INTERNAL_ACCESS
#include "team.h"
#include "account.h"
#include "connection.h"
#include "clan.h"
#undef TEAM_INTERNAL_ACCESS
#undef CLAN_INTERNAL_ACCESS
#include "common/tag.h"
#include "common/xalloc.h"
#include "common/flags.h"
#include "sql_dbcreator.h"
#include "storage_sql.h"
#ifdef WITH_SQL_MYSQL
#include "sql_mysql.h"
#endif
#ifdef WITH_SQL_PGSQL
#include "sql_pgsql.h"
#endif
#ifdef WITH_SQL_SQLITE3
#include "sql_sqlite3.h"
#endif
#ifdef WITH_SQL_ODBC
#include "sql_odbc.h"
#endif
#include "common/elist.h"
#include "attr.h"
#include "attrgroup.h"
#include "common/setup_after.h"

#define CURRENT_DB_VERSION 150

#define DB_MAX_ATTRKEY	128
#define DB_MAX_ATTRVAL  180
#define DB_MAX_TAB	64

#define SQL_UID_FIELD		"uid"
#define STORAGE_SQL_DEFAULT_UID	0

#define SQL_ON_DEMAND	1

static int sql_init(const char *);
static int sql_close(void);
static unsigned sql_read_maxuserid(void);
static t_storage_info *sql_create_account(char const *);
static t_storage_info *sql_get_defacct(void);
static int sql_free_info(t_storage_info *);
static int sql_read_attrs(t_storage_info *, t_read_attr_func, void *);
static t_attr *sql_read_attr(t_storage_info *, const char *);
static int sql_write_attrs(t_storage_info *, const attrs_type&);
static int sql_read_accounts(int,t_read_accounts_func, void *);
static t_storage_info * sql_read_account(const char *,unsigned);
static int sql_cmp_info(t_storage_info *, t_storage_info *);
static const char *sql_escape_key(const char *);
static int sql_load_clans(t_load_clans_func cb);
static int sql_write_clan(void *data);
static int sql_remove_clan(int clantag);
static int sql_remove_clanmember(int);
static int sql_load_teams(t_load_teams_func cb);
static int sql_write_team(void *data);
static int sql_remove_team(unsigned int teamid);

t_storage storage_sql = {
    sql_init,
    sql_close,
    sql_read_maxuserid,
    sql_create_account,
    sql_get_defacct,
    sql_free_info,
    sql_read_attrs,
    sql_write_attrs,
    sql_read_attr,
    sql_read_accounts,
    sql_read_account,
    sql_cmp_info,
    sql_escape_key,
    sql_load_clans,
    sql_write_clan,
    sql_remove_clan,
    sql_remove_clanmember,
    sql_load_teams,
    sql_write_team,
    sql_remove_team
};

static t_sql_engine *sql = NULL;
static unsigned int defacct;

const static char *tablesCap[] = { "BNET", "Record","Team", NULL };
#ifndef SQL_ON_DEMAND
const static char *tables[] = { "BNET", "Record", "profile", "friend", "Team", NULL };

static const char *_db_add_tab(const char *tab, const char *key)
{
    static char nkey[DB_MAX_ATTRKEY];

    strncpy(nkey, tab, sizeof(nkey) - 1);
    nkey[strlen(nkey) + 1] = '\0';
    nkey[strlen(nkey)] = '_';
    strncpy(nkey + strlen(nkey), key, sizeof(nkey) - strlen(nkey));
    return nkey;
}

#endif				/* SQL_ON_DEMAND */

static int _db_get_tab(const char *key, char **ptab, char **pcol)
{
    static char tab[DB_MAX_ATTRKEY];
    static char col[DB_MAX_ATTRKEY];

    strncpy(tab, key, DB_MAX_TAB - 1);
    tab[DB_MAX_TAB - 1] = 0;

    if (!strchr(tab, '_'))
	return -1;
	
    *(strchr(tab, '_')) = 0;
    strncpy(col, key + strlen(tab) + 1, DB_MAX_TAB - 1);
    col[DB_MAX_TAB - 1] = 0;
    /* return tab and col as 2 static buffers */

	const char **tmp;
	for(tmp=tablesCap;*tmp;tmp++)
	{
		if(strcasecmp(tab,*tmp)==0)
		{
			strcpy(tab,*tmp);
			break;
		}
	}

	*ptab = tab;
    *pcol = col;
    return 0;
}

static int sql_init(const char *dbpath)
{
    char *tok, *path, *tmp, *p;
    const char *dbhost = NULL;
    const char *dbname = NULL;
    const char *dbuser = NULL;
    const char *dbpass = NULL;
    const char *driver = NULL;
    const char *dbport = NULL;
    const char *dbsocket = NULL;
    const char *def = NULL;

    path = xstrdup(dbpath);
    tmp = path;
    while ((tok = strtok(tmp, ";")) != NULL)
    {
	tmp = NULL;
	if ((p = strchr(tok, '=')) == NULL)
	{
	    eventlog(eventlog_level_error, __FUNCTION__, "invalid storage_path, no '=' present in token");
	    xfree((void *) path);
	    return -1;
	}
	*p = '\0';
	if (strcasecmp(tok, "host") == 0)
	    dbhost = p + 1;
	else if (strcasecmp(tok, "mode") == 0)
	    driver = p + 1;
	else if (strcasecmp(tok, "name") == 0)
	    dbname = p + 1;
	else if (strcasecmp(tok, "port") == 0)
	    dbport = p + 1;
	else if (strcasecmp(tok, "socket") == 0)
	    dbsocket = p + 1;
	else if (strcasecmp(tok, "user") == 0)
	    dbuser = p + 1;
	else if (strcasecmp(tok, "pass") == 0)
	    dbpass = p + 1;
	else if (strcasecmp(tok, "default") == 0)
	    def = p + 1;
	else
	    eventlog(eventlog_level_warn, __FUNCTION__, "unknown token in storage_path : '%s'", tok);
    }

    if (driver == NULL)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "no mode specified");
	xfree((void *) path);
	return -1;
    }

    if (def == NULL)
	defacct = STORAGE_SQL_DEFAULT_UID;
    else
	defacct = atoi(def);

    do
    {
#ifdef WITH_SQL_MYSQL
	if (strcasecmp(driver, "mysql") == 0)
	{
	    sql = &sql_mysql;
	    if (sql->init(dbhost, dbport, dbsocket, dbname, dbuser, dbpass))
	    {
		eventlog(eventlog_level_error, __FUNCTION__, "got error init db");
		sql = NULL;
		xfree((void *) path);
		return -1;
	    }
	    break;
	}
#endif				/* WITH_SQL_MYSQL */
#ifdef WITH_SQL_PGSQL
	if (strcasecmp(driver, "pgsql") == 0)
	{
	    sql = &sql_pgsql;
	    if (sql->init(dbhost, dbport, dbsocket, dbname, dbuser, dbpass))
	    {
		eventlog(eventlog_level_error, __FUNCTION__, "got error init db");
		sql = NULL;
		xfree((void *) path);
		return -1;
	    }
	    break;
	}
#endif				/* WITH_SQL_PGSQL */
#ifdef WITH_SQL_SQLITE3
	if (strcasecmp(driver, "sqlite3") == 0)
	{
	    sql = &sql_sqlite3;
	    if (sql->init(NULL, 0, NULL, dbname, NULL, NULL))
	    {
		eventlog(eventlog_level_error, __FUNCTION__, "got error init db");
		sql = NULL;
		xfree((void *) path);
		return -1;
	    }
	    break;
	}
#endif				/* WITH_SQL_SQLITE3 */
#ifdef WITH_SQL_ODBC
	if (strcasecmp(driver, "odbc") == 0)
	{
	    sql = &sql_odbc;
	    if (sql->init(dbhost, dbport, dbsocket, dbname, dbuser, dbpass))
	    {
		eventlog(eventlog_level_error, __FUNCTION__, "got error init db");
		sql = NULL;
		xfree((void *) path);
		return -1;
	    }
	    break;
	}
#endif				/* WITH_SQL_ODBC */
	eventlog(eventlog_level_error, __FUNCTION__, "no driver found for '%s'", driver);
	xfree((void *) path);
	return -1;
    }
    while (0);

    xfree((void *) path);

    sql_dbcreator(sql);

    return 0;
}

static int sql_close(void)
{
    if (sql == NULL)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "sql not initilized");
	return -1;
    }

    sql->close();
    sql = NULL;
    return 0;
}

static unsigned sql_read_maxuserid(void)
{
    t_sql_res *result;
    t_sql_row *row;
    long maxuid;

    if (sql == NULL)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "sql not initilized");
	return 0;
    }

    if ((result = sql->query_res("SELECT max(uid) FROM BNET")) == NULL) {
	eventlog(eventlog_level_error, __FUNCTION__, "error trying query: \"SELECT max(uid) FROM BNET\"");
	return 0;
    }

    row = sql->fetch_row(result);
    if (row == NULL || row[0] == NULL)
    {
	sql->free_result(result);
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL max");
	return 0;
    }

    maxuid = atol(row[0]);
    sql->free_result(result);
    if (maxuid < 0)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "got invalid maxuserid");
	return 0;
    }

    return maxuid;
}

static t_storage_info *sql_create_account(char const *username)
{
    char query[1024];
    t_sql_res *result = NULL;
    t_sql_row *row;
    int uid = maxuserid + 1;
    char str_uid[32];
    t_storage_info *info;

    if (!sql)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return NULL;
    }

    sprintf(str_uid, "%u", uid);

	string strName(username);
	replace_all(strName,"'","\\'");
    sprintf(query, "SELECT count(*) FROM BNET WHERE acct_username='%s'", strName.c_str());
    if ((result = sql->query_res(query)) != NULL)
    {
	int num;

	row = sql->fetch_row(result);
	if (row == NULL || row[0] == NULL)
	{
	    sql->free_result(result);
	    eventlog(eventlog_level_error, __FUNCTION__, "got NULL count");
	    return NULL;
	}
	num = atol(row[0]);
	sql->free_result(result);
	if (num > 0)
	{
	    eventlog(eventlog_level_error, __FUNCTION__, "got existant username");
	    return NULL;
	}
    } else
    {
	eventlog(eventlog_level_error, __FUNCTION__, "error trying query: \"%s\"", query);
	return NULL;
    }

    info = xmalloc(sizeof(t_sql_info));
    *((unsigned int *) info) = uid;
    sprintf(query, "DELETE FROM BNET WHERE uid = '%s';", str_uid);
    sql->query(query);
    sprintf(query, "INSERT INTO BNET (uid) VALUES('%s');", str_uid);
    if (sql->query(query))
    {
	eventlog(eventlog_level_error, __FUNCTION__, "user insert failed");
	xfree((void *) info);
	return NULL;
    }

    sprintf(query, "DELETE FROM profile WHERE uid = '%s';", str_uid);
    sql->query(query);
    sprintf(query, "INSERT INTO profile (uid) VALUES('%s');", str_uid);
    if (sql->query(query))
    {
	eventlog(eventlog_level_error, __FUNCTION__, "user insert failed");
	xfree((void *) info);
	return NULL;
    }

    sprintf(query, "DELETE FROM Record WHERE uid = '%s';", str_uid);
    sql->query(query);
    sprintf(query, "INSERT INTO Record (uid) VALUES('%s');", str_uid);
    if (sql->query(query))
    {
	eventlog(eventlog_level_error, __FUNCTION__, "user insert failed");
	xfree((void *) info);
	return NULL;
    }

    sprintf(query, "DELETE FROM friend WHERE uid = '%s';", str_uid);
    sql->query(query);
    sprintf(query, "INSERT INTO friend (uid) VALUES('%s');", str_uid);
    if (sql->query(query))
    {
	eventlog(eventlog_level_error, __FUNCTION__, "user insert failed");
	xfree((void *) info);
	return NULL;
    }

    return info;
}

static int sql_read_attrs(t_storage_info * info, t_read_attr_func cb, void *data)
{
#ifndef SQL_ON_DEMAND
    char query[1024];
    t_sql_res *result = NULL;
    t_sql_row *row;
    char **tab;
    unsigned int uid;

    if (!sql)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return -1;
    }

    if (info == NULL)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL storage info");
	return -1;
    }

    if (cb == NULL)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL callback");
	return -1;
    }

    uid = *((unsigned int *) info);

    for (tab = tables; *tab; tab++)
    {
	sprintf(query, "SELECT * FROM %s WHERE uid='%u'", *tab, uid);

//      eventlog(eventlog_level_trace, __FUNCTION__, "query: \"%s\"",query);

	if ((result = sql->query_res(query)) != NULL && sql->num_rows(result) == 1 && sql->num_fields(result) > 1)
	{
	    unsigned int i;
	    t_sql_field *fields, *fentry;

	    if ((fields = sql->fetch_fields(result)) == NULL)
	    {
		eventlog(eventlog_level_error, __FUNCTION__, "could not fetch the fields");
		sql->free_result(result);
		return -1;
	    }

	    if (!(row = sql->fetch_row(result)))
	    {
		eventlog(eventlog_level_error, __FUNCTION__, "could not fetch row");
		sql->free_fields(fields);
		sql->free_result(result);
		return -1;
	    }

	    for (i = 0, fentry = fields; *fentry; fentry++, i++)
	    {			/* we have to skip "uid" */
		char *output;
		/* we ignore the field used internally by sql */
		if (strcmp(*fentry, SQL_UID_FIELD) == 0)
		    continue;

//              eventlog(eventlog_level_trace, __FUNCTION__, "read key (step1): '%s' val: '%s'", _db_add_tab(*tab, *fentry), unescape_chars(row[i]));
		if (row[i] == NULL)
		    continue;	/* its an NULL value sql field */

//              eventlog(eventlog_level_trace, __FUNCTION__, "read key (step2): '%s' val: '%s'", _db_add_tab(*tab, *fentry), unescape_chars(row[i]));
		if (cb(_db_add_tab(*tab, *fentry), (output = unescape_chars(row[i])), data))
		    eventlog(eventlog_level_error, __FUNCTION__, "got error from callback on UID: %u", uid);
		if (output)
		    xfree((void *) output);
//              eventlog(eventlog_level_trace, __FUNCTION__, "read key (final): '%s' val: '%s'", _db_add_tab(*tab, *fentry), unescape_chars(row[i]));
	    }

	    sql->free_fields(fields);
	}
	if (result)
	    sql->free_result(result);
    }
#endif				/* SQL_ON_DEMAND */
    return 0;
}

static t_attr *sql_read_attr(t_storage_info * info, const char *key)
{
#ifdef SQL_ON_DEMAND
    char query[1024];
    t_sql_res *result = NULL;
    t_sql_row *row;
    char *tab, *col;
    unsigned int uid;
    t_attr    *attr;

    if (!sql)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return NULL;
    }

    if (info == NULL)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL storage info");
	return NULL;
    }

    if (key == NULL)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL key");
	return NULL;
    }

    uid = *((unsigned int *) info);

    if (_db_get_tab(key, &tab, &col) < 0)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "error from db_get_tab");
	return NULL;
    }

    sprintf(query, "SELECT %s FROM %s WHERE " SQL_UID_FIELD " = %d", col, tab, uid);
    if ((result = sql->query_res(query)) == NULL)
	return NULL;

    if (sql->num_rows(result) != 1)
    {
//      eventlog(eventlog_level_debug, __FUNCTION__, "wrong numer of rows from query (%s)", query);
	sql->free_result(result);
	return NULL;
    }

    if (!(row = sql->fetch_row(result)))
    {
	eventlog(eventlog_level_error, __FUNCTION__, "could not fetch row");
	sql->free_result(result);
	return NULL;
    }

    if (row[0] == NULL)
    {
//      eventlog(eventlog_level_debug, __FUNCTION__, "NULL value from query (%s)", query);
	sql->free_result(result);
	return NULL;
    }

    attr = attr_create(key, row[0]);
    sql->free_result(result);

    return  attr;
#else
    return NULL;
#endif				/* SQL_ON_DEMAND */
}

/* write ONLY dirty attributes */
int sql_write_attrs(t_storage_info * info, const attrs_type&attrs)
{
    char query[1024];
    char escape[DB_MAX_ATTRVAL * 2 + 1];	/* sql docs say the escape can take a maximum of double original size + 1 */
    char safeval[DB_MAX_ATTRVAL];
    char *p, *tab, *col;
    t_attr *attr;
    t_hlist *curr;
    unsigned int uid;

    if (!sql)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return -1;
    }

    if (info == NULL)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL sql info");
	return -1;
    }


    uid = *((unsigned int *) info);

	BEGIN(_attr,attrs)
	{
		attr=*_attr;

	if (!attr_get_dirty(attr))
	    continue;		/* save ONLY dirty attributes */

	if (attr_get_key(attr) == NULL) {
	    eventlog(eventlog_level_error, __FUNCTION__, "found NULL key in attributes list");
	    continue;
	}

	if (attr_get_val(attr) == NULL)	{
	    eventlog(eventlog_level_error, __FUNCTION__, "found NULL value in attributes list");
	    continue;
	}

	if (_db_get_tab(attr_get_key(attr), &tab, &col) < 0) {
	    eventlog(eventlog_level_error, __FUNCTION__, "error from db_get_tab");
	    continue;
	}

	strncpy(safeval, attr_get_val(attr), DB_MAX_ATTRVAL - 1);
	safeval[DB_MAX_ATTRVAL - 1] = 0;
	for (p = safeval; *p; p++)
	    if (*p == '\'')	/* value shouldn't contain ' */
		*p = '"';

	sql->escape_string(escape, safeval, strlen(safeval));

	strcpy(query, "UPDATE ");
	strncat(query, tab, 64);
	strcat(query, " SET ");
	strncat(query, col, 64);
	strcat(query, "='");
	strcat(query, escape);
	strcat(query, "' WHERE uid='");
	sprintf(query + strlen(query), "%u", uid);
	strcat(query, "'");

//      eventlog(eventlog_level_trace, "db_set", "update query: %s", query);

	if (sql->query(query) || !sql->affected_rows()) {
	    char query2[512];

//	    eventlog(eventlog_level_debug, __FUNCTION__, "trying to insert new column %s", col);
	    strcpy(query2, "ALTER TABLE ");
	    strncat(query2, tab, DB_MAX_TAB);
	    strcat(query2, " ADD COLUMN ");
	    strncat(query2, col, DB_MAX_TAB);
	    strcat(query2, " VARCHAR(128);");

//          eventlog(eventlog_level_trace, __FUNCTION__, "alter query: %s", query2);
	    sql->query(query2);

	    /* try query again */
//          eventlog(eventlog_level_trace, "db_set", "retry insert query: %s", query);
	    if (sql->query(query) || !sql->affected_rows()) {
		// Tried everything, now trying to insert that user to the table for the first time
		sprintf(query2, "INSERT INTO %s (uid,%s) VALUES ('%u','%s')", tab, col, uid, escape);
//              eventlog(eventlog_level_error, __FUNCTION__, "update failed so tried INSERT for the last chance");
		if (sql->query(query2))
		{
		    eventlog(eventlog_level_error, __FUNCTION__, "could not INSERT attribute '%s'->'%s'", attr_get_key(attr), attr_get_val(attr));
		    continue;
		}
	    }
	}

	attr_clear_dirty(attr);
    }

    return 0;
}

static int sql_read_accounts(int flag,t_read_accounts_func cb, void *data)
{
    char query[1024];
    t_sql_res *result = NULL;
    t_sql_row *row;
    t_storage_info *info;

    if (!sql)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return -1;
    }

    if (cb == NULL)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "get NULL callback");
	return -1;
    }

    /* don't actually load anything here if ST_FORCE is not set as SQL is indexed */
    if (!FLAG_ISSET(flag,ST_FORCE)) return 1;

    strcpy(query, "SELECT uid FROM BNET");
    if ((result = sql->query_res(query)) != NULL)
    {
	if (sql->num_rows(result) <= 1)
	{
	    sql->free_result(result);
	    return 0;		/* empty user list */
	}

	while ((row = sql->fetch_row(result)) != NULL)
	{
	    if (row[0] == NULL)
	    {
		eventlog(eventlog_level_error, __FUNCTION__, "got NULL uid from db");
		continue;
	    }

	    if ((unsigned int) atoi(row[0]) == defacct)
		continue;	/* skip default account */

	    info = xmalloc(sizeof(t_sql_info));
	    *((unsigned int *) info) = atoi(row[0]);
	    cb(info, data);
	}
	sql->free_result(result);
    } else
    {
	eventlog(eventlog_level_error, __FUNCTION__, "error query db (query:\"%s\")", query);
	return -1;
    }

    return 0;
}

static t_storage_info * sql_read_account(const char *name, unsigned uid)
{
    char query[1024];
    t_sql_res *result = NULL;
    t_sql_row *row;
    t_storage_info *info;

    if (!sql)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return NULL;
    }

    /* SELECT uid from BNET WHERE uid=x sounds stupid, I agree but its a clean
     * way to check for account existence by an uid */
    if (name) 
	{
		string strName(name);
		replace_all(strName,"'","\\'");
		sprintf(query, "SELECT uid FROM BNET WHERE acct_username='%s'", strName.c_str());
	}
    else
	sprintf(query, "SELECT uid FROM BNET WHERE uid=%u", uid);
    result = sql->query_res(query);
    if (!result) {
	eventlog(eventlog_level_error, __FUNCTION__, "error query db (query:\"%s\")", query);
	return NULL;
    }

    if (sql->num_rows(result) < 1)
    {
        sql->free_result(result);
        return NULL;	/* empty user list */
    }

    row = sql->fetch_row(result);
    if (!row) {
	/* could not fetch row, this should not happen */
	sql->free_result(result);
	return NULL;
    }

    if (row[0] == NULL)
	/* empty UID field */
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL uid from db");
    else if ((unsigned int) atoi(row[0]) == defacct);
    /* skip default account */
    else {
	info = xmalloc(sizeof(t_sql_info));
	*((unsigned int *) info) = atoi(row[0]);
	sql->free_result(result);
	return info;
    }

    sql->free_result(result);
    return NULL;
}

static int sql_cmp_info(t_storage_info * info1, t_storage_info * info2)
{
    return *((unsigned int *) info1) != *((unsigned int *) info2);
}

static int sql_free_info(t_storage_info * info)
{
    if (info)
	xfree((void *) info);

    return 0;
}

static t_storage_info *sql_get_defacct(void)
{
    t_storage_info *info;

    info = xmalloc(sizeof(t_sql_info));
    *((unsigned int *) info) = defacct;

    return info;
}

static const char *sql_escape_key(const char *key)
{
    const char *newkey = key;
    char *p;
    int idx;

    for(idx = 0, p = (char *)newkey; *p; p++, idx++)
	if ((*p < '0' || *p > '9') && (*p < 'a' || *p > 'z') && (*p < 'A' || *p > 'Z')) {
	    newkey = xstrdup(key);
	    p = (char *)(newkey + idx);
	    *(p++) = '_';
	    for(; *p; p++)
		if ((*p < '0' || *p > '9') && (*p < 'a' || *p > 'z') && (*p < 'A' || *p > 'Z'))
		    *p = '_';
	    break;
	}

    return newkey;
}

int db_get_version(void)
{
    t_sql_res *result = NULL;
    t_sql_row *row;
    int version = 0;

    if ((result = sql->query_res("SELECT value FROM pvpgn WHERE name = 'db_version'")) == NULL)
	return 0;
    if (sql->num_rows(result) == 1 && (row = sql->fetch_row(result)) != NULL && row[0] != NULL)
	version = atoi(row[0]);

    sql->free_result(result);

    return version;
}

void _sql_db_set_version(int version)
{
    char query[1024];

    sprintf(query, "UPDATE pvpgn SET value = '%d' WHERE name = 'db_version';", version);
    if (sql->query(query) || sql->affected_rows() < 1)
    {
	sql->query("CREATE TABLE pvpgn (name varchar(128) NOT NULL PRIMARY KEY, value varchar(255));");
	sprintf(query, "INSERT INTO pvpgn (name, value) VALUES('db_version', '%d');", version);
	sql->query(query);
    }
}

static int sql_load_clans(t_load_clans_func cb)
{
    t_sql_res *result;
    t_sql_res *result2;
    t_sql_row *row;
    t_sql_row *row2;
    char query[1024];
    t_clan *clan;
    int member_uid;
    t_clanmember *member;

    if (!sql)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return -1;
    }

    if (cb == NULL)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "get NULL callback");
	return -1;
    }

    strcpy(query, "SELECT cid, short, name, motd, creation_time FROM clan WHERE cid > 0");
    if ((result = sql->query_res(query)) != NULL)
    {
	if (sql->num_rows(result) < 1)
	{
	    sql->free_result(result);
	    return 0;		/* empty clan list */
	}

	while ((row = sql->fetch_row(result)) != NULL)
	{
	    if (row[0] == NULL)
	    {
		eventlog(eventlog_level_error, __FUNCTION__, "got NULL cid from db");
		continue;
	    }

	    clan = (t_clan*)xmalloc(sizeof(t_clan));

	    if (!(clan->clanid = atoi(row[0])))
	    {
		eventlog(eventlog_level_error, __FUNCTION__, "got bad cid");
		sql->free_result(result);
		return -1;
	    }

	    clan->clantag = atoi(row[1]);

	    clan->clanname = xstrdup(row[2]);
	    clan->clan_motd = xstrdup(row[3]);
	    clan->creation_time = atoi(row[4]);
	    clan->created = 1;
	    clan->modified = 0;
	    clan->channel_type = prefs_get_clan_channel_default_private();
	    clan->members = list_create();

	    sprintf(query, "SELECT uid, status, join_time FROM clanmember WHERE cid='%u'", clan->clanid);

	    if ((result2 = sql->query_res(query)) != NULL)
	    {
		if (sql->num_rows(result2) >= 1)
		    while ((row2 = sql->fetch_row(result2)) != NULL)
		    {
			member = (t_clanmember*)xmalloc(sizeof(t_clanmember));
			if (row2[0] == NULL)
			{
			    eventlog(eventlog_level_error, __FUNCTION__, "got NULL uid from db");
			    continue;
			}
			if (!(member_uid = atoi(row2[0])))
			    continue;
			if (!(member->memberacc = accountlist_find_account_by_uid(member_uid)))
			{
			    eventlog(eventlog_level_error, __FUNCTION__, "cannot find uid %u", member_uid);
			    xfree((void *) member);
			    continue;
			}
			member->status = atoi(row2[1]);
			member->join_time = atoi(row2[2]);
			member->clan	  = clan;

			if ((member->status == CLAN_NEW) && (time(NULL) - member->join_time > prefs_get_clan_newer_time() * 3600))
			{
			    member->status = CLAN_PEON;
			    clan->modified = 1;
			    member->modified = 1;
			}

			list_append_data(clan->members, member);

			account_set_clanmember((t_account*)member->memberacc, member);
			eventlog(eventlog_level_trace, __FUNCTION__, "added member: uid: %i status: %c join_time: %u", member_uid, member->status + '0', (unsigned) member->join_time);
		    }
		sql->free_result(result2);
		cb(clan);
	    } else
		eventlog(eventlog_level_error, __FUNCTION__, "error query db (query:\"%s\")", query);
	}

	sql->free_result(result);
    } else
    {
	eventlog(eventlog_level_error, __FUNCTION__, "error query db (query:\"%s\")", query);
	return -1;
    }
    return 0;
}

static int sql_write_clan(void *data)
{
    char query[1024];
    t_sql_res *result;
    t_sql_row *row;
    t_elem *curr;
    t_clanmember *member;
    t_clan *clan = (t_clan *) data;
    int num;

    if (!sql)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return -1;
    }

    sprintf(query, "SELECT count(*) FROM clan WHERE cid='%u'", clan->clanid);
    if ((result = sql->query_res(query)) != NULL)
    {
	row = sql->fetch_row(result);
	if (row == NULL || row[0] == NULL)
	{
	    sql->free_result(result);
	    eventlog(eventlog_level_error, __FUNCTION__, "got NULL count");
	    return -1;
	}
	num = atol(row[0]);
	sql->free_result(result);
	if (num < 1)
	    sprintf(query, "INSERT INTO clan (cid, short, name, motd, creation_time) VALUES('%u', '%d', '%s', '%s', '%u')", clan->clanid, clan->clantag, clan->clanname, clan->clan_motd, (unsigned) clan->creation_time);
	else
	    sprintf(query, "UPDATE clan SET short='%d', name='%s', motd='%s', creation_time='%u' WHERE cid='%u'", clan->clantag, clan->clanname, clan->clan_motd, (unsigned) clan->creation_time, clan->clanid);
	if (sql->query(query) < 0)
	{
	    eventlog(eventlog_level_error, __FUNCTION__, "error trying query: \"%s\"", query);
	    return -1;
	}
	LIST_TRAVERSE(clan->members, curr)
	{
	    unsigned int uid;

	    if (!(member = (t_clanmember*)elem_get_data(curr)))
	    {
		eventlog(eventlog_level_error, __FUNCTION__, "got NULL elem in list");
		continue;
	    }
	    if ((member->status == CLAN_NEW) && (time(NULL) - member->join_time > prefs_get_clan_newer_time() * 3600))
	    {
		member->status = CLAN_PEON;
		member->modified = 1;
	    }
	    if (member->modified)
	    {
		uid = account_get_uid((t_account*)member->memberacc);
		sprintf(query, "SELECT count(*) FROM clanmember WHERE uid='%u'", uid);
		if ((result = sql->query_res(query)) != NULL)
		{
		    row = sql->fetch_row(result);
		    if (row == NULL || row[0] == NULL)
		    {
			sql->free_result(result);
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL count");
			return -1;
		    }
		    num = atol(row[0]);
		    sql->free_result(result);
		    if (num < 1)
			sprintf(query, "INSERT INTO clanmember (cid, uid, status, join_time) VALUES('%u', '%u', '%d', '%u')", clan->clanid, uid, member->status, (unsigned) member->join_time);
		    else
			sprintf(query, "UPDATE clanmember SET cid='%u', status='%d', join_time='%u' WHERE uid='%u'", clan->clanid, member->status, (unsigned) member->join_time, uid);
		    if (sql->query(query) < 0)
		    {
			eventlog(eventlog_level_error, __FUNCTION__, "error trying query: \"%s\"", query);
			return -1;
		    }
		} else
		{
		    eventlog(eventlog_level_error, __FUNCTION__, "error trying query: \"%s\"", query);
		    return -1;
		}
		member->modified = 0;
	    }
	}
    } else
    {
	eventlog(eventlog_level_error, __FUNCTION__, "error trying query: \"%s\"", query);
	return -1;
    }

    return 0;
}

static int sql_remove_clan(int clantag)
{
    char query[1024];
    t_sql_res *result;
    t_sql_row *row;

    if (!sql)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return -1;
    }

    sprintf(query, "SELECT cid FROM clan WHERE short = '%d'", clantag);
    if (!(result = sql->query_res(query)))
    {
	eventlog(eventlog_level_error, __FUNCTION__, "error query db (query:\"%s\")", query);
	return -1;
    }

    if (sql->num_rows(result) != 1)
    {
	sql->free_result(result);
	return -1;		/*clan not found or found more than 1 */
    }

    if ((row = sql->fetch_row(result)))
    {
	unsigned int cid = atoi(row[0]);
	sprintf(query, "DELETE FROM clanmember WHERE cid='%u'", cid);
	if (sql->query(query) != 0)
	    return -1;
	sprintf(query, "DELETE FROM clan WHERE cid='%u'", cid);
	if (sql->query(query) != 0)
	    return -1;
    }

    sql->free_result(result);

    return 0;
}

static int sql_remove_clanmember(int uid)
{
    char query[1024];

    if (!sql)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return -1;
    }

    sprintf(query, "DELETE FROM clanmember WHERE uid='%u'", uid);
    if (sql->query(query) != 0)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "error trying query: \"%s\"", query);
	return -1;
    }

    return 0;
}

static int sql_load_teams(t_load_teams_func cb)
{
    t_sql_res *result;
    t_sql_row *row;
    char query[1024];
    t_team *team;
    int i;

    if (!sql)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return -1;
    }

    if (cb == NULL)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "get NULL callback");
	return -1;
    }

    strcpy(query, "SELECT teamid, size, clienttag, lastgame, member1, member2, member3, member4, wins,losses, xp, level, rank FROM arrangedteam WHERE teamid > 0");
    if ((result = sql->query_res(query)) != NULL)
    {
	if (sql->num_rows(result) < 1)
	{
	    sql->free_result(result);
	    return 0;		/* empty team list */
	}

	while ((row = sql->fetch_row(result)) != NULL)
	{
	    if (row[0] == NULL)
	    {
		eventlog(eventlog_level_error, __FUNCTION__, "got NULL teamid from db");
		continue;
	    }

	    team = (t_team*)xmalloc(sizeof(t_team));

	    if (!(team->teamid = atoi(row[0])))
	    {
		eventlog(eventlog_level_error, __FUNCTION__, "got bad teamid");
		sql->free_result(result);
		return -1;
	    }

	    team->size = atoi(row[1]);
	    team->clienttag=tag_str_to_uint(row[2]);
	    team->lastgame = strtoul(row[3],NULL,10);
	    team->teammembers[0] = strtoul(row[4],NULL,10);
	    team->teammembers[1] = strtoul(row[5],NULL,10);
	    team->teammembers[2] = strtoul(row[6],NULL,10);
	    team->teammembers[3] = strtoul(row[7],NULL,10);
	    
	    for (i=0; i<MAX_TEAMSIZE;i++)
	    {
	       if (i<team->size)
	       {
		    if ((team->teammembers[i]==0))
		    {
	    		eventlog(eventlog_level_error,__FUNCTION__,"invalid team data: too few members");
	    		xfree((void *)team);
	    		goto load_team_failure;
		    }
	       }
	       else
	       {
	   	    if ((team->teammembers[i]!=0))
		    {
	    		eventlog(eventlog_level_error,__FUNCTION__,"invalid team data: too many members");
	    		xfree((void *)team);
	    		goto load_team_failure;
		    }

	       }
	       team->members[i] = NULL;
	    }
	
	    team->wins = atoi(row[8]);
	    team->losses = atoi(row[9]);
	    team->xp = atoi(row[10]);
	    team->level = atoi(row[11]);
	    team->rank = atoi(row[12]);

	    eventlog(eventlog_level_trace,__FUNCTION__,"succesfully loaded team %u",team->teamid);
	    cb(team);
	    load_team_failure:
	    ;    
	}

	sql->free_result(result);
    } else
    {
	eventlog(eventlog_level_error, __FUNCTION__, "error query db (query:\"%s\")", query);
	return -1;
    }
    return 0;
}

static int sql_write_team(void *data)
{
    char query[1024];
    t_sql_res *result;
    t_sql_row *row;
    t_team *team = (t_team *) data;
    int num;

    if (!sql)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return -1;
    }

    sprintf(query, "SELECT count(*) FROM arrangedteam WHERE teamid='%u'", team->teamid);
    if ((result = sql->query_res(query)) != NULL)
    {
	row = sql->fetch_row(result);
	if (row == NULL || row[0] == NULL)
	{
	    sql->free_result(result);
	    eventlog(eventlog_level_error, __FUNCTION__, "got NULL count");
	    return -1;
	}
	num = atol(row[0]);
	sql->free_result(result);
	if (num < 1)
	    sprintf(query, "INSERT INTO arrangedteam (teamid, size, clienttag, lastgame, member1, member2, member3, member4, wins,losses, xp, level, rank) VALUES('%u', '%c', '%s', '%u', '%u', '%u', '%u', '%u', '%d', '%d', '%d', '%d', '%d')", team->teamid,team->size+'0',clienttag_uint_to_str(team->clienttag),(unsigned int)team->lastgame,team->teammembers[0],team->teammembers[1],team->teammembers[2],team->teammembers[3],team->wins,team->losses,team->xp,team->level,team->rank);
	else
	    sprintf(query, "UPDATE arrangedteam SET size='%c', clienttag='%s', lastgame='%u', member1='%u', member2='%u', member3='%u', member4='%u', wins='%d', losses='%d', xp='%d', level='%d', rank='%d' WHERE teamid='%u'",team->size+'0',clienttag_uint_to_str(team->clienttag),(unsigned int)team->lastgame,team->teammembers[0],team->teammembers[1],team->teammembers[2],team->teammembers[3],team->wins,team->losses,team->xp,team->level,team->rank,team->teamid);
	if (sql->query(query) < 0)
	{
	    eventlog(eventlog_level_error, __FUNCTION__, "error trying query: \"%s\"", query);
	    return -1;
	}
    } else
    {
	eventlog(eventlog_level_error, __FUNCTION__, "error trying query: \"%s\"", query);
	return -1;
    }

    return 0;
}

static int sql_remove_team(unsigned int teamid)
{
    char query[1024];

    if (!sql)
    {
	eventlog(eventlog_level_error, __FUNCTION__, "sql layer not initilized");
	return -1;
    }

    sprintf(query, "DELETE FROM arrangedteam WHERE teamid='%u'", teamid);
    if (sql->query(query) != 0)
        return -1;

    return 0;
}

#endif				/* WITH_SQL */
