//
//  c_module_to_extend_lua.h
//  LuaSrc
//
//  Created by TTc on 15-1-19.
//  Copyright (c) 2015年 LuaSrc. All rights reserved.
//

#ifndef LuaSrc_c_module_to_extend_lua_h
#define LuaSrc_c_module_to_extend_lua_h

#include "util.h"

int luaopen_mylib(lua_State *L);
int luaopen_arraylib(lua_State *L);
int luaopen_dir(lua_State *L);
#endif
