/*
** $Id: lstate.c,v 2.99.1.2 2013/11/08 17:45:31 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/

/*
    全局状态机  lua_state  Lua当前运行时状态描述
*/
#include <stddef.h>
#include <string.h>

#define lstate_c
#define LUA_CORE

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


#if !defined(LUAI_GCPAUSE)
#define LUAI_GCPAUSE	200  /* 200% */
#endif

#if !defined(LUAI_GCMAJOR)
#define LUAI_GCMAJOR	200  /* 200% */
#endif

#if !defined(LUAI_GCMUL)
#define LUAI_GCMUL	200 /* GC runs 'twice the speed' of memory allocation */
#endif


#define MEMERRMSG	"not enough memory"


/*
** a macro to help the creation of a unique random seed when a state is
** created; the seed is used to randomize hashes.
*/
#if !defined(luai_makeseed)
#include <time.h>
#define luai_makeseed()		cast(unsigned int, time(NULL))
#endif



/*
** thread state + extra space
*/
/*LX的定义 
 1:在lua_State之前留出了大小为LUAI_EXTRASPACE字节的空间。
 2:面对外部用户操作的指针是L而不是LX,但L所占据的内存块的前面却是有所保留的。
 3:这是一个有趣的技巧。用户可以在拿到L指针后向前移动指针,取得一些LUAI_EXTRASPACE中额外的数据。
 4:把这些数据放在前面而不是lua_State结构的后面避免了向用户暴露结构的大小。
 5:这里,LUAI_EXTRASPACE是通过编译配置的,默认为0;
 6:开启LUAI_EXTRASPACE后,需要一系列的宏提供支持（luai_userstateopen(L)。。。。）
 7:给L附加一些用户自定义信息在追求性能的环境很有意义。可以在为Lua编写的C模块中,
   直接偏移L指针来获取一些附加信息。这比去读取L中的注册表要高效的多。
 8:另一方面,在多线程环境下,访问注册表本身会改变L的状态,是线程不安全的。
 */
typedef struct LX {
#if defined(LUAI_EXTRASPACE)
  char buff[LUAI_EXTRASPACE];
#endif
  lua_State l;
} LX;


/*
** Main thread combines a thread state and the global state
*/
typedef struct LG {
  LX l;
  global_State g;
} LG;



#define fromstate(L)	(cast(LX *, cast(lu_byte *, (L)) - offsetof(LX, l)))


/*
** Compute an initial seed as random as possible. In ANSI, rely on
** Address Space Layout Randomization (if present) to increase
** randomness..
 这个随机种子是在 Lua_State 创建时放在全局表中的,它利用了构造状态机的内存地址随机性以及用户 可配置的一个随机量同时来决定.
*/
#define addbuff(b,p,e) \
  { size_t t = cast(size_t, e); \
    memcpy(buff + p, &t, sizeof(t)); p += sizeof(t); }

static unsigned int makeseed (lua_State *L) {
  char buff[4 * sizeof(size_t)];
  unsigned int h = luai_makeseed();
  int p = 0;
  addbuff(buff, p, L);   /* heap variable */
  addbuff(buff, p, &h);  /* local variable */
  addbuff(buff, p, luaO_nilobject);  /* global variable */
  addbuff(buff, p, &lua_newstate);  /* public function */
  lua_assert(p == sizeof(buff));
  return luaS_hash(buff, p, h);
}


/*
** set GCdebt to a new value keeping the value (totalbytes + GCdebt)
** invariant
*/
void luaE_setdebt (global_State *g, l_mem debt) {
  g->totalbytes -= (debt - g->GCdebt);
  g->GCdebt = debt;
}

/**
 *  扩展CallInfo CI具体执行部分并不直接调用这个API 而是使用另一个宏 next_ci(L)
 *
 *  @param L
 *
 *  @return
 */
CallInfo *luaE_extendCI (lua_State *L) {
  CallInfo *ci = luaM_new(L, CallInfo);
  lua_assert(L->ci->next == NULL);
  L->ci->next = ci;
  ci->previous = L->ci;
  ci->next = NULL;
  return ci;
}

/* 在GC的时候只需要调用luaE_freeCI就可以释放过长的链表CallInfo*/
void luaE_freeCI (lua_State *L) {
  CallInfo *ci = L->ci;
  CallInfo *next = ci->next;
  ci->next = NULL;
  while ((ci = next) != NULL) {
    next = ci->next;
    luaM_free(L, ci);
  }
}

/* 对Lua数据栈和CallInfo数组的初始化:
 1:一开始,数据栈的空间很有限,只有 2倍的LUA_MINSTACK(默认是20)的大小。
 2:Lua供C使用的栈相关 API都是不检查数据栈越界的,这是因为通常我们编写C扩展都能把数据栈
    空间的使用控制在LUA_MINSTACK以内,或是显式扩展。
 3: 对每次数据栈访问都强制做越界检查是非常低效的。
 4:数据栈不够用的时候,可以扩展。这种扩展是用realloc实现的,每次至少分配比原来大一倍的空间,
   并把旧的数据复制到新空间。
 */
