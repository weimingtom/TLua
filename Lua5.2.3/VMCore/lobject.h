/*
 ** $Id: lobject.h,v 2.71.1.1 2013/04/12 18:48:47 roberto Exp $
 ** Type definitions for Lua objects
 ** See Copyright Notice in lua.h
 */


/*
 对象操作的一些函数
 API以LuaO为前缀；不同的数据类型最终被统一定义为 Lua Object
 */

#ifndef lobject_h
#define lobject_h


#include <stdarg.h>


#include "llimits.h"
#include "lua.h"


/*
 ** Extra tags for non-values
 */
#define LUA_TPROTO	LUA_NUMTAGS
#define LUA_TUPVAL	(LUA_NUMTAGS+1)
#define LUA_TDEADKEY	(LUA_NUMTAGS+2)

/*
 ** number of all possible tags (including LUA_TNONE but excluding DEADKEY)
 */
#define LUA_TOTALTAGS	(LUA_TUPVAL+2)


/*
 ** tags for Tagged Values have the following use of bits:
 ** bits 0-3: actual tag (a LUA_T* value)
 ** bits 4-5: variant bits
 ** bit 6: whether value is collectable
 */

#define VARBITS		(3 << 4)


/*
 ** LUA_TFUNCTION variants:
 ** 0 - Lua function
 ** 1 - light C function
 ** 2 - regular C function (closure)
 */

/* Variant tags for functions */
#define LUA_TLCL	(LUA_TFUNCTION | (0 << 4))  /* Lua closure */
#define LUA_TLCF	(LUA_TFUNCTION | (1 << 4))  /* light C function */
#define LUA_TCCL	(LUA_TFUNCTION | (2 << 4))  /* C closure */


/* Variant tags for strings */
#define LUA_TSHRSTR	(LUA_TSTRING | (0 << 4))  /* short strings */
#define LUA_TLNGSTR	(LUA_TSTRING | (1 << 4))  /* long strings */


/* Bit mark for collectable types */
#define BIT_ISCOLLECTABLE	(1 << 6)

/* mark a tag as collectable */
#define ctb(t)			((t) | BIT_ISCOLLECTABLE)


/*
 ** Union of all collectable objects
 */
typedef union GCObject GCObject;


/*
 ** Common Header for all collectable objects (in macro form, to be
 ** included in other objects)
 * 这些需要gc的数据类型,都会有一个CommonHeader的成员,并且这个成员在结构体定义的最开始部分
 */
#define CommonHeader	GCObject *next; lu_byte tt; lu_byte marked


/*
 ** Common header in struct form
 */
typedef struct GCheader {
    CommonHeader;
} GCheader;



/*
 ** Union of all Lua values
 */
typedef union Value Value;


#define numfield	lua_Number n;    /* numbers */



/*
 ** Tagged Values. This is the basic representation of values in Lua,
 ** an actual value plus a tag with its type.
 */
/* 为了区分联合中存放的数据类型,再额外绑定一个类型字段 */
/* 于是Lua代码中又有了一个TValuefields将Value和类型结合在一起:*/
#define TValuefields	Value value_; int tt_

typedef struct lua_TValue TValue;


/* macro defining a nil value */
#define NILCONSTANT	{NULL}, LUA_TNIL


#define val_(o)		((o)->value_)
#define num_(o)		(val_(o).n)


/* raw type tag of a TValue */
#define rttype(o)	((o)->tt_)

/* tag with no variants (bits 0-3) */
#define novariant(x)	((x) & 0x0F)

/* type tag of a TValue (bits 0-3 for tags + variant bits 4-5) */
#define ttype(o)	(rttype(o) & 0x3F)

/* type tag of a TValue with no variants (bits 0-3) */
#define ttypenv(o)	(novariant(rttype(o)))


