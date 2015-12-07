#!/usr/bin/lua

local db = require("DataBase")

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

function CreateM3U(host,SourceList)
   local m3u = "#EXTM3U".."\n"

   for _,f in pairs(SourceList) do
      if f.system == "dvbs" or f.system == "dvbs2" then
         for _,c in ipairs(f.ChannelList) do
            m3u = m3u .. "#EXTINF:0,"..f.title.." - "..c.title.."\n"
                      .. "rtsp://"..host..":554/?src="..f.src.."&"..c.request.."\n"
         end
      else
         for _,c in ipairs(f.ChannelList) do
            m3u = m3u .. "#EXTINF:0,"..f.title.." - "..c.title.."\n"
                      .. "rtsp://"..host..":554/?src="..c.request.."\n"
         end
      end
   end
   return m3u
end

local SourceList = {}

for _,f in ipairs(db.SourceList) do
  f.ChannelList = {}
  SourceList[f.refid] = f
end

for _,c in ipairs(db.ChannelList) do
  local f = SourceList[c.refid]
  if f then
    table.insert(f.ChannelList,c)
  end
end

if method == "GET" then
  local filename = nil
  local subtype = nil
  local data = nil

  if string.match(query,"select=m3u") then
    filename = "channellist.m3u"
    subtype = "m3u"
    data = CreateM3U("10.0.4.24",SourceList)
  end

  if data then
    http_print(proto.." 200" )
    http_print("Pragma: no-cache")
    http_print("Cache-Control: no-cache")
    http_print("Content-Type: text/"..subtype)
    if filename then
      http_print('Content-Disposition: filename="'..filename..'"')
    end
    http_print(string.format("Content-Length: %d",#data))
    http_print()
    http_print(data)
  else
    SendError("404","not found")
  end

else
  SendError("500","What")
end
