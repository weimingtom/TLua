/*
** $Id: llex.h,v 1.72.1.1 2013/04/12 18:48:47 roberto Exp $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/

/*
 *  词法分析器 : 词法分析 (API以LuaX 为前缀)
 */
#ifndef llex_h
#define llex_h

#include "lobject.h"
#include "lzio.h"


#define FIRST_RESERVED	257



/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER RESERVED"
* 这里的每个字符串都是与某个保留字Token类型一一对应的
*/
enum RESERVED {
  /* terminal symbols denoted by reserved words */
  TK_AND = FIRST_RESERVED, TK_BREAK,
  TK_DO, TK_ELSE, TK_ELSEIF, TK_END, TK_FALSE, TK_FOR, TK_FUNCTION,
  TK_GOTO, TK_IF, TK_IN, TK_LOCAL, TK_NIL, TK_NOT, TK_OR, TK_REPEAT,
  TK_RETURN, TK_THEN, TK_TRUE, TK_UNTIL, TK_WHILE,
  /* other terminal symbols */
  TK_CONCAT, TK_DOTS, TK_EQ, TK_GE, TK_LE, TK_NE, TK_DBCOLON, TK_EOS,
  TK_NUMBER, TK_NAME, TK_STRING
};

/* number of reserved words */
#define NUM_RESERVED	(cast(int, TK_WHILE-FIRST_RESERVED+1))


typedef union {
  lua_Number r;
  TString *ts;
} SemInfo;  /* semantics information 语义信息*/


typedef struct Token {
  int token; /*类型既可以是一个字符,也可以是一个enum RESERVED.enum RESERVED从257开始,就是为了留给字符使用*/
  SemInfo seminfo;
} Token;  /*Token用来表示一个单词,它包括类型token和语义seminfo*/


/* state of the lexer plus state of the parser when shared by all
   functions 
   不仅用于保存当前的词法分析状态信息,而且也保存了整个编译系统的全局状态
 */
typedef struct LexState {
  int current;  /* current character (charint) */
  int linenumber;  /* input line counter */
  int lastline;  /* line of last token `consumed' */
  Token t;  /* current token */
  Token lookahead;  /* look ahead token */
  struct FuncState *fs;/*current function (parser)指向当前正在编译的函数的FuncState*/
  struct lua_State *L;
  ZIO *z;  /* input stream */
  Mbuffer *buff;  /* buffer for tokens */
  struct Dyndata *dyd;/*dynamic structures used by the parser指向全局的Dyndata数据*/
  TString *source;  /* current source name */
  TString *envn;  /* environment variable name */
  char decpoint;  /* locale decimal point */
} LexState;


LUAI_FUNC void luaX_init (lua_State *L);
LUAI_FUNC void luaX_setinput (lua_State *L, LexState *ls, ZIO *z,
                              TString *source, int firstchar);
LUAI_FUNC TString *luaX_newstring (LexState *ls, const char *str, size_t l);
LUAI_FUNC void luaX_next (LexState *ls);
LUAI_FUNC int luaX_lookahead (LexState *ls);
LUAI_FUNC l_noret luaX_syntaxerror (LexState *ls, const char *s);
LUAI_FUNC const char *luaX_token2str (LexState *ls, int token);


#endif
