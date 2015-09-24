#!/usr/bin/lua

local host = os.getenv("HTTP_HOST")
local proto = os.getenv("SERVER_PROTOCOL")
local query = os.getenv("QUERY_STRING")

function http_print(s)
  if s then
    io.stdout:write(tostring(s).."\r\n")
  else
    io.stdout:write("\r\n")
  end
end

local hex_to_char = function(x)
   return string.char(tonumber(x,16))
end

local userver = "download.digital-devices.de/download/linux"
local data = nil
local delimages = false

if query == "set=beta" then
  local file = io.open("/config/updateserver","w")
  if file then
    file:write(userver.."/beta".."\n")
    file:close()
    delimages = true
  end
elseif query == "set=std" then
   local file = io.open("/config/updateserver","r")
   if file then
     file:close()
     os.remove("/config/updateserver")
     delimages = true
   end
elseif query:sub(1,4) == "set=" then
    userver = query:sub(5)
    if userver ~= "" then
      userver = userver:gsub("%%(%x%x)",hex_to_char)
      -- userver = userver:gsub("+"," ")
      local file = io.open("/config/updateserver","w")
      if file then
         file:write(userver.."\n")
         file:close()
         delimages = true
      end
    else
      os.remove("/config/updateserver")
      delimages = true
    end
else
   data = "{ \"UpdateServer\":\""
   local file = io.open("/config/updateserver","r")
   if file then
      local tmp = file:read("*l")
      file:close()
      if tmp ~= userver.."/beta" then
         data = data .. tmp
      end      
   end
   data = data.."\" }"   
end

if delimages then
    os.execute("rm -f /config/octonet.*.img")
    os.execute("rm -f /config/octonet.*.sha")
end

if data then
   http_print(proto.." 200" )
   http_print("Pragma: no-cache")
   http_print("Cache-Control: no-cache")
   http_print("Content-Type: application/json; charset=UTF-8")
   http_print(string.format("Content-Length: %d",#data))
   http_print()
   http_print(data)
else
   http_print(proto.." 303")
   http_print("Location: http://"..host.."/update.html")
   http_print("")
end

