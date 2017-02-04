/*
** $Id: ldo.c,v 2.108.1.3 2013/11/08 18:22:50 roberto Exp $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/

/*
   函数调用以及栈管理
*/

#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#define ldo_c
#define LUA_CORE

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"
/*
** {======================================================
** Error-recovery functions
** =======================================================
*/

/*
** LUAI_THROW/LUAI_TRY define how Lua does exception handling. By
** default, Lua handles errors with exceptions when compiling as
** C++ code, with _longjmp/_setjmp when asked to use them, and with
** longjmp/setjmp otherwise.
*/
#if !defined(LUAI_THROW)

#if defined(__cplusplus) && !defined(LUA_USE_LONGJMP)
/* C++ exceptions */
#define LUAI_THROW(L,c)		throw(c)
#define LUAI_TRY(L,c,a) \
	try { a } catch(...) { if ((c)->status == 0) (c)->status = -1; }
#define luai_jmpbuf		int  /* dummy variable */

#elif defined(LUA_USE_ULONGJMP)
/* in Unix, try _longjmp/_setjmp (more efficient) */
#define LUAI_THROW(L,c)		_longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)		if (_setjmp((c)->b) == 0) { a }
#define luai_jmpbuf		jmp_buf

#else
/* default handling with long jumps */
#define LUAI_THROW(L,c)		longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)		if (setjmp((c)->b) == 0) { a }
#define luai_jmpbuf		jmp_buf

#endif

#endif



/* chain list of long jump buffers */
struct lua_longjmp {
  struct lua_longjmp *previous;
  luai_jmpbuf b;
  volatile int status;  /* error code */
};


static void seterrorobj (lua_State *L, int errcode, StkId oldtop) {
  switch (errcode) {
    case LUA_ERRMEM: {  /* memory error? */
      setsvalue2s(L, oldtop, G(L)->memerrmsg); /* reuse preregistered msg. */
      break;
    }
    case LUA_ERRERR: {
      setsvalue2s(L, oldtop, luaS_newliteral(L, "error in error handling"));
      break;
    }
    default: {
      setobjs2s(L, oldtop, L->top - 1);  /* error message on current top */
      break;
    }
  }
  L->top = oldtop + 1;
}

/**
 *  考虑到新构造的线程可能在不受保护的情况下运行,这时的任何错误都必须被捕获,不能让程序崩溃.
     这种情况合理的处理方式就是把正在运行的线程标记为死线程,并且在主线程中抛出异常.
 */
l_noret luaD_throw (lua_State *L, int errcode) {
  if (L->errorJmp) {  /* thread has an error handler? */
    L->errorJmp->status = errcode;  /* set status */
    LUAI_THROW(L, L->errorJmp);  /* jump to it */
  }
  else {  /* thread has no error handler */
    L->status = cast_byte(errcode);  /* mark it as dead */
    if (G(L)->mainthread->errorJmp) {  /* main thread has a handler? */
      setobjs2s(L, G(L)->mainthread->top++, L->top - 1);  /* copy error obj. */
      luaD_throw(G(L)->mainthread, errcode);  /* re-throw in main thread */
    }
    else {  /* no handler at all; abort */
      if (G(L)->panic) {  /* panic function? */
        lua_unlock(L);
        G(L)->panic(L);  /* call it (last chance to jump out) */
      }
      abort();
    }
  }
}

/**  截获内存分配的异常情况
 *  只是设置好异常处理方法,然后调用 f_luaopen
 */
int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud) {
  unsigned short oldnCcalls = L->nCcalls;
  struct lua_longjmp lj;
  lj.status = LUA_OK;
  lj.previous = L->errorJmp;  /* chain new error handler */
  L->errorJmp = &lj;
  LUAI_TRY(L, &lj,
    (*f)(L, ud);
  );
  L->errorJmp = lj.previous;  /* restore old error handler */
  L->nCcalls = oldnCcalls;
  return lj.status;
}

/*====================================================== */

