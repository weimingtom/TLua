/*************************************************************************
	> File Name: l_util.h
	> Author:TTc 
	> Mail:liutianshxkernel@.gmail.com 
	> Created Time: 二  5/ 9 10:07:48 2017
 ************************************************************************/

#ifndef _L_UTIL_H
#define _L_UTIL_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "luaconf.h"



void error(lua_State *L, const char *fmt, ...);

void stack_dump(lua_State *L);

void load(lua_State *L, const char *file_name);
/*a general interface can be userd to call lua function in c*/
void call_va(lua_State *L,const char *funcname,const char *sig, ...);

#endif
