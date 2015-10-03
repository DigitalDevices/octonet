#!/usr/bin/lua

local socket = require("socket")
local url = require("socket.url")

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

function SendError(err,desc)
  http_print(proto.." "..err)
  http_print("Content-Type: text/html")
  http_print()
  local file = io.open("e404.html")
  if file then
    local tmp = file:read("*a")
    tmp = string.gsub(tmp,"404 Not Found",err .. " " .. desc)
    http_print(tmp)
    file:close()
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
      local valid = false
      local path = url.parse("http://"..userver)
      if path.host then
         local ip = socket.dns.toip(path.host)
         if ip == nil then
            ip = path.host
         end
         local p1,p2 = ip:match("(%d+)%.(%d+)%.%d+%.%d+")
         p1 = tonumber(p1)
         p2 = tonumber(p2)
         valid = (p1 == 10) or ((p1 == 172) and (p2 >= 16) and (p2 <= 31)) or ((p1 == 192) and (p2 == 168))          
      end
      if valid then
         local file = io.open("/config/updateserver","w")
         if file then
            file:write(userver.."\n")
            file:close()
            delimages = true
         end
      else
         SendError(400, "Invalid or not local: ".. userver)
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