/* Macros to test type */
#define checktag(o,t)		(rttype(o) == (t))
#define checktype(o,t)		(ttypenv(o) == (t))
#define ttisnumber(o)		checktag((o), LUA_TNUMBER)
#define ttisnil(o)		checktag((o), LUA_TNIL)
#define ttisboolean(o)		checktag((o), LUA_TBOOLEAN)
#define ttislightuserdata(o)	checktag((o), LUA_TLIGHTUSERDATA)
#define ttisstring(o)		checktype((o), LUA_TSTRING)
#define ttisshrstring(o)	checktag((o), ctb(LUA_TSHRSTR))
#define ttislngstring(o)	checktag((o), ctb(LUA_TLNGSTR))
#define ttistable(o)		checktag((o), ctb(LUA_TTABLE))
#define ttisfunction(o)		checktype(o, LUA_TFUNCTION)
#define ttisclosure(o)		((rttype(o) & 0x1F) == LUA_TFUNCTION)
#define ttisCclosure(o)		checktag((o), ctb(LUA_TCCL))
#define ttisLclosure(o)		checktag((o), ctb(LUA_TLCL))
#define ttislcf(o)		checktag((o), LUA_TLCF)
#define ttisuserdata(o)		checktag((o), ctb(LUA_TUSERDATA))
#define ttisthread(o)		checktag((o), ctb(LUA_TTHREAD))
#define ttisdeadkey(o)		checktag((o), LUA_TDEADKEY)

#define ttisequal(o1,o2)	(rttype(o1) == rttype(o2))

/* Macros to access values */
#define nvalue(o)	check_exp(ttisnumber(o), num_(o))
#define gcvalue(o)	check_exp(iscollectable(o), val_(o).gc)/*  GCObject这个union,将所有需要gc的数据类型全部囊括其中,这样在定位和查找不同类型的数据时就来的方便多了,而如果只想要它们的GC部分,可以通过GCheader gch*/
#define pvalue(o)	check_exp(ttislightuserdata(o), val_(o).p)
#define rawtsvalue(o)	check_exp(ttisstring(o), &val_(o).gc->ts)
#define tsvalue(o)	(&rawtsvalue(o)->tsv)
#define rawuvalue(o)	check_exp(ttisuserdata(o), &val_(o).gc->u)
#define uvalue(o)	(&rawuvalue(o)->uv)
#define clvalue(o)	check_exp(ttisclosure(o), &val_(o).gc->cl)
#define clLvalue(o)	check_exp(ttisLclosure(o), &val_(o).gc->cl.l)
#define clCvalue(o)	check_exp(ttisCclosure(o), &val_(o).gc->cl.c)
#define fvalue(o)	check_exp(ttislcf(o), val_(o).f)
#define hvalue(o)	check_exp(ttistable(o), &val_(o).gc->h)
#define bvalue(o)	check_exp(ttisboolean(o), val_(o).b)
#define thvalue(o)	check_exp(ttisthread(o), &val_(o).gc->th)
/* a dead value may get the 'gc' field, but cannot access its contents */
#define deadvalue(o)	check_exp(ttisdeadkey(o), cast(void *, val_(o).gc))

#define l_isfalse(o)	(ttisnil(o) || (ttisboolean(o) && bvalue(o) == 0))

//Lua内部用一个宏,表示哪些数据类型需要进行gc操作的
//可以看到,LUA_TSTRING(包括LUA_TSTRING)之后的数据类型,都需要进行gc操作.
#define iscollectable(o)	(rttype(o) & BIT_ISCOLLECTABLE)


/* Macros for internal tests */
#define righttt(obj)		(ttype(obj) == gcvalue(obj)->gch.tt)

#define checkliveness(g,obj) \
lua_longassert(!iscollectable(obj) || \
(righttt(obj) && !isdead(g,gcvalue(obj))))


/* Macros to set values */
#define settt_(o,t)	((o)->tt_=(t))

#define setnvalue(obj,x) \
{ TValue *io=(obj); num_(io)=(x); settt_(io, LUA_TNUMBER); }

#define setnilvalue(obj) settt_(obj, LUA_TNIL)

#define setfvalue(obj,x) \
{ TValue *io=(obj); val_(io).f=(x); settt_(io, LUA_TLCF); }

#define setpvalue(obj,x) \
{ TValue *io=(obj); val_(io).p=(x); settt_(io, LUA_TLIGHTUSERDATA); }

#define setbvalue(obj,x) \
{ TValue *io=(obj); val_(io).b=(x); settt_(io, LUA_TBOOLEAN); }

#define setgcovalue(L,obj,x) \
{ TValue *io=(obj); GCObject *i_g=(x); \
val_(io).gc=i_g; settt_(io, ctb(gch(i_g)->tt)); }

#define setsvalue(L,obj,x) \
{ TValue *io=(obj); \
TString *x_ = (x); \
val_(io).gc=cast(GCObject *, x_); settt_(io, ctb(x_->tsv.tt)); \
checkliveness(G(L),io); }

