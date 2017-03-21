function MakeCounter()
local t = 0
    return function()
        t = t+1
    return t
    end
end

foo = MakeCounter()
foo1 = MakeCounter()
print(foo)
print(foo())
print(foo())
print(foo1)
print(foo1())
print(foo1())
print("------------------------")

function printHello(a,b)  --a,b为参数 
    print(a,b); 
    print("Hello") 
end 

printHello(10,20)


print("-------------------------")

printTTc = function (a,b) 
    return a+b end

local result = printTTc(20,30)
print(result)
