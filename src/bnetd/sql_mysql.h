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
#ifndef INCLUDED_SQL_MYSQL_PROTOS
#define INCLUDED_SQL_MYSQL_PROTOS

#include "storage.h"

extern t_sql_engine sql_mysql;
extern int mysql_query_low(char *que);
extern void mysql_escape_string_low(char *escape, const char *from, int len);

#endif /* INCLUDED_SQL_MYSQL_PROTOS */
