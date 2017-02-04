/*
** $Id: ltable.c,v 2.72.1.1 2013/04/12 18:48:47 roberto Exp $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/


/*
** Implementation of tables (aka arrays, objects, or hash tables).
** Tables keep its elements in two parts: an array part and a hash part.
** Non-negative integer keys are all candidates to be kept in the array
** part. The actual size of the array is the largest `n' such that at
** least half the slots between 0 and n are in use.
** Hash uses a mix of chained scatter table with Brent's variation.
** A main invariant of these tables is that, if an element is not
** in its main position (i.e. the `original' position that its hash gives
** to it), then the colliding element is in its own main position.
** Hence even when the load factor reaches 100%, performance remains good.
*/

/*
            table表处理库
*/
#include <string.h>

#define ltable_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lvm.h"


/*
** max size of array part is 2^MAXBITS
*/
#if LUAI_BITSINT >= 32
#define MAXBITS		30
#else
#define MAXBITS		(LUAI_BITSINT-2)
#endif

#define MAXASIZE	(1 << MAXBITS)


#define hashpow2(t,n)		(gnode(t, lmod((n), sizenode(t))))

#define hashstr(t,str)		hashpow2(t, (str)->tsv.hash)
#define hashboolean(t,p)	hashpow2(t, p)


/*
** for some types, it is better to avoid modulus by power of 2, as
** they tend to have many 2 factors.
*/
#define hashmod(t,n)	(gnode(t, ((n) % ((sizenode(t)-1)|1))))


#define hashpointer(t,p)	hashmod(t, IntPoint(p))


#define dummynode		(&dummynode_)

#define isdummy(n)		((n) == dummynode)

static const Node dummynode_ = {
  {NILCONSTANT},  /* value */
  {{NILCONSTANT, NULL}}  /* key */
};


/*
** hash for lua_Numbers 数字类型的哈希值
 *1:当数字类型为键,且没有置入数组部分时,我们需要对它们取哈希值,便于放进哈希表内。
 *2:在5.1之前,对数字类型的哈希运算非常简单。就是对其占用的内存块的数据,按整数形式相加
 *3:  5.2后 代码修改如下实现
 *4: 这里把数字哈希算法函数提取出来,变成了用户可配置的函数 luai_hashnum.这个函数默认定义在llimits.h中。
*/
static Node *hashnum (const Table *t, lua_Number n) {
  int i;
  luai_hashnum(i, n);
  if (i < 0) {
    if (cast(unsigned int, i) == 0u - i)  /* use unsigned to avoid overflows */
      i = 0;  /* handle INT_MIN */
    i = -i;  /* must be a positive value */
  }
  return hashmod(t, i);
}



/*
** returns the `main' position of an element in a table (that is, the index
** of its hash value) 惰性计算
*/
static Node *mainposition (const Table *t, const TValue *key) {
  switch (ttype(key)) {
    case LUA_TNUMBER:
      return hashnum(t, nvalue(key));
    case LUA_TLNGSTR: {
      TString *s = rawtsvalue(key);
      if (s->tsv.extra == 0) {  /* no hash? */
        s->tsv.hash = luaS_hash(getstr(s), s->tsv.len, s->tsv.hash);
        s->tsv.extra = 1;  /* now it has its hash */
      }
      return hashstr(t, rawtsvalue(key));
    }
    case LUA_TSHRSTR:
      return hashstr(t, rawtsvalue(key));
    case LUA_TBOOLEAN:
      return hashboolean(t, bvalue(key));
    case LUA_TLIGHTUSERDATA:
      return hashpointer(t, pvalue(key));
    case LUA_TLCF:
      return hashpointer(t, fvalue(key));
    default:
      return hashpointer(t, gcvalue(key));
  }
}


/*
** returns the index for `key' if `key' is an appropriate key to live in
** the array part of the table, -1 otherwise.
*/
static int arrayindex (const TValue *key) {
  if (ttisnumber(key)) {
    lua_Number n = nvalue(key);
    int k;
    lua_number2int(k, n);
    if (luai_numeq(cast_num(k), n))
      return k;
  }
  return -1;  /* `key' did not match some condition */
}


