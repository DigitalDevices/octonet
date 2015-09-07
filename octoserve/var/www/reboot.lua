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

Rebooting = "false"

if( query == "sjiwjsiowjs" ) then
  Rebooting = "true";
  local uImage = io.open("/boot/uImage")
  if( uImage ) then
    uImage:close()
  else
    -- Cleanup server home
    os.execute("rm -rf /var/www/*")
    os.execute("rm -rf /var/dms/*")
    os.execute("rm -rf /var/channels/*")
  end
    
  os.execute("/etc/init.d/S99octo stop")
  os.execute("sync")
  os.execute("reboot")
elseif( query == "restart_octo" ) then
  Rebooting = "true";
  os.execute("/etc/init.d/S99octo restartoctonet")
elseif( query == "restart_dms" ) then
  Rebooting = "true";
  os.execute("/etc/init.d/S92dms restart")
end

JSONData = "{\"Rebooting\":"..Rebooting.."}" 

http_print(proto.." 200" )
http_print("Pragma: no-cache")
http_print("Content-Type: application/json; charset=UTF-8")
http_print(string.format("Content-Length: %d",#JSONData))
http_print()
http_print(JSONData)
