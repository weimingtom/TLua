/*
	
	Example:
	$gcc -Wall test_extend_lua_by_c.c util.c  c_function_to_extend_lua.c c_module_to_extend_lua.c c_userdata_to_extend_lua.c -llua -lm -ldl -o  test_extend_lua_by_c
	$./test_extend_lua_by_c
	--extend lua by c function--
	libsin: -0.98803162409286
	mysin:  -0.98803162409286
	--extend lua by c module--
	mysininmodule:  -0.98803162409286
	i:      2
	i:      4
	i:      6
	--test upvalue in c function--
	10
	hi
	table: 0x1c153f0
	10      hi      table: 0x1c153f0        3
	--test userdata--
	array(1000)
	1000
	true
	false

	Analyse:
    Lua 中调用   C 中的数据结构 变量 函数

*/
#include <stdio.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "util.h"
#include "test_extend_lua_by_c.h"
#include "c_function_to_extend_lua.h"



void open_c_func_to_extend_lua(lua_State *L)
{
//	lua_pushcfunction(L,l_sin);
//    printf("stack size = %d\n",lua_gettop(L)); /*stack size = 0*/
//
//	lua_setglobal(L,"mysin");
////
//    printf("stack size = %d\n",lua_gettop(L)); /*stack size = 0*/
//
//	lua_pushcfunction(L,l_sin);
//    printf("stack size = %d\n",lua_gettop(L)); /*stack size = 0*/

	/*open c module*/
	luaopen_mylib(L);
	//luaopen_arraylib(L);

	/*register c function to use userdata with gc*/
	//luaopen_dir(L);
}

void test_extend_lua_by_c(lua_State *L){
    luaL_openlibs(L);
    open_c_func_to_extend_lua(L);

    char *fname = "/Users/administrator/Desktop/studySpace/ReadingLua/LuaSrc/LuaSrc/script/TestLuaExtendedbyC.lua";

    load(L, fname);
}
int main00009(void)
{
	lua_State *L = luaL_newstate();

	luaL_openlibs(L);
	open_c_func_to_extend_lua(L);
    char *fname = "/Users/administrator/Desktop/studySpace/studyC/LuaBriC/LuaBriC/C/TestLuaExtendedbyC.lua";

	load(L,fname); /*load confile file*/

	return 0;
}