/* 
 1：有些外部结构对数据栈的引用需要修正为正确的新地址。
 2:这些需要修正的位置包括upvalue以及执行栈对数据栈的引用.
*/
static void correctstack (lua_State *L, TValue *oldstack) {
  CallInfo *ci;
  GCObject *up;
  L->top = (L->top - oldstack) + L->stack;
  for (up = L->openupval; up != NULL; up = up->gch.next)
    gco2uv(up)->v = (gco2uv(up)->v - oldstack) + L->stack;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top = (ci->top - oldstack) + L->stack;
    ci->func = (ci->func - oldstack) + L->stack;
    if (isLua(ci))
      ci->u.l.base = (ci->u.l.base - oldstack) + L->stack;
  }
}


/* some space for error handling */
#define ERRORSTACKSIZE	(LUAI_MAXSTACK + 200)


void luaD_reallocstack (lua_State *L, int newsize) {
  TValue *oldstack = L->stack;
  int lim = L->stacksize;
  lua_assert(newsize <= LUAI_MAXSTACK || newsize == ERRORSTACKSIZE);
  lua_assert(L->stack_last - L->stack == L->stacksize - EXTRA_STACK);
  luaM_reallocvector(L, L->stack, L->stacksize, newsize, TValue);
  for (; lim < newsize; lim++)
    setnilvalue(L->stack + lim); /* erase new segment */
  L->stacksize = newsize;
  L->stack_last = L->stack + newsize - EXTRA_STACK;
  correctstack(L, oldstack);
}

/* 数据栈的空间扩展
 1:数据栈扩展的过程,伴随着数据拷贝。这些数据都是可以直接值复制的,
    所以不需要在扩展之后修正其中的指针。
 2:但,有些外部结构对数据栈的引用需要修正为正确的新地址。这些需要修正的位置包括
     upvalue以及执行栈对数据栈的引用.
 3:这个过程由correctstack函数实现
 */
void luaD_growstack (lua_State *L, int n) {
  int size = L->stacksize;
  if (size > LUAI_MAXSTACK)  /* error after extra size? */
    luaD_throw(L, LUA_ERRERR);
  else {
    int needed = cast_int(L->top - L->stack) + n + EXTRA_STACK;
    int newsize = 2 * size;
    if (newsize > LUAI_MAXSTACK) newsize = LUAI_MAXSTACK;
    if (newsize < needed) newsize = needed;
    if (newsize > LUAI_MAXSTACK) {  /* stack overflow? */
      luaD_reallocstack(L, ERRORSTACKSIZE);
      luaG_runerror(L, "stack overflow");
    }
    else
      luaD_reallocstack(L, newsize);
  }
}


static int stackinuse (lua_State *L) {
  CallInfo *ci;
  StkId lim = L->top;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    lua_assert(ci->top <= L->stack_last);
    if (lim < ci->top) lim = ci->top;
  }
  return cast_int(lim - L->stack) + 1;  /* part of stack in use */
}


void luaD_shrinkstack (lua_State *L) {
  int inuse = stackinuse(L);
  int goodsize = inuse + (inuse / 8) + 2*EXTRA_STACK;
  if (goodsize > LUAI_MAXSTACK) goodsize = LUAI_MAXSTACK;
  if (inuse > LUAI_MAXSTACK ||  /* handling stack overflow? */
      goodsize >= L->stacksize)  /* would grow instead of shrink? */
    condmovestack(L);  /* don't change stack (change only for debugging) */
  else
    luaD_reallocstack(L, goodsize);  /* shrink it */
}