/*
** returns the index of a `key' for table traversals. First goes all
** elements in the array part, then elements in the hash part. The
** beginning of a traversal is signaled by -1.
**  支持检索一个死键,查询这个死键的下一个键位
*/
static int findindex (lua_State *L, Table *t, StkId key) {
  int i;
  if (ttisnil(key)) return -1;  /* first iteration */
  i = arrayindex(key);
  if (0 < i && i <= t->sizearray)  /* is `key' inside array part? */
    return i-1;  /* yes; that's the index (corrected to C) */
  else {
    Node *n = mainposition(t, key);
    for (;;) {  /* check whether `key' is somewhere in the chain */
      /* key may be dead already, but it is ok to use it in `next' */
      if (luaV_rawequalobj(gkey(n), key) ||
            (ttisdeadkey(gkey(n)) && iscollectable(key) &&
             deadvalue(gkey(n)) == gcvalue(key))) {
        i = cast_int(n - gnode(t, 0));  /* key index in hash table */
        /* hash elements are numbered after array ones */
        return i + t->sizearray;
      }
      else n = gnext(n);
      if (n == NULL)
        luaG_runerror(L, "invalid key to " LUA_QL("next"));  /* key not found */
    }
  }
}

/*   table的迭代
 1:在Lua中,并没有提供一个自维护状态的迭代器。而是给出了一个luaH_next方法。
 2:传入上一个键,返回下一个键值对;这就是luaH_next所要实现的。
 3:它尝试返回传入的key在数组部分中的下一个非空值。当超出数组部分后,则检索哈希表中的对应位置, 
   并返回哈希表中对应节点在储存空间分布上的下一个节点处的键值对。
 4:在大多数其它语言中,遍历一个无序集合的过程中,通常不允许对这个集合做任何修改。
   即使允许,也可能产生未定义的结果。
 5:在Lua中也一样,遍历一个table的过程中,向这个table插入一个新键这个行为, 
    将无法预测后续的遍历行为
 6:但是,Lua却允许在遍历过程中,修改table中已存在的键对应的值
 7:由于Lua没有显式的从table中删除键的操作,只能对不需要的键设为空。
 8:一旦在迭代过程中发生垃圾收集,对键值赋值为空的操作就有可能导致垃圾收集过程中把这个键值对
  标记为死键
 9:所以,在next操作中,从上一个键定位下一个键时,需要支持检索一个死键,查询这个死键的下一个键位。
   具体代码见findindex的实现
 */
int luaH_next (lua_State *L, Table *t, StkId key) {
  int i = findindex(L, t, key);  /* find original element */
  for (i++; i < t->sizearray; i++) {  /* try first array part */
    if (!ttisnil(&t->array[i])) {  /* a non-nil value? */
      setnvalue(key, cast_num(i+1));
      setobj2s(L, key+1, &t->array[i]);
      return 1;
    }
  }
  for (i -= t->sizearray; i < sizenode(t); i++) {  /* then hash part */
    if (!ttisnil(gval(gnode(t, i)))) {  /* a non-nil value? */
      setobj2s(L, key, gkey(gnode(t, i)));
      setobj2s(L, key+1, gval(gnode(t, i)));
      return 1;
    }
  }
  return 0;  /* no more elements */
}


/*
** {=============================================================
** Rehash
** ==============================================================
*/
/*
 计算完毕得到这个数组每个元素的数据之后,也就是得到了落在每个范围内的数据数量,
 来开始计算怎样才能最大限度的使用这部分空间.这个算法由函数computesize实现:
*/
static int computesizes (int nums[], int *narray) {
  int i;
  int twotoi;  /* 2^i */
  int a = 0;  /* number of elements smaller than 2^i */
  int na = 0;  /* number of elements to go to array part */
  int n = 0;  /* optimal size for array part */
  for (i = 0, twotoi = 1; twotoi/2 < *narray; i++, twotoi *= 2) {
    if (nums[i] > 0) {
      a += nums[i];
      if (a > twotoi/2) {  /* more than half elements present? */
        n = twotoi;  /* optimal size (till now) */
        na = a;  /* all elements smaller than n will go to array part */
      }
    }
    if (a == *narray) break;  /* all elements already counted */
  }
  *narray = n;
  lua_assert(*narray/2 <= na && na <= *narray);
  return na;
}


