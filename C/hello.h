//
//  hello.h
//  LuaSrc
//
//  Created by TTc on 2017/3/3.
//  Copyright © 2017年 LuaSrc. All rights reserved.
//

#ifndef hello_h
#define hello_h

#include "util.h"
#include <stdio.h>

void test_coroutine(lua_State *L,const char *root);

void test_compiler(lua_State *L,const char *root);

void test_hello(lua_State *L,const char *root);

#endif /* hello_h */
