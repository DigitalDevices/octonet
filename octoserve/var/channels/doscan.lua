#!/usr/bin/lua

--
local newdecoder = require("lunajson.decoder")
local newencoder = require("lunajson.encoder")


function GetIPAddr()
  local myip = nil
  local ifconfig = io.popen("ifconfig eth0")
  if ifconfig then
    local eth0 = ifconfig:read("*a")
    ifconfig:close()
    myip = string.match(eth0,"inet addr%:(%d+%.%d+%.%d+%.%d+)")
  end
  return myip
end

function LoadTransponderList()
   local tl = nil
   local f = io.open("/config/TransponderList.json","r")
   if not f then
      f = io.open("/var/channels/TransponderList.json","r")
   end

   if f then
      local t = f:read("*a")
      f:close()
      print(#t)

      local decode = newdecoder()
      tl = decode(t)
   end
   return tl
end

function GetGroup(ChannelList,Title)
   local Group
   for _,c in ipairs(ChannelList.GroupList) do
      if c.Title == Title then
         Group = c
      end
   end
   if not Group then
      Group = { Title=Title, ChannelList = { [0] = 0 } }
      table.insert(ChannelList.GroupList,Group)
   end
   return Group
end

local key = arg[1]

local ipAddr = GetIPAddr()
local tl = LoadTransponderList()

local ChannelList = {}
ChannelList.GroupList = {}

local Max = 999999
local Count = 0

if arg[2] then
   Max = tonumber(arg[2])
end

if tl.SourceList then
   for _,Source in ipairs(tl.SourceList) do
      print(Source.Title)
      if key == Source.Key then
         local SourceOptions = ""
         if Source.UseNIT then
            SourceOptions = SourceOptions.."--use_nit "
         end
         if Source.DVBType == "S" then
            SourceOptions = '--src=1'
         end

         for _,Transponder in ipairs(Source.TransponderList) do
            Count = Count + 1
            if Count > Max then
               break
            end

            local Params = ""
            local p,v
            for p,v in Transponder.Request:gmatch("(%a+)=([%w%.]+)") do
               if p == "freq" then
                  Params = Params .. " --freq="..v
               elseif p == "pol" then
                  Params = Params .. " --pol="..v
               elseif p == "msys" then
                  Params = Params .. " --msys="..v
               elseif p == "sr" then
                  Params = Params .. " --sr="..v
               elseif p == "mtype" then
                  Params = Params .. " --mtype="..v
               end
            end

            print("--------------------------------------------------------------")
            print('octoscan '..SourceOptions..Params..' '..ipAddr)
            local octoscan = io.popen('octoscan '..SourceOptions..' '..Params..' '..ipAddr,"r")
            if octoscan then
               while true do
                  local line = octoscan:read("*l")
                  if not line then
                     break
                  end
                  print(line)
                  local pname = "?"
                  local sname = "?"
                  local sid = 0
                  local pids = ""
                  local tracks= { [0] = 0 }
                  local isradio = false
                  if line == "BEGIN" then
                     while true do
                        line = octoscan:read("*l")
                        if not line then
                           break
                        end
                        print(line)
                        if line == "END" then
                           local channel = { Title=sname, Service=sid, Request = Request.."&pids=0,"..pids, Tracks=tracks }
                           local cname = pname
                           if isradio then
                              cname = "Radio - "..pname
                           end
                           local category = GetGroup(ChannelList,cname)
                           if category then
                              category.ChannelList[0] = category.ChannelList[0] + 1
                              table.insert(category.ChannelList,channel)
                           end
                           break
                        end
                        local par,val = line:match("^ (%a+):(.*)")
                        if par == "RADIO" then
                           isradio = true
                        elseif par == "PNAME" then
                           pname = val
                        elseif par == "SNAME" then
                           sname = val
                        elseif par == "SID" then
                           sid = tonumber(value)
                        elseif par == "PIDS" then
                           pids = val
                        elseif par == "APIDS" then
                           local track
                           for track in val:gmatch("%d+") do
                              tracks[0] = tracks[0] + 1
                              table.insert(tracks,tonumber(track))
                           end
                        end
                     end
                  elseif line:sub(1,5) == "TUNE:" then
                     Request = line:sub(6)
                  end
               end
               octoscan:close()
            end
         end
      end
   end
end

local encode = newencoder()
cl = encode(ChannelList)

if cl then
   local f = io.open("/config/ChannelList.json","w+")
   if f then
      f:write(cl)
      f:close()
   end
end
