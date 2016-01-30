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

local function LoadChannelList(infile)
   local cl = nil
   local f = nil
   if infile then
      f = io.open(infile,"r")
   else
      f = io.open("/config/ChannelList.json","r")
   end

   if f then
      local t = f:read("*a")
      f:close()
      local decode = newdecoder()
      cl = decode(t)
   end
   return cl
end

local function Report(count,Title)
   local f = io.open("/tmp/doscan.lock/doscan.tmp","w+")
   if f then
      f:write(count..":"..Title)
      f:close()
      os.execute("mv /tmp/doscan.lock/doscan.tmp /tmp/doscan.lock/doscan.msg")
   end
end

local infile = nil
local outfile = "/config/epg.gz"
local ipAddr = nil

local a
for _,a in ipairs(arg) do
   local par,val = a:match("(%a+)=(.+)")
   if par == "in" then
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

local cl = LoadChannelList(infile)

local Max = 999999
local Count = 0
local ChannelCount = 0
local EventCount = 0

Report(ChannelCount,"*")

local tl = {}
local Options = ""

if not cl then
   Report(0,"Channellist not Found, channelscan required")
elseif not cl.GroupList then
   Report(0,"Invalid channellist")
else
   local Group
   for _,Group in ipairs(cl.GroupList) do
      for _,Channel in ipairs(Group.ChannelList) do
         if Channel.ID then
            local Params = ""
            local p,v
            for p,v in Channel.Request:gmatch("(%a+)=([%w%.]+)") do
               if p == "src" then
                  Params = Params .. " --src="..v
               elseif p == "freq" then
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
            local Key,Sid = Channel.ID:match("^(%a+):%d+:%d+:(%d+)")
            if Key and Sid then
               local t = tl[Params]
               if t then
                  t.Sids = t.Sids..","..Sid
               else
                  tl[Params] = { Key=Key, Sids=Sid }
                  ChannelCount = ChannelCount + 1
               end
            end
         end
      end
   end

   os.execute("rm "..outfile)
   local gzip = io.popen('gzip >'..outfile..".tmp","w")
   if gzip then
      gzip:write('{"EventList":[');
      local newline = "\n "
      local Params
      local Key, t
      for Params,t in pairs(tl) do
         Report(Count,"of "..ChannelCount.." transponders, "..EventCount.." events")
         Count = Count + 1
         Key = t.Key
         print("--------------------------------------------------------------")
         collectgarbage();
         local cmd = 'octoscan --eit_sid '..t.Sids..' '..Options..Params..' '..ipAddr
         print(cmd)
         local octoscan = io.popen(cmd,"r")
         if octoscan then
            while true do
               local line = octoscan:read("*l")
               if not line then
                  break
               end
--~                print(line)
               local eid = nil
               local name = nil
               local text = nil
               local time = nil
               local duration = nil
               local language = nil
               if line == "EVENT" then
                  while true do
                     line = octoscan:read("*l")
                     if not line then
                        break
                     end
--~                      print(line)
                     if line == "END" then
                        if eid then
                           local event = { ID = Key..":"..eid }
                           if name then
                              event.Name = name
                           end
                           if text then
                              event.Text = text
                           end
                           if time then
                              event.Time = time
                           end
                           if duration then
                              event.Duration = duration
                           end
                           -- if language then
                           --    event.Language = language
                           -- end

                           local encode = newencoder()
                           local e = encode(event)
                           gzip:write(newline..e)
                           newline = ",\n "
                           EventCount = EventCount + 1
                        end
                        collectgarbage();
                        break
                     end

                     local par,val = line:match("^ (%a+):(.*)")
                     if par == "ID" then
                        eid = val
                     elseif par == "NAME" then
                        name = val
                     elseif par == "TEXT" then
                        text = val
                     elseif par == "TIME" then
                        time = val
                     elseif par == "DUR" then
                        duration = val
                     -- elseif par == "LANG" then
                     --    language = val
                     end
                  end
               end
            end
            octoscan:close()
         end
      end
      gzip:write("\n]}")
      gzip:close()
   else
      Report(0,"Internal error")
   end

--~    if outfile == "/config/ChannelList.json" then
--~       os.execute("mv /config/ChannelList.json /config/ChannelList.bak")
--~    end

   Report(ChannelCount,"EPG done "..EventCount.." events")
end

os.execute("mv "..outfile..".tmp "..outfile)
os.execute("mv /tmp/doscan.lock/doscan.msg /tmp/doscan.msg")
os.execute("rm -rf /tmp/doscan.lock");
