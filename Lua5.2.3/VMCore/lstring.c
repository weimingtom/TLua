/*
** $Id: lstring.c,v 2.26.1.1 2013/04/12 18:48:47 roberto Exp $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/
/*
            字符串池
*/
#include <string.h>

#define lstring_c
#define LUA_CORE

#include "lua.h"

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"


/*
** Lua will use at most ~(2^LUAI_HASHLIMIT) bytes from a string to
** compute its hash
*/
#if !defined(LUAI_HASHLIMIT)
#define LUAI_HASHLIMIT		5
#endif


/*
** equality for long strings
 长字符串比较,当长度不同时,自然是不同的字符串。而长度相同时,则需要逐字节比较
*/
int luaS_eqlngstr (TString *a, TString *b) {
  size_t len = a->tsv.len;
  lua_assert(a->tsv.tt == LUA_TLNGSTR && b->tsv.tt == LUA_TLNGSTR);
  return (a == b) ||  /* same instance or... */
    ((len == b->tsv.len) &&  /* equal length and ... */
     (memcmp(getstr(a), getstr(b), len) == 0));  /* equal contents */
}

/*
** equality for strings
 *比较两个字符串是否相同,需要区分长短字符串。子类型不同自然不是相同的字符串。
 1:短字符串因为经过的内部化,所以不必比较字符串内容,而仅需要比较对象地址即可。Lua 用一个宏 eqshrstr(A,B)来 高效的实现它.
*/
int luaS_eqstr (TString *a, TString *b) {
  return (a->tsv.tt == b->tsv.tt) &&
         (a->tsv.tt == LUA_TSHRSTR ? eqshrstr(a, b) : luaS_eqlngstr(a, b));
}
/*
   1: 大量文本处理中的输入的字符串不再通过哈希内部化进入全局字符串表。
   2: 同时,使用了一个随机种子用于哈希值的计算,使攻击者无法轻易构造出拥有相同哈希值的不同字符串。
   3: 这个随机种子是在 lua_State 创建时放在全局表中的,它利用了构造状态机的内存地址随机性以及用户可配置的一个随机量同时来决定.
 */
unsigned int luaS_hash (const char *str, size_t l, unsigned int seed) {
  unsigned int h = seed ^ cast(unsigned int, l);
  size_t l1;
  size_t step = (l >> LUAI_HASHLIMIT) + 1;
  for (l1 = l; l1 >= step; l1 -= step)
    h = h ^ ((h<<5) + (h>>2) + cast_byte(str[l1 - 1]));
  return h;
}
/*
** resizes the string table
 * 当哈希表中字符串的数量(nuse 域)超过预定容量(size域)时.可以预计hash冲突必然发生.
 * 这个时候就调用luaS_resize方法把字符串表的哈希链表数组扩大,重新排列所有字符串的位置.
*/
void luaS_resize (lua_State *L, int newsize) {
  int i;
  stringtable *tb = &G(L)->strt;
  /* cannot resize while GC is traversing strings */
  luaC_runtilstate(L, ~bitmask(GCSsweepstring));
  if (newsize > tb->size) {
    luaM_reallocvector(L, tb->hash, tb->size, newsize, GCObject *);
    for (i = tb->size; i < newsize; i++) tb->hash[i] = NULL;
  }
  /* rehash */
  for (i=0; i<tb->size; i++) {
    GCObject *p = tb->hash[i];
    tb->hash[i] = NULL;
    while (p) {  /* for each node in the list */
      GCObject *next = gch(p)->next;  /* save next */
      unsigned int h = lmod(gco2ts(p)->hash, newsize);  /* new position */
      gch(p)->next = tb->hash[h];  /* chain it */
      tb->hash[h] = p;
      resetoldbit(p);  /* see MOVE OLD rule */
      p = next;
    }
  }
  if (newsize < tb->size) {
    /* shrinking slice must be empty */
    lua_assert(tb->hash[newsize] == NULL && tb->hash[tb->size - 1] == NULL);
    luaM_reallocvector(L, tb->hash, tb->size, newsize, GCObject *);
  }
  tb->size = newsize;
}
/*
** creates a new string object
 * 1: 每在Lua状态机内部创建一个string都会按C风格字符串存放,以兼容C接口;
 * 2: 这样不违背Lua自己用内存块加长度的方式储存字符串的规则;
 * 3: 在把Lua字符串传递出去和C语言做交互时,又不必做额外的转换.
**/

