/*
** $Id: lfunc.c,v 2.30.1.1 2013/04/12 18:48:47 roberto Exp $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/



/*
 函数原型及闭包管理  ；不同的数据类型最终被统一定义为 Lua Object
 */
#include <stddef.h>

#define lfunc_c
#define LUA_CORE

#include "lua.h"

#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"



Closure *luaF_newCclosure (lua_State *L, int n) {
  Closure *c = &luaC_newobj(L, LUA_TCCL, sizeCclosure(n), NULL, 0)->cl;
  c->c.nupvalues = cast_byte(n);
  return c;
}

/**
 *  只绑定了 Proto 却不包括初始化 upvalue 对象的过程。这是因为构造 Lua 闭包有两种可能的途径。
 *  第一:Lua 闭包一般在虚拟机运行的过程中被动态构造出来的。这时,闭包需引用的 upvalue 都在当前
 *      的数据栈上。利用 luaF_findupval 这个 API 可以把数据栈上的值转换为 upvalue
 *
 * 第二种生成 Lua 闭包的方式是加载一段 Lua 代码。每段 Lua 代码都会被编译成一个函数原型,但 Lua 的外部 API 是不返回函数原型对象的,而是把这个函数原型转换为一个 Lua 闭包对象。如果从源代码加载 的话,不可能有用户构建出来的 upvalue 的。
 *
 */
Closure *luaF_newLclosure (lua_State *L, int n) {
  Closure *c = &luaC_newobj(L, LUA_TLCL, sizeLclosure(n), NULL, 0)->cl;
  c->l.p = NULL;
  c->l.nupvalues = cast_byte(n);
  while (n--) c->l.upvals[n] = NULL;
  return c;
}

/**
 *  第二种情况的LClosure 的 upvalue 并不源于数据栈,需要利用这个函数构建
 */
UpVal *luaF_newupval (lua_State *L) {
  UpVal *uv = &luaC_newobj(L, LUA_TUPVAL, sizeof(UpVal), NULL, 0)->uv;
  uv->v = &uv->u.value;
  setnilvalue(uv->v);
  return uv;
}

/**
 *  可以把数据栈上的值转换为 upvalue
 *  这个函数先在当前的 openupval 链表中寻找是否已经转化过,如果有就复用之。
    否则,构造一个新的 upvalue 对象,并把它串到 openupval 链表中
 */
UpVal *luaF_findupval (lua_State *L, StkId level) {
  global_State *g = G(L);
  GCObject **pp = &L->openupval;
  UpVal *p;
  UpVal *uv;
  while (*pp != NULL && (p = gco2uv(*pp))->v >= level) {
    GCObject *o = obj2gco(p);
    lua_assert(p->v != &p->u.value);
    lua_assert(!isold(o) || isold(obj2gco(L)));
    if (p->v == level) {  /* found a corresponding upvalue? */
      if (isdead(g, o))   /* is it dead? */
        changewhite(o);   /* resurrect it */
      return p;
    }
    pp = &p->next;
  }
  /* not found: create a new one */
  uv = &luaC_newobj(L, LUA_TUPVAL, sizeof(UpVal), pp, 0)->uv;
  uv->v = level;  /* current value lives in the stack */
  uv->u.l.prev = &g->uvhead;  /* double link it in `uvhead' list */
  uv->u.l.next = g->uvhead.u.l.next;
  uv->u.l.next->u.l.prev = uv;
  g->uvhead.u.l.next = uv;
  lua_assert(uv->u.l.next->u.l.prev == uv && uv->u.l.prev->u.l.next == uv);
  return uv;
}


static void unlinkupval (UpVal *uv) {
  lua_assert(uv->u.l.next->u.l.prev == uv && uv->u.l.prev->u.l.next == uv);
  uv->u.l.next->u.l.prev = uv->u.l.prev;  /* remove from `uvhead' list */
  uv->u.l.prev->u.l.next = uv->u.l.next;
}


void luaF_freeupval (lua_State *L, UpVal *uv) {
  if (uv->v != &uv->u.value)  /* is it open? */
    unlinkupval(uv);  /* remove from open list */
  luaM_free(L, uv);  /* free upvalue */
}

/**
 *  当离开一个代码块时,这个代码块中定义的局部变量变得不可见。Lua 会调整数据栈指针,销毁掉这些 变量。若这些栈值被某些闭包以开放 upvalue 的形式引用,就需要把它们关闭;
 
    关闭操作:删掉已经无人引用的 upvalue 、把数据从数据栈上复制到 UpVal 结构中,并修正 UpVal 中的指针v。
 *
 */
void luaF_close (lua_State *L, StkId level) {
  UpVal *uv;
  global_State *g = G(L);
  while (L->openupval != NULL && (uv = gco2uv(L->openupval))->v >= level) {
    GCObject *o = obj2gco(uv);
    lua_assert(!isblack(o) && uv->v != &uv->u.value);
    L->openupval = uv->next;  /* remove from `open' list */
    if (isdead(g, o))
      luaF_freeupval(L, uv);  /* free upvalue */
    else {
      unlinkupval(uv);  /* remove upvalue from 'uvhead' list */
      setobj(L, &uv->u.value, uv->v);  /* move value to upvalue slot */
      uv->v = &uv->u.value;  /* now current value lives here */
      gch(o)->next = g->allgc;  /* link upvalue into 'allgc' list */
      g->allgc = o;
      luaC_checkupvalcolor(g, uv);
    }
  }
}

/**
 *  new 一个Proto 结构
 */
Proto *luaF_newproto (lua_State *L) {
  Proto *f = &luaC_newobj(L, LUA_TPROTO, sizeof(Proto), NULL, 0)->p;
  f->k = NULL;
  f->sizek = 0;
  f->p = NULL;
  f->sizep = 0;
  f->code = NULL;
  f->cache = NULL;
  f->sizecode = 0;
  f->lineinfo = NULL;
  f->sizelineinfo = 0;
  f->upvalues = NULL;
  f->sizeupvalues = 0;
  f->numparams = 0;
  f->is_vararg = 0;
  f->maxstacksize = 0;
  f->locvars = NULL;
  f->sizelocvars = 0;
  f->linedefined = 0;
  f->lastlinedefined = 0;
  f->source = NULL;
  return f;
}

/**
 *  释放一个Proto结构
 */
void luaF_freeproto (lua_State *L, Proto *f) {
  luaM_freearray(L, f->code, f->sizecode);
  luaM_freearray(L, f->p, f->sizep);
  luaM_freearray(L, f->k, f->sizek);
  luaM_freearray(L, f->lineinfo, f->sizelineinfo);
  luaM_freearray(L, f->locvars, f->sizelocvars);
  luaM_freearray(L, f->upvalues, f->sizeupvalues);
  luaM_free(L, f);
}


/*
** Look for n-th local variable at line `line' in function `func'.
** Returns NULL if not found.
*/
const char *luaF_getlocalname (const Proto *f, int local_number, int pc) {
  int i;
  for (i = 0; i<f->sizelocvars && f->locvars[i].startpc <= pc; i++) {
    if (pc < f->locvars[i].endpc) {  /* is variable active? */
      local_number--;
      if (local_number == 0)
        return getstr(f->locvars[i].varname);
    }
  }
  return NULL;  /* not found */
}

