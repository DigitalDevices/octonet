#!/usr/bin/lua

--
local newdecoder = require("lunajson.decoder")
local newencoder = require("lunajson.encoder")


local function GetIPAddr()
  local myip = nil
  local ifconfig = io.popen("ifconfig eth0")
  if ifconfig then
    local eth0 = ifconfig:read("*a")
    ifconfig:close()
    myip = string.match(eth0,"inet addr%:(%d+%.%d+%.%d+%.%d+)")
  end
  return myip
end

local function LoadTransponderList(infile)
   local tl = nil
   local f = nil
   if infile then
      f = io.open(infile,"r")
   else
      f = io.open("/config/TransponderList.json","r")
      if not f then
         f = io.open("/var/channels/TransponderList.json","r")
      end
   end

   if f then
      local t = f:read("*a")
      f:close()
      local decode = newdecoder()
      tl = decode(t)
   end
   return tl
end

local function GetGroup(ChannelList,Title)
   local Group
   for _,c in ipairs(ChannelList.GroupList) do
      if c.Title == Title then
         Group = c
      end
   end
   if not Group then
      Group = { Title=Title, ChannelList = { } }
      table.insert(ChannelList.GroupList,Group)
   end
   return Group
end

local function CompareTitle(a,b)
   if a.Order then
      if b.Order then
         if a.Order == b.Order then
            return a.Title < b.Title
         else
            return a.Order < b.Order
         end
      else
         return true
      end
   elseif b.Order then
      return false
   end
   return a.Title < b.Title
end

local function Report(count,Title)
   local f = io.open("/tmp/doscan.lock/doscan.tmp","w+")
   if f then
      f:write(count..":"..Title)
      f:close()
      os.execute("mv /tmp/doscan.lock/doscan.tmp /tmp/doscan.lock/doscan.msg")
   end
end

local keys = {}
local include_radio     = 1
local include_encrypted = 0
local infile = nil
local outfile = "/config/ChannelList.json"
local ipAddr = nil
local sort = nil
local include_sitables = nil
local restart_dms = nil

local a
for _,a in ipairs(arg) do
   local par,val = a:match("(%a+)=(.+)")
   if par == "key" then
      local key,src = val:match("(%w+)%.(%d+)")
      if key then
         keys[key] = tonumber(src)
      else
         keys[val] = 1
      end
   elseif par == "radio" then
      include_radio = tonumber(val)
   elseif par == "enc" then
      include_encrypted = tonumber(val)
   elseif par == "sort" then
      sort = val
   elseif par == "sitables" then
      include_sitables = val
   elseif par == "restartdms" then
      restart_dms = val
   elseif par == "in" then
      infile = val
   elseif par == "out" then
      outfile = val
   elseif par == "ip" then
      ipAddr = val
   end
end

if not ipAddr then
   ipAddr = GetIPAddr()
end
local tl = LoadTransponderList(infile)

local CIMappings = {}

if tl.CIMapList then
   local CIMap
   local ID
   for _,CIMap in ipairs(tl.CIMapList) do
      for _,ID in ipairs(CIMap.CIMappings) do
         CIMappings[ID] = CIMap
      end
   end
end

local ChannelOverwrites = {}
if tl.ChannelOverwriteList then
   local ChannelOverwrite
   for _,ChannelOverwrite in ipairs(tl.ChannelOverwriteList) do
      ChannelOverwrites[ChannelOverwrite.ID] = ChannelOverwrite
   end
end

local ChannelList = {}
ChannelList.GroupList = {}

if tl.GroupList then
   local Group
   local Order
   for _,Group in ipairs(tl.GroupList) do
      local g = GetGroup(ChannelList,Group.Title)
      if Group.Order then
         g.Order = Group.Order
      end
   end
end

local Max = 999999
local Count = 0
local ChannelCount = 0

Report(ChannelCount,"*")

if tl.SourceList then
   local Source
   for _,Source in ipairs(tl.SourceList) do
      if keys[Source.Key] then
         Report(ChannelCount,Source.Title)
         print("Scanning: "..Source.Title)
         local SourceOptions = ""
         if Source.DVBType == "S" then
            SourceOptions = '--src='..keys[Source.Key].." "
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
               elseif p == "bw" then
                  Params = Params .. " --bw="..v
               elseif p == "plp" then
                  Params = Params .. " --plp="..v
               elseif p == "mtype" then
                  Params = Params .. " --mtype="..v
               end
            end

            local Options = SourceOptions
            if Transponder.UseNIT then
               Options = SourceOptions.."--use_nit "
            end

            print("--------------------------------------------------------------")
            print('octoscan '..Options..Params..' '..ipAddr)
            local octoscan = io.popen('octoscan '..Options..Params..' '..ipAddr,"r")
            if octoscan then
               while true do
                  local line = octoscan:read("*l")
                  if not line then
                     break
                  end
                  print(line)
                  local pname = "?"
                  local sname = "?"
                  local onid = 0
                  local tsid = 0
                  local sid = 0
                  local pids = ""
                  local tracks= { }
                  local isradio = nil
                  local isencrypted = false
                  local hasepg = nil
                  if line == "SERVICE" then
                     while true do
                        line = octoscan:read("*l")
                        if not line then
                           break
                        end
                        print(line)
                        if line == "END" then
                           local all_pids = "0"
                           if include_sitables then
