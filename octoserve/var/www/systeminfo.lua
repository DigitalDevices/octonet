#!/usr/bin/lua

local host = os.getenv("HTTP_HOST")
local proto = os.getenv("SERVER_PROTOCOL")
local query = os.getenv("QUERY_STRING")

function http_print(s)
  io.stdout:write(s.."\r\n")
end

http_print("HTTP/1.1 200")
http_print("Pragma: no-cache")
http_print("Content-Type: application/x-javascript")
--http_print("Content-Type: text/plain")
http_print("")

local ddtest = io.popen("ddtest reg 0+4","r")
local ddo = ddtest:read("*a")
ddtest:close()

local registers = {}
for v in string.gmatch(ddo,"%((%-?%d+)%)") do
  table.insert(registers,v)
end

local tmp = io.popen("uname -r -m","r")
local tmp1 = tmp:read()
tmp:close()

-- local uname = string.match(tmp1,"Linux (%.+)")
-- http_print(tmp1)
-- http_print(uname)
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

beta = "false"
tmp = io.open("/config/updateserver")
if tmp then
  beta = "true"
  tmp:close()
end

http_print(string.format("var linuxver = \"%s\";",uname))
http_print(string.format("var fpgaver = \"%d.%d\";",(registers[1] / 65536) % 65536,registers[1] % 65536))
http_print(string.format("var fpgatype = \"%0X\";",registers[3] / 65536))
-- http_print(string.format("var fpgaver = \"%d.%d\";",registers[1] >> 16,registers[1] & 0xffff))
-- http_print(string.format("var fpgatype = \"%0X\";",registers[3] >> 16))
http_print(string.format("var fwdate = \"%s\";",fwdate))
http_print(string.format("var host = \"%s\";",host))
http_print(string.format("var beta = %s;",beta))
