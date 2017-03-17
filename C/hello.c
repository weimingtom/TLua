//
//  hello.c
//  LuaSrc
//
//  Created by TTc on 2017/3/3.
//  Copyright © 2017年 LuaSrc. All rights reserved.
//

#include "hello.h"

void
test_hello(lua_State *L,const char *root)
{
    char path [128];
    memset(path, '\0', sizeof(path));
    strcat(path, root);
    strcat(path, "helloworld.lua");
    printf("fname => %s \n",path);
    load(L, path);
}
