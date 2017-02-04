//
//  test_extend_lua_by_c.h
//  LuaSrc
//
//  Created by TTc on 15-1-19.
//  Copyright (c) 2015年 LuaSrc. All rights reserved.
//

#ifndef LuaSrc_test_extend_lua_by_c_h
#define LuaSrc_test_extend_lua_by_c_h
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "util.h"
#include "c_module_to_extend_lua.h"

void test_extend_lua_by_c(lua_State *L);
#endif
