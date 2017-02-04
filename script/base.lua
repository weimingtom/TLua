--base.lua


local base = {}

function base.new()
    local obj = {}
    setmetatable(obj,{__index = base})
    return obj
end

function base.sayHello()
    print("say hello ttc~~!")
end


return base