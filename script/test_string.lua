


str = "TTC1"
len = string.len(str)

print(len)



function add(a,b)
return a+b
end

local c = add(1,2)

print(c)




function MakeCounter()
print("MakeCounter=>")
local t = 0
    return function()
        print("tttt")
        t = t+1
    return t
    end
end

MakeCounter()
local a = MakeCounter
a()
print("---------")
print(a)

