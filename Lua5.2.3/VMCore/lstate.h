/*
** $Id: lstate.h,v 2.82.1.1 2013/04/12 18:48:47 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/

/*      全局状态机  lua_state  Lua当前运行时状态描述

Lua 虚拟机的外在数据形式是一个lua_State 结构体,取名State大概意为Lua虚拟机的当前状态。
全局State引用了整个虚拟机的所有数据。这个全局State的相关代码放在lstate.c中,API使用 luaE 为前缀。
*/

#ifndef lstate_h
#define lstate_h

#include "lua.h"

#include "lobject.h"
#include "ltm.h"
#include "lzio.h"



/*

** Some notes about garbage-collected objects:  All objects in Lua must
** be kept somehow accessible until being freed.
**
** Lua keeps most objects linked in list g->allgc. The link uses field
** 'next' of the CommonHeader.
**
** Strings are kept in several lists headed by the array g->strt.hash.
**
** Open upvalues are not subject to independent garbage collection. They
** are collected together with their respective threads. Lua keeps a
** double-linked list with all open upvalues (g->uvhead) so that it can
** mark objects referred by them. (They are always gray, so they must
** be remarked in the atomic step. Usually their contents would be marked
** when traversing the respective threads, but the thread may already be
** dead, while the upvalue is still accessible through closures.)
**
** Objects with finalizers are kept in the list g->finobj.
**
** The list g->tobefnz links all objects being finalized.

*/


struct lua_longjmp;  /* defined in ldo.c */



/* extra stack space to handle TM calls and some other extras */
#define EXTRA_STACK   5


#define BASIC_STACK_SIZE        (2*LUA_MINSTACK)


/* kinds of Garbage Collection */
#define KGC_NORMAL	0
#define KGC_EMERGENCY	1	/* gc was forced by an allocation failure */
#define KGC_GEN		2	/* generational collection */

/** 
 *  所有短字符串均被存放在全局表(global_State)的 strt 域中.strt是string table 的简写,它是一个哈希表: 一个字符串TString,首先根据hash算法算出hash值,这就是stringtable中hash的索引值,如果这里已经有元素,则使用链表串接起来.
 */
typedef struct stringtable {
  GCObject **hash;
  lu_int32 nuse;  /* number of elements */
  int size;
} stringtable;


/*
** information about a call  函数栈的环境
*/
/*Lua调用栈
 1:Lua把调用栈和数据栈分开保存。
 2:调用栈放在一个叫做CallInfo的结构中,以双向链表的形式储存在线程对象里。
 3:CallInfo 保存着正在调用的函数的运行状态；状态标示存放在lu_byte callstatus中。
 4:部分数据和函数的类型有关,以联合形式存放
 5:C 函数与 Lua函数的结构不完全相同
 6:callstatus中保存了一位标志用来区分是C函数还是Lua函数
 7:正在调用的函数一定存在于数据栈上,在CallInfo结构中,func指向正在执行的函数在数据栈上的位置
   需要记录这个信息,是因为如果当前是一个Lua函数,且传入的参数个数不定的时候,需要用这个位置和当
   前数据栈底的位置相减,获得不定参数的准确数量
 8:同时,func还可以帮助我们调试嵌入式Lua代码:在用 GDB这样的调试器调试代码时,可以方便的查看C中
   的调用栈信息,但一旦嵌入Lua ,我们很难理解运行过程中的Lua代码的调用栈;不理解Lua的内部结构,
    就可能面对一个简单的lua_State变量束手无策.
 9:实际上,遍历L中的Ci域指向的CallInfo链表可以获得完整的Lua调用链;
  而每一级的CallInfo中, 都可以进一步的通过 func域取得所在函数的更详细信息。
  当func为一个Lua函数时,根据它的函数原型,可以获得源文件名、行号等诸多调试信息。
 10:CallInfo是一个标准的双向链表结构,不直接被GC模块管理
 11:这个双向链表表达的是一个逻辑上的栈, 在运行过程中,并不是每次调入更深层次的函数,
    就立刻构造出一个CallInfo节点。
 12:整个CallInfo链表会在运行中被反复复用。直到GC的时候才清理那些比当前调用层次更深的无用节点。
   lstate.c中有luaE_extendCI的实现
 13:也就是说,调用者只需要把CallInfo链表当成一个无限长的堆栈使用即可
 14:当调用层次返回,之前分配 的节点可以被后续调用行为复用。
 15:在GC的时候只需要调用luaE_freeCI就可以释放过长的链表。
*/
typedef struct CallInfo {
  StkId func;  /* function index in the stack */
  StkId	top;  /* top for this function */  //CallInfo栈顶
  struct CallInfo *previous, *next;  /* dynamic call link */
  short nresults;  /* expected number of results from this function */
  lu_byte callstatus;   /*保存了一位标志用来区分是C函数还是Lua函数*/
  ptrdiff_t extra;
  union {
    struct {  /* only for Lua functions */
      StkId base;  /* base for this function  当前函数的数据栈指针*/
      const Instruction *savedpc;/*保存有指向当前指令的指针*/
    } l;
    struct {  /* only for C functions */
      int ctx;  /* context info. in case of yields */
      lua_CFunction k;  /* continuation in case of yields 延续点 k*/
      ptrdiff_t old_errfunc;
      lu_byte old_allowhook;
      lu_byte status;
    } c;
  } u;
} CallInfo;