/**
 *  Lua可以为每个线程设置一个钩子函数,用于调试、统计和其它一些特殊用法
 *  钩子函数是一个 C 函数,用内部 API luaD_hook 封装起来
 *  这里做的事情并不复杂:保存数据栈、构造调试信息、通过设置 allowhook 禁掉钩子的递归调用,然后调用C版本的钩子函数,完毕后恢复这些状态
**/
void luaD_hook (lua_State *L, int event, int line) {
  lua_Hook hook = L->hook;
  if (hook && L->allowhook) {
    CallInfo *ci = L->ci;
    ptrdiff_t top = savestack(L, L->top);
    ptrdiff_t ci_top = savestack(L, ci->top);
    lua_Debug ar;
    ar.event = event;
    ar.currentline = line;
    ar.i_ci = ci;
    luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
    ci->top = L->top + LUA_MINSTACK;
    lua_assert(ci->top <= L->stack_last);
    L->allowhook = 0;  /* cannot call hooks inside a hook */
    ci->callstatus |= CIST_HOOKED;
    lua_unlock(L);
    (*hook)(L, &ar);
    lua_lock(L);
    lua_assert(!L->allowhook);
    L->allowhook = 1;
    ci->top = restorestack(L, ci_top);
    L->top = restorestack(L, top);
    ci->callstatus &= ~CIST_HOOKED;
  }
}


static void callhook (lua_State *L, CallInfo *ci) {
  int hook = LUA_HOOKCALL;
  ci->u.l.savedpc++;  /* hooks assume 'pc' is already incremented */
  if (isLua(ci->previous) &&
      GET_OPCODE(*(ci->previous->u.l.savedpc - 1)) == OP_TAILCALL) {
    ci->callstatus |= CIST_TAIL;
    hook = LUA_HOOKTAILCALL;
  }
  luaD_hook(L, hook, -1);
  ci->u.l.savedpc--;  /* correct 'pc' */
}

/**
 *  LUA函数 调整变长参数表;只在Lua函数中出现.当一个函数接收变长参数时,这部分的参数是放在上一级
    数据栈帧尾部的.adjust_varargs将需要个固定参数复制到被调用的函数的新一级数据栈帧上,而变长参数留在原地.
 */
static StkId adjust_varargs (lua_State *L, Proto *p, int actual) {
  int i;
  int nfixargs = p->numparams;
  StkId base, fixed;
  lua_assert(actual >= nfixargs);
  /* move fixed parameters to final position */
  luaD_checkstack(L, p->maxstacksize);  /* check again for new 'base' */
  fixed = L->top - actual;  /* first fixed argument */
  base = L->top;  /* final position of first argument */
  for (i=0; i<nfixargs; i++) {
    setobjs2s(L, L->top++, fixed + i);
    setnilvalue(fixed + i);
  }
  return base;
}

/**
 *  有些对象是通过元表驱动函数调用的行为的,这时需要通过 tryfuncTM 函数取得真正的调用函数
 */
static StkId tryfuncTM (lua_State *L, StkId func) {
  const TValue *tm = luaT_gettmbyobj(L, func, TM_CALL);
  StkId p;
  ptrdiff_t funcr = savestack(L, func);
  if (!ttisfunction(tm))
    luaG_typeerror(L, func, "call");
  /* Open a hole inside the stack at `func' */
  for (p = L->top; p > func; p--) setobjs2s(L, p, p-1);
  incr_top(L);
  func = restorestack(L, funcr);  /* previous call may change stack */
  setobj2s(L, func, tm);  /* tag method is the new function to be called */
  return func;
}



