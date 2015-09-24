#!/usr/bin/lua

function SaveOctoserveConf(Section,Values)
  local ConfStart = ""
  local ConfEnd = ""
  local f = io.open("/config/octoserve.conf","r")
  if f then
    while true do
      line = f:read()
      if not line then break end
      if string.match(line,"^%["..Section.."%]") then break end
      ConfStart = ConfStart .. line .. "\n"
    end
    while true do
      line = f:read()
      if not line then break end
      if string.match(line,"^%[%w+%]") then
        ConfEnd = ConfEnd .. line .. "\n"
        break 
      end
    end
    while true do
      line = f:read()
      if not line then break end
      ConfEnd = ConfEnd .. line .. "\n"
    end
    f:close()
    os.remove("/config/octoserve.bak")
    os.rename("/config/octoserve.conf","/config/octoserve.bak")
  end
    
  f = io.open("/config/octoserve.conf","w")
  if ConfStart then 
    f:write(ConfStart)
  end
  f:write("["..Section.."]\n")
  f:write(Values)
  if ConfEnd then
    f:write(ConfEnd)
  end
  f:close()
end

function LoadOctoserveConf(Section)
  local f = io.open("/config/octoserve.conf","r")
  local Values = {}
  local line
  if f then    
    while true do
      line = f:read()
      if not line then break end
      if string.match(line,"^%["..Section.."%]") then break end
    end
    while true do
      line = f:read()
      if not line then break end
      if string.match(line,"^%[%w+%]") then break end
      if not string.match(line,"^%#") then 
        table.insert(Values,line)
      end
    end
    f:close()
  end
  return(Values) 
end

function http_print(s)
  if s then
    io.stdout:write(tostring(s).."\r\n")
  else
    io.stdout:write("\r\n")
  end
end


local host = os.getenv("HTTP_HOST")
local proto = os.getenv("SERVER_PROTOCOL")
local query = os.getenv("QUERY_STRING")

if query ~= "" then

  http_print(proto.." 303")
  http_print("Location: http://"..host.."/wait.html?5")
  http_print()

  -- print(string.format("Set Unicable %s", query ))

  local Values = ""

  Values = Values.."# SCIF Settings\n"
  Values = Values.."#   Manufacturer = nn : Index to selected manaufacturer (only used in config webpage)\n"
  Values = Values.."#   Unit         = nn : Index to selected unit (only used in config webpage)\n"
  Values = Values.."#   Type         = nn : Type of unit:  1: EN 50494, 2: TS 50607\n"
  Values = Values.."#   TunerN       = Slot,Frequency[,Pin]  Slot = 1..nn, Frequency = 950..2150, Pin = 0-255\n"
  Values = Values.."#                                        Slot = 0 (no SCIF)\n"
  Values = Values.."#                                        Slot = 1..8 for EN 50494, 1..32 for TS 50607\n"
  
  if query ~= "reset" then
    local params = {}
    for w in string.gmatch(query,"(%u%w+%=%d+%,?%d*%,?%d*)") do
      table.insert(params,w)
    end

    -- TODO: More validation
    for _,v in ipairs(params) do
      Values = Values..v.."\n"
    end
  else
    Values = Values.."Type=0\n"
    Values = Values.."Tuner1=0\n"
    Values = Values.."Tuner2=0\n"
    Values = Values.."Tuner3=0\n"
    Values = Values.."Tuner4=0\n"
    Values = Values.."Tuner5=0\n"
    Values = Values.."Tuner6=0\n"
    Values = Values.."Tuner7=0\n"
    Values = Values.."Tuner8=0\n"
  end

  SaveOctoserveConf("scif",Values)
  os.execute("/etc/init.d/S99octo restartoctoserve&")
else
  http_print(proto.." 200")
  http_print("Pragma: no-cache")
  http_print("Cache-Control: no-cache")
  http_print("Content-Type: application/x-javascript")
  http_print()

  Values = LoadOctoserveConf("scif")
  
  print("Tuner = new Array();")
  for _,v in pairs(Values) do
    name,i,v1,v2,v3 = string.match(v,"(%a+)(%d-)%=(%d+)%,?(%d*)%,?(%d*)")
    
    if name == "Tuner" then
      http_print(string.format("Tuner[%d] = new Object();",i-1))
      http_print(string.format("Tuner[%d].Slot = %d;",i-1,v1))
      if v2 == "" then v2 = 0 end
      http_print(string.format("Tuner[%d].Freq = %d;",i-1,v2))
      if v3 == "" then v3 = -1 end
      http_print(string.format("Tuner[%d].Pin = %d;",i-1,v3))
    else
      http_print( name .. " = " .. v1 .. ";" )
    end
  end
end
