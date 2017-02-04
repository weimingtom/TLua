/*
** $Id: ltm.h,v 2.11.1.1 2013/04/12 18:48:47 roberto Exp $
** Tag methods
** See Copyright Notice in lua.h
*/

/*          元方法；元表处理
 
        API以LuaT为前缀；元表的处理在 ltm.c中
 */
#ifndef ltm_h
#define ltm_h


#include "lobject.h"


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER TM"
*/
//每个meta method对应的bit定义在ltm.h文件中
typedef enum {
  TM_INDEX,
  TM_NEWINDEX,
  TM_GC,
  TM_MODE,
  TM_LEN,
  TM_EQ,  /* last tag method with `fast' access */
  TM_ADD,
  TM_SUB,
  TM_MUL,
  TM_DIV,
  TM_MOD,
  TM_POW,
  TM_UNM,
  TM_LT,
  TM_LE,
  TM_CONCAT,
  TM_CALL,
  TM_N		/* number of elements in the enum */
} TMS;


/*
1: 这个宏用来在table被修改时,清空这组标记位,强迫重新做元方法查询。
2: 只要充当元表的table没有被修改,缺失元方法这样的查询结果就可以缓存在这组标记位中了。
3：我们可以看到gfasttm这个宏能够快速的剔除不存在的元方法。
4: 另一个优化点是,不必在每次做元方法查询的时候都压入元方法的名字。
    在lua_state初始化时,lua对这些元方法生成了字符串对象
 */
#define gfasttm(g,et,e) ((et) == NULL ? NULL : \
  ((et)->flags & (1u<<(e))) ? NULL : luaT_gettm(et, e, (g)->tmname[e]))

#define fasttm(l,et,e)	gfasttm(G(l), et, e)

#define ttypename(x)	luaT_typenames_[(x) + 1]
#define objtypename(x)	ttypename(ttypenv(x))

LUAI_DDEC const char *const luaT_typenames_[LUA_TOTALTAGS];


LUAI_FUNC const TValue *luaT_gettm (Table *events, TMS event, TString *ename);
LUAI_FUNC const TValue *luaT_gettmbyobj (lua_State *L, const TValue *o,
                                                       TMS event);
LUAI_FUNC void luaT_init (lua_State *L);

#endif
