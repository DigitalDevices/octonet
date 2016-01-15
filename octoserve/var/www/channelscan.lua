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
  query=arg[1]
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

--
local newdecoder = require("lunajson.decoder")
local newencoder = require("lunajson.encoder")

local function LoadTransponderList()
   local tl = nil
   local f = nil
   f = io.open("/config/TransponderList.json","r")
   if not f then
      f = io.open("/var/channels/TransponderList.json","r")
   end

   if f then
      local t = f:read("*a")
      f:close()
      local decode = newdecoder()
      tl = decode(t)
   end
   return tl
end

if method == "GET" then
  local filename = nil
  local contenttype = "application/json; charset=utf-8"
  local data = nil

  local q,v
  local params = ""
  local cmd = ""
  for q,v in query:gmatch("(%a+)=([%w%.]+)") do
      if q == "select" then
         cmd = v
      else
         params = params.." "..q.."="..v
      end
  end

  if cmd == "keys" then
      local tl = LoadTransponderList()
      if tl then
         local kl = { KeyList = { } }
         local s
         for _,s in ipairs(tl.SourceList) do
            table.insert(kl.KeyList, { Key=s.Key,Title=s.Title,DVBType=s.DVBType })
         end
         kl.KeyList[0] = #kl.KeyList
         local encode = newencoder()
         data = encode(kl)
      end
  elseif cmd == "scan" then
      local rc = os.execute("mkdir /tmp/doscan.lock")
      if rc ~= 0 then
         data = '{"status":"busy"}'
      else
         data = '{"status":"retry"}'
         os.execute("/var/channels/doscan.lua "..params.." >/tmp/doscan.log 2>&1 &")
      end
   elseif cmd == "status" then
      local js = { }
      local f = io.open("/tmp/doscan.lock/doscan.msg")
      if f then
         js.status = "active"
      else
         f = io.open("/tmp/doscan.msg")
         if f then
            js.status = "done"
         else
            js.status = "retry"
         end
      end
      if f then
         local m = f:read("*l")
         local count,msg = m:match("(%d+):(.*)")
         js.count = count
         js.msg = msg
         f:close()
      end
      local encode = newencoder()
      data = encode(js)
  end

  if data then
    http_print(proto.." 200" )
    http_print("Pragma: no-cache")
    http_print("Cache-Control: no-cache")
    http_print("Content-Type: "..contenttype)
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
