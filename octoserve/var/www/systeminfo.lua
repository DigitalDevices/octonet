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

function readattr(attr)
   local value = "0"
   local ddbridge = io.open("/sys/class/ddbridge/ddbridge0/"..attr,"r");
   if ddbridge then
     value = ddbridge:read("*l")
     ddbridge:close()
     value = value:gsub("0x","")
   end
   return value
end

http_print("HTTP/1.1 200")
http_print("Pragma: no-cache")
http_print("Cache-Control: no-cache")
http_print("Content-Type: application/x-javascript")
http_print()

dev0 = tonumber(readattr("devid0"),16)
hwid = tonumber(readattr("hwid"),16)

local tmp = io.popen("uname -r -m","r")
local tmp1 = tmp:read()
tmp:close()

local uname = tmp1

tmp = io.popen("ls /config/*.img","r")
local fwimg = tmp:read("*a")
tmp:close()

local images = {}
for v in string.gmatch(fwimg,"%.(%d+)%.") do
  table.insert(images,v)
end
local fwdate = "";
if images[1] then fwdate = images[1] end

suffix = ""
tmp = io.open("/config/updateserver")
if tmp then
   local updateserver = tmp:read("*l")
   if updateserver == "download.digital-devices.de/download/linux/beta" then
      suffix = "BETA"
   else
      if #updateserver > 17 then
         updateserver = ".."..updateserver:sub(-15)
      end
      suffix = "("..updateserver..")"
   end
   tmp:close()
end

http_print(string.format("var linuxver = \"%s\";",uname))
http_print(string.format("var fpgaver = \"%d.%d\";",(hwid / 65536) % 256,hwid % 65536))
http_print(string.format("var fpgatype = \"%0X\";",dev0 / 65536))
http_print(string.format("var fwdate = \"%s\";",fwdate))
http_print(string.format("var host = \"%s\";",host))
http_print(string.format("var suffix = \"%s\";",suffix))