static TString *createstrobj (lua_State *L, const char *str, size_t l,
                              int tag, unsigned int h, GCObject **list) {
  TString *ts;
  size_t totalsize;  /* total size of TString object */
  totalsize = sizeof(TString) + ((l + 1) * sizeof(char));
  ts = &luaC_newobj(L, tag, totalsize, list, 0)->ts;
  ts->tsv.len = l;
  ts->tsv.hash = h;
  ts->tsv.extra = 0;
  memcpy(ts+1, str, l*sizeof(char));
  ((char *)(ts+1))[l] = '\0';  /* ending 0 */
  return ts;
}
/*
** creates a new short string, inserting it into string table
*/
static TString *newshrstr (lua_State *L, const char *str, size_t l,
                                       unsigned int h) {
  GCObject **list;  /* (pointer to) list where it will be inserted */
  stringtable *tb = &G(L)->strt;
  TString *s;
  if (tb->nuse >= cast(lu_int32, tb->size) && tb->size <= MAX_INT/2)
    luaS_resize(L, tb->size*2);  /* too crowded */
  list = &tb->hash[lmod(h, tb->size)];
  s = createstrobj(L, str, l, LUA_TSHRSTR, h, list);
  tb->nuse++;
  return s;
}
/*   短字符串的内部化
** checks whether short string exists and reuses it or creates a new one
 1:所有的短字符串都被内部化放在全局的字符串表中.这张表是用一个哈希表来实现.
 2:这是一个开散列的哈希表实现.一个字符串被放入字符串表的时候,先检查一下表中有没有相同的字符串.
  如果有,则复用已有的字符串;没有则创建一个新的,碰到哈希值相同的字符串,
  简单的串在同一个哈希位的链表上即可.

 3:注意 isdead(G(L),o) 行,这里需要检查表中的字符串是否是死掉的字符串
 4:这是因为Lua的垃圾收集过程是分步完成的.而向字符串池添加新字符串在任何步骤之间都可能发生.
   有可能在标记完字符串后发现有些字符串没有任何引用,
   但在下个步骤中又产生了相同的字符串导致这个字符串复活.
 5:当哈希表中字符串的数量用nuse域超过预定容量用size域时。可以预计hash冲突必然发生。
   这个时候就调用luaS_resize方法把字符串表的哈希链表数组扩大,重新排列所有字符串的位置。
 6:这个过程和Lua表的处理类似,不过里面涉及垃圾收集的一些细节
 */
static TString *internshrstr (lua_State *L, const char *str, size_t l) {
  GCObject *o;
  global_State *g = G(L);
  unsigned int h = luaS_hash(str, l, g->seed);
  for (o = g->strt.hash[lmod(h, g->strt.size)];
       o != NULL;
       o = gch(o)->next) {
    TString *ts = rawgco2ts(o);
    if (h == ts->tsv.hash &&
        l == ts->tsv.len &&
        (memcmp(str, getstr(ts), l * sizeof(char)) == 0)) {
      if (isdead(G(L), o))  /* string is dead (but was not collected yet)? */
        changewhite(o);  /* resurrect it  复活这个字符串*/
      return ts;
    }
  }
  return newshrstr(L, str, l, h);  /* not found; create a new string */
}
/*
** new string (with explicit length)
*/
TString *luaS_newlstr (lua_State *L, const char *str, size_t l) {
  if (l <= LUAI_MAXSHORTLEN)  /* short string? */
    return internshrstr(L, str, l);
  else {
    if (l + 1 > (MAX_SIZET - sizeof(TString))/sizeof(char))
      luaM_toobig(L);
    return createstrobj(L, str, l, LUA_TLNGSTR, G(L)->seed, NULL);
  }
}
/*
** new zero-terminated string
*/
TString *luaS_new (lua_State *L, const char *str) {
  return luaS_newlstr(L, str, strlen(str));
}
/*
 1: UserData在Lua中并没有太特别的地方,在储存形式上和字符串相同。
 2: 可以看成是拥有独立元表,不被内部化处理,也不需要追加'\0'的字符串。
 3: 在实现上,只是对象结构从TString换成了UData。
 4: 所以实现代码也被放在lstring.c中,其 API也以luaS为前缀。
*/
Udata *luaS_newudata (lua_State *L, size_t s, Table *e) {
  Udata *u;
  if (s > MAX_SIZET - sizeof(Udata))
    luaM_toobig(L);
  u = &luaC_newobj(L, LUA_TUSERDATA, sizeof(Udata) + s, NULL, 0)->u;
  u->uv.len = s;
  u->uv.metatable = NULL;
  u->uv.env = e;
  return u;
}
