//
//  lua_Call_c.c
//  LuaSrc
//
//  Created by TTc on 15-2-9.
//  Copyright (c) 2015年 LuaSrc. All rights reserved.
//

#include "lua_Call_c.h"
#include "luaconf.h"
#include <math.h>

/*
 不出意外，Lua中的参数也是通过虚拟堆栈传递的。因此C函数必须自己从堆栈中读取参数。注意在Lua中调用函数时是不会做原型检查的，Lua
 代码调用C函数时传递几个参数，虚拟堆栈中就会有几个参数，因此C代码在从堆栈中读取参数的时候最好自己检查一下堆栈的大小和参数类型是
 否符合预期。这里为了简化起见我们就不做类型检查了.
 */
static int t_sin (lua_State *L)
{
    double d = lua_tonumber(L, 1);
    d = sin(d); /* 这里是C函数实现自己功能的代码 */
    lua_pushnumber(L, d);/* 在完成计算后，只需将结果重新写入虚拟堆栈即可（写入的这个值就是函数的返回值） */
    return 1;  /* 函数的返回值是函数返回参数的个数。没错，Lua函数可以有多个返回值。 */
}

/* 这个函数将C函数写入Lua的命名空间中。 */
static void
regist_func(lua_State *l)
{
    lua_pushcfunction(l, t_sin);
    lua_setglobal(l, "mysin");
}


static int
l_add(lua_State *L)
{
    double d1 = luaL_checknumber(L, 1); // 这个1代表了从左到右lua调用这个函数时传入的第一个参数 lua只有double
    double d2 = luaL_checknumber(L, 2);
    
    lua_pushnumber(L, d1 + d2);			// 返回值压入lua栈
    return 1;
}

static int
l_sub(lua_State *L)
{
    double d1 = luaL_checknumber(L, 1);
    double d2 = luaL_checknumber(L, 2);
    
    lua_pushnumber(L, d1 - d2);
    return 1;
}

int luaopen_calc(lua_State *L)
{
    luaL_checkversion(L); // 防止链接多分lua库 参考：http://blog.codingnow.com/2012/01/lua_link_bug.html
    
    luaL_Reg l[] = {
        {"add", l_add},
        {"sub", l_sub},
        {NULL, NULL}
    };
    
    luaL_newlib(L, l); // Creates a new table and registers there the functions in list l.
    return 1;
}


void
test_lua_Call_c(lua_State *L,const char *root)
{
    char path [128];
    memset(path, '\0', sizeof(path));
    strcat(path, root);
    strcat(path, "lua_Call_c.lua");
    printf("fname => %s \n",path);

    regist_func(L);
    
    load(L, path);
}

