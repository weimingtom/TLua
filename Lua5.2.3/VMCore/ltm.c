/*
** $Id: ltm.c,v 2.14.1.1 2013/04/12 18:48:47 roberto Exp $
** Tag methods
** See Copyright Notice in lua.h
*/

/*
        元方法；元表处理
*/
#include <string.h>

#define ltm_c
#define LUA_CORE

#include "lua.h"

#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


static const char udatatypename[] = "userdata";
/* 类型的名字
 1:最后,有一段和元方法不太相关的代码也放在ltm模块中,
 2:在ltm.h/ltm.c中还为每个lua类型提供了字符串描述.
  它用于输出调试信息以及作为lua_typename的返回值.
   这个字符串并未在其它场合用到 所以也没有为其预生成string对象.
 3:udatatypename在这里单独并定义出来,多半出于严谨的考虑.让userdata和lightuserdata
   返回的"userdata"字符串指针保持一致.
 */
LUAI_DDEF const char *const luaT_typenames_[LUA_TOTALTAGS] = {
  "no value",
  "nil", "boolean", udatatypename, "number",
  "string", "table", "function", udatatypename, "thread",
  "proto", "upval"  /* these last two cases are used for tests only */
};

/* 元方法的优化: 元表元方法的初始化
1: 一个优化点是,不必在每次做元方法查询的时候都压入元方法的名字,
   在state初始化时,lua对这些元方法生成了字符串对象;
   这些都是全局共用的,在初始化完毕之后只可读不可写,也不能回收.
 
2: 这样,在table查询这些字符串要比我们使用lua_getfield这样的外部API要快的多。
   通过调用luaT_gettmbyobj可以获得需要的元方法
 */
void luaT_init (lua_State *L) {
  static const char *const luaT_eventname[] = {  /* ORDER TM */
    "__index", "__newindex",
    "__gc", "__mode", "__len", "__eq",
    "__add", "__sub", "__mul", "__div", "__mod",
    "__pow", "__unm", "__lt", "__le",
    "__concat", "__call"
  };
  int i;
  for (i=0; i<TM_N; i++) {
    G(L)->tmname[i] = luaS_new(L, luaT_eventname[i]);
    luaS_fix(G(L)->tmname[i]); /* never collect these names将这些字符串设置为不可回收的 */
  }
}


/*
** function to be used with macro "fasttm": optimized for absence of
** tag methods
 1: 这里,真正做查找工作的,是函数luaT_gettm,但是外面直接调用的,却是宏fasttm和宏gfasttm,
    两者的区别在于一个使用的参数是lua_State指针,一个使用的参数是global_State指针.
 
 2: 这里会做一个优化,当第一次查找表中的某个元方法并且没有找到时,会将Table中的flags成员对应的
    位做置位操作,这样下一次再来查找该表中同样的元方法时如果该位已经为1,那么直接返回NULL即可.
 

*/
const TValue *luaT_gettm (Table *events, TMS event, TString *ename) {
  const TValue *tm = luaH_getstr(events, ename);
  lua_assert(event <= TM_EQ);
  if (ttisnil(tm)) {  /* no tag method? */
    events->flags |= cast_byte(1u<<event);  /* cache this fact */
    return NULL;
  }
  else return tm;
}

/*
  通过调用 luaT_gettmbyobj 可以获得需要的元方法。
 1: 根据不同的数据类型,返回元方法的函数,只有在数据类型为Table以及udata的时候,才能拿到对象的metatable表.
 2: 其他时候是到global_State结构体的成员mt来获取的,但是这个对于其他的数据类型而言,一直是空值.
 */
const TValue *luaT_gettmbyobj (lua_State *L, const TValue *o, TMS event) {
  Table *mt;
  switch (ttypenv(o)) {
    case LUA_TTABLE:
      mt = hvalue(o)->metatable;
      break;
    case LUA_TUSERDATA:
      mt = uvalue(o)->metatable;
      break;
    default:
      mt = G(L)->mt[ttypenv(o)];
  }
  return (mt ? luaH_getstr(mt, G(L)->tmname[event]) : luaO_nilobject);
}

