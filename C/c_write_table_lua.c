//
//  c_write_table_lua.c
//  LuaSrc
//
//  Created by TTc on 15-2-9.
//  Copyright (c) 2015年 LuaSrc. All rights reserved.
//

#include "c_write_table_lua.h"

static void
write_table(lua_State *L)
{
    lua_newtable(L);              /* 新建table并放入堆栈。对于lua空间中没有table的情况可以使用lua_newtable新建一个table；如果是写入已有table，则应该使用lua_getglobal将数据从lua空间读入虚拟堆栈 */
    
    lua_pushstring(L, "r");       /* 将要写入的键压入堆栈 */
    lua_pushnumber(L, (double)10); /* 将要写入的值压入堆栈 */
    lua_settable(L, -3);          /* 执行table的写入，函数的第二个参数是table在虚拟堆栈中的位置 */
    
    lua_pushstring(L, "b");       /* 重复三次，一共写入了"r", "g", "b" 三个成员 */
    lua_pushnumber(L, (double)12);
    lua_settable(L, -3);
    
    lua_pushstring(L, "g");
    lua_pushnumber(L, (double)15);
    lua_settable(L, -3);
    
    lua_setglobal(L, "background"); /* 最后将新table写入lua全局命名空间 */
}


void
test_c_write_table_lua(lua_State *L,const char *root)
{
    char path [128];
    memset(path, '\0', sizeof(path));
    strcat(path, root);
    strcat(path, "c_write_table_lua.lua");
    printf("fname => %s \n",path);

    write_table(L);

    load(L, path);

}






