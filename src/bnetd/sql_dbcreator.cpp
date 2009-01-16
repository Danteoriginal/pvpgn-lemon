/*
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

#define  SQL_DBCREATOR_INTERNAL_ACCESS
#include "sql_dbcreator.h"
#undef   SQL_DBCREATOR_INTERNAL_ACCESS
#include "storage_sql.h"

#include "common/eventlog.h"
#include "common/util.h"

#include "compat/strdup.h"
#include "common/list.h"
#include "common/xalloc.h"
#include "prefs.h"
#include "common/setup_after.h"

t_elem  * curr_table  = NULL;
t_elem  * curr_column = NULL;
t_elem  * curr_cmd    = NULL;

t_db_layout * db_layout;

t_column * create_column(char * name, char * value)
{
  t_column * column;
  
  if (!(name))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"got NULL column name");
      return NULL;
    }
  
  if (!(value))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"got NULL column value");
      return NULL;
    }
  
  column = (t_column*)xmalloc(sizeof(t_column));
  column->name  = xstrdup(name);
  column->value = xstrdup(value);

  return column;
};

void dispose_column(t_column * column)
{
  if (column)
    {
      if (column->name)  xfree((void *)column->name);
      if (column->value) xfree((void *)column->value);
      xfree((void *)column);
    }
}

t_table * create_table(char * name)
{
  t_table * table;
  
  if (!(name))
    {
      eventlog(eventlog_level_error,__FUNCTION__,"got NULL table name");
      return NULL;
    }
  
  table = (t_table*)xmalloc(sizeof(t_table));
  table->name = xstrdup(name);

  table->columns = list_create();
  table->sql_commands = list_create();
  
  return table;
}

void dispose_table(t_table * table)
{
  t_elem * curr;
  t_column * column;
  char * sql_command;
  
  if (table)
    {
      if (table->name) xfree((void *)table->name);
      // free list
      if (table->columns)
        {
          LIST_TRAVERSE(table->columns,curr)
	    {
	      if (!(column = (t_column*)elem_get_data(curr)))
		{
		  eventlog(eventlog_level_error,__FUNCTION__,"found NULL entry in list");
		  continue;
		}
	      dispose_column(column);
	      list_remove_elem(table->columns,&curr);
	    }
	  
	  list_destroy(table->columns);
        }

      if (table->sql_commands)
        {
          LIST_TRAVERSE(table->sql_commands,curr)
	    {
	      if (!(sql_command = (char*)elem_get_data(curr)))
		{
		  eventlog(eventlog_level_error,__FUNCTION__,"found NULL entry in list");
		  continue;
		}
	      xfree((void *)sql_command);
	      list_remove_elem(table->sql_commands,&curr);
	    }
	  
	  list_destroy(table->sql_commands);
        }
      
      
      xfree((void *)table);
    }
}

void table_add_column(t_table * table, t_column * column)
{
  if ((table) && (column))
    {
      list_append_data(table->columns,column);
    }
}

void table_add_sql_command(t_table * table, char * sql_command)
{
  if ((table) && (sql_command))
    {
      list_append_data(table->sql_commands,sql_command);
    }
}

t_db_layout * create_db_layout()
{
  t_db_layout * db_layout;
  
  db_layout = (t_db_layout*)xmalloc(sizeof(t_db_layout));

  db_layout->tables = list_create();
  
  return db_layout;
}

void dispose_db_layout(t_db_layout * db_layout)
{
  t_elem  * curr;
  t_table * table;
 

  if (db_layout)
    {
      if (db_layout->tables)
        {
          LIST_TRAVERSE(db_layout->tables,curr)
	    {
	      if (!(table = (t_table*)elem_get_data(curr)))
		{
		  eventlog(eventlog_level_error,__FUNCTION__,"found NULL entry in list");
		  continue;
		}
	      dispose_table(table);
	      list_remove_elem(db_layout->tables,&curr);
	    }
          list_destroy(db_layout->tables);
        }
      xfree((void *)db_layout);
    }
  
}

void db_layout_add_table(t_db_layout * db_layout, t_table * table)
{
  if ((db_layout) && (table))
    {
      list_append_data(db_layout->tables,table);
    }
}

t_table * db_layout_get_first_table(t_db_layout * db_layout)
{
  t_table * table;

  curr_column = NULL;

  if (!(db_layout))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL db_layout");
    return NULL;
  }

  if (!(db_layout->tables))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"found NULL db_layout->tables");
    return NULL;
  }

  if (!(curr_table = list_get_first(db_layout->tables)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"db_layout has no tables");
    return NULL;
  }

  if (!(table = (t_table*)elem_get_data(curr_table)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL elem in list");
    return NULL;
  }

  return table;
}

t_table * db_layout_get_next_table(t_db_layout * db_layout)
{
  t_table * table;

  curr_column = NULL;

  if (!(curr_table))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL curr_table");
    return NULL;
  }

  if (!(curr_table = elem_get_next(db_layout->tables, curr_table))) return NULL;

  if (!(table = (t_table*)elem_get_data(curr_table)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL elem in list");
    return NULL;
  }

  return table;
}

t_column * table_get_first_column(t_table * table)
{
  t_column * column;

  if (!(table))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL table");
    return NULL;
  }

  if (!(table->columns))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL table->columns");
    return NULL;
  }

  if (!(curr_column = list_get_first(table->columns)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"table has no columns");
    return NULL;
  }

  if (!(column = (t_column*)elem_get_data(curr_column)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL elem in list");
    return NULL;
  }

  return column;
}

t_column * table_get_next_column(t_table * table)
{
  t_column * column;

  if (!(curr_column))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL curr_column");
    return NULL;
  }

  if (!(curr_column = elem_get_next(table->columns, curr_column))) return NULL;

  if (!(column = (t_column*)elem_get_data(curr_column)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL elem in list");
    return NULL;
  }

  return column;
}

char * table_get_first_sql_command(t_table * table)
{
  char * sql_command;

  if (!(table))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL table");
    return NULL;
  }

  if (!(table->sql_commands))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL table->sql_commands");
    return NULL;
  }

  if (!(curr_cmd = list_get_first(table->sql_commands)))
  {
    return NULL;
  }

  if (!(sql_command = (char*)elem_get_data(curr_cmd)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL elem in list");
    return NULL;
  }

  return sql_command;
}

char * table_get_next_sql_command(t_table * table)
{
  char * sql_command;

  if (!(curr_cmd))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL curr_cmd");
    return NULL;
  }

  if (!(curr_cmd = elem_get_next(table->sql_commands, curr_cmd))) return NULL;

  if (!(sql_command = (char*)elem_get_data(curr_cmd)))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL elem in list");
    return NULL;
  }

  return sql_command;
}

int load_db_layout(char const * filename)
{
  FILE * fp;
  int    lineno;
  char * line        = NULL;
  char * tmp         = NULL;
  char * table       = NULL;
  char * column      = NULL;
  char * value       = NULL;
  char * sqlcmd      = NULL;
  t_table * _table   = NULL;
  t_column * _column = NULL;

  if (!(filename))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"got NULL filename");
    return -1;
  }

  if (!(fp = fopen(filename,"r")))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"can't open sql_DB_layout file");
    return -1;
  }

  if (!(db_layout = create_db_layout()))
  {
    eventlog(eventlog_level_error,__FUNCTION__,"failed to create db_layout");
    fclose(fp);
    return -1;
  }

  for (lineno=1; (line = file_get_line(fp)) ; lineno++)
  {
    switch (line[0])
    {
      case '[':
        table = &line[1];
        if (!(tmp = strchr(table,']')))
        {
          eventlog(eventlog_level_error,__FUNCTION__,"missing ']' in line %i",lineno);
          continue;
        }
        tmp[0]='\0';
        if (_table)  db_layout_add_table(db_layout,_table);

        _table = create_table(table);

        break;
      case '"':
        if (!(_table))
        {
          eventlog(eventlog_level_error,__FUNCTION__,"found a column without previous table in line %i",lineno);
          continue;
        }
        column = &line[1];
        if (!(tmp = strchr(column,'"')))
        {
          eventlog(eventlog_level_error,__FUNCTION__,"missing '\"' at the end of column definition in line %i",lineno);
          continue;
        }
        tmp[0]='\0';
        tmp++;
        if (!(tmp = strchr(tmp,'"')))
        {
          eventlog(eventlog_level_error,__FUNCTION__,"missing default value in line %i",lineno);
          continue;
        }
        value = ++tmp;
        if (!(tmp = strchr(value,'"')))
        {
          eventlog(eventlog_level_error,__FUNCTION__,"missing '\"' at the end of default value in line %i",lineno);
          continue;
        }
        tmp[0]='\0';
        _column = create_column(column,value);
        table_add_column(_table,_column);
        _column = NULL;
        break;
      case ':':
        if (!(_table))
        {
          eventlog(eventlog_level_error,__FUNCTION__,"found a sql_command without previous table in line %i",lineno);
          continue;
        }
	if (line[1]!='"')
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"missing starting '\"' in sql_command definition on line %i",lineno);
	  continue;
	}
	sqlcmd = &line[2];
        if (!(tmp = strchr(sqlcmd,'"')))
        {
          eventlog(eventlog_level_error,__FUNCTION__,"missing ending '\"' in sql_command definition on line %i",lineno);
          continue;
        }
        tmp[0]='\0';
	table_add_sql_command(_table,xstrdup(sqlcmd));

	break;
      case '#':
        break;
      default:
        eventlog(eventlog_level_error,__FUNCTION__,"illegal starting symbol at line %i",lineno);
    }
  }
  if (_table) db_layout_add_table(db_layout,_table);

  file_get_line(NULL); // clear file_get_line buffer
  fclose(fp);
  return 0;
}

int sql_dbcreator(t_sql_engine * sql)
{
  t_table     * table;
  t_column    * column;
  char        _column[1024];
  char        query[1024];
  char        * sqlcmd;

  load_db_layout(prefs_get_DBlayoutfile());

  eventlog(eventlog_level_info, __FUNCTION__,"Creating missing tables and columns (if any)");
 
  for (table = db_layout_get_first_table(db_layout);table;table = db_layout_get_next_table(db_layout))
  {
     column = table_get_first_column(table);
     sprintf(query,"CREATE TABLE %s (%s default %s)",table->name,column->name,column->value); 
     //create table if missing
     if (!(sql->query(query)))
     {
       eventlog(eventlog_level_info,__FUNCTION__,"added missing table %s to DB",table->name);
       eventlog(eventlog_level_info,__FUNCTION__,"added missing column %s to table %s",column->name,table->name);
     }

    for (;column;column = table_get_next_column(table))
    {
      sprintf(query,"ALTER TABLE %s ADD %s DEFAULT %s",table->name,column->name,column->value);
      if (!(sql->query(query)))
      {
        eventlog(eventlog_level_info,__FUNCTION__,"added missing column %s to table %s",column->name,table->name);
/*
	sscanf(column->name,"%s",_column);
	sprintf(query,"ALTER TABLE %s ALTER %s SET DEFAULT %s",table->name,_column,column->value);
	
	// If failed, try alternate language.  (From ZSoft for sql_odbc.)
	if(sql->query(query)) {
	    sprintf(query,"ALTER TABLE %s ADD DEFAULT %s FOR %s",table->name,column->value,_column);
	    sql->query(query);
	}
	// ALTER TABLE BNET add default 'false' for auth_admin;
*/
      }
    }

    for (sqlcmd = table_get_first_sql_command(table);sqlcmd;sqlcmd = table_get_next_sql_command(table))
    {
        if (!(sql->query(sqlcmd)))
        {
           eventlog(eventlog_level_info,__FUNCTION__,"sucessfully issued: %s",sqlcmd);
        }
    }

    column = table_get_first_column(table);
    sscanf(column->name,"%s",_column); //get column name without format infos
    sprintf(query,"INSERT INTO %s (%s) VALUES (%s)",table->name,_column,column->value);
    if (!(sql->query(query)))
    {
      eventlog(eventlog_level_info,__FUNCTION__,"added missing default account to table %s",table->name);
    }

  }
  
  dispose_db_layout(db_layout);
 
  eventlog(eventlog_level_info,__FUNCTION__,"finished adding missing tables and columns");  
  return 0;
}

#endif /* WITH_SQL */
