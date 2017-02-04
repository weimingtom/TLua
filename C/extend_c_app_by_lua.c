/*
	
	Example:
	$gcc -Wall extend_c_app_by_lua.c -llua -lm -ldl -o extend_c_app_by_lua
	$./extend_c_app_by_lua 
	stack size = 0
	w=10,h=20
	r=76,g=25,b=0
	result=0.239713

	Analyse:
 
 
 
  C 中调用   Lua中的数据结构 变量 函数

*/
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "util.h"

#define MAX_COLOR  255

static char *fname = "/Users/administrator/Desktop/studySpace/studyC/LuaBriC/LuaBriC/C/TestConfig.lua";

struct ColorTable{
	char *name;
	unsigned char red,green,blue;
}colortable[] = {
	{"WHITE",MAX_COLOR,MAX_COLOR,MAX_COLOR},
	{"RED",MAX_COLOR,0,0},
	{"GREEN",0,MAX_COLOR,0},
	{"BLUE",0,0,MAX_COLOR},
	{NULL,0,0,0},
};



void get_globar_var(lua_State *L, int *w,int *h,int *x,int *y)
{
	/*push global var in stack*/
    lua_getglobal(L, "pointx");
    lua_getglobal(L, "pointy");
	lua_getglobal(L,"width");
	lua_getglobal(L,"height");
    
    if (!lua_isnumber(L,-4))
        error(L,"x should be a number\n");
    
    if (!lua_isnumber(L,-3))
        error(L,"y should be a number\n");
	if (!lua_isnumber(L,-2))
		error(L,"width should be a number\n");

	if (!lua_isnumber(L,-1))
		error(L,"height should be a number\n");
    *x = lua_tonumber(L,-4);
    *y = lua_tonumber(L,-3);
	*w = lua_tonumber(L,-2);
	*h = lua_tonumber(L,-1);
}

/*use global var in c*/
void test_global_var(lua_State *L)
{
	int w = 0,h = 0,x = 0 , y = 0;

	get_globar_var(L,&w,&h,&x,&y);

	printf("w=%d,h=%d,x=%d,y=%d\n",w,h,x,y); /*w=10,h=20,x=100,y=200*/

}

/*get field from table in stack top*/
static int getfield(lua_State *L, const char *key)
{
    printf("stack size = %d\n",lua_gettop(L)); /*stack size = 0*/

	int result;

	if(!lua_istable(L,-1))
		error(L,"the element in stack top is not a table in get key %s\n",key);
	
	/*next two lines can be replaced by lua_getfield(L,-1,key)*/
	lua_pushstring(L,key);
    printf("stack size = %d\n",lua_gettop(L)); /*stack size = 0*/

	lua_gettable(L,-2);

	if(!lua_isnumber(L,-1))
		error(L,"invalid component in background color");

	result = (int)(lua_tonumber(L,-1) * MAX_COLOR);
	lua_pop(L,1);  /* lua_pop(L,n) == lua_settop(L, -(n)-1)*/

	return result;
}

/*use table in c*/
void test_table(lua_State *L)
{
	int red,green,blue;
	lua_getglobal(L,"background");
//    const char* typename = lua_typename(L,-1);
//    printf("typename==%s",typename);
    int type = lua_type(L, -1);
    printf("type===%d \n",type);
    const char *typename = lua_typename(L, type);
    printf("typename==%s \n",typename);

	if (lua_isstring(L,-1))
	{
		const char *colorname = lua_tostring(L,-1);
		int i ;
		for(i = 0; colortable[i].name != NULL; i++)
			if (strcmp(colortable[i].name,colorname) == 0)
				break;

			if (colortable[i].name == NULL)
				error(L,"invaild color name (%s)",colorname);
			else
			{
				red = colortable[i].red;
				green = colortable[i].green;
				blue = colortable[i].blue;
			}
	}
	else if(lua_istable(L,-1))
	{
		red = getfield(L,"r");
		green = getfield(L,"g");
		blue = getfield(L,"b");
	}
	else
		error(L,"invaild value for 'background'");

	printf("r=%d,g=%d,b=%d\n",red,green,blue);
}


/*use lua function in c*/
void test_lua_function(lua_State *L)
{
	double x = 0.5, y = 0.5;
	double result;

	lua_getglobal(L,"luaAdd");  /*push f function in stack*/
    printf("stack size = %d\n",lua_gettop(L)); /*stack size = 0*/

	lua_pushnumber(L,x);  /*push two parameter */
    printf("stack size = %d\n",lua_gettop(L)); /*stack size = 0*/
	lua_pushnumber(L,y);
    printf("stack size = %d\n",lua_gettop(L)); /*stack size = 0*/

	if(lua_pcall(L,2,1,0) != 0)
		error(L,"error running function 'f':%s\n",lua_tostring(L,-1));

	if (!lua_isnumber(L,-1))
		error(L,"function 'f' must be return number\n");
	
	result = lua_tonumber(L,-1);
	printf("result=%g\n",result); /*result=0.25*/
}
int main0004(void)
{
	lua_State *L = luaL_newstate();
	load(L,fname); /*load confile file*/
	printf("stack size = %d\n",lua_gettop(L)); /*stack size = 0*/

//	lua_settop(L,0); /*clear stack*/
//	test_global_var(L);  /*test TestConfig.lua*/
//    printf("stack size = %d\n",lua_gettop(L)); /*stack size = 0*/


//	lua_settop(L,0); /*clear stack*/
//	test_table(L);
//
	lua_settop(L,0); /*clear stack*/
	luaL_openlibs(L);
	test_lua_function(L);

	return 0;
}
