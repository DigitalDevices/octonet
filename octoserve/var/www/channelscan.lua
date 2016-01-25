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

function perr(s)
   io.stderr:write(tostring(s).."\n")
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

local function CheckTransponderList(tl)
   local i,Source
   for i,Source in ipairs(tl.SourceList) do
      if not Source.Title then
         error("SourceList["..i.."].Title missing",0)
      end
      if not Source.Key then
         error("SourceList["..i.."].Key missing",0)
      end
      if not Source.DVBType then
         error("SourceList["..i.."].DVBType missing",0)
      end
      if not Source.TransponderList then
         error("SourceList["..i.."].TransponderList missing",0)
      end
      local j,Transponder
      for j,Transponder in ipairs(Source.TransponderList) do
         if not Transponder.Request then
            error("SourceList["..i.."].TransponderList["..j.."].Request missing",0)
         end
      end
   end
   if tl.CIMapList then
      local CIMap
      for i,CIMap in ipairs(tl.CIMapList) do
         if not CIMap.Slot then
            error("CIMapList["..i.."].Slot missing",0)
         end
      end
   end
   if tl.ChannelOverwriteList then
      local ChannelOverwrite
      for i,ChannelOverwrite in ipairs(tl.ChannelOverwriteList) do
         if not ChannelOverwrite.ID then
            error("ChannelOverwriteList["..i.."].ID missing",0)
         end
      end
   end
   if tl.GroupList then
      local Group
      for i,Group in ipairs(tl.GroupList) do
         if not Group.Title then
            error("GroupList["..i.."].Title missing",0)
         end
      end
   end
   return "TransponderList.json"
end

local function CheckChannelList(cl)
   local i,Group
   for i,Group in ipairs(cl.GroupList) do
      if not Group.Title then
         error("GroupList["..i.."].Title missing",0)
      end
      local j,Channel
      for j,Channel in ipairs(Group.ChannelList) do
         if not Channel.Title then
            error("GroupList["..i.."].ChannelList["..j.."].Title missing",0)
         end
         if not Channel.Request then
            error("GroupList["..i.."].ChannelList["..j.."].Request missing",0)
         end
      end
   end
   return "ChannelList.json"
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

local function Delete(params)
   local data = nil
   local rc = os.execute("mkdir /tmp/doscan.lock")
   if rc ~= 0 then
      data = '{"status":"busy"}'
   else
      data = '{"status":"deleted"}'
      os.execute("rm /config/ChannelList.json");
      if params == " all=true" then
         os.execute("rm /config/TransponderList.json");
      end
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

local function Upload()
   local data = nil
   local rc = os.execute("mkdir /tmp/doscan.lock")
   if rc ~= 0 then
      data = '{"status":"busy"}'
   else
      local boundary = string.match(ctype,"boundary=(.*)")
      if boundary then
         while true do
           local line = io.stdin:read()
           line = string.gsub(line,"\r","")
           if line == "" then break end
         end

         local filedata = io.stdin:read("*a")
         local i = filedata:find("--"..boundary,1,true)
         if i then
            filedata = filedata:sub(1,i-1)
         else
            filedata = "{}"
         end

         local decode = newdecoder()
         local rc,json = pcall(decode,filedata)

         if rc then
            rc = false
            local msg = "invalid list"
            if json.SourceList then
               rc,msg = pcall(CheckTransponderList,json)
               data = '{"status":"updated", "msg":"Transponder list"}'
            elseif json.GroupList then
               rc,msg = pcall(CheckChannelList,json)
            end
            if rc then
               local f = io.open("/config/"..msg,"w+")
               if f then
                  f:write(filedata)
                  f:close()
                  data = '{"status":"updated", "msg":"'..msg..'"}'
               else
                  data = '{"status":"error", "msg":"'..msg..' not saved"}'
               end
            else
               data = '{"status":"error", "msg":"'..msg..'"}'
            end
         else
            local msg = "unknown error"
            if json then
               msg = json:match(".-: (.*)")
            end
            data = '{"status":"error", "msg":"'..msg..'"}'
         end

      else
        data = '{"status":"error","msg":"malformed request"}'
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
   elseif ctype:match("multipart/form%-data") then
      cmd = "upload"
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
   data = Delete(params)
elseif cmd == "restore" then
   data = Restore()
elseif cmd == "upload" then
   data = Upload()
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
