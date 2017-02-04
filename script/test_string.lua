


str = "TTC"
len = string.len(str)

print(len)

--[[

function add(a,b)
return a+b
end

local c = add(1,2)

print(c)

--]]

--[[
function MakeCounter()
local t = 0
return function()
t = t+1
return t
end
end

local a = MakeCounter
print()

--]]