#define setuvalue(L,obj,x) \
{ TValue *io=(obj); \
val_(io).gc=cast(GCObject *, (x)); settt_(io, ctb(LUA_TUSERDATA)); \
checkliveness(G(L),io); }

#define setthvalue(L,obj,x) \
{ TValue *io=(obj); \
val_(io).gc=cast(GCObject *, (x)); settt_(io, ctb(LUA_TTHREAD)); \
checkliveness(G(L),io); }

#define setclLvalue(L,obj,x) \
{ TValue *io=(obj); \
val_(io).gc=cast(GCObject *, (x)); settt_(io, ctb(LUA_TLCL)); \
checkliveness(G(L),io); }

#define setclCvalue(L,obj,x) \
{ TValue *io=(obj); \
val_(io).gc=cast(GCObject *, (x)); settt_(io, ctb(LUA_TCCL)); \
checkliveness(G(L),io); }

#define sethvalue(L,obj,x) \
{ TValue *io=(obj); \
val_(io).gc=cast(GCObject *, (x)); settt_(io, ctb(LUA_TTABLE)); \
checkliveness(G(L),io); }

#define setdeadvalue(obj)	settt_(obj, LUA_TDEADKEY)



#define setobj(L,obj1,obj2) \
{ const TValue *io2=(obj2); TValue *io1=(obj1); \
io1->value_ = io2->value_; io1->tt_ = io2->tt_; \
checkliveness(G(L),io1); }


/*
 ** different types of assignments, according to destination
 */

/* from stack to (same) stack */
#define setobjs2s	setobj
/* to stack (not from same stack) */
#define setobj2s	setobj
#define setsvalue2s	setsvalue
#define sethvalue2s	sethvalue
#define setptvalue2s	setptvalue
/* from table to same table */
#define setobjt2t	setobj
/* to table */
#define setobj2t	setobj
/* to new object */
#define setobj2n	setobj
#define setsvalue2n	setsvalue


/* check whether a number is valid (useful only for NaN trick) */
#define luai_checknum(L,o,c)	{ /* empty */ }


/*
 ** {======================================================
 ** NaN Trick
 ** =======================================================
 */
#if defined(LUA_NANTRICK)

/*
 ** numbers are represented in the 'd_' field. All other values have the
 ** value (NNMARK | tag) in 'tt__'. A number with such pattern would be
 ** a "signaled NaN", which is never generated by regular operations by
 ** the CPU (nor by 'strtod')
 */

/* allows for external implementation for part of the trick */
#if !defined(NNMARK)	/* { */


#if !defined(LUA_IEEEENDIAN)
#error option 'LUA_NANTRICK' needs 'LUA_IEEEENDIAN'
#endif


#define NNMARK		0x7FF7A500
#define NNMASK		0x7FFFFF00

#undef TValuefields
#undef NILCONSTANT

#if (LUA_IEEEENDIAN == 0)	/* { */

/* little endian */
#define TValuefields  \
union { struct { Value v__; int tt__; } i; double d__; } u
#define NILCONSTANT	{{{NULL}, tag2tt(LUA_TNIL)}}
/* field-access macros */
#define v_(o)		((o)->u.i.v__)
#define d_(o)		((o)->u.d__)
#define tt_(o)		((o)->u.i.tt__)

#else				/* }{ */

/* big endian */
#define TValuefields  \
union { struct { int tt__; Value v__; } i; double d__; } u
#define NILCONSTANT	{{tag2tt(LUA_TNIL), {NULL}}}
/* field-access macros */
#define v_(o)		((o)->u.i.v__)
#define d_(o)		((o)->u.d__)
#define tt_(o)		((o)->u.i.tt__)

#endif				/* } */

#endif			/* } */


/* correspondence with standard representation */
#undef val_
#define val_(o)		v_(o)
#undef num_
#define num_(o)		d_(o)


#undef numfield
#define numfield	/* no such field; numbers are the entire struct */

/* basic check to distinguish numbers from non-numbers */
#undef ttisnumber
#define ttisnumber(o)	((tt_(o) & NNMASK) != NNMARK)

#define tag2tt(t)	(NNMARK | (t))

#undef rttype
#define rttype(o)	(ttisnumber(o) ? LUA_TNUMBER : tt_(o) & 0xff)