/*
** Bits in CallInfo status
*/
#define CIST_LUA	(1<<0)	/* call is running a Lua function */
#define CIST_HOOKED	(1<<1)	/* call is running a debug hook */
#define CIST_REENTRY	(1<<2)	/* call is running on same invocation of
                                   luaV_execute of previous call */
#define CIST_YIELDED	(1<<3)	/* call reentered after suspension */
#define CIST_YPCALL	(1<<4)	/* call is a yieldable protected call */
#define CIST_STAT	(1<<5)	/* call has an error status (pcall) */
#define CIST_TAIL	(1<<6)	/* call was tail called */
#define CIST_HOOKYIELD	(1<<7)	/* last hook called yielded */

/* 标志用来区分是 C 函 数还是 Lua 函数*/
#define isLua(ci)	((ci)->callstatus & CIST_LUA)


/*
** `global state', shared by all threads of this state
*/
/*
 global_State是一个进程独有的数据结构,它其中的很多数据会被该进程中所有的lua_State所共享.
 换言之,一个进程只会有一个global_State,但是却可能有多份lua_State,它们之间是一对多的关系.
 */
typedef struct global_State {
  lua_Alloc frealloc;  /* function to reallocate memory */
  void *ud;         /* auxiliary data to `frealloc' */
  lu_mem totalbytes;  /* number of bytes currently allocated - GCdebt */
  l_mem GCdebt;  /* bytes allocated not yet compensated by the collector */
  lu_mem GCmemtrav;  /* memory traversed by the GC */
  lu_mem GCestimate;  /* an estimate of the non-garbage memory in use */
  stringtable strt;  /* hash table for strings */
  TValue l_registry;   //registry表
  unsigned int seed;  /* randomized seed for hashes 随机种子散列 */
  lu_byte currentwhite;/*来表示当前是那个标志位用来标识white */
  lu_byte gcstate;  /* state of garbage collector */
  lu_byte gckind;  /* kind of GC running */
  lu_byte gcrunning;  /* true if GC is running */
  int sweepstrgc;  /* position of sweep in `strt' */
  GCObject *allgc;  /* list of all collectable objects */
  GCObject *finobj;  /* list of collectable objects with finalizers */
  GCObject **sweepgc;  /* current position of sweep in list 'allgc' */
  GCObject **sweepfin;  /* current position of sweep in list 'finobj' */
  GCObject *gray;  /* list of gray objects */
  GCObject *grayagain;  /* list of objects to be traversed atomically */
  GCObject *weak;  /* list of tables with weak values */
  GCObject *ephemeron;  /* list of ephemeron tables (weak keys) */
  GCObject *allweak;  /* list of all-weak tables */
  GCObject *tobefnz;  /* list of userdata to be GC */
  UpVal uvhead;  /* head of double-linked list of all open upvalues */
  Mbuffer buff;  /* temporary buffer for string concatenation 字符串连接的临时缓冲区 */
  int gcpause;  /* size of pause between successive GCs */
  int gcmajorinc;  /* pause between major collections (only in gen. mode) */
  int gcstepmul;  /* GC `granularity' */
  lua_CFunction panic;  /* to be called in unprotected errors */
  struct lua_State *mainthread;
  const lua_Number *version;  /* pointer to version number */
  TString *memerrmsg;  /* memory-error message */
  TString *tmname[TM_N];  /* array with tag-method names */ //元方法 字符串数据
  struct Table *mt[LUA_NUMTAGS];  /* metatables for basic types */
} global_State;


