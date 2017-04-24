do
    local var, limit, step = tonumber(e1), tonumber(e2), tonumber(e3) 
    if not (var and limit and step) then error() end
    while (step > 0 and var <= limit) or (step <= 0 and var >= limit)
        do  
            local v = var block
            var = var + step  
        end
    end
