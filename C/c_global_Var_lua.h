//
//  c_global_Var_lua.h
//  LuaSrc
//
//  Created by TTc on 15-2-9.
//  Copyright (c) 2015年 LuaSrc. All rights reserved.
//

#ifndef __LuaSrc__c_global_Var_lua__
#define __LuaSrc__c_global_Var_lua__

#include <stdio.h>
#include "lua.h"
void get_global(lua_State *L);
void set_global(lua_State *L);
void test_global();
#endif /* defined(__LuaSrc__c_global_Var_lua__) */
