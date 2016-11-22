#!/usr/bin/lua

local socket = require("socket")

local devicepath = "/sys/class/ddbridge/ddbridge0"
local interval = 30
local nValues = 2880
local LogFile = "/tmp/Temperatur.log"
local HighTemp = 50
local LowTemp = 45

function ReadTemp(sensor)
   local temp = 0
   local file = io.open(devicepath.."/temp"..sensor)
   if file then
      temp = file:read()
      if temp == "no sensor" then 
         temp = 0
      else
         temp = math.floor(tonumber(temp)/1000)
      end
      file:close()
   end
   return temp
end

function sleep(sec)
   socket.select(nil,nil,sec)
end

function SetFan(value)
   local gpio = io.open("/sys/class/gpio/gpio106/value","w");
   if gpio then
     gpio:write(value)
     gpio:close()
   end
end

local gpio = io.open("/sys/class/gpio/export","w");
if gpio then
  gpio:write("106")
  gpio:close()
end

local NumSensors = 0
local i
local temps
local temp
local Sensor = {}

local TempList = {}
local StartIndex = 1
local count = interval
local fanstate = -1

sleep(30)

local ddbridge = io.open("/sys/class/ddbridge/ddbridge0/devid0","r");
if ddbridge then
  local devid = ddbridge:read("*l")
  ddbridge:close()
  if devid == "0307dd01" then 
    fanstate = 1
    ddbridge = io.open("/sys/class/ddbridge/ddbridge0/fanspeed1","r"); 
    if ddbridge then
      local fs = tonumber(ddbridge:read("*l"))
      ddbridge:close()
      if fs > 0 and fs < 17000 then 
         fanstate = -2
      end
    end
  end 
end

for i = 0,4,1 do 
   temp = ReadTemp(i)
   if temp > 0 then 
      Sensor[NumSensors] = i
      NumSensors = NumSensors + 1
   end
end

local fh = io.open(LogFile,"w")
if fh then
   fh:write(NumSensors..","..interval..","..fanstate.."\n")
   fh:close()
end

if NumSensors == 0 then
   return
end

while true do
   sleep(1)
   temps = ""
   for i = 0, NumSensors - 1, 1 do
      temp  = ReadTemp(Sensor[i])
      
      if temp == 0 then
         print(" fanControl Error ")
         return
      end
      
      if i == 0 then 
         temps = temp 
      else
         temps = temps .. "," .. temp
      end
      
      if fanstate == 0 and temp >= HighTemp then
        SetFan(1)
        fanstate = 1
      elseif fanstate == 1 and temp < LowTemp then
        SetFan(0)
        fanstate = 0
      end
      
   end
   
   count = count - 1 
   if count == 0 then
      count = interval
      
      if #TempList < nValues then
         TempList[#TempList+1] = temps 
      else
         TempList[StartIndex] = temps
         StartIndex = StartIndex + 1
         if StartIndex > nValues then StartIndex = 1 end
      end
      
      TmpLogFile = os.tmpname()
      
      fh = io.open(TmpLogFile,"w")
      if fh then
         fh:write(NumSensors..","..interval..","..fanstate.."\n")
      
         for i = StartIndex-1, 1, -1 do
            fh:write(TempList[i].."\n")
         end
         
         for i = #TempList, StartIndex, -1 do
            fh:write(TempList[i].."\n")
         end
         
         fh:close()
         os.remove(LogFile)
         os.rename(TmpLogFile,LogFile)
      end
   end
   collectgarbage()
end
