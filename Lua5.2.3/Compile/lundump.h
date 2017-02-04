/*
** $Id: lundump.h,v 1.39.1.1 2013/04/12 18:48:47 roberto Exp $
** load precompiled Lua chunks
** See Copyright Notice in lua.h
*/

/*       预编译 && 还原预编译的字节码
 
 为了满足某些需求,加快代码翻译的流程。还可以采用预编译的过程。把运行时编译的结果,生成为字节码。
 这个过程以及逆过程由ldump.c和lundump.h实现。其 API以luaU为前缀。
 */
#ifndef lundump_h
#define lundump_h

#include "lobject.h"
#include "lzio.h"

/* load one chunk; from lundump.c */
LUAI_FUNC Closure* luaU_undump (lua_State* L, ZIO* Z, Mbuffer* buff, const char* name);

/* make header; from lundump.c */
LUAI_FUNC void luaU_header (lu_byte* h);

/* dump one chunk; from ldump.c */
LUAI_FUNC int luaU_dump (lua_State* L, const Proto* f, lua_Writer w, void* data, int strip);

/* data to catch conversion errors */
#define LUAC_TAIL		"\x19\x93\r\n\x1a\n"

/* size in bytes of header of binary files */
#define LUAC_HEADERSIZE		(sizeof(LUA_SIGNATURE)-sizeof(char)+2+6+sizeof(LUAC_TAIL)-sizeof(char))

#endif