#undef settt_
#define settt_(o,t)	(tt_(o) = tag2tt(t))

#undef setnvalue
#define setnvalue(obj,x) \
{ TValue *io_=(obj); num_(io_)=(x); lua_assert(ttisnumber(io_)); }

#undef setobj
#define setobj(L,obj1,obj2) \
{ const TValue *o2_=(obj2); TValue *o1_=(obj1); \
o1_->u = o2_->u; \
checkliveness(G(L),o1_); }


/*
 ** these redefinitions are not mandatory, but these forms are more efficient
 */

#undef checktag
#undef checktype
#define checktag(o,t)	(tt_(o) == tag2tt(t))
#define checktype(o,t)	(ctb(tt_(o) | VARBITS) == ctb(tag2tt(t) | VARBITS))

#undef ttisequal
#define ttisequal(o1,o2)  \
(ttisnumber(o1) ? ttisnumber(o2) : (tt_(o1) == tt_(o2)))


#undef luai_checknum
#define luai_checknum(L,o,c)	{ if (!ttisnumber(o)) c; }

#endif
/* }====================================================== */



/*
 ** {======================================================
 ** types and prototypes
 ** =======================================================
 */

/* 数据栈
 1:Lua中的数据可以这样分为两类:值类型和引用类型。
 2:值类型可以被任意复制,而引用类型共享一份数据,由GC负责维护生命期。
 3:Lua使用一个联合union Value来保存数据
 4:从这里我们可以看到,引用类型用一个指针GCObject *gc来间接引用,
 而其它值类型都直接保存在联合中。
 5:为了区分联合中存放的数据类型,再额外绑定一个类型字段。
 */
union Value {
    GCObject *gc;    /* collectable objects */
    void *p;         /* light userdata */
    int b;           /* booleans */
    lua_CFunction f; /* light C functions */
    numfield         /* numbers */
};
/*
 1:这里,使用繁杂的宏定义,TValuefields 以及numfield是为了应用一个被称为NaN Traic的技巧。
 2:lua_State的数据栈,就是一个TValue的数组。代码中用StkId类型来指代对TValue的引用。
 */
/* 为了区分联合中存放的数据类型,再额外绑定一个类型字段 */
//这些合在一起,最后形成了Lua中的TValue结构体,在Lua中的任何数据都可以通过该结构体进行表示:
struct lua_TValue {
    TValuefields;
};


typedef TValue *StkId;  /* index to stack elements */




/*
 ** Header for string value; string bytes follow the end of this structure
 1:可以看见是一个union,其目的是为了让TString数据类型按照L_Umaxalign类型来进行对齐.
 
 2: 相同的短字符串在同一个 Lua State 中只存在唯一一份,这被称为字符串的内部化.
 
 3: 长字符串则独立存放,从外部压入一个长字符串时,简单复制一遍字符串,并不立刻计算其 hash 值, 而是标记一下 extra 域.直到需要对字符串做键匹配时,才惰性计算 hash 值,加快以后的键比较过程
 */
typedef union TString {
    L_Umaxalign dummy;  /* ensures maximum alignment for strings */
    struct {
        CommonHeader;
        lu_byte extra;  /* reserved words for short strings; "has hash" for longs */
        unsigned int hash; /*可以用来加快字符串的匹配和查找*/
        size_t len;  /* number of characters in string 由于 Lua 并不以 \0 结尾来识别字符串的长度,故需要一个 len 域来记录其长度*/
    } tsv;
} TString;


/* get the actual string (array of bytes) from a TString */
/*字符串的数据内容并没有被分配独立一块内存来保存,而是直接追加在 TString 结构的后面。用 getstr 这个宏就可以取到实际的 C 字符串指针。*/
#define getstr(ts)	cast(const char *, (ts) + 1)

/* get the actual string (array of bytes) from a Lua value */
#define svalue(o)       getstr(rawtsvalue(o))


/*
 ** Header for userdata; memory area follows the end of this structure
 ** UserData的数据部分和字符串一样
 */
typedef union Udata {
    L_Umaxalign dummy;  /* ensures maximum alignment for `local' udata */
    struct {
        CommonHeader;
        struct Table *metatable;
        struct Table *env;
        size_t len;  /* number of bytes */
    } uv;
} Udata;



/*
 ** Description of an upvalue for function prototypes
 *为了让upvalue可比较,函数原型中记录了upvalue的描述信息
 */
