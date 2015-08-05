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

function GetDevID()
  local devid = nil
  local tmp = io.open("/config/device.id")
  if tmp then
    devid = tonumber(tmp:read())
    tmp:close()
  end
  return devid
end

function ReadList()
  local list = {}
  local devid = GetDevID()
  if not devid then return nil end
  local file = io.open("/config/mcsetup.csv","r")
  if file then
    local line = file:read()
    if line then
      line = string.gsub(line,'"','')
      if line:match("TITLE,REQUEST,PIDS,LANPORTS") then
        local i = 1
        while i < 13 do
          line = file:read()
          if not line then break end
          if DebugFlag then print(line) end
          local title,request,pids,lanports = line:match('%"(.*)%",%"(.*)%",%"P(%a-[%d%:]-)%",%"L(%a-[%d%:]-)%"')
          if not title then
            title,request,pids = line:match('%"(.*)%",%"(.*)%",%"P(%a-[%d%:]-)%"')
          end
          if not title or not request or not pids then
            break
          end
          if pids == "" then break end
          if lanports then
            if lanports == "" then lanports = nil end
          end

          table.insert(list, { title = title, ip = "239."..devid..".2."..i, port = "6670", lanports = lanports } )
          i = i + 1
        end
      elseif line:match("TITLE,REQUEST,PIDS,PROTO,IP,PORT,TTL,LANPORTS") then
        while true do
          line = file:read()
          if not line then break end
          if DebugFlag then print(line) end
          local title,request,pids,proto,ip,port,ttl,lanports = line:match('%"(.*)%",%"(.*)%",%"P(%a-[%d%:]-)%",%"(%u%u%u)%",%"(%d*%.%d*%.%d*%.%d*)%",(%d*),(%d*),%"L(%a-[%d%:]-)%"')
          if not title then
            title,request,pids,proto,ip,port,ttl = line:match('%"(.*)%",%"(.*)%",%"P(%a-[%d%:]-)%",%"(%u%u%u)%",%"(%d*%.%d*%.%d*%.%d*)%",(%d*),(%d*)')
          end
          if not title or not request or not pids then
            break
          end
          if pids == "" then break end
          if proto ~= "UDP" and proto ~= "RTP" then break end
          
          pids = string.gsub(pids,":",",")
          if lanports then 
            lanports = string.gsub(lanports,":",",") 
            if lanports == "" then lanports = nil end
            if lanports == "all" then lanports = "1,2,3,4,5" end
          end
          local url 
          table.insert(list,{ title = title, proto = proto, ip = ip, port = port,lanports = lanports })
        end
      end
    end
    file:close()
  end

  return list
end

local mclist = ReadList()


http_print("HTTP/1.1 200")
http_print("Pragma: no-cache")
http_print("Content-Type: application/x-javascript")
http_print()

http_print('Multicast = new Array();')
local i,entry
for i,entry in ipairs(mclist) do
  http_print(string.format("Multicast[%d] = new Object();",i-1))
  http_print(string.format("Multicast[%d].Title = '%s';",i-1,entry.title:gsub("'","\\'")))
  http_print(string.format("Multicast[%d].Dest = '%s:%s';",i-1,entry.ip,entry.port))
  if entry.lanports then
    http_print(string.format("Multicast[%d].LanPorts = '%s';",i-1,entry.lanports:gsub(":"," ")))
  end
end
