/*
 * Copyright (C) 2002,2003 Mihai RUSU (dizzy@roedu.net)
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

#ifndef INCLUDED_STORAGE_SQL_TYPES
#define INCLUDED_STORAGE_SQL_TYPES

typedef unsigned int t_sql_info;

/* mysql and pgsql at least return a pointer to something */
#define t_sql_res void

typedef char * t_sql_row;

typedef char * t_sql_field;

typedef struct {
    int (*init)(const char *host, const char *port, const char *socket, const char *name, const char *user, const char *pass);
    int (*close)(void);
    t_sql_res * (*query_res)(const char *);
    int (*query)(const char *);
    t_sql_row * (*fetch_row)(t_sql_res *);
    void (*free_result)(t_sql_res *);
    unsigned int (*num_rows)(t_sql_res *);
    unsigned int (*num_fields)(t_sql_res *);
    unsigned int (*affected_rows)(void);
    t_sql_field * (*fetch_fields)(t_sql_res *);
    int (*free_fields)(t_sql_field *);
    void (*escape_string)(char *, const char *, int);
} t_sql_engine;

#endif /* INCLUDED_STORAGE_SQL_TYPES */

#ifndef JUST_NEED_TYPES
#ifndef INCLUDED_STORAGE_SQL_PROTOS
#define INCLUDED_STORAGE_SQL_PROTOS

#include "storage.h"

extern t_storage storage_sql;

#endif /* INCLUDED_STORAGE_SQL_PROTOS */
#endif /* JUST_NEED_TYPES */
