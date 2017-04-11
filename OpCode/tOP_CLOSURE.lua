function counter()
    local t = 0
    return function()
        t = t + 1
        return t
    end
end
local a = counter()
local b = counter()
print(a)
print(a())
print(a())
print(b)
print(b())
print(b())