#define next_ci(L) (L->ci = (L->ci->next ? L->ci->next : luaE_extendCI(L)))
/*
 *    returns true if function has been executed (C function)
    备好Lua函数栈,创建CallInfo指针并且根据相应信息进行初始化
    在调用函数之前函数栈环境相关的操作
 1: 从lua_State的CallInfo数组中得到一个新的CallInfo结构体,设置它的func/base/top指针.
 2: 是把多余的函数参数赋值为nil,比如一个函数定义中需要的是两个参数,实际传入的只有一个,
    那么多出来的那个参数在这里会被赋值为nil.
 3: 将这里创建的CallInfo指针的top/base指针赋值给lua_State结构体的top/base指针.
 4: 调用函数完毕之后,又需要根据前一个函数调用时的环境进行恢复
*/
int luaD_precall (lua_State *L, StkId func, int nresults) {
  lua_CFunction f;
  CallInfo *ci;
  int n;  /* number of arguments (Lua) or returns (C) */
  ptrdiff_t funcr = savestack(L, func);
  switch (ttype(func)) {
    case LUA_TLCF:  /* light C function */
      f = fvalue(func);
      goto Cfunc;
    case LUA_TCCL: {  /* C closure */
      f = clCvalue(func)->f;
     Cfunc:
      luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
      ci = next_ci(L);  /* now 'enter' new function */
      ci->nresults = nresults;
      ci->func = restorestack(L, funcr);
      ci->top = L->top + LUA_MINSTACK;
      lua_assert(ci->top <= L->stack_last);
      ci->callstatus = 0;
      luaC_checkGC(L);  /* stack grow uses memory */
      if (L->hookmask & LUA_MASKCALL)
        luaD_hook(L, LUA_HOOKCALL, -1);
      lua_unlock(L);
      n = (*f)(L);  /* do the actual call */
      lua_lock(L);
      api_checknelems(L, n);
      luaD_poscall(L, L->top - n);  /*调用函数完毕之后,又需要根据前一个函数调用时的环境进行恢复 */
      return 1;
    }
    case LUA_TLCL: {  /* Lua function: prepare its call  Lua函数调用*/
      StkId base;
      Proto *p = clLvalue(func)->p; //p就是分析Lua脚本之后生成的 Proto指针，而它的code成员变量前面提到过存放的就是分析后的指令
      n = cast_int(L->top - func) - 1;  /* number of real arguments */
      luaD_checkstack(L, p->maxstacksize);
      for (; n < p->numparams; n++)
        setnilvalue(L->top++);  /* complete missing arguments */
      if (!p->is_vararg) {
        func = restorestack(L, funcr);
        base = func + 1;
      }
      else {
        base = adjust_varargs(L, p, n);
        func = restorestack(L, funcr);  /* previous call can change stack */
      }
      ci = next_ci(L);  /* now 'enter' new function */
      ci->nresults = nresults;
      ci->func = func;
      ci->u.l.base = base;
      ci->top = base + p->maxstacksize;
      lua_assert(ci->top <= L->stack_last);
      ci->u.l.savedpc = p->code;  /* starting point */ //p就是
      ci->callstatus = CIST_LUA;
      L->top = ci->top;
      luaC_checkGC(L);  /* stack grow uses memory */
      if (L->hookmask & LUA_MASKCALL)
        callhook(L, ci);
      return 0;
    }
    default: {  /* not a function */
      func = tryfuncTM(L, func);  /* retry with 'function' tag method */
      return luaD_precall(L, func, nresults);  /* now it must be a function */
    }
  }
}
/**
 *  数据栈的调整
**/
int luaD_poscall (lua_State *L, StkId firstResult) {
  StkId res;
  int wanted, i;
  CallInfo *ci = L->ci;
  if (L->hookmask & (LUA_MASKRET | LUA_MASKLINE)) {
    if (L->hookmask & LUA_MASKRET) {
      ptrdiff_t fr = savestack(L, firstResult);  /* hook may change stack */
      luaD_hook(L, LUA_HOOKRET, -1);
      firstResult = restorestack(L, fr);
    }
    L->oldpc = ci->previous->u.l.savedpc;  /* 'oldpc' for caller function */
  }
  res = ci->func;  /* res == final position of 1st result */
  wanted = ci->nresults;
  L->ci = ci = ci->previous;  /* back to caller */
  /* move results to correct place */
  for (i = wanted; i != 0 && firstResult < L->top; i--)
    setobjs2s(L, res++, firstResult++);
  while (i-- > 0)
    setnilvalue(res++);
  L->top = res;
  return (wanted - LUA_MULTRET);  /* 0 iff wanted == LUA_MULTRET */
}


/*
** Call a function (C or Lua). The function to be called is at *func.
** The arguments are on the stack, right after the function.
** When returns, all the results are on the stack, starting at the original
** function position.
*/

