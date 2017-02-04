/*
** $Id: lualib.h,v 1.43.1.1 2013/04/12 18:48:47 roberto Exp $
** Lua standard libraries
** See Copyright Notice in lua.h
*/

/*
 作为嵌入式语言,其实完全可以不提供任何库及函数。全部由宿主系统注入到 State 中即可。也的确有 许多系统是这么用的。但Lua的官方版本还是提供了少量必要的库。尤其是一些基础函数如 pairs 、error、 settatable、type等等,完成了语言的一些基本特性,几乎很难不使用。
 而 coroutine 、string 、table 、math 等等库,也很常用。Lua 提供了一套简洁的方案,允许你自由加载 你需要的部分,以控制最终执行文件的体积和内存的占用量。主动加载这些内建库进入 lua_State,是由在 lualib.h 中的API实现的
*/
#ifndef lualib_h
#define lualib_h

#include "lua.h"


/*     打开基础库   */
LUAMOD_API int (luaopen_base) (lua_State *L);

/*     打开协程库   */
#define LUA_COLIBNAME	"coroutine"
LUAMOD_API int (luaopen_coroutine) (lua_State *L);
/*     打开table库   */
#define LUA_TABLIBNAME	"table"
LUAMOD_API int (luaopen_table) (lua_State *L);
/*     打开IO库   */
#define LUA_IOLIBNAME	"io"
LUAMOD_API int (luaopen_io) (lua_State *L);
/*     打开操作系统相关 库   */
#define LUA_OSLIBNAME	"os"
LUAMOD_API int (luaopen_os) (lua_State *L);
/*     打开 字符串 string 库   */
#define LUA_STRLIBNAME	"string"
LUAMOD_API int (luaopen_string) (lua_State *L);
/*     打开 位操作 库   */
#define LUA_BITLIBNAME	"bit32"
LUAMOD_API int (luaopen_bit32) (lua_State *L);
/*     打开 数学函数 库   */
#define LUA_MATHLIBNAME	"math"
LUAMOD_API int (luaopen_math) (lua_State *L);
/*     打开 debug 库   */
#define LUA_DBLIBNAME	"debug"
LUAMOD_API int (luaopen_debug) (lua_State *L);
/*     打开 包管理 库   */
#define LUA_LOADLIBNAME	"package"
LUAMOD_API int (luaopen_package) (lua_State *L);


/* open all previous libraries */
LUALIB_API void (luaL_openlibs) (lua_State *L);



#if !defined(lua_assert)
#define lua_assert(x)	((void)0)
#endif


#endif
