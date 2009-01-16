/*
 * Copyright (C) 2004 Dizzy (dizzy@roedu.net)
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

#ifndef __ATTR_INCLUDED__
#define __ATTR_INCLUDED__

#include "common/elist.h"

typedef struct attr_struct {
//    const char 		*key;
//    const char 		*val;
	string key;
	string val;
    int			dirty;
//    t_hlist		link;
} t_attr;

#include "common/xalloc.h"

static inline t_attr *attr_create(const char *key, const char *val)
{
    t_attr *attr;

//    attr = (t_attr*)xmalloc(sizeof(t_attr));
    attr = new t_attr;
    attr->dirty = 0;
//    hlist_init(&attr->link);
	attr->key = key?key:"";
	boost::algorithm::to_lower(attr->key);
    attr->val = val?val:"";

    return attr;
}

static inline int attr_destroy(t_attr *attr)
{
	delete attr;
	return 0;
}

static inline int attr_get_dirty(t_attr *attr)
{
    return attr->dirty;
}

static inline void attr_clear_dirty(t_attr *attr)
{
    attr->dirty = 0;
}

static inline const char *attr_get_key(t_attr *attr)
{
    return attr->key.c_str();
}

static inline const char *attr_get_val(t_attr *attr)
{
    return attr->val.c_str();
}

static inline void attr_set_val(t_attr *attr, const char *val)
{
	attr->val = val?val:"";
}

static inline void attr_set_dirty(t_attr *attr)
{
    attr->dirty = 1;
}

#endif /* __ATTR_H_INCLUDED__ */
