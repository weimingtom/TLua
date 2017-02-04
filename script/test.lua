--test.lua

local base = {}

function base.new()
    print("base.new---called")
    local obj = {}
    setmetatable(obj,{__index = base})
    return obj
end

function base.sayHello()
    print("say hello ttc~~!")
end



local test = {}

--setmetatable(test,{__index = base})

function test.new()
    print("test.new---called")
    local obj = {}
    setmetatable(obj,{__index = base})
    return obj
end


print("Hello World")

local a = test.new()
a.new()