/* 
 这个函数主要就是对Lua栈和CallInfo数组的初始化
 可以看到的是,在这个函数中初始化了两个数组,分别保存Lua栈和CallInfo结构体数组.
*/

//可以看到,最开始创建了一个TValue的数组,由stack保存它的首地址;接下来将一个nil值压入栈中,最后base和top成员都指向stack的下一个位置(因为第一个位置已经保存了一个nil值).

//这就是整个Lua虚拟机环境在初始化的时候创建的Lua栈结构,在后面这里将是实际执行Lua代码的舞台.
/*
 1: lua_State的ci成员指向CallInfo类型数组的第一个成员,
    成员ci同时也是当前在调用的函数相关的CallInfo指针,
     在发生函数调用的时候这个变量会随之改变.
 2: ci的fun成员指向lua_State的top指针,在赋值完毕之后向函数栈中压入了一个nil值,
    也就是说,第一个CallInfo数组的元素,其func位置存放的是一个nil值.
 3: 最后,ci的top指针指向lua_State的top指针之后的LUA_MINSTACK位置.
*/
static void stack_init (lua_State *L1, lua_State *L) {
  int i; CallInfo *ci;
  /* initialize stack array */
  L1->stack = luaM_newvector(L, BASIC_STACK_SIZE, TValue); //realloc
  L1->stacksize = BASIC_STACK_SIZE;
  for (i = 0; i < BASIC_STACK_SIZE; i++)
    setnilvalue(L1->stack + i);  /* erase new stack */
  L1->top = L1->stack;
  L1->stack_last = L1->stack + L1->stacksize - EXTRA_STACK;
  /* initialize first ci */
  ci = &L1->base_ci;
  ci->next = ci->previous = NULL;
  ci->callstatus = 0;
  ci->func = L1->top;
  setnilvalue(L1->top++);  /* 'function' entry for this 'ci' */
  ci->top = L1->top + LUA_MINSTACK;
  L1->ci = ci;
}

/* 
 Lua数据栈的释放
 */
static void freestack (lua_State *L) {
  if (L->stack == NULL)
    return;  /* stack not completely built yet */
  L->ci = &L->base_ci;  /* free the entire 'ci' list */
  luaE_freeCI(L);
  luaM_freearray(L, L->stack, L->stacksize);  /* free stack array */
}


/*  对用于C注册函数的registry对象进行初始化;创建注册表和它的预定义值
** Create registry table and its predefined values
*/
static void init_registry (lua_State *L, global_State *g) {
  TValue mt;
  /* create registry */
  Table *registry = luaH_new(L);
  sethvalue(L, &g->l_registry, registry);
  luaH_resize(L, registry, LUA_RIDX_LAST, 0);
  /* registry[LUA_RIDX_MAINTHREAD] = L */
  setthvalue(L, &mt, L);
  luaH_setint(L, registry, LUA_RIDX_MAINTHREAD, &mt);
  /* registry[LUA_RIDX_GLOBALS] = table of globals */
  sethvalue(L, &mt, luaH_new(L));
  luaH_setint(L, registry, LUA_RIDX_GLOBALS, &mt);
}


/*  执行最核心的初始化:主线程的数据栈,初始化注册表,给出一个基本的字符串池,初始化元表
**  用的字符串,初始化词法分析用的 token 串,初始化内存错误信息.
** open parts of the state that may cause memory-allocation errors
*/
static void f_luaopen (lua_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
  stack_init(L, L);  /* init stack  Lua数据栈的初始化  */
  init_registry(L, g);/* 对用于C注册函数的registry对象进行初始化 */
  luaS_resize(L, MINSTRTABSIZE);  /* initial size of string table */
  luaT_init(L);     /*元表元方法的初始化*/
  luaX_init(L);    /*分配用户LEX的 Token 串 TString*/
  /* pre-create memory-error message 初始化内存错误信息*/
  g->memerrmsg = luaS_newliteral(L, MEMERRMSG);
  luaS_fix(g->memerrmsg);  /* it should never be collected */
  g->gcrunning = 1;  /* allow gc */
  g->version = lua_version(NULL);
  luai_userstateopen(L);
}


/* 将L中的G指针指向刚分配内存的G
** preinitialize a state with consistent values without allocating
** any memory (to avoid errors)
*/
static void preinit_state (lua_State *L, global_State *g) {
  G(L) = g;
  L->stack = NULL;
  L->ci = NULL;
  L->stacksize = 0;
  L->errorJmp = NULL;
  L->nCcalls = 0;
  L->hook = NULL;
  L->hookmask = 0;
  L->basehookcount = 0;
  L->allowhook = 1;
  resethookcount(L);
  L->openupval = NULL;
  L->nny = 1;
  L->status = LUA_OK;
  L->errfunc = 0;
}

/**
 *  关闭VM 并且释放资源
 */
static void close_state (lua_State *L) {
  global_State *g = G(L);
  luaF_close(L, L->stack);  /* close all upvalues for this thread */
  luaC_freeallobjects(L);  /* collect all objects */
  if (g->version)  /* closing a fully built state? */
    luai_userstateclose(L);
  luaM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
  luaZ_freebuffer(L, &g->buff);
  freestack(L);
  lua_assert(gettotalbytes(g) == sizeof(LG));
  (*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);  /* free main block */
}