--~                               if isencrypted then
--~                                  all_pids = all_pids..",1"
--~                               end
                              all_pids = all_pids..",16,17,18,20"
                           end
                           local cireq = ""
                           local ID = string.format("%s:%d:%d:%d",Source.Key,onid,tsid,sid)
                           if isencrypted then
                              local CIMap = CIMappings[ID]
                              if CIMap then
                                 local pmt = pids:match("(%d+),?")
                                 if pmt then
                                    cireq = "&x_ci="..CIMap.Slot.."&x_pmt="..pmt.."."..sid
                                    isencrypted = false
                                 end
                              end
                           end

                           if pname == "" then
                              pname = Source.Title
                           end
                           local gname = pname
                           if isradio then
                              gname = "Radio - "..gname
                           end

                           local Order = none
                           local ChannelOverwrite = ChannelOverwrites[ID]
                           if ChannelOverwrite then
                              Order = ChannelOverwrite.Order
                              if ChannelOverwrite.Group then
                                 gname = ChannelOverwrite.Group
                              end
                              if ChannelOverwrite.Pids then
                                 pids = ChannelOverwrite.Pids
                              end
                              if ChannelOverwrite.Title then
                                 sname = ChannelOverwrite.Title
                              end
                           end

                           if #pids > 0 then
                              all_pids = all_pids..","..pids
                           end
                           local channel = { Title=sname,
                                             Request = '?'..Request..cireq.."&pids="..all_pids,
                                             Tracks=tracks,
                                             ID=ID, Order=Order, Radio=isradio }
                           if not hasepg then
                              channel.NoEPG = true
                           end
                           if ((not isradio) or (include_radio > 0)) and ((not isencrypted) or (include_encrypted > 0)) then
                              local group = GetGroup(ChannelList,gname)
                              if group then
                                 table.insert(group.ChannelList,channel)
                                 ChannelCount = ChannelCount + 1
                                 Report(ChannelCount,sname)
                              end
                           end
                           break
                        end
                        local par,val = line:match("^ (%a+):(.*)")
                        if par == "RADIO" then
                           isradio = true
                        elseif par == "EIT" then
                           if val:sub(2,2) == "1" then
                              hasepg = true
                           end
                        elseif par == "PNAME" then
                           pname = val
                        elseif par == "SNAME" then
                           sname = val
                        elseif par == "ONID" then
                           onid = tonumber(val)
                        elseif par == "TSID" then
                           tsid = tonumber(val)
                        elseif par == "SID" then
                           sid = tonumber(val)
                        elseif par == "PIDS" then
                           pids = val
                        elseif par == "APIDS" then
                           local track
                           for track in val:gmatch("%d+") do
                              table.insert(tracks,tonumber(track))
                           end
                           tracks[0] = #tracks
                        elseif par == "ENC" then
                           isencrypted = true
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

   for _,group in ipairs(ChannelList.GroupList) do
      if sort then
         table.sort(group.ChannelList,CompareTitle)
      end
      for _,channel in ipairs(group.ChannelList) do
         channel.Order = nil
      end
      group.ChannelList[0] = #group.ChannelList
   end
   if sort then
      table.sort(ChannelList.GroupList,CompareTitle)
   end
   for _,group in ipairs(ChannelList.GroupList) do
      group.Order = nil
   end

   ChannelList.GroupList[0] = #ChannelList.GroupList

   local encode = newencoder()
   cl = encode(ChannelList)

   if outfile == "/config/ChannelList.json" then
      os.execute("mv /config/ChannelList.json /config/ChannelList.bak")
   end
   if cl then
      local f = io.open(outfile,"w+")
      if f then
         f:write(cl)
         f:close()
      end
   end

end

Report(ChannelCount,"Channels found")
os.execute("mv /tmp/doscan.lock/doscan.msg /tmp/doscan.msg")

if restart_dms then
   os.execute("/etc/init.d/S92dms restart")
end
os.execute("rm -rf /tmp/doscan.lock");