/**
 *  将会进入luaV_execute函数,这里是虚拟机执行代码的主函数:
 *  allowyield 参数用来标示这次调用是否可以在其中挂起
 */
void luaD_call (lua_State *L, StkId func, int nResults, int allowyield) {
  if (++L->nCcalls >= LUAI_MAXCCALLS) {//这里首先对Lua函数调用层次做判断
    if (L->nCcalls == LUAI_MAXCCALLS)
      luaG_runerror(L, "C stack overflow");
    else if (L->nCcalls >= (LUAI_MAXCCALLS + (LUAI_MAXCCALLS>>3)))
      luaD_throw(L, LUA_ERRERR);  /* error while handing stack error */
  }
  if (!allowyield) L->nny++;
    //下来调用luaD_precall进行函数调用之前的一些准备工作
  if (!luaD_precall(L, func, nResults))  /* is a Lua function? */
    luaV_execute(L);  /* call it  将会进入luaV_execute函数,这里是虚拟机执行代码的主函数:*/
  if (!allowyield) L->nny--;
  L->nCcalls--;
}

/**
 *  当执行流中断于一次C函数调用,finishCcall函数能完成
    当初执行了一半的C函数的剩余工作
  */
static void finishCcall (lua_State *L) {
  CallInfo *ci = L->ci;
  int n;
  lua_assert(ci->u.c.k != NULL);  /* must have a continuation */
  lua_assert(L->nny == 0);
  if (ci->callstatus & CIST_YPCALL) {  /* was inside a pcall? */
    ci->callstatus &= ~CIST_YPCALL;  /* finish 'lua_pcall' */
    L->errfunc = ci->u.c.old_errfunc;
  }
  /* finish 'lua_callk'/'lua_pcall' */
  adjustresults(L, ci->nresults);
  /* call continuation function */
  if (!(ci->callstatus & CIST_STAT))  /* no call status? */
    ci->u.c.status = LUA_YIELD;  /* 'default' status */
  lua_assert(ci->u.c.status != LUA_OK);
  ci->callstatus = (ci->callstatus & ~(CIST_YPCALL | CIST_STAT)) | CIST_YIELDED;
  lua_unlock(L);
  n = (*ci->u.c.k)(L);
  lua_lock(L);
  api_checknelems(L, n);
  /* finish 'luaD_precall' */
  luaD_poscall(L, L->top - n);
}

/**
 *  如果检查到 lua 的调用栈上有未尽的工作,必须完成它。
 */
static void unroll (lua_State *L, void *ud) {
  UNUSED(ud);
  for (;;) {
    if (L->ci == &L->base_ci)  /* stack is empty? */
      return;  /* coroutine finished normally */
    if (!isLua(L->ci))  /* C function? */
      finishCcall(L);
    else {  /* Lua function */
      luaV_finishOp(L);  /* finish interrupted instruction */
      luaV_execute(L);  /* execute down to higher C 'boundary' */
    }
  }
}
/*
** check whether thread has a suspended protected call
**  检查线程是否 是一个 已经被暂停的 protected call
*/
static CallInfo *findpcall (lua_State *L) {
  CallInfo *ci;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {  /* search for a pcall */
    if (ci->callstatus & CIST_YPCALL)
      return ci;
  }
  return NULL;  /* no pending pcall */
}

/**
 *  recover用来把错误引导到调用栈上最近的lua_pcallk的延续点上.
    它首先回溯 CallInfo 栈,找到从C中调用lua_pcallk的位置.
 */
static int recover (lua_State *L, int status) {
  StkId oldtop;
  CallInfo *ci = findpcall(L);
  if (ci == NULL) return 0;  /* no recovery point */
  /* "finish" luaD_pcall */
  oldtop = restorestack(L, ci->extra);
  luaF_close(L, oldtop);
  seterrorobj(L, status, oldtop);
  L->ci = ci;
  L->allowhook = ci->u.c.old_allowhook;
  L->nny = 0;  /* should be zero to be yieldable */
  luaD_shrinkstack(L);
  L->errfunc = ci->u.c.old_errfunc;
  ci->callstatus |= CIST_STAT;  /* call has error status */
  ci->u.c.status = status;  /* (here it is) */
  return 1;  /* continue running the coroutine */
}


