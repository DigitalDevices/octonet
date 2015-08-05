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

local userver = "download.digital-devices.de/download/linux"

if query == "set=beta" then
  local file = io.open("/config/updateserver","w")
  if file then
    file:write(userver.."/beta/".."\n")
    file:close()
  end
elseif query == "set=std" then
  os.remove("/config/updateserver")
  os.execute("rm -f /config/octonet.*.img")
  os.execute("rm -f /config/octonet.*.sha")
end

http_print(proto.." 303")
http_print("Location: http://"..host.."/update.html")
http_print("")
