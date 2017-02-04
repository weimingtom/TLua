-- 注意这里不能使用calc.lua来命名 这样会使得require在查找calc时出现死循环

local calc = require "calc"

print(calc.add(20, 40))
print(calc.sub(20, 40))

