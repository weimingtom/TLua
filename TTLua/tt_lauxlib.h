//
//  tt_lauxlib.h
//  LuaSrc
//
//  Created by TTc on 15/11/20.
//  Copyright © 2015年 LuaSrc. All rights reserved.
//

#ifndef tt_lauxlib_h
#define tt_lauxlib_h

#include <stdio.h>

#define luaL_newlibtable(L,l) \
    lua_createtable(L,0,sizeof(l)/sizeof((l)[0])-1)

#define luaL_newlib(L,l) (luaL_newlibtable(L,l),luaL_setfuncs(L,l,0))
#endif /* tt_lauxlib_h */