/*
** signal an error in the call to 'resume', not in the execution of the
** coroutine itself. (Such errors should not be handled by any coroutine
** error handler and should not kill the coroutine.)
*/
static l_noret resume_error (lua_State *L, const char *msg, StkId firstArg) {
  L->top = firstArg;  /* remove args from the stack */
  setsvalue2s(L, L->top, luaS_new(L, msg));  /* push error message */
  api_incr_top(L);
  luaD_throw(L, -1);  /* jump back to 'lua_resume' */
}


/*
** do the work for 'lua_resume' in protected mode
*/
static void resume (lua_State *L, void *ud) {
  int nCcalls = L->nCcalls;
  StkId firstArg = cast(StkId, ud);
  CallInfo *ci = L->ci;
  if (nCcalls >= LUAI_MAXCCALLS)
    resume_error(L, "C stack overflow", firstArg);
  if (L->status == LUA_OK) {  /* may be starting a coroutine */
    if (ci != &L->base_ci)  /* not in base level? */
      resume_error(L, "cannot resume non-suspended coroutine", firstArg);
    /* coroutine is in base level; start running it */
    if (!luaD_precall(L, firstArg - 1, LUA_MULTRET))  /* Lua function? */
      luaV_execute(L);  /* call it */
  }
  else if (L->status != LUA_YIELD)
    resume_error(L, "cannot resume dead coroutine", firstArg);
  else {  /* resuming from previous yield */
    L->status = LUA_OK;
    ci->func = restorestack(L, ci->extra);
    if (isLua(ci))  /* yielded inside a hook? */
      luaV_execute(L);  /* just continue running Lua code */
    else {  /* 'common' yield */
      if (ci->u.c.k != NULL) {  /* does it have a continuation? */
        int n;
        ci->u.c.status = LUA_YIELD;  /* 'default' status */
        ci->callstatus |= CIST_YIELDED;
        lua_unlock(L);
        n = (*ci->u.c.k)(L);  /* call continuation */
        lua_lock(L);
        api_checknelems(L, n);
        firstArg = L->top - n;  /* yield results come from continuation */
      }
      luaD_poscall(L, firstArg);  /* finish 'luaD_precall' */
    }
    unroll(L, NULL);
  }
  lua_assert(nCcalls == L->nCcalls);
}

/**
 * 延续
 */
LUA_API int lua_resume (lua_State *L, lua_State *from, int nargs) {
  int status;
  int oldnny = L->nny;  /* save 'nny' */
  lua_lock(L);
  luai_userstateresume(L, nargs);
  L->nCcalls = (from) ? from->nCcalls + 1 : 1;
  L->nny = 0;  /* allow yields */
  api_checknelems(L, (L->status == LUA_OK) ? nargs + 1 : nargs);
  status = luaD_rawrunprotected(L, resume, L->top - nargs);
  if (status == -1)  /* error calling 'lua_resume'? */
    status = LUA_ERRRUN;
  else {  /* yield or regular error */
    while (status != LUA_OK && status != LUA_YIELD) {  /* error? */
      if (recover(L, status))  /* recover point? */
        status = luaD_rawrunprotected(L, unroll, NULL);  /* run continuation */
      else {  /* unrecoverable error */
        L->status = cast_byte(status);  /* mark thread as `dead' */
        seterrorobj(L, status, L->top);
        L->ci->top = L->top;
        break;
      }
    }
    lua_assert(status == L->status);
  }
  L->nny = oldnny;  /* restore 'nny' */
  L->nCcalls--;
  lua_assert(L->nCcalls == ((from) ? from->nCcalls : 0));
  lua_unlock(L);
  return status;
}

