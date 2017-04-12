//
//  main.c
//  LuaSrc
//
//  Created by TTc on 14-12-25.
//  Copyright (c) 2014年 LuaSrc. All rights reserved.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "lobject.h"

#include "util.h"
#include "hello.h"
//#include "capi_example.h"
//#include "test_extend_lua_by_c.h"
//#include "c_function_to_extend_lua.h"
//#include "c_module_to_extend_lua.h"
//C与Lua的 全局变量交互
#include "c_global_Var_lua.h"
//C中调用Lua函数
#include "c_Call_lua.h"
//Lua中调用C函数
#include "lua_Call_c.h"
//C语言中读取Lua中的 表结构 table
#include "c_read_table_lua.h"
//C语言写入数据到Lua中的 表结构 table
#include "c_write_table_lua.h"
//c模块自定义userdata 扩展到lua
#include "c_userdate_lua.h"

//json包
//#include "lua_cjson.h"


//static const char * load_config = "\
//    local config_name = ...\
//    local f = assert(io.open(config_name))\
//    local code = assert(f:read \'*a\')\
//    local function getenv(name) return assert(os.getenv(name), \'os.getenv() failed: \' .. name) end\
//    code = string.gsub(code, \'%$([%w_%d]+)\', getenv)\
//    f:close()\
//    local result = {}\
//    assert(load(code,\'=(load)\',\'t\',result))()\
//    return result\
//";

#define MAXPATH 1024



int
main(int argc, const char * argv[])
{
//    int tlcl =  (LUA_TFUNCTION | (0 << 4));
//    int tlcf =  (LUA_TFUNCTION | (1 << 4));
//    int tccl =	(LUA_TFUNCTION | (2 << 4));
//    printf("tlcl===%d \n tlcf===%d \n tccl==%d \n",tlcl,tlcf,tccl);
    const char * config_file = NULL ;
    if (argc > 1)
    {
        config_file = argv[1];
    }
    else
    {
        fprintf(stderr, "Need a config file.\n");
        return 1;
    }
   
//    int err = luaL_loadstring(L, load_config);
//    assert(err == LUA_OK);

    //luaL_requiref(L, "json", lua_cjson_new, 0);

    const char *root = config_file;

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    
    test_compiler(L, root);
//    test_hello(L,root);
    
//    test_global(L,root);
//    test_c_call_lua(L,root);
//    test_lua_Call_c(L,root);
//
//    test_c_read_table_lua(L,root);
//    test_c_write_table_lua(L,root);
//    test_c_userdate_lua(L,root);
    return 0;
    
}



