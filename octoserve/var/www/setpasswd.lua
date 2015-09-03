#!/usr/bin/lua

local host = os.getenv("HTTP_HOST")
local proto = os.getenv("SERVER_PROTOCOL")
local query = os.getenv("QUERY_STRING")
local method = os.getenv("REQUEST_METHOD")
local clength = os.getenv("CONTENT_LENGTH")
local ctype = os.getenv("CONTENT_TYPE")

function http_print(s)
  if s then
    io.stdout:write(tostring(s).."\r\n")
  else
    io.stdout:write("\r\n")
  end
end

if #arg> 0 then
  method="GET"
  query="admin"
  proto = "HTTP/1.0"
end

function SendError(err,desc)
  http_print(proto.." "..err)
  http_print()
  local file = io.open("e404.html")
  if file then
    local tmp = file:read("*a")
    tmp = string.gsub(tmp,"404 Not Found",err .. " " .. desc)
    http_print(tmp)
    file:close()
  end
end


if method == "GET" then
  file = io.open("/tmp/pw","w")
  if file then 
    file:write(query.."\n")
    file:write(query.."\n")
    file:close()
    os.execute('passwd admin </tmp/pw >/dev/null');
    os.execute('rm -f /tmp/pw >/dev/null');
  end
  http_print(proto.." 204")
  http_print()
  return
  
else
  SendError("500","What")  
end
