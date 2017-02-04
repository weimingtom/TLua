--test  cjson model

local json = require "json"

local user_xx = {}
user_xx["1"]  = 22
user_xx["2"]  = 33
user_xx["3"]  = 44
user_xx["4"]  = 55

local usr = {}

usr["name"]  = "liutian"
usr["age"]  = 22
usr["user_xx"]  = user_xx

-- encode to json 
local usr_encode = json.encode(usr)

if usr_encode then
	print("usr_encode: "..usr_encode)
end


--decode json to Lua value
local  usr_msg = json.decode(usr_encode)

if usr_msg then
	print("name=>",usr_msg["name"])
	print("age=>",usr_msg.age)
	local user_xx = usr_msg["user_xx"]
	print("user_xx...")
	for k,v in pairs(user_xx) do
		print(k,v)
	end
end