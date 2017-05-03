local co = coroutine.create(function () 
      for  i=1 ,10 do
         print("co=>"..i)
         coroutine.yield()
      end
end)

print(co)

--print("1==>"..coroutine.status(co))

--coroutine.resume(co)

--print("2==>"..coroutine.status(co))

--coroutine.resume(co)

--print("3==>"..coroutine.status(co))

for i = 1,10 do
    coroutine.resume(c1)
    print(i.."==>"..coroutine.status(co))

end
