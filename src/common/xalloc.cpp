#include "stdafx.h";
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

#include "common/setup_before.h"
#ifndef XALLOC_SKIP

#ifdef HAVE_STDDEF_H
# include <stddef.h>
#endif
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
#include "common/eventlog.h"
#include "common/xalloc.h"
#define XALLOC_INTERNAL_ACCESS
#include "common/setup_after.h"
#undef XALLOC_INTERNAL_ACCESS

static t_oom_cb oom_cb = NULL;

#undef CHANNEL_WAR3XP

typedef struct s_stack{
	int sz;
	int max;
	void **stk;
} stack;

stack mem[256];
static int allocount=0;
static int deinstalled=0;

void xalloc_install()
{
	int i=0;
	for(i=0;i<256;i++)
	{
		mem[i].max=2;
		mem[i].sz=0;
		mem[i].stk=(void**)calloc(2,sizeof(void*));
	}
	allocount=0;
}

void xalloc_deinstall()
{
	int i=0,j;
	for(i=0;i<256;i++)
	{
		if(mem[i].stk)
		{
//			if(mem[i].sz>0)
//				eventlog(eventlog_level_fatal, __FUNCTION__, "size[ %d ] : %d, %d",i, mem[i].sz, mem[i].max);
			for(j=0;j<mem[i].sz;j++)
			{
				if(mem[i].stk[j])
				{
					free(mem[i].stk[j]);
				}
			}
			free(mem[i].stk);
		}
	}
	deinstalled=1;
//	eventlog(eventlog_level_fatal, __FUNCTION__, "allocate count : %d",allocount);
}

void *xmalloc_real(size_t size, const char *fn, unsigned ln)
{
	;
    void *res;
#ifndef CHANNEL_WAR3XP
	unsigned char num;
	int i,m2=16,m3=24;

allocount++;

	if(size<=8)
		num=2,size=8;
	else if(size<=12)
		num=3,size=12;
	else
	for(i=4;i<256;i+=2)
	{
		if(size<=m2)
		{
			num=i;
			size=m2;
			break;
		}
		else if(size<=m3)
		{
			num=i+1;
			size=m3;
			break;
		}
		m2<<=1;
		m3<<=1;
	}
	size+=4;
	if(mem[num].sz==0)
	{
		res = malloc(size);
		*((unsigned char*)res)=num;
		res= (void*)(((char*)res)+4);
		return res;
	}
	else
	{
		mem[num].sz--;
		res= (void *)(((char*)(mem[num].stk[mem[num].sz]))+4);
		return res;
	}

#else

    res = malloc(size);
    if (!res) {
		eventlog(eventlog_level_fatal, __FUNCTION__, "out of memory (from %s:%u)",fn,ln);
		if (oom_cb && oom_cb() && (res = malloc(size))) return res;
		abort();
    }

    return res;
#endif
}

void *xcalloc_real(size_t nmemb, size_t size, const char *fn, unsigned ln)
{    void *res;
	;
#ifndef CHANNEL_WAR3XP
	unsigned char num;
	int i;

	res=xmalloc_real(size,__FILE__,__LINE__);

	for(i=0;i<size;i++)
		*(((char*)res)+i)=0;
	return res;

#else
    res = calloc(nmemb,size);
    if (!res) {
	eventlog(eventlog_level_fatal, __FUNCTION__, "out of memory (from %s:%u)",fn,ln);
	if (oom_cb && oom_cb() && (res = calloc(nmemb,size))) return res;
	abort();
    }

    return res;
#endif
}

void *xrealloc_real(void *ptr, size_t size, const char *fn, unsigned ln)
{
	;
    void *res;
#ifndef CHANNEL_WAR3XP
	unsigned char num;
	int i;

/*	if(mem[0].max!=2)
	{
		res = realloc(ptr,size);
		if (!res) {
		eventlog(eventlog_level_fatal, __FUNCTION__, "out of memory (from %s:%u)",fn,ln);
		if (oom_cb && oom_cb() && (res = realloc(ptr,size))) return res;
		abort();
		}

		return res;
	}*/

	res=xmalloc_real(size,__FILE__,__LINE__);
	if(ptr==NULL)
	{
		return res;
	}
	if(
		*(unsigned char*)((unsigned char*)res-4)
		<=
		*(unsigned char*)((unsigned char*)ptr-4)
		)
		size=*(unsigned char*)((unsigned char*)res-4);
	else
		size=*(unsigned char*)((unsigned char*)ptr-4);

	if(size&1)
		size=3<<((size>>1)+1);
	else
		size=2<<((size>>1)+1);
//	size+=4;

	for(i=size-1;i>=0;i--)
		*(((unsigned char*)res)+i)=*(((unsigned char*)ptr)+i);
	xfree_real(ptr,__FILE__,__LINE__);
	return res;
//    void *res;
#else

    res = realloc(ptr,size);
    if (!res) {
	eventlog(eventlog_level_fatal, __FUNCTION__, "out of memory (from %s:%u)",fn,ln);
	if (oom_cb && oom_cb() && (res = realloc(ptr,size))) return res;
	abort();
    }

    return res;
#endif
}

char *xstrdup_real(const char *str, const char *fn, unsigned ln)
{
	;
    char *res;
#ifndef CHANNEL_WAR3XP
    char *out;
	unsigned char num;
	int i, size, m2=16,m3=24;

	size=strlen(str)+1;

	if(!size)
		return NULL;
	else if (!(out = (char*)xmalloc_real(size,__FILE__,__LINE__)))
        return NULL;

    strcpy((char*)out,str);
    return out;
#else
    res = strdup(str);
    if (!res) {
	eventlog(eventlog_level_fatal, __FUNCTION__, "out of memory (from %s:%u)",fn,ln);
	if (oom_cb && oom_cb() && (res = strdup(str))) return res;
	abort();
    }

    return res;
#endif
}

void xfree_real(void *ptr, const char *fn, unsigned ln)
{
	;
#ifndef CHANNEL_WAR3XP
	unsigned char num;
    if (!ptr) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL ptr (from %s:%u)",fn,ln);
	return;
    }
	if(deinstalled)
	{
		free((void*)((unsigned char*)ptr-4));
		return;
	}
	
	num=*((unsigned char*)ptr-4);
	if(mem[num].sz==mem[num].max)
	{
		void ** tmp;
		int j;
		mem[num].stk = (void**)realloc(mem[num].stk,mem[num].max*2*sizeof(void*));
		mem[num].max<<=1;
	}
	mem[num].stk[mem[num].sz]=(void*)((unsigned char*)ptr-4);
	mem[num].sz++;

	return;
#else
    if (!ptr) {
	eventlog(eventlog_level_error, __FUNCTION__, "got NULL ptr (from %s:%u)",fn,ln);
	return;
    }

    free(ptr);
#endif
}

void xalloc_setcb(t_oom_cb cb)
{
    oom_cb = cb;
}

#endif /* XALLOC_SKIP */
