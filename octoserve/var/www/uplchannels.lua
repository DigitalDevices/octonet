#!/usr/bin/lua

local host = os.getenv("HTTP_HOST")
local proto = os.getenv("SERVER_PROTOCOL")
local query = os.getenv("QUERY_STRING")
local method = os.getenv("REQUEST_METHOD")
local clength = os.getenv("CONTENT_LENGTH")
local ctype = os.getenv("CONTENT_TYPE")

if arg[1] then
  query = arg[1]
  if query == "get" then query = "" end
  proto = "HTTP/1.0"
  host = "local"
end

function SendError(err,desc)
  io.stdout:write(proto.." "..err.."\r\n")
  io.stdout:write("Content-Type: text/html".."\r\n")
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

if method ~= "POST" or not clength or not ctype then
  SendError("404","?")
  return
end

if not string.match(ctype,"multipart/form%-data") then
  SendError("404","??")
  return
end

local boundary = string.match(ctype,"boundary=(.*)") 
if not boundary then
  SendError("404","???")
  return
end

local filename = nil

while true do
  local line = io.stdin:read()
  line = string.gsub(line,"\r","")
  if line == "" then break end
  if not filename then
    filename = string.match(line,'filename=%"(.*)%"')
  end
end

data = io.stdin:read("*a")
data = string.sub(data,1,#data - #boundary - 4)

local file = io.open("/tmp/"..filename,"w")
if file then
  file:write(data)
  file:close()
end

if string.match(filename,"%.tar%.gz$") then
  os.execute("rm -rf /config/channels;mkdir /config/channels;cd /config/channels;gunzip -c /tmp/"..filename.."|tar -xf -");
elseif string.match(filename,"%.zip$") then
  os.execute("rm -rf /config/channels;mkdir /config/channels;cd /config/channels;unzip -q /tmp/"..filename);
end

os.remove("/tmp/"..filename)

-- TODO validate

io.stdout:write(proto.." 303".."\r\n")
io.stdout:write("Location: http://"..host.."/reboot.html")
io.stdout:write("\r\n")
 
