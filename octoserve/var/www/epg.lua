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


if method == "GET" then
   local filename = "epg.json"
   contenttype = "application/json; charset=utf-8"
   local data = nil

   local f = io.open("/config/epg.gz","r")
   if not f then
      f = io.open("/var/channels/epg.gz","r")
   end

   if f then
      http_print(proto.." 200" )
      http_print("Pragma: no-cache")
      http_print("Cache-Control: no-cache")
      http_print("Content-Type: "..contenttype)
      if filename then
         http_print('Content-Disposition: filename="'..filename..'"')
      end
      --http_print(string.format("Content-Length: %d",#data))
      http_print(string.format("Content-Encoding: gzip"))
      http_print()
      while true do
         data = f:read(65536)
         if not data then
            break
         end
         io.stdout:write(data)
      end
      f:close()
   else
      SendError("404","not found")
   end

else
  SendError("500","What")
end
