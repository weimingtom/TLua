//
//  c_Call_lua.h
//  LuaSrc
//
//  Created by TTc on 15-2-9.
//  Copyright (c) 2015年 LuaSrc. All rights reserved.
//

#ifndef __LuaSrc__c_Call_lua__
#define __LuaSrc__c_Call_lua__

#include "l_util.h"

void test_c_call_lua(lua_State *L,const char *root);

double c_func(lua_State *L, double x, double y);

#endif /* defined(__LuaSrc__c_Call_lua__) */
