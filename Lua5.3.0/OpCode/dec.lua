-- Decompiled using luadec 2.2 rev: 895d923 for Lua 5.3 from https://github.com/viruscamp/luadec
-- Command line: coroutine.out 

-- params : ...
-- function num : 0 , upvalues : _ENV
print_array = function(t)
  -- function num : 0_0 , upvalues : _ENV
  if not t then
    for _,value in ipairs({}) do
      (io.write)(value, " ")
    end
    ;
    (io.write)("\n")
  end
end

genpermu = function(t, n)
  -- function num : 0_1 , upvalues : _ENV
  if not n then
    n = #t
  end
  if n <= 1 then
    (coroutine.yield)(t)
  else
    for i = n, 1, -1 do
      t[i] = t[n]
      genpermu(t, n - 1)
      t[i] = t[n]
    end
  end
end

permutations = function(a)
  -- function num : 0_2 , upvalues : _ENV
  local co = (coroutine.create)(function()
    -- function num : 0_2_0 , upvalues : _ENV, a
    genpermu(a)
  end
)
  return function()
    -- function num : 0_2_1 , upvalues : _ENV, co
    local code, res = (coroutine.resume)(co)
    return res
  end

end

for p in permutations({"a", "b", "c"}) do
  print_array(p)
end