/**
 *  挂起(中断)与延续
    lua_yieldk是一个公开 API ,只用于给 Lua 程序编写 C 扩展模块使用。所以处于这个函数内部时,一定 处于一个 C 函数调用中。但钩子函数的运行是个例外。钩子函数本身是一个 C 函数,但是并不是通常正常 的 C API 调用进来的。在 Lua 函数中触发钩子会认为当前状态是处于 Lua 函数执行中。这个时候允许 yield 线程,但无法正确的处理 C 层面的延续点。所以禁止传入延续点函数。而对于正常的 C 调用,允许修改延 续点 k 来改变执行流程。这里只需要简单的把 k 和 ctx 设入 L ,其它的活交给 resume 去处理就可以了
 */
LUA_API int lua_yieldk (lua_State *L, int nresults, int ctx, lua_CFunction k) {
  CallInfo *ci = L->ci;
  luai_userstateyield(L, nresults);
  lua_lock(L);
  api_checknelems(L, nresults);
  if (L->nny > 0) {
    if (L != G(L)->mainthread)
      luaG_runerror(L, "attempt to yield across a C-call boundary");
    else
      luaG_runerror(L, "attempt to yield from outside a coroutine");
  }
  L->status = LUA_YIELD;
  ci->extra = savestack(L, ci->func);  /* save current 'func' */
  if (isLua(ci)) {  /* inside a hook? */
    api_check(L, k == NULL, "hooks cannot continue after yielding");
  }
  else {
    if ((ci->u.c.k = k) != NULL)  /* is there a continuation? */
      ci->u.c.ctx = ctx;  /* save context */
    ci->func = L->top - nresults - 1;  /* protect stack below results */
    luaD_throw(L, LUA_YIELD);
  }
  lua_assert(ci->callstatus & CIST_HOOKED);  /* must be inside a hook */
  lua_unlock(L);
  return 0;  /* return to 'luaD_hook' */
}

/**  函数调用
 *   受保护的函数调用可以看成是一个C层面意义上完整的过程.在lua中，pcall是函数而不是语言机制完成的.受保护的函数调用一定在C层面进出一次调用栈.
 */
int luaD_pcall (lua_State *L, Pfunc func, void *u,
                ptrdiff_t old_top, ptrdiff_t ef) {
  int status;
  CallInfo *old_ci = L->ci;
  lu_byte old_allowhooks = L->allowhook;
  unsigned short old_nny = L->nny;
  ptrdiff_t old_errfunc = L->errfunc;
  L->errfunc = ef;
  status = luaD_rawrunprotected(L, func, u);
  if (status != LUA_OK) {  /* an error occurred? */
    StkId oldtop = restorestack(L, old_top);
    luaF_close(L, oldtop);  /* close possible pending closures */
    seterrorobj(L, status, oldtop);
    L->ci = old_ci;
    L->allowhook = old_allowhooks;
    L->nny = old_nny;
    luaD_shrinkstack(L);
  }
  L->errfunc = old_errfunc;
  return status;
}



/*
** Execute a protected parser.
** SParser结构体只会在分析过程中使用
*/
struct SParser {  /* data to `f_parser' */
  ZIO *z;
  Mbuffer buff;  /* dynamic structure used by the scanner */
  Dyndata dyd;  /* dynamic structures used by the parser */
  const char *mode;
  const char *name;
};

/**
 *  检查是binary 还是 text
 */
static void checkmode (lua_State *L, const char *mode, const char *x) {
  if (mode && strchr(mode, x[0]) == NULL) {
    luaO_pushfstring(L,
       "attempt to load a %s chunk (mode is " LUA_QS ")", x, mode);
    luaD_throw(L, LUA_ERRSYNTAX);
  }
}

/* 这是对一个Lua代码进行分析的入口函数*/
/*
 1: 如果暂时不深究词法分析的细节,仅看这个函数对外的输出,那么可以看到:在完成词法分析之后,返回了Proto类型的指针tf,然后将其绑定在新创建的Closure指针上,初始化UpValue,最后压入Lua栈中.
 
 2: 不难想像,Lua词法分析之后产生的opcode等相关数据都在这个Proto类型的结构体中.
 
 3: 回过头再来看lua_pcall函数是如何将产生的opcode放入虚拟机执行的.
 */

