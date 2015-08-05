#!/usr/bin/lua

function WriteSwitch(addr,reg,value)
  os.execute(string.format("ddtest mdio %02X %02X %04X",addr,reg,value))
end

function ReadSwitch(addr,reg)
  local ddtest = io.popen(string.format("ddtest mdio %02X %02X",addr,reg),"r")
  local ddo = ddtest:read()
  ddtest:close()
  return tonumber(ddo,16)
end

function WaitSwitch(addr,reg)
  local s
  repeat
    s = ReadSwitch(addr,reg)
    if not s then break end
  until s < 32768 
end

function GetSernbr()
  local ddtest = io.popen("ddtest mem c 4","r")
  local ddo = ddtest:read()
  ddtest:close()  
  local snr = string.sub(ddo,14,15)..string.sub(ddo,17,18)..string.sub(ddo,20,21)
  return tonumber(snr,16)/2  
end

local mclist = {}
table.insert(mclist,"224.0.0.1")
table.insert(mclist,"224.0.0.2")
table.insert(mclist,"224.0.0.22")
table.insert(mclist,"239.255.255.250")

local tmp
tmp = ReadSwitch(16,0)

if arg[1] then
  local i
  for i = 1, #arg, 1 do
    if arg[i] == "goff" then
      WriteSwitch(16,22,37102)
      WaitSwitch(16,22)
    end
    if arg[i] == "gon" then
      WriteSwitch(16,22,37103)
      WaitSwitch(16,22)
    end
    if arg[i] == "gb" then
      WriteSwitch(16,22,57380)
      WaitSwitch(16,22)
      WriteSwitch(16,22,37101)
      WaitSwitch(16,22)
    end
    if arg[i] == "yoff" then
      WriteSwitch(18,22,37102)
      WaitSwitch(18,22)
    end
    if arg[i] == "yon" then
      WriteSwitch(18,22,37103)
      WaitSwitch(18,22)
    end
    if arg[i] == "yb" then
      WriteSwitch(18,22,57380)
      WaitSwitch(18,22)
      WriteSwitch(18,22,37101)
      WaitSwitch(18,22)
    end
  end
else
  WriteSwitch(16,22,37103)
  WaitSwitch(16,22)
  WriteSwitch(18,22,37102)
  WaitSwitch(18,22)
  if GetSernbr() < 2300 then
    WriteSwitch(16,22,32786)
    WriteSwitch(17,22,32786)
    WriteSwitch(18,22,32786)
    WriteSwitch(19,22,32786)
    WriteSwitch(20,22,32786)
  else
    WriteSwitch(16,22,32819)
    WriteSwitch(17,22,32819)
    WriteSwitch(18,22,32819)
    WriteSwitch(19,22,32819)
    WriteSwitch(20,22,32819)
  end
  WriteSwitch(22, 7,40961)
  WriteSwitch(22, 4,   95)
  for _,addr in ipairs(mclist) do
    local b1,b2,b3,b4 = string.match(addr,"(%d+)%.(%d+)%.(%d+)%.(%d+)")
    b2 = tonumber(b2)
    b3 = tonumber(b3)
    b4 = tonumber(b4)
    if b2 > 127 then b2 = b2 - 128 end
    WriteSwitch(27, 12, 1015)
    WriteSwitch(27, 13, 256)
    WriteSwitch(27, 14, 24064 + b2)
    WriteSwitch(27, 15, b3 * 256 + b4)
    WriteSwitch(27, 11, 45056)
    WaitSwitch(27, 11)    
  end
  WriteSwitch(28, 5, 5367)
  WriteSwitch(21, 4, 119)
end