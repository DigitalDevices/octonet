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

function CreateM3U(host)
   local m3u = {}
   table.insert(m3u,"#EXTM3U".."\n")

   local file = io.open("/config/ChannelList.json")
   if file then
      local json = file:read("*a")
      file:close()
      local newdecoder = require("lunajson.decoder")
      local decode = newdecoder()
      local channellist = decode(json)
      for _,group in ipairs(channellist.GroupList) do
         for _,channel in ipairs(group.ChannelList) do
            table.insert(m3u,"#EXTINF:0,"..group.Title.." - "..channel.Title.."\n")
            table.insert(m3u,"rtsp://"..host..":554/"..channel.Request.."\n")
         end
      end
   else
      local SourceList = LoadSourceList()
      for _,f in pairs(SourceList) do
         if f.system == "dvbs" or f.system == "dvbs2" then
            for _,c in ipairs(f.ChannelList) do
               table.insert(m3u,"#EXTINF:0,"..f.title.." - "..c.title.."\n")
               table.insert(m3u,"rtsp://"..host..":554/?src="..f.src.."&"..c.request.."\n")
            end
         else
            for _,c in ipairs(f.ChannelList) do
               table.insert(m3u,"#EXTINF:0,"..f.title.." - "..c.title.."\n")
               table.insert(m3u,"rtsp://"..host..":554/?"..c.request.."\n")
            end
         end
      end
   end
   return table.concat(m3u)
end

local function CompareTitle(a,b)
   return a.Title < b.Title
end

function Legacy2JSON()
   local newencoder = require("lunajson.encoder")
   local SourceList = LoadSourceList()
   local ChannelList = {}
   ChannelList.GroupList = {}
   local Group
   local Channel
   local f,c

   for _,f in pairs(SourceList) do
      Group = { Title=f.title, ChannelList = { } }
      table.insert(ChannelList.GroupList,Group)

      if f.system == "dvbs" or f.system == "dvbs2" then
         src = 'src='..f.src..'&'
      end
      for _,c in ipairs(f.ChannelList) do
         local tracks = {}
         local track
         for track in c.tracks:gmatch("%d+") do
            table.insert(tracks,tonumber(track))
         end
         tracks[0] = #tracks

         Channel = { Title=c.title,
                     Request = '?'..src..c.request,
                     Tracks=tracks }

         table.insert(Group.ChannelList,Channel)
      end
   end

   for _,Group in ipairs(ChannelList.GroupList) do
      table.sort(Group.ChannelList,CompareTitle)
      Group.ChannelList[0] = #Group.ChannelList
   end
   table.sort(ChannelList.GroupList,CompareTitle)
   ChannelList.GroupList[0] = #ChannelList.GroupList

   local encode = newencoder()
   local data = encode(ChannelList)
   return data
end

function CreateJSON(host)
   local data = nil
   local file = io.open("/config/ChannelList.json")
   if file then
      data = file:read("*a")
      file:close()
   else
      data = Legacy2JSON()
   end
   return data
end

function LoadSourceList()
   local db = require("DataBase")
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
   return SourceList
end

if method == "GET" then
  local filename = nil
  local contenttype = nil
  local data = nil

  if string.match(query,"select=m3u") then
    filename = "channellist.m3u"
    contenttype = "text/m3u; charset=utf-8"
    data = CreateM3U(host)
  elseif string.match(query,"select=json") then
    filename = "ChannelList.json"
    contenttype = "application/json; charset=utf-8"
    data = CreateJSON(host)
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
