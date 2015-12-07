#!/usr/bin/lua

local db = require("DataBase")

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


http_print(proto.." 200")
--http_print("Pragma: no-cache")
--http_print("Cache-Control: no-cache")
http_print("Content-Type: application/x-javascript")
http_print("")

local SourceList = {}

for _,f in ipairs(db.SourceList) do
  f.ChannelList = {}
  SourceList[f.refid] = f
  http_print("// " .. f.refid .. " " .. f.title )
end

for _,c in ipairs(db.ChannelList) do
  local f = SourceList[c.refid]
  if f then
    table.insert(f.ChannelList,c)
  end
end

local isat = 0
local icable = 0
local iter = 0
local ichannel = 0

http_print("var SourceListSat = new Array();")
http_print("var SourceListCable = new Array();")
http_print("var SourceListTer = new Array();")

for _,f in pairs(SourceList) do
  if f.system == "dvbs" or f.system == "dvbs2" then
    http_print("")
    http_print(string.format("SourceListSat[%d] = new Object();",isat))
    http_print(string.format("SourceListSat[%d].name = '%s';",isat,f.title))
    http_print(string.format("SourceListSat[%d].ChannelList = new Array();",isat))
    
    ichannel = 0
    for _,c in ipairs(f.ChannelList) do
      http_print("")
      http_print(string.format("SourceListSat[%d].ChannelList[%d] = new Object();",isat,ichannel))
      http_print(string.format("SourceListSat[%d].ChannelList[%d].name = '%s';",isat,ichannel,string.gsub(c.title,"'","\\'")))
      http_print(string.format("SourceListSat[%d].ChannelList[%d].request = '?src=%s&%s';",isat,ichannel,f.src,c.request))
      http_print(string.format("SourceListSat[%d].ChannelList[%d].tracks = new Array(%s);",isat,ichannel,c.tracks))
      ichannel = ichannel + 1
    end
    isat = isat + 1
  end
  if f.system == "dvbc" or f.system == "dvbc2" then
    http_print("")
    http_print(string.format("SourceListCable[%d] = new Object();",icable))
    http_print(string.format("SourceListCable[%d].name = '%s';",icable,f.title))
    http_print(string.format("SourceListCable[%d].ChannelList = new Array();",icable))
    
    ichannel = 0
    for _,c in ipairs(f.ChannelList) do
      http_print("")
      http_print(string.format("SourceListCable[%d].ChannelList[%d] = new Object();",icable,ichannel))
      http_print(string.format("SourceListCable[%d].ChannelList[%d].name = '%s';",icable,ichannel,string.gsub(c.title,"'","\\'")))
      http_print(string.format("SourceListCable[%d].ChannelList[%d].request = '?%s';",icable,ichannel,c.request))
      http_print(string.format("SourceListCable[%d].ChannelList[%d].tracks = new Array(%s);",icable,ichannel,c.tracks))
      ichannel = ichannel + 1
    end
    icable = icable + 1
  end
end

