//
//  c_global_Var_lua.c
//  LuaSrc
//
//  Created by TTc on 15-2-9.
//  Copyright (c) 2015年 LuaSrc. All rights reserved.
//

#include "c_global_Var_lua.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "util.h"
void get_global(lua_State *L)
{
    int global_var1,global_var2;
    lua_getglobal(L, "global_var1"); /* 从lua的变量空间中将全局变量global_var1读取出来放入虚拟堆栈中 */
    
    global_var1 = lua_tonumber(L, -1); /* 从虚拟堆栈中读取刚才压入堆栈的变量，-1表示读取堆栈最顶端的元素 */
    printf("Read global var1 from C: %d\n", global_var1);
    lua_getglobal(L, "global_var2");
    global_var2 = lua_tonumber(L, -1);
    printf("Read global var2 from C: %d\n", global_var2);
}

void set_global(lua_State *L)
{
    lua_pushinteger(L, 9);
    lua_setglobal(L, "global_var1");
    printf("set global var from C:9\n");
}


void test_global(){
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    char *fname = "/Users/administrator/Desktop/studySpace/ReadingLua/LuaSrc/LuaSrc/script/c_global_Var_lua.lua";
    load(L, fname);
    get_global(L);
    set_global(L);
    get_global(L);
}