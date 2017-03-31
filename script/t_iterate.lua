function values(t)
    local i = 0
    return function()
        i=i+1
        return t[i]
    end
end


t = {10,200,3000}

iter  = values(t)

while true do
    local elem = iter()
    if elem == nil then
        break
    end
    print(elem)
end
