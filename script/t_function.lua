function values(t)
    local i = 0
    return function()
        i=i+1
        return t[i]
    end
end



t = {10,200,3000}

iter  = values(t)

for elem in iter do
    print(elem)
end

iter1  = values(t)

for elem in iter1 do
    print("----------")
    print(elem)
end

