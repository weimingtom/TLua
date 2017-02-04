/*
** $Id: lparser.h,v 1.70.1.1 2013/04/12 18:48:47 roberto Exp $
** Lua Parser
** See Copyright Notice in lua.h
*/

/*           
 *  语法分析器 : 代码翻译及预编译字节码 (API以LuaY 为前缀)
 */
#ifndef lparser_h
#define lparser_h

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/*
** Expression descriptor
*/
typedef enum {
  VVOID,	/* no value */
  VNIL,
  VTRUE,
  VFALSE,
  VK,		/* info = index of constant in `k' */
  VKNUM,	/* nval = numerical value */
  VNONRELOC,	/* info = result register */
  VLOCAL,	/* info = local register */
  VUPVAL,       /* info = index of upvalue in 'upvalues' */
  VINDEXED,	/* t = table register/upvalue; idx = index R/K */
  VJMP,		/* info = instruction pc */
  VRELOCABLE,	/* info = instruction pc */
  VCALL,	/* info = instruction pc */
  VVARARG	/* info = instruction pc */
} expkind;


#define vkisvar(k)	(VLOCAL <= (k) && (k) <= VINDEXED)
#define vkisinreg(k)	((k) == VNONRELOC || (k) == VLOCAL)

typedef struct expdesc {
  expkind k;   /* k表示该表达式的类型 */
  union {
    struct {  /* for indexed variables (VINDEXED) */
      short idx;  /* index (R/K) */
      lu_byte t;  /* table (register or upvalue) */
      lu_byte vt;  /* whether 't' is register (VLOCAL) or upvalue (VUPVAL) */
    } ind;
    int info;  /* for generic use */
    lua_Number nval;  /* for VKNUM 用来存储数据为数字的情况 */
  } u;
  int t;  /* patch list of `exit when true' */
  int f;  /* patch list of `exit when false' */
} expdesc;


/* description of active local variable */
typedef struct Vardesc {
  short idx;  /* variable index in stack */
} Vardesc;


/* description of pending goto statements and label statements */
typedef struct Labeldesc {
  TString *name;  /* label identifier */
  int pc;  /* position in code 如果是label,pc表示这个label对应的当前函数指令集合的位置，也就是待跳转的指令位置;如果是goto,则代表为这个goto生成的OP_JMP指令的位置*/
  int line;  /* line where it appeared */
  lu_byte nactvar;  /* local level where it appears in current block
                     代表解析此goto或者label时,函数有多少个有效的局部变量,用来在
                     跳转时决定需要关闭哪些upvalue*/
} Labeldesc;


/* list of labels or gotos */
typedef struct Labellist {
  Labeldesc *arr;  /* array */
  int n;  /* number of entries in use */
  int size;  /* array size */
} Labellist;


/* dynamic structures used by the parser */
/* Dyndata是一个全局数据,他本身也是一个栈;对应上面的FuncState栈,
 * Dyndata保存了每个FuncState对应的局部变量描述列表,goto列表和
 * label 列表.这些数据会跟着当前FuncState进行压栈和弹栈.
 */
typedef struct Dyndata {
  struct {           /* list of active local variables */
    Vardesc *arr;
    int n;
    int size;
  } actvar;
  Labellist gt;      /* list of pending gotos */
  Labellist label;   /* list of active labels */
} Dyndata;


/* control of blocks */
struct BlockCnt;  /* defined in lparser.c */


/*
 Proto结构体是分析完一个Lua文件之后的产物,所以需要在分析阶段需要有中
 这个在分析过程中临时使用的数据就不再使用了,间使用到的临时数据用来保存
 它,向它(中间变量)里面写入数据,待分析完毕之后,这个结构体就是FuncState,
 */
/* state needed to generate code for a given function */
typedef struct FuncState {
  Proto *f;  /* current function header 绑定的Proto结构体*/
  Table *h;  /* table to find (and reuse) elements in `k' */
  struct FuncState *prev;  /* enclosing function */
  struct LexState *ls;  /* lexical state */
  struct BlockCnt *bl;/* chain of current blocks函数本身使用block来控制局部变量的有效范围 */
  int pc;  /* next position to code (equivalent to `ncode')当前的pc索引,类似于CPU执行指令时的计数器,在这里这个变量用于保存当前已经向Proto的code数组写入了多少数据 */
  int lasttarget;   /* 'label' of last 'jump label' */
  int jpc;  /* list of pending jumps to `pc' */
  int nk;  /* number of elements in `k' */
  int np;  /* number of elements in `p' */
  int firstlocal;  /* index of first local var (in Dyndata array) */
  short nlocvars;  /* number of elements in 'f->locvars' */
  lu_byte nactvar;  /* number of active local variables */
  lu_byte nups;  /* number of upvalues */
  lu_byte freereg;  /* first free register当前的可用寄存器索引 */
} FuncState;


LUAI_FUNC Closure *luaY_parser (lua_State *L, ZIO *z, Mbuffer *buff,
                                Dyndata *dyd, const char *name, int firstchar);


#endif
