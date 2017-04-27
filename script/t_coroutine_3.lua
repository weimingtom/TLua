function foo(a)
    print("foo", a)
    return coroutine.yield(2 * a)
end

co = coroutine.create(function ( a, b  )
    print("co-body", a, b)
    local r = foo(a + 1)
    print("co-body", r)
    local r, s = coroutine.yield(a + b, a - b)
    print("co-body", r, s)
    return b, "end"
end)

print("main1", coroutine.resume(co, 1, 10))
print("main2", coroutine.resume(co, "r"))
print("main3", coroutine.resume(co, "x", "y"))
print("main4", coroutine.resume(co, "x", "y"))

