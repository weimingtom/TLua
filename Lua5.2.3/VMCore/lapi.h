/*
** $Id: lapi.h,v 2.7.1.1 2013/04/12 18:48:47 roberto Exp $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/
/*          Lua  C 语言接口API
 
 Lua设计的初衷之一就为了最好的和宿主系统相结合。它是一门嵌入式语言,所以必须提供和宿主系 统交互的API 。这些API以C函数的形式提供,大多数实现在lapi.c中。API直接以lua为前缀,可供C编写的程序库直接调用。
 
 以上这些就构成了让Lua运转起来的最小代码集合
*/
#ifndef lapi_h
#define lapi_h


#include "llimits.h"
#include "lstate.h"

#define api_incr_top(L)   {L->top++; api_check(L, L->top <= L->ci->top, \
				"stack overflow");}

#define adjustresults(L,nres) \
    { if ((nres) == LUA_MULTRET && L->ci->top < L->top) L->ci->top = L->top; }

#define api_checknelems(L,n)	api_check(L, (n) < (L->top - L->ci->func), \
				  "not enough elements in the stack")


#endif
