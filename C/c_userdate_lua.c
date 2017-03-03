//
//  c_userdate_lua.c
//  LuaSrc
//
//  Created by TTc on 15-2-9.
//  Copyright (c) 2015年 LuaSrc. All rights reserved.
//

#include "c_userdate_lua.h"

//我们通过使用C语言实现一个Lua数组来演示Lua实现自定义用户数据。数组的结构如下所示：
typedef struct NumArray
{
    int size;        //表示数组的大小
    double values[]; //此处的values仅代表一个double*类型的指针，values指向NumArray结构后部紧跟的数据的地址
} NumArray;

/* 新建array */
static int
newarray (lua_State *L){
    int n = luaL_checkint(L, -1);
    size_t nbytes = sizeof(NumArray) + n*sizeof(double);
    NumArray *a = (NumArray *)lua_newuserdata(L, nbytes); //新建一个大小为nbytes的userdata并压入堆栈。
    a->size = n; //初始化NumArray的大小
    return 1;
}

/* 设置array中的数值 */
static int
setarray(lua_State *L)
{
    NumArray *a = (NumArray *)lua_touserdata(L, -3); //将堆栈中的userdata读取出来
    int index = luaL_checkint(L, -2);       //读取索引
    double value = luaL_checknumber(L, -1); //读取数值
    luaL_argcheck(L, NULL != a, 1, "'array' expected"); //检查参数的返回，如果第二个表达式为假，则抛出最后一个参数指定的错误信息
    luaL_argcheck(L, index >= 0 && index <= a->size, 1, "index out of range");
    a->values[index - 1] = value; //将lua中写入的数值设置到C数组中
    return 0;
}

/* 读取array中的数值 */
static int
getarray(lua_State *L)
{
    NumArray *a = (NumArray *)lua_touserdata(L, -2); //前面的步骤和setarray中的相同
    int index = luaL_checkint(L, -1);
    
    luaL_argcheck(L, NULL != a, 1, "'array' expected");
    luaL_argcheck(L, index >= 1 && index <= a->size, 1, "index out of range");
    
    lua_pushnumber(L, a->values[index - 1]); //将C数组中的数值压入堆栈
    
    return 1;
}

/* 获取array的大小 */
static int
getsize(lua_State *L)
{
    NumArray *a = (NumArray *)lua_touserdata(L, -1);
    luaL_argcheck(L, NULL != a, 1, "'array' expected");
    
    lua_pushnumber(L, a->size);
    
    return 1;
}

int
arraylib(lua_State *L)
{
    luaL_Reg l[] = {
        {"new", newarray},
        {"set", setarray},
        {"get", getarray},
        {"size", getsize},
        {NULL, NULL}
        
    };
    luaL_newlib(L, l);
    return 1;
}

void
test_c_userdate_lua(lua_State *L,const char *root)
{
    char path [128];
    memset(path, '\0', sizeof(path));
    strcat(path, root);
    strcat(path, "c_userdate_lua.lua");
    printf("fname => %s \n",path);

    luaL_requiref(L, "array", arraylib, 0);

    load(L, path);
}
