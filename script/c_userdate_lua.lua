--c_userdate_lua.lua
array = require "array"
a = array.new(1000)
print(a)
print(array.size(a))
for i=1,999 do
    array.set(a, i, 1/i)
end


print ("Print first 10 elements")
for i=1,10 do
    print(array.get(a, i) )
end