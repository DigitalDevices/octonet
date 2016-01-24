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

local function GetCMD(s)
   local q,v
   local params = ""
   local cmd = ""
   for q,v in s:gmatch("(%a+)=([%w%.]+)") do
      if q == "select" then
         cmd = v
      elseif q ~= "t" then
         params = params.." "..q.."="..v
      end
   end
   return cmd,params
end

local function Keys()
   local data = nil
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
   return data
end

local function Scan(params)
   local data = nil
   local rc = os.execute("mkdir /tmp/doscan.lock")
   if rc ~= 0 then
      data = '{"status":"busy"}'
   else
      data = '{"status":"retry"}'
      local f = io.open("/tmp/doscan.msg","w+")
      if f then
         f:write("Scanning")
         f:close()
      end
      os.execute("/var/channels/doscan.lua "..params.." >/tmp/doscan.log 2>&1 &")
   end
   return data
end

local function Status()
   local data = nil
   local js = { }
   local f = io.open("/tmp/doscan.lock/doscan.msg")
   if f then
      js.status = "active"
      local m = f:read("*l")
      local count,msg = m:match("(%d+):(.*)")
      js.count = count
      js.msg = msg
      f:close()
   else
      f = io.open("/tmp/doscan.msg")
      if f then
         local m = f:read("*l")
         local count,msg = m:match("(%d+):(.*)")
         if count and msg then
            js.count = count
            js.msg = msg
            js.status = "done"
         else
            if m == "Scanning" then
               js.status = "retry"
            else
               js.status = nil
            end
         end
         f:close()
      else
         js.status = ""
      end
   end
   local encode = newencoder()
   data = encode(js)
   return data
end

local function Delete()
   local data = nil
   local rc = os.execute("mkdir /tmp/doscan.lock")
   if rc ~= 0 then
      data = '{"status":"busy"}'
   else
      data = '{"status":"deleted"}'
      os.execute("rm /config/ChannelList.json");
      os.execute("rm /tmp/doscan.msg");
      os.execute("rm -rf /tmp/doscan.lock");
   end
   return data
end

local function Restore()
   local data = nil
   local rc = os.execute("mkdir /tmp/doscan.lock")
   if rc ~= 0 then
      data = '{"status":"busy"}'
   else
      local rc = os.execute("mv /config/ChannelList.bak /config/ChannelList.json");
      if rc == 0 then
         data = '{"status":"restored", "count":1}'
      else
         data = '{"status":"restored", "count":0}'
      end
      os.execute("rm /tmp/doscan.msg");
      os.execute("rm -rf /tmp/doscan.lock");
   end
   return data
end

local filename = nil
local contenttype = "application/json; charset=utf-8"
local data = nil
local cmd = ""
local params

if method == "GET" then
   cmd,params = GetCMD(query)
elseif method == "POST" and clength and ctype then
   if ctype:match("application/x%-www%-form%-urlencoded") then
      local query = io.read(tonumber(clength))
      query = string.gsub(query,"\r","")
      cmd,params = GetCMD(query)
   end
else
   SendError("500","What")
   return
end

if cmd == "keys" then
   data = Keys()
elseif cmd == "scan" then
   data = Scan(params)
elseif cmd == "status" then
   data = Status()
elseif cmd == "delete" then
   data = Delete()
elseif cmd == "restore" then
   data = Restore()
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
