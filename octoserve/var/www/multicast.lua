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
  query="select=m3u"
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

function GetDevID()
  local devid = nil
  local tmp = io.open("/config/device.id")
  if tmp then
    devid = tonumber(tmp:read())
    tmp:close()
  end
  return devid
end

function CreateM3U()
  local m3u = nil
  local devid = GetDevID()
  if not devid then return nil end
  local file = io.open("/config/mcsetup.csv","r")
  if file then
    local line = file:read()
    if line then
      line = string.gsub(line,'"','')
      if string.match(line,"TITLE,REQUEST,PIDS,LANPORTS") then
        local i = 1
        m3u = "#EXTM3U".."\n"
        while i < 13 do
          line = file:read()
          if not line then break end
          if DebugFlag then print(line) end
          local title,request,pids,lanports = string.match(line,'%"(.*)%",%"(.*)%",%"P(%a-[%d%:]-)%",%"L(%a-[%d%:]-)%"')
          if not title then
            title,request,pids = string.match(line,'%"(.*)%",%"(.*)%",%"P(%a-[%d%:]-)%"')
          end
          if not title or not request or not pids then
            break
          end
          if pids == "" then break end

          m3u = m3u .. "#EXTINF:0,"..title.."\n"
                    .. "rtp://239."..devid..".2."..i..":6670\n"
          i = i + 1
        end
      elseif line:match("TITLE,REQUEST,PIDS,PROTO,IP,PORT,TTL,LANPORTS") then
        local i = 1
        m3u = "#EXTM3U".."\n"
        while i < 13 do
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
          if proto == "UDP" then ip = "@"..ip end

          m3u = m3u .. "#EXTINF:0,"..title.."\n"
                    .. proto:lower() .. "://"..ip..":"..port.."\n"
          i = i + 1
        end
      end
    end
    file:close()
  end

  return m3u
end

if method == "GET" then
  local path = nil
  local disposition = nil
  local subtype="csv"

  if string.match(query,"select=sample") then
    local name = string.match(query,"name=(%w+)")
    if name then
      path = "/var/mcsetup/"..name..".csv"
      disposition = name..".csv"
    else
      SendError("404","Request Error")
      return
    end
  elseif string.match(query,"select=current") then
    path = "/config/mcsetup.csv"
    disposition = "mcsetup.csv"
  elseif string.match(query,"select=m3u") then
    path = "m3u"
    disposition = "mclist.m3u"
    subtype = "m3u"
  elseif string.match(query,"select=copy") then
    local name = string.match(query,"name=(%w+)")
    if name then
      path = "/var/mcsetup/"..name..".csv"
      disposition = "copy"
    else
      SendError("404","Request Error")
      return
    end
  elseif string.match(query,"select=disable") then
    disposition = "disable"
  end

  if disposition == "disable" then
    os.remove("/config/mcsetup.csv")
    os.execute('echo "1" >/tmp/mc.tmp;mv -f /tmp/mc.tmp /tmp/mc.signal');
    http_print(proto.." 303")
    http_print("Location: http://"..host.."/multicast.html")
    http_print("")
    return
  end
  
  if disposition == "copy" then
    os.execute("cp "..path.." /config/mcsetup.csv")
    os.execute('echo "1" >/tmp/mc.tmp;mv -f /tmp/mc.tmp /tmp/mc.signal');
    http_print(proto.." 303")
    http_print("Location: http://"..host.."/multicast.html")
    http_print()
    return
  end
  
  local data = nil  
  if path == "m3u" then
    data = CreateM3U()
  else
    local tmp = io.open(path,"r")
    if tmp then
      data = tmp:read("*a")
      tmp:close()
    end
    
  end
  
  if data then
    http_print(proto.." 200" )
    http_print("Pragma: no-cache")
    http_print("Content-Type: text/"..subtype)
    http_print('Content-Disposition: filename="'..disposition..'"')
    http_print(string.format("Content-Length: %d",#data))
    http_print()
    http_print(data)
  else
    SendError("404",disposition.." not found")
  end

elseif method == "POST" and clength and ctype then

  if not string.match(ctype,"multipart/form%-data") then
    SendError("404","??")
    return
  end

  local boundary = string.match(ctype,"boundary=(.*)") 
  if not boundary then
    SendError("404","???")
    return
  end

  while true do
    local line = io.stdin:read()
    line = string.gsub(line,"\r","")
    if line == "" then break end
  end

  data = io.stdin:read(16384)
  data = string.sub(data,1,#data - #boundary - 4)
  if data:match("^\239\187\191") then data = data:sub(4) end
  
  data = string.gsub(data,"\r\n","\n") -- Windows -> Unix
  data = string.gsub(data,"\r","\n") -- MAC -> Unix
  
  -- if data:match("^TITLE,REQUEST,PIDS,PROTO,IP,PORT,TTL,LANPORTS") then
  if data:match("^TITLE,REQUEST,PIDS,LANPORTS") or data:match("^TITLE,REQUEST,PIDS,PROTO,IP,PORT,TTL,LANPORTS") then
    file = io.open("/config/mcsetup.csv","w")
    if file then 
      file:write(data)
      file:close()
      os.execute('echo "1" >/tmp/mc.tmp;mv -f /tmp/mc.tmp /tmp/mc.signal');
    end
  end
  
  http_print(proto.." 303")
  http_print("Location: http://"..host.."/multicast.html")
  http_print()
  
else
  SendError("500","What")  
end
