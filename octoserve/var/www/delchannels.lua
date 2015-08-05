#!/usr/bin/lua

local host = os.getenv("HTTP_HOST")
local proto = os.getenv("SERVER_PROTOCOL")
local query = os.getenv("QUERY_STRING")

if arg[1] then
  query = arg[1]
  if query == "get" then query = "" end
  proto = "HTTP/1.0"
  host = "local"
end

function SendError(err,desc)
  io.stdout:write(proto.." "..err.."\r\n")
  io.stdout:write("\r\n")
  local file = io.open("e404.html")
  if file then
    local tmp = file:read("*a")
    tmp = string.gsub(tmp,"404 Not Found",err .. " " .. desc)
    io.stdout:write(tmp)
    file:close()
  end
end


os.execute("rm -rf /config/channels")

io.stdout:write(proto.." 303".."\r\n")
io.stdout:write("Location: http://"..host.."/reboot.html".."\r\n")
io.stdout:write("\r\n")
 
