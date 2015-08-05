#!/usr/bin/lua

local socket = require("socket")
local mime = require("mime")
local url = require("socket.url")

print(#arg)
if #arg ~= 4 then
  os.exit(1)
end

local path, uuid, seq, b64 = unpack(arg)

print(path,uuid,seq)
print(b64)

local values = mime.unb64(b64)
if values == "nil" then
  values =""
end

local evnt = '<?xml version="1.0" encoding="utf-8"?>'.."\r\n"
          .. '<a:propertyset xmlns:a="urn:schemas-upnp-org:event-1-0/">'.."\r\n"
          .. "<a:property>\r\n"
          .. values
          .. "</a:property>\r\n"
          .. "</a:propertyset>\r\n"

-- socket.sleep(0.5)

for p in string.gmatch(path,"<(.+)>") do

  local parsed_url = url.parse(p)
  print(parsed_url.host,parsed_url.port,parsed_url.path)

  local ip = parsed_url.host
  local port = parsed_url.port
  local path = parsed_url.path

  if port and port ~= "0" then
    print("send ",ip,port,path)
    
    local r = "NOTIFY "..path.." HTTP/1.1\r\n"
            .. "Host: "..ip..":"..port.."\r\n"
            .. "Content-Type: text/xml; charset=\"utf-8\"\r\n"
            .. "Content-Length: "..string.format("%d",#evnt).."\r\n"
            .. "NT: upnp:event\r\n"
            .. "NTS: upnp:propchange\r\n"
            .. "SID: uuid:"..uuid.."\r\n"
            .. "SEQ: "..seq.."\r\n"
            .. "\r\n"
            .. evnt    
    
    tcp = socket.tcp()
    tcp:settimeout(2)
    
    if tcp:connect(ip,port) then

      tcp:send(r)
      tcp:receive("*l")
      tcp:close()

      break
    end
    tcp:close()
  end

end