/*
 1: 首先看到的是SParser结构体,这个结构体只会在分析过程中使用,在它内部的成员变量有已经读取进
 内存的Lua脚本文件内容,文件名称等数据.
 
 2: 这里将调用zgetc函数拿到一个int型数据,以此来判断即将进行分析的内容,
 是已经经过了luac预编译的二进制内容,还是Lua脚本,根据不同的情况调用不同的parser来进行分析.
 
 3:函数最后  将Closure指针push到了Lua栈中,同时将栈指针加1,由此可知分析完Lua脚本文件产生的Closure指针一定是在Lua栈顶.
 
 4:到了这里,已经将分析Lua脚本文件的流程和其中重要的数据结构做了概述,也就是luaL_loadfile函数调用的过程,
    接下来就是分析如何根据这一步的结果来执行Lua虚拟机指令了.
 
 5:调用luaL_loadfile并且成功返回之后,产生的Closure指针被放在了Lua栈顶,
   这时需要调用lua_pcall(L, 0, LUA_MULTRET, 0)执行Lua指令.这里对lua_pcall函数参数做一些解释,
   第一个参数自不必解释;第二个参数表示调用这个函数时的参数数量,前面已经提到过,对一个Lua文件进行分析时
   也可以看做是分析一个Lua函数,只不过其函数参数数量为0,因此此处传递进来的第二个参数是0;第三个参数表示
   的该函数返回参数的数量,调用完毕之后将根据这个参数对Lua栈进行调整,同样的由于这里是一个Lua文件,所以需
   要传递的是LUA_MULTRET这个常量,表示多返回值;最后一个参数表示的是出错处理函数在Lua栈中得位置,只有非0值才有意义.
 */
static void f_parser (lua_State *L, void *ud) {
  int i;
  Closure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  int c = zgetc(p->z);  /* read first character */
  if (c == LUA_SIGNATURE[0]) {
    checkmode(L, p->mode, "binary");
    cl = luaU_undump(L, p->z, &p->buff, p->name);
  }
  else {
    checkmode(L, p->mode, "text");
    cl = luaY_parser(L, p->z, &p->buff, &p->dyd, p->name, c);/*词法/语法分析入口*/
  }
  lua_assert(cl->l.nupvalues == cl->l.p->sizeupvalues);
  for (i = 0; i < cl->l.nupvalues; i++) {  /* initialize upvalues */
    UpVal *up = luaF_newupval(L);
    cl->l.upvals[i] = up;      /* 初始化upvale */
    luaC_objbarrier(L, cl, up);/*它将Closure指针push到了Lua栈中,同时将栈指针加1,由此可知分析完Lua脚本文件产生的Closure指针一定是在Lua栈顶.*/
  }
}
/**
 *
 */
int luaD_protectedparser (lua_State *L, ZIO *z, const char *name,
                                        const char *mode) {
  struct SParser p;
  int status;
  L->nny++;  /* cannot yield during parsing */
  p.z = z; p.name = name; p.mode = mode;
  p.dyd.actvar.arr = NULL; p.dyd.actvar.size = 0;
  p.dyd.gt.arr = NULL; p.dyd.gt.size = 0;
  p.dyd.label.arr = NULL; p.dyd.label.size = 0;
  luaZ_initbuffer(L, &p.buff);
  status = luaD_pcall(L, f_parser, &p, savestack(L, L->top), L->errfunc);
  luaZ_freebuffer(L, &p.buff);
  luaM_freearray(L, p.dyd.actvar.arr, p.dyd.actvar.size);
  luaM_freearray(L, p.dyd.gt.arr, p.dyd.gt.size);
  luaM_freearray(L, p.dyd.label.arr, p.dyd.label.size);
  L->nny--;
  return status;
}


