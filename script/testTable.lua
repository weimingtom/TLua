function print_ipairs(t)
    for k, v in ipairs(t) do
        print(k)
    end
end

local t = {}

t[1] = 0
t[100] = 0

print_ipairs(t)