/*      线程
 1:把数据栈和调用栈合起来就构成了Lua中的线程。
 2:在同一个Lua虚拟机中的不同线程因为共享了global_State而很难做到真正意义上的并发。
 3:它也绝非操作系统意义上的线程,但在行为上很相似。用户可以resume一个线程,
    线程可以被yield打断。
 4:Lua的执行过程就是围绕线程进行的。
 5:我们从lua_newthread阅读起,可以更好的理解它的数据结构。
 6:这里我们能发现,内存中的线程结构并非lua_State,而是一个叫LX的东西。
 */
LUA_API lua_State *lua_newthread (lua_State *L) {
  lua_State *L1;
  lua_lock(L);
  luaC_checkGC(L);
  L1 = &luaC_newobj(L, LUA_TTHREAD, sizeof(LX), NULL, offsetof(LX, l))->th;
  setthvalue(L, L->top, L1);
  api_incr_top(L);
  preinit_state(L1, G(L));
  L1->hookmask = L->hookmask;
  L1->basehookcount = L->basehookcount;
  L1->hook = L->hook;
  resethookcount(L1);
  luai_userstatethread(L, L1);
  stack_init(L1, L);  /* init stack */
  lua_unlock(L);
  return L1;
}


void luaE_freethread (lua_State *L, lua_State *L1) {
  LX *l = fromstate(L1);
  luaF_close(L1, L1->stack);  /* close all upvalues for this thread */
  lua_assert(L1->openupval == NULL);
  luai_userstatefree(L, L1);
  freestack(L1);
  luaM_free(L, l);
}


/*    NEW 一个 Lua VM
 *  这里主要完成的是lua_State结构体的初始化及其成员变量以及global_State结构体的初始化工作.
 */
LUA_API lua_State *lua_newstate (lua_Alloc f, void *ud) {
  int i;
  lua_State *L;
  global_State *g;
  LG *l = cast(LG *, (*f)(ud, NULL, LUA_TTHREAD, sizeof(LG)));/*给LG分配内存,并强制转换为LG类型*/
  if (l == NULL) return NULL;
  L = &l->l.l;
  g = &l->g;
  L->next = NULL;
  L->tt = LUA_TTHREAD;
  g->currentwhite = bit2mask(WHITE0BIT, FIXEDBIT);
  L->marked = luaC_white(g);
  g->gckind = KGC_NORMAL;
  preinit_state(L, g); /*将L中的G指针指向刚分配内存(LG)的G*/
  g->frealloc = f;
  g->ud = ud;
  g->mainthread = L;   /*同时将G中的L指针指向刚分配的内存(LG)的L；这里开始 L与G是你中有我，我中有你，同时这时候的L被认为是G中的 mainthread*/
  g->seed = makeseed(L);
  g->uvhead.u.l.prev = &g->uvhead;
  g->uvhead.u.l.next = &g->uvhead;
  g->gcrunning = 0;  /* no GC while building state */
  g->GCestimate = 0;
  g->strt.size = 0;
  g->strt.nuse = 0;
  g->strt.hash = NULL;
  setnilvalue(&g->l_registry);
  luaZ_initbuffer(L, &g->buff);/*buffer用于 Lua 源代码文本的解析过程,以及字符串处理需要的临时空间。Lua 的单个 State 工作在单线 程模式下,所以在内部需要时,总是重复利用这个指针指向的临时空间*/
  g->panic = NULL;
  g->version = NULL;
  g->gcstate = GCSpause;
  g->allgc = NULL;
  g->finobj = NULL;
  g->tobefnz = NULL;
  g->sweepgc = g->sweepfin = NULL;
  g->gray = g->grayagain = NULL;
  g->weak = g->ephemeron = g->allweak = NULL;
  g->totalbytes = sizeof(LG);
  g->GCdebt = 0;
  g->gcpause = LUAI_GCPAUSE;
  g->gcmajorinc = LUAI_GCMAJOR;
  g->gcstepmul = LUAI_GCMUL;
  for (i=0; i < LUA_NUMTAGS; i++) g->mt[i] = NULL;
    /**
     *  Lua 在处理虚拟机创建的过程非常的小心。
      由于内存管理器是外部传入的,不可能保证它的返回结果。到底有多少内存可供使用也是未知数。为了保证 Lua 虚拟机的健壮性,需要检查所有的内存分配结果。Lua 自身有完整的异常处理机制可以处理这些错误。所以 Lua 的初始化过程是分两步进行的,首先初始化不需要额外分配内存的部分,把异常处理机制先建立起来,然后去调用可能引用内存分配失败导致错误的初始化代码。
     */
  if (luaD_rawrunprotected(L, f_luaopen, NULL) != LUA_OK) { //只是设置好异常处理方法,然后调用 f_luaopen
    /* memory allocation error: free partial state */
    close_state(L);
    L = NULL;
  }
  return L;
}


LUA_API void lua_close (lua_State *L) {
  L = G(L)->mainthread;  /* only the main thread can be closed */
  lua_lock(L);
  close_state(L);
}


