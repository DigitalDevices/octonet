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

if arg[1] then
  query = arg[1]
  if query == "get" then query = "" end
  proto = "HTTP/1.0"
  host = "local"
end

os.execute("rm -rf /config/channels")

http_print(proto.." 303")
http_print("Location: http://"..host.."/reboot.html")
http_print()
 
