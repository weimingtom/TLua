--


function lua_func (x, y)
    print("Parameters are: ", x, y)
    return (x^2 * math.sin(y))/(1-x)
end