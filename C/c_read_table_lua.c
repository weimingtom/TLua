//
//  c_read_table_lua.c
//  LuaSrc
//
//  Created by TTc on 15-2-9.
//  Copyright (c) 2015年 LuaSrc. All rights reserved.
//

#include "c_read_table_lua.h"


static void
read_table(lua_State *L)
{
    double resault;
    lua_getglobal(L, "background");     /* 将表从lua空间复制到虚拟堆栈（应该是仅拷贝索引，否则速度无法保证） */
    lua_pushstring(L, "b");             /* 将要读取的键压入虚拟堆栈 */
    lua_gettable(L, -2);                /* 使用lua_gettable读取table，其第二个参数为table在虚拟堆栈中的索引（-1为key，所以-2为table） */
    resault = lua_tonumber(L, -1);      /* 将读取出的结果复制到C空间 */
    lua_pop(L, 1);                      /* 将结果出栈，将堆栈恢复成调用前的样子 */
    printf("Read from lua table: %f\n", resault);
}

void
test_c_read_table_lua(){
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    
    char *fname = "/Users/administrator/Desktop/studySpace/ReadingLua/LuaSrc/LuaSrc/script/c_read_table_lua.lua";

    load(L, fname);
    
    read_table(L);
}