static int countint (const TValue *key, int *nums) {
  int k = arrayindex(key);
  if (0 < k && k <= MAXASIZE) {  /* is `key' an appropriate array index? */
    nums[luaO_ceillog2(k)]++;  /* count as such */
    return 1;
  }
  else
    return 0;
}


static int numusearray (const Table *t, int *nums) {
  int lg;
  int ttlg;  /* 2^lg */
  int ause = 0;  /* summation of `nums' */
  int i = 1;  /* count to traverse all array keys */
  for (lg=0, ttlg=1; lg<=MAXBITS; lg++, ttlg*=2) {  /* for each slice */
    int lc = 0;  /* counter */
    int lim = ttlg;
    if (lim > t->sizearray) {
      lim = t->sizearray;  /* adjust upper limit */
      if (i > lim)
        break;  /* no more elements to count */
    }
    /* count elements in range (2^(lg-1), 2^lg] */
    for (; i <= lim; i++) {
      if (!ttisnil(&t->array[i-1]))
        lc++;
    }
    nums[lg] += lc;
    ause += lc;
  }
  return ause;
}


static int numusehash (const Table *t, int *nums, int *pnasize) {
  int totaluse = 0;  /* total number of elements */
  int ause = 0;  /* summation of `nums' */
  int i = sizenode(t);
  while (i--) {
    Node *n = &t->node[i];
    if (!ttisnil(gval(n))) {
      ause += countint(gkey(n), nums);
      totaluse++;
    }
  }
  *pnasize += ause;
  return totaluse;
}

/**
 *  用来初始化数组部分
 */
static void setarrayvector (lua_State *L, Table *t, int size) {
  int i;
  luaM_reallocvector(L, t->array, t->sizearray, size, TValue);
  for (i=t->sizearray; i<size; i++)
     setnilvalue(&t->array[i]);
  t->sizearray = size;
}

/**
 *  用来初始化哈希表部分
 */
static void setnodevector (lua_State *L, Table *t, int size) {
  int lsize;
  if (size == 0) {  /* no elements to hash part? */
    t->node = cast(Node *, dummynode);  /* use common `dummynode' */
    lsize = 0;
  }
  else {
    int i;
    lsize = luaO_ceillog2(size);
    if (lsize > MAXBITS)
      luaG_runerror(L, "table overflow");
    size = twoto(lsize);
    t->node = luaM_newvector(L, size, Node);
    for (i=0; i<size; i++) {
      Node *n = gnode(t, i);
      gnext(n) = NULL;
      setnilvalue(gkey(n));
      setnilvalue(gval(n));
    }
  }
  t->lsizenode = cast_byte(lsize);
  t->lastfree = gnode(t, size);  /* all positions are free */
}


void luaH_resize (lua_State *L, Table *t, int nasize, int nhsize) {
  int i;
  int oldasize = t->sizearray;
  int oldhsize = t->lsizenode;
  Node *nold = t->node;  /* save old hash ... */
  if (nasize > oldasize)  /* array part must grow? */
    setarrayvector(L, t, nasize);
  /* create new hash part with appropriate size */
  setnodevector(L, t, nhsize);
  if (nasize < oldasize) {  /* array part must shrink? */
    t->sizearray = nasize;
    /* re-insert elements from vanishing slice */
    for (i=nasize; i<oldasize; i++) {
      if (!ttisnil(&t->array[i]))
        luaH_setint(L, t, i + 1, &t->array[i]);
    }
    /* shrink array */
    luaM_reallocvector(L, t->array, oldasize, nasize, TValue);
  }
  /* re-insert elements from hash part */
  for (i = twoto(oldhsize) - 1; i >= 0; i--) {
    Node *old = nold+i;
    if (!ttisnil(gval(old))) {
      /* doesn't need barrier/invalidate cache, as entry was
         already present in the table */
      setobjt2t(L, luaH_set(L, t, gkey(old)), gval(old));
    }
  }
  if (!isdummy(nold))
    luaM_freearray(L, nold, cast(size_t, twoto(oldhsize))); /* free old array */
}


void luaH_resizearray (lua_State *L, Table *t, int nasize) {
  int nsize = isdummy(t->node) ? 0 : sizenode(t);
  luaH_resize(L, t, nasize, nsize);
}


/*
 1:rehash的主要工作是统计当前table中到底有多少有效键值对,以及决定数组部分需要开辟多少空间。
   其原则是最终数组部分的利用率需要超过50%
 2:Lua使用一个rehash函数中定义在栈上的nums数组来做这个整数键统计工作。这个数组按2的整数幂次
   来分开统计各个区段间的整数键个数。统计过程的实现见numusearray和numusehash函数。
 3:最终,computesizes函数计算出不低于50%利用率下,数组该维持多少空间。同时,还可以得到有多少
   有效键将被储存在哈希表里。
 4:根据这些统计数据,rehash函数调用luaH_resize这个api来重新调整数组部分和哈希部分的大小,
   并把不能放在数组里的键值对重新塞入哈希表。
*/

//重新分配Lua表空间的代码,比较复杂,可能会涉及到数组和Hash两部分数据的迁移.它的入口函数是rehash函数.

/*   算法如下：
 首先分配一个位图nums,将其中的所有位置0,这个位图的意义在于:nums数组中第i个元素存放的是key在2^(i-1), 2^i之间的元素数量
 遍历lua Table中的数组部分,计算在数组部分中的元素数量,更新对应的nums数组元素数量.(numusearray函数)
 遍历lua Table中的Hash部分,因为其中也可能存放了正整数,也根据这里的正整数数量更新对应的nums数组元素数量.(numusehash函数)
 此时nums数组已经有了当前这个Table中所有正整数的分配统计,逐个遍历nums数组,获得其范围区间内所包含的整数数量大于50%的最大index,做为rehash之后的数组大小,超过这个范围的正整数,就分配到Hash部分了.(computesizes函数)
 根据上面计算得到的调整后的数组和Hash桶大小对Lua表进行调整(resize函数)
 */

/*
 对上面的伪代码算法做一下解释.Lua的设计思想,是简单高效,并且还要尽量的节省内存资源.rehash的过程,除了增大Lua表的大小以容纳新的数据之外,还希望能借此机会对原有的数组和Hash部分进行一个调整,让两部分都尽可能发挥其存储的最高容纳效率.那么,这里的标准是什么呢?希望在调整过后,数组在每一个2次方位置,其容纳的元素数量都超过了该范围的50%,能达到这个目标的话,那么Lua认为这个数组范围就发挥了最大的效率.
 */
static void rehash (lua_State *L, Table *t, const TValue *ek) {
  int nasize, na;
  int nums[MAXBITS+1];  /* nums[i] = number of keys with 2^(i-1) < k <= 2^i */
  int i;
  int totaluse;
  for (i=0; i<=MAXBITS; i++) nums[i] = 0;  /* reset counts */
  nasize = numusearray(t, nums);  /* count keys in array part */
  totaluse = nasize;  /* all those keys are integer keys */
  totaluse += numusehash(t, nums, &nasize);  /* count keys in hash part */
  /* count extra key */
  nasize += countint(ek, nums);
  totaluse++;
  /* compute new size for array part */
  na = computesizes(nums, &nasize);
  /* resize the table to new computed sizes */
  luaH_resize(L, t, nasize, totaluse - na);
}


/*   Table的构造函数 */
/* 其中setnodevector 用来初始化哈希表部分.内存管理部分则使用了 LuaM相关API */
Table *luaH_new (lua_State *L) {
  Table *t = &luaC_newobj(L, LUA_TTABLE, sizeof(Table), NULL, 0)->h;
  t->metatable = NULL;
  t->flags = cast_byte(~0);
  t->array = NULL;
  t->sizearray = 0;
  setnodevector(L, t, 0);/*用来初始化哈希表部分*/
  return t;
}

/*   Table的析构函数 */
void luaH_free (lua_State *L, Table *t) {
  if (!isdummy(t->node))
    luaM_freearray(L, t->node, cast(size_t, sizenode(t)));
  luaM_freearray(L, t->array, t->sizearray);
  luaM_free(L, t);
}


static Node *getfreepos (Table *t) {
  while (t->lastfree > t->node) {
    t->lastfree--;
    if (ttisnil(gkey(t->lastfree)))
      return t->lastfree;
  }
  return NULL;  /* could not find a free place */
}



/*
** inserts a new key into a hash table; first, check whether key's main
** position is free. If not, check whether colliding node is in its main
** position or not: if it is not, move colliding node to an empty place and
** put new key in its main position; otherwise (colliding node is in its main
** position), new key goes to an empty position.
*/
/*
 1:创建操作的API为luaH_newkey,阅读它的实现就能对整个table有一个全面的认识。
 2:它只负责在哈希表中创建出一个不存在的键,而不关数组部分的工作。
 3:因为table的数组部分操作和C语言数组没有什么不同,不需要特别处理。
 4:lua的哈希表以闭散列方式实现。每个可能的键值,在哈希表中都有一个主要位置,称作mainposition
 5:创建一个新键时,检查mainposition,若无人使用,则可以直接设置为这个新键。
 6:若之前有其它键占据了这个位置,则检查占据此位置的键的主位置是不是这里。
 7:若两者位置冲突,则利用Node结构中的next域, 以一个单向链表的形式把它们链起来;
 8:否则,新键占据这个位置,而老键更换到新位置并根据它的主键找到属于它的链的那条单向链表中上
   一个结点,重新链入。
 9:无论是哪种冲突情况,都需要在哈希表中找到一个空闲可用的结点。
    这里是在getfreepos函数中,递减lastfree域来实现的.
 10:Lua也不会在设置键位的值为nil时而回收空间,而是在预先准备好的哈希空间使用完后惰性回收;
    即在getfreepos递减到哈希空间头时,做一次rehash操作
 */
TValue *luaH_newkey (lua_State *L, Table *t, const TValue *key) {
  Node *mp;
  if (ttisnil(key)) luaG_runerror(L, "table index is nil");
  else if (ttisnumber(key) && luai_numisnan(L, nvalue(key)))
    luaG_runerror(L, "table index is NaN");
  mp = mainposition(t, key);
  if (!ttisnil(gval(mp)) || isdummy(mp)) {  /* main position is taken? */
    Node *othern;
    Node *n = getfreepos(t);  /* get a free place */
    if (n == NULL) {  /* cannot find a free place? */
      rehash(L, t, key);  /* grow table */
      /* whatever called 'newkey' take care of TM cache and GC barrier */
      return luaH_set(L, t, key);  /* insert key into grown table */
    }
    lua_assert(!isdummy(n));
    othern = mainposition(t, gkey(mp));
    if (othern != mp) {  /* is colliding node out of its main position? */
      /* yes; move colliding node into free position */
      while (gnext(othern) != mp) othern = gnext(othern);  /* find previous */
      gnext(othern) = n;  /* redo the chain with `n' in place of `mp' */
      *n = *mp;  /* copy colliding node into free pos. (mp->next also goes) */
      gnext(mp) = NULL;  /* now `mp' is free */
      setnilvalue(gval(mp));
    }
    else {  /* colliding node is in its own main position */
      /* new node will go into free position */
      gnext(n) = gnext(mp);  /* chain new position */
      gnext(mp) = n;
      mp = n;
    }
  }
  setobj2t(L, gkey(mp), key);
  luaC_barrierback(L, obj2gco(t), key);
  lua_assert(ttisnil(gval(mp)));
  return gval(mp);
}


/*
** search function for integers
*/
const TValue *luaH_getint (Table *t, int key) {
  /* (1 <= key && key <= t->sizearray) */
  if (cast(unsigned int, key-1) < cast(unsigned int, t->sizearray))
    return &t->array[key-1];
  else {
    lua_Number nk = cast_num(key);
    Node *n = hashnum(t, nk);
    do {  /* check whether `key' is somewhere in the chain */
      if (ttisnumber(gkey(n)) && luai_numeq(nvalue(gkey(n)), nk))
        return gval(n);  /* that's it */
      else n = gnext(n);
    } while (n);
    return luaO_nilobject;
  }
}


/*
** search function for short strings
** 短字符串查询:luaH_getstr回避逐字节的字符串比较操作
*/
const TValue *luaH_getstr (Table *t, TString *key) {
  Node *n = hashstr(t, key);
  lua_assert(key->tsv.tt == LUA_TSHRSTR);
  do {  /* check whether `key' is somewhere in the chain */
    if (ttisshrstring(gkey(n)) && eqshrstr(rawtsvalue(gkey(n)), key))
      return gval(n);  /* that's it */
    else n = gnext(n);
  } while (n);
  return luaO_nilobject;
}


