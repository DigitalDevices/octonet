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

local function cmp_title(a,b)
   return a.Title < b.Title
end

local keys = {}
local include_radio     = 1
local include_encrypted = 0
local infile = nil
local outfile = "/config/ChannelList.json"
local ipAddr = nil
local sort = nil
local include_sitables = nil

local a

for _,a in ipairs(arg) do
   local par,val = a:match("(%a+)=(.+)")
   if par == "key" then
      local key,src = val:match("(%a+),(d+)")
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

local ChannelList = {}
ChannelList.GroupList = {}

local Max = 999999
local Count = 0

if tl.SourceList then
   for _,Source in ipairs(tl.SourceList) do
      if keys[Source.Key] then
         print("Scanning: "..Source.Title)
         local SourceOptions = ""
         if Source.UseNIT then
            SourceOptions = SourceOptions.."--use_nit "
         end
         if Source.DVBType == "S" then
            SourceOptions = '--src='..keys[Source.Key]
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
                  local tracks= { }
                  local isradio = false
                  local isencrypted = false
                  if line == "BEGIN" then
                     while true do
                        line = octoscan:read("*l")
                        if not line then
                           break
                        end
                        print(line)
                        if line == "END" then
                           local all_pids = ",0"
                           if include_sitables then
                              if isencrypted then
                                 all_pids = all_pids..",1"
                              end
                              all_pids = all_pids..",16,17,18,20"
                           end
                           if #pids > 0 then
                              all_pids = all_pids .. ",pids"
                           end
                           local channel = { Title=sname, Service=sid, Request = '?'..Request.."&pids="..pids, Tracks=tracks }
                           local gname = pname
                           if isradio then
                              gname = "Radio - "..gname
                           end
                           if not isradio or (include_radio > 0) then
                              local group = GetGroup(ChannelList,gname)
                              if group then
                                 table.insert(group.ChannelList,channel)
                              end
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
                              table.insert(tracks,tonumber(track))
                           end
                           tracks[0] = #tracks
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
         table.sort(group.ChannelList,cmp_title)
      end
      group.ChannelList[0] = #group.ChannelList
   end
   if sort then
      table.sort(ChannelList.GroupList,cmp_title)
   end
   ChannelList.GroupList[0] = #ChannelList.GroupList

   local encode = newencoder()
   cl = encode(ChannelList)

   if cl then
      local f = io.open(outfile,"w+")
      if f then
         f:write(cl)
         f:close()
      end
   end

end