typedef struct Upvaldesc {
    TString *name;  /* upvalue name (for debug information) */
    lu_byte instack;  /* whether it is in stack 
                       描述了函数将引用的这个 upvalue 是否恰好处于定义这个函数的函数中.
                       这时,upvalue 是这个外层函数的局部变量,它位于数据栈上
                       */
    lu_byte idx;  /* index of upvalue (in stack or in outer function's list)
                   指的是upvalue 的序号.对于关闭的upvalue,已经无法从栈上取到,idx指外
                   层函数的upvalue表中的索引号;对于在数据栈上的upvalue,序号即变量对应的寄存器号*/
} Upvaldesc;


/*
 ** Description of a local variable for function prototypes
 ** (used for debug information)
 */
typedef struct LocVar {
    TString *varname;
    int startpc;  /* first point where variable is active */
    int endpc;    /* first point where variable is dead */
} LocVar;


/*
 ** Function Prototypes
 */

/*
 其实Proto结构体才是Lua解释器分析Lua脚本过程中最重要的数据结构,
 而Closure,CallInfo结构体不过是这个过程为了提供给Lua虚拟机执行
 指令装载Proto结构体用的中间载体,最终用到的还是Proto结构体中得数据.
 
 向它(中间变量)里面写入数据,待分析完毕之后,这个在分析过程中临时使用的数据就不再使用了,
 这个结构体就是FuncState,这里也仅是列举出它里面与指令相关的成员:
 
    Proto *f:绑定的Proto结构体.
 
    int pc:当前的pc索引,类似于CPU执行指令时的计数器,在这里这个变量用于保存当前已经向Proto的code数组写入了多少数据.
 
    int freereg:当前的可用寄存器索引.

 由上面的分析可知,当执行Lua解释器对一个Lua脚本文件进行分析之后,最终返回的就是一个Proto结构体指针,但是需要注意的是,前面对Proto结构体的说明中提过,这个结构体与Lua函数一一对应,但是这个说法并不是特别精确,比如我们这里的测试代码打印一个"Hello World"字符串,这就不是一个函数,但是即便如此,分析完这个文件之后也会有一个对应的Proto结构体,因此需要把Lua函数的概念扩大到Lua模块,可以说一个Lua模块也是一个Lua函数,对一个Lua模块分析也是相当于对一个Lua函数进行分析,只不过这个函数没有参数,没有函数名.
 */
typedef struct Proto {
    CommonHeader;        //Lua通用数据相关的成员
    TValue *k;           //存放function常量的数组
    Instruction *code;   //存放指令(bytecode)的数组
    struct Proto **p;    //如果本函数中还有内部定义的函数,那么这些内部函数相关的Proto指针就放在这个数组里.
    int *lineinfo;       //存放对应指令的行号信息,与前面的code数组内的元素一一对应.
    LocVar *locvars;     //存放函数内局部变量的数组.
    Upvaldesc *upvalues;  /* upvalue information */
    union Closure *cache;  /*闭包重用(弱引用) last created closure with this prototype */
    TString  *source;    //Lua脚本文件名
    int sizeupvalues;    //前面upvalues数组的大小.
    int sizek;           //k数组的大小.
    int sizecode;        //code数组的大小.
    int sizelineinfo;    //lineinfo数组的大小.
    int sizep;                     //p数组的大小
    int sizelocvars;               //localvars数组的大小
    int linedefined;     //函数body的第一行行号.
    int lastlinedefined; //函数body的最后一行.
    GCObject *gclist;              //gc链表
    lu_byte numparams;   //函数参数数量.
    lu_byte is_vararg;   //有以下几种取值 #define VARARG_HASARG 1 / VARARG_ISVARARG 2 / VARARG_NEEDSAR 4 标识了这个函数是否需要不定数量的参数
    lu_byte maxstacksize;//函数最大stack大小,由类型是lu_byte可知,最大是255.
} Proto;



/*
 ** Lua Upvalues
 */
typedef struct UpVal {
    CommonHeader;
    TValue *v;  /* points to stack or to its own value */
    union {
        TValue value;  /* the value (when closed) */
        struct {  /* double linked list (when open) */
            struct UpVal *prev;
            struct UpVal *next;
        } l;
    } u;
} UpVal;


/*
 ** Closures
 */

#define ClosureHeader \
CommonHeader; lu_byte nupvalues; GCObject *gclist