/*
** per thread' state
*/
/*
 1： Lua栈是一个数组来模拟的数据结构，在每个lua_State创建时就进行了初始化,其中stack指针指向该数组的起始位置,top指针会随着从Lua栈中压入/退出数据而有所增/减,而base指针则随着每次Lua调用函数的CallInfo结构体的base指针做变化.
 
 2：每个CallInfo结构体与函数调用相关,虽然它也有自己的base/top指针,但是这两个指针还是指向lua_State的Lua栈.
 3：在每次函数调用的前后,lua_State的base/top指针会随着该次函数调用的CallInfo指针的base/top指针做变化.
 4：lua_State的base指针是一个很重要的数据,因为读取Lua栈的数据,以及执行Lua虚拟机的OpCode时拿到的数据,都是以这个指针为基准位置来获取的.
 
 5：综合以上3,4两点可知,其实拿到的也就是当前函数调用环境中的数据.
 
 6: Lua中提供了一系列的API用于Lua栈的操作,大体可以分为以下几类:
 
   向Lua栈中压入数据,这类函数的命名规律是lua_push*,压入相应的数据到Lua栈中之后,都会自动将所操作的lua_State的top指针递增,因为原先的位置已经被新的数据占据了,递增top指针指向Lua栈的下一个可用位置.
 
   获取Lua栈的元素,这类函数的命名规律是Lua_to*,这类函数根据传入的Lua栈索引,取出相应的数组元素,返回所需要的数据.
*/
struct lua_State {
  CommonHeader;                                     //Lua通用数据相关
  lu_byte status;                                   //当前状态
  StkId top;  /* first free slot in the stack */            //栈顶元素地址
  global_State *l_G;                                        //指向全局状态指针
  CallInfo *ci;  /* call info for current function */       //指向当前函数的CallInfo数据指针
  const Instruction *oldpc;  /* last pc traced */            //当前函数的pc指针()
  StkId stack_last;  /* last free slot in the stack */      //指向Lua栈最后位置
  StkId stack;  /* stack base */                            //指向Lua栈起始位置
  int stacksize;                                            //Lua栈大小
  unsigned short nny;  /* number of non-yieldable calls in stack */
  unsigned short nCcalls;  /* number of nested C calls */   //C函数调用的层数
  lu_byte hookmask;                                         //hook mask位
  lu_byte allowhook;
  int basehookcount;
  int hookcount;
  lua_Hook hook;
  GCObject *openupval;  /* 当前线程中记录的链表(double link)list of open upvalues in this stack */
  GCObject *gclist;
  struct lua_longjmp *errorJmp;  /* current error recover point */
  ptrdiff_t errfunc;  /* current error handling function (stack index) */
  CallInfo base_ci;  /* CallInfo for first level (C calling Lua)  指向CallInfo数组的起始位置 */
};


#define G(L)	(L->l_G)


/*
** Union of all collectable objects
 * 在Lua中就使用了一个GCObject的union将所有可gc类型囊括了进来.
*/
union GCObject {
  GCheader gch;  /* common header */
  union TString ts;
  union Udata u;
  union Closure cl;
  struct Table h;
  struct Proto p;
  struct UpVal uv;
  struct lua_State th;  /* thread */
};


#define gch(o)		(&(o)->gch)

/* macros to convert a GCObject into a specific value */
#define rawgco2ts(o)  \
	check_exp(novariant((o)->gch.tt) == LUA_TSTRING, &((o)->ts))
#define gco2ts(o)	(&rawgco2ts(o)->tsv)
#define rawgco2u(o)	check_exp((o)->gch.tt == LUA_TUSERDATA, &((o)->u))
#define gco2u(o)	(&rawgco2u(o)->uv)
#define gco2lcl(o)	check_exp((o)->gch.tt == LUA_TLCL, &((o)->cl.l))
#define gco2ccl(o)	check_exp((o)->gch.tt == LUA_TCCL, &((o)->cl.c))
#define gco2cl(o)  \
	check_exp(novariant((o)->gch.tt) == LUA_TFUNCTION, &((o)->cl))
#define gco2t(o)	check_exp((o)->gch.tt == LUA_TTABLE, &((o)->h))
#define gco2p(o)	check_exp((o)->gch.tt == LUA_TPROTO, &((o)->p))
#define gco2uv(o)	check_exp((o)->gch.tt == LUA_TUPVAL, &((o)->uv))
#define gco2th(o)	check_exp((o)->gch.tt == LUA_TTHREAD, &((o)->th))

/* macro to convert any Lua object into a GCObject */
#define obj2gco(v)	(cast(GCObject *, (v)))


/* actual number of total bytes allocated */
#define gettotalbytes(g)	((g)->totalbytes + (g)->GCdebt)

LUAI_FUNC void luaE_setdebt (global_State *g, l_mem debt);
LUAI_FUNC void luaE_freethread (lua_State *L, lua_State *L1);
LUAI_FUNC CallInfo *luaE_extendCI (lua_State *L);
LUAI_FUNC void luaE_freeCI (lua_State *L);


#endif