/*
** main search function  table的查询操作
 1:查询操作luaH_get的实现要简单的多。
 2:当查询键为整数键且在数组范围内时,在数组部分查询;否则, 根据键的哈希值去哈希表中查询。
 3:拥有相同哈希值的冲突键值对,在哈希表中由Node的next域单向链起来,所以遍历这个链表就可以了
 4:从luaH_get的实现中可以看到,遇到短字符串查询时,就会去调用luaH_getstr回避逐字节的字符串
 比较操作。
*/
const TValue *luaH_get (Table *t, const TValue *key) {
  switch (ttype(key)) {
    case LUA_TSHRSTR: return luaH_getstr(t, rawtsvalue(key));
    case LUA_TNIL: return luaO_nilobject;
    case LUA_TNUMBER: {
      int k;
      lua_Number n = nvalue(key);
      lua_number2int(k, n);
      if (luai_numeq(cast_num(k), n)) /* index is int? */
        return luaH_getint(t, k);  /* use specialized version */
      /* else go through */
    }
    default: {
      Node *n = mainposition(t, key);
      do {  /* check whether `key' is somewhere in the chain */
        if (luaV_rawequalobj(gkey(n), key))
          return gval(n);  /* that's it */
        else n = gnext(n);
      } while (n);
      return luaO_nilobject;
    }
  }
}


/*
** beware: when using this function you probably need to check a GC
** barrier and invalidate the TM cache.
*/
TValue *luaH_set (lua_State *L, Table *t, const TValue *key) {
  const TValue *p = luaH_get(t, key);
  if (p != luaO_nilobject)
    return cast(TValue *, p);
  else return luaH_newkey(L, t, key);
}


void luaH_setint (lua_State *L, Table *t, int key, TValue *value) {
  const TValue *p = luaH_getint(t, key);
  TValue *cell;
  if (p != luaO_nilobject)
    cell = cast(TValue *, p);
  else {
    TValue k;
    setnvalue(&k, cast_num(key));
    cell = luaH_newkey(L, t, &k);
  }
  setobj2t(L, cell, value);
}

/*
 1:Lua的table的长度定义只对序列表有效.所以在实现的时候,仅需要遍历table的数组部分;
 2:只有当数组部分填满时才需要进一步的去检索哈希表.它使用二分法,来快速在哈希表中快速定位
   一个非空的整数键的位置。
*/
static int unbound_search (Table *t, unsigned int j) {
  unsigned int i = j;  /* i is zero or a present index */
  j++;
  /* find `i' and `j' such that i is present and j is not */
  while (!ttisnil(luaH_getint(t, j))) {
    i = j;
    j *= 2;
    if (j > cast(unsigned int, MAX_INT)) {  /* overflow? */
      /* table was built with bad purposes: resort to linear search */
      i = 1;
      while (!ttisnil(luaH_getint(t, i))) i++;
      return i - 1;
    }
  }
  /* now do a binary search between them */
  while (j - i > 1) {
    unsigned int m = (i+j)/2;
    if (ttisnil(luaH_getint(t, m))) j = m;
    else i = m;
  }
  return i;
}


/*
** Try to find a boundary in table `t'. A `boundary' is an integer index
** such that t[i] is non-nil and t[i+1] is nil (and 0 if t[1] is nil).
*/
int luaH_getn (Table *t) {
  unsigned int j = t->sizearray;
  if (j > 0 && ttisnil(&t->array[j - 1])) {
    /* there is a boundary in the array part: (binary) search for it */
    unsigned int i = 0;
    while (j - i > 1) {
      unsigned int m = (i+j)/2;
      if (ttisnil(&t->array[m - 1])) j = m;
      else i = m;
    }
    return i;
  }
  /* else must find a boundary in hash part */
  else if (isdummy(t->node))  /* hash part is empty? */
    return j;  /* that is easy... */
  else return unbound_search(t, j);
}



#if defined(LUA_DEBUG)

Node *luaH_mainposition (const Table *t, const TValue *key) {
  return mainposition(t, key);
}

int luaH_isdummy (Node *n) { return isdummy(n); }

#endif
