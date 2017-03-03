//
//  c_Call_lua.c
//  LuaSrc
//
//  Created by TTc on 15-2-9.
//  Copyright (c) 2015年 LuaSrc. All rights reserved.
//

#include "c_Call_lua.h"


double
c_func(lua_State *L, double x, double y)
{
    double z;
    lua_getglobal(L, "lua_func");    /* 首先将lua函数从Lua Space放入虚拟堆栈中 */
    lua_pushnumber(L, x);            /* 然后再把所需的参数入栈 */
    lua_pushnumber(L, y);
    if (lua_pcall(L, 2, 1, 0) != 0)
    {
        /* 使用pcall调用刚才入栈的函数，pcall的参数的含义为：pcall(Lua_state, 参数格式, 返回值个数, 错误处理函数所在的索引)，最后一个参数暂时先忽略 */
        error(L, "error running lua function: $s", lua_tostring(L, -1));
    }
    z = lua_tonumber(L, -1);  /* 将函数的返回值读取出来 */
    lua_pop(L, 1);  /* 将返回值弹出堆栈，将堆栈恢复到调用前的样子 */
    printf("Return from lua:%f\n", z);
    return z;
}

static void
callLua(lua_State *L)
{
    double xx = 3;
    double yy = 2;
    double zz =  c_func(L, xx, yy);
    printf("C function Return zz===>%.2f\n",zz);
}


void
test_c_call_lua(lua_State *L,const char *root)
{
    char path [128];
    memset(path, '\0', sizeof(path));
    strcat(path, root);
    strcat(path, "c_Call_lua.lua");
    printf("fname => %s \n",path);
    load(L, path);
    
    callLua(L);
}

