#!/usr/bin/lua

local socket = require("socket")
local mime = require("mime")
local rtsp = require("rtsp")
local mc = require("mc")

local ip = "10.0.4.53"
local devid = 5
local DoPlay = true
local mclist = false
local configfile = "/config/mcsetup.csv"
local server = false

local a
for _,a in ipairs(arg) do
  if a == "server" then server = true end
  if a == "noplay" then DoPlay = false end
end

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

function GetDevID()
  local devid = nil
  local tmp = io.open("/config/device.id")
  if tmp then
    devid = tonumber(tmp:read())
    tmp:close()
  end
  return devid
end

function WaitSignal()
  print("WaitSignal")
  while true do
    socket.select(nil,nil,1)
  end
  print("Exit WaitSignal")
end

if server then
  socket.select(nil,nil,10)
end

while true do
  
  ip = GetIPAddr()
  devid = GetDevID()
  print(ip,devid)
  
  if devid and ip then  
    mclist = mc:ReadSetupFile(configfile)
    print(mclist,#mclist)
    if #mclist > 0 then
      local SessionList = mc:Setup(mclist,ip,devid)  
      if #SessionList > 0 then 

        if DoPlay then 
          mc:Play(SessionList,ip)
        end

        local rc = mc:KeepAlive(SessionList,ip)
        if rc > 0 then
          mc:TearDown(SessionList,ip)
        end
        if rc == 1 then
          break
        end
        if rc == 0 then
          socket.select(nil,nil,10)
        end
      end
    else
      while not mc.CheckSignal() do
        socket.select(nil,nil,1)
      end
    end
  end 
  collectgarbage()
end



  -- local server = socket.tcp()
  -- server:settimeout(2)

  -- local cseq = 1

  -- if server:connect(ip,554) then
    -- rtsp:SendRequest(server,ip,"DESCRIBE",nil,cseq)
    -- local rc,attributes,sdp = rtsp:ReadResponse(server)
    -- cseq = cseq + 1
    -- print(rc)
    -- for n,a in pairs(attributes) do
      -- print(n,a)
    -- end
    -- if sdp then
      -- for i,a in ipairs(sdp) do
        -- print(string.format("%2d:%s",i,a))
      -- end
    -- end
    -- server:close()
  -- end