typedef struct CClosure {
    ClosureHeader;
    lua_CFunction f;
    TValue upvalue[1];  /* list of upvalues */
} CClosure;


typedef struct LClosure {
    ClosureHeader;
    struct Proto *p;
    UpVal *upvals[1];  /* list of upvalues */
} LClosure;

//1:当执行Lua解释器对一个Lua脚本文件进行分析之后,最终返回的就是一个Proto结构体指针
//2:返回Proto指针之后,将会创建一个Closure结构体
typedef union Closure {
    CClosure c; //C函数相关信息
    LClosure l; //Lua函数相关信息.
} Closure;


#define isLfunction(o)	ttisLclosure(o)

#define getproto(o)	(clLvalue(o)->p)

/*******************************************************************/
/********************Table表的数据结构********************************/

/*
 因为这样已经足以表达TKey所需要的使命:tvk用来存储key的数值,而next用来存储Hash桶中的下一个结点.
 可是,在Lua的代码中针对这个数据结构如此"曲折"的定义,实际上是为了对齐并且节省空间的考量.
 */
typedef union TKey {
    struct {
        TValuefields;
        struct Node *next;  /* for chaining */
    } nk;
    TValue tvk;
} TKey;

/*
 Lua表中结点的数据类型.首先从Node的类型定义可以看出,它包含两个成员,
 其中一个表示结点的key,另一个表示结点的value.值部分就不多解释了,
 还是Lua中的通用数据类型TValue
 */
typedef struct Node {
    TValue i_val;
    TKey i_key;
} Node;

/*
  这里注意一个细节,lsizenode使用的时byte类型,而sizearray使用的是int类型.
 由于在Lua的Hash桶部分,每个Hash值相同的数据,会以链表的形式串起来在该Hash桶中,
 所以即使数量用完了也不要紧.因此这里使用byte类型,而且是原数据的log2值,
 因为要根据这个值还原回原来的真实数据,也只是需要移位操作罢了,速度很快.
 这个细节中可以看到,在Lua的内存使用方面,还是扣了不少细节的地方.
 */

typedef struct Table {
    CommonHeader;
    lu_byte flags;  /* 1<<p means tagmethod(p) is not present 用于表示在这个表中提供了哪些meta method.在最开始,这个flags是空的,也就是为0,当查找了一次之后,如果该表中存在某个meta method,那么将该meta method对应的flag bit置为1,这样下一次查找时只需要比较这个bit就可以知道了*/
    lu_byte lsizenode;  /* log2 of size of `node' array 该Lua表Hash桶大小的log2值,同时有此可知,Hash桶数组大小一定是2的次方,即如果Hash桶数组要扩展的话,也是以每次在原大小基础上乘以2的形式扩展*/
    struct Table *metatable; /*存放该Lua表的meta表*/
    TValue *array;  /* array part 指向该Lua表的数组部分起始位置的指针*/
    Node *node;      /*指向该Lua表的Hash桶数组起始位置的指针*/
    Node *lastfree;  /* any free position is before this position 指向该Lua表Hash桶数组的最后位置的指针*/
    GCObject *gclist; /*GC相关的链表*/
    int sizearray;  /* size of `array' array  Lua表数组部分的大小*/
} Table;



/*
 ** `module' operation for hashing (size is always a power of 2)
 */
#define lmod(s,size) \
(check_exp((size&(size-1))==0, (cast(int, (s) & ((size)-1)))))


#define twoto(x)	(1<<(x))
#define sizenode(t)	(twoto((t)->lsizenode))


/*
 ** (address of) a fixed nil value
 */
#define luaO_nilobject		(&luaO_nilobject_)


LUAI_DDEC const TValue luaO_nilobject_;


LUAI_FUNC int luaO_int2fb (unsigned int x);
LUAI_FUNC int luaO_fb2int (int x);
LUAI_FUNC int luaO_ceillog2 (unsigned int x);
LUAI_FUNC lua_Number luaO_arith (int op, lua_Number v1, lua_Number v2);
LUAI_FUNC int luaO_str2d (const char *s, size_t len, lua_Number *result);
LUAI_FUNC int luaO_hexavalue (int c);
LUAI_FUNC const char *luaO_pushvfstring (lua_State *L, const char *fmt,
                                         va_list argp);
LUAI_FUNC const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaO_chunkid (char *out, const char *source, size_t len);


#endif

