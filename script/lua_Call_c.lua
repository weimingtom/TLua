

--Lua调用C函数其实就是用C编写Lua的扩展，使用C为Lua编写扩展也非常简单。所有C扩展的函数都有一个固定的函数原型，如下所示：
ret = mysin(30)
print("Lua Call C ret===>",ret)
