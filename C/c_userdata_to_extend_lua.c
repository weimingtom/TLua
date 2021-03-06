/*
	Example:

	Analyse:
*/
#include "c_userdata_to_extend_lua.h"


#define BITS_PER_WORD (CHAR_BIT * sizeof(unsigned int))
#define I_WORD(i)     ((unsigned int)(i)/BITS_PER_WORD)
#define I_BIT(i)      (1 << ((unsigned int)(i)%BITS_PER_WORD))

#define UDATA_MTABLE_NAME "MyArray"
#define checkarray(L) (NumArray*)luaL_checkudata(L,1,UDATA_MTABLE_NAME)


typedef struct NumArray
{
	int size;
	unsigned int values[1];
}NumArray;

static int newarray(lua_State *L)
{
	int i,n;
	size_t nbytes;
	NumArray *a;

	n = luaL_checkint(L,1);
	luaL_argcheck(L,n>=1,1,"invalid size");

	nbytes = sizeof(NumArray) + I_WORD(n - 1) * sizeof(unsigned int);

	a = (NumArray*)lua_newuserdata(L,nbytes);

	a->size = n;
	for (i = 0; i <=I_WORD(n - 1); i++)
		a->values[i] = 0;

	/*set metatable for userdata*/
	luaL_getmetatable(L,UDATA_MTABLE_NAME);
	lua_setmetatable(L,-2);

	return 1; /*new userdata in stack top*/
}

static unsigned int *getindex(lua_State *L,unsigned int *mask)
{
	NumArray *a = checkarray(L);
	int index = luaL_checkint(L,2) - 1;

	luaL_argcheck(L,(index >= 0 && index < a->size),1,"index out of range");

	/*return*/
	*mask = I_BIT(index);
	return &(a->values[I_WORD(index)]);
}

static int setarray(lua_State *L)
{
	unsigned int mask;
	unsigned int *entry = getindex(L,&mask);
	luaL_checkany(L,3);  /*ensure have a parameter*/

	if(lua_toboolean(L,3))
		(*entry) |= mask;
	else
		(*entry) &= (~mask);

	return 0;
}

static int getarray(lua_State *L)
{
	unsigned int mask;
	unsigned int *entry = getindex(L,&mask);

	lua_pushboolean(L,(*entry) & mask);

	return 1;
}

static int getsize(lua_State *L)
{
	NumArray *a = checkarray(L);
	luaL_argcheck(L,a != NULL,1,"bit array expected");

	lua_pushinteger(L,a->size);

	return 1;
}

static int array2string(lua_State *L)
{
	NumArray *a = checkarray(L);
	luaL_argcheck(L,a != NULL,1,"bit array expected");

	lua_pushfstring(L,"array(%d)",a->size);

	return 1;
}

static const struct luaL_Reg arraylib_f[] = {
	{"new",newarray},
	{NULL,NULL},
};

static const struct luaL_Reg arraylib_m[] = {
	{"set",setarray},
	{"get",getarray},
	{"size",getsize},
	{"__tostring",array2string},
	{NULL,NULL},
};

int luaopen_arraylib(lua_State *L)
{
	luaL_newmetatable(L,UDATA_MTABLE_NAME);
	
	/*metatable.__index = metatable*/
	lua_pushvalue(L,-1);
	lua_setfield(L,-2,"__index");

	/*save interface in metatable*/
    //5.1  version
//	luaL_register(L,NULL,arraylib_m);
//  luaL_register(L,"array",arraylib_f);
    //version 5.2
    lua_newtable(L);
    luaL_setfuncs(L, arraylib_m, 0);
    lua_newtable(L);
    luaL_setfuncs(L, arraylib_f, 0);

	return 1;
}
