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

local path = nil
local tmp = "/var/tmp/xxxx"

if string.match(query,"select=user") then
  local file = io.open("/config/channels/sourcelist.csv")
  if file then
    path = "/config/channels"
    file:close()
  else
    path = "/var/channels"
  end
elseif string.match(query,"select=system") then
  path = "/var/channels"
else
  SendError("404","channel list not found")
  return
end

local gz = nil

os.execute("cd ".. path .. ";tar -cf " .. tmp .. ".tar" .. " *.csv *.txt");

local p = io.popen("gzip -c " .. tmp .. ".tar")
if p then
  gz = p:read("*a")
  p:close()
end

if gz then
  http_print(proto.." 200")
  http_print("Pragma: no-cache")
  http_print("Cache-Control: no-cache")
  http_print("Content-Type: application/gzip")
  http_print('Content-Disposition: filename="channels.tar.gz"')
  http_print(string.format("Content-Length: %d",#gz))
  http_print()
  io.stdout:write(gz)
else
  SendError("500","internal error")
end


