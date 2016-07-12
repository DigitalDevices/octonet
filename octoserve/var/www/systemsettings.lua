#!/usr/bin/lua

local url = require("socket.url")

function SaveOctoserveConf(Section,Values)
  local ConfStart
  local ConfEnd
  local f = io.open("/config/octoserve.conf","r")
  if f then
    local CurConf = f:read("*a")
    f:close()
    ConfStart,ConfEnd = string.match(CurConf,"(.-)%["..Section.."%].+\n(%[.+)")
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

function ReadSetting(name)
  local enabled = "false"
  local tmp = io.open("/config/"..name..".enabled","r")
  if tmp then
    enabled = "true"
    tmp:close()
  end
  return(enabled)
end

function WriteSetting(name,enabled)
  local wasenabled = false
  if os.remove("/config/"..name..".enabled") then wasenabled = true end
  if( enabled ) then
    local f = io.open("/config/"..name..".enabled","w")
    if f then
      f:write("1")
      f:close()
    end
  end
  return enabled ~= wasenabled
end

function WriteConfigFile(name,value)
  local f = io.open("/config/"..name,"w")
  if f then
    f:write(value)
    f:close()
  end
end

function CheckMaxS8()
   local isMaxS8 = "false"
   local tmp = io.open("/sys/class/ddbridge/ddbridge0/devid1")
   if tmp then
      devid = tmp:read("*l")
      tmp:close()
      if devid == "0007dd01" then
         isMaxS8 = "true"
      end
   end
   return isMaxS8
end

function GetBoxName()
   local boxname = ""
   local tmp = io.open("/config/boxname")
   if tmp then
      boxname = tmp:read("*l")
      boxname = boxname:gsub("OctopusNet:","",1);
      tmp:close()
   end
   return boxname
end

function GetMSMode()
   local msmode = "quad"
   local tmp = io.open("/config/msmode")
   if tmp then
      msmode = tmp:read("*l")
      tmp:close()
   elseif ReadSetting('noswitch') == "true" then
      msmode = "none"
   end
   return msmode
end


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

if arg[1] then
  query = arg[1]
  proto = "HTTP/1.0"
  host = "local"
end

if query ~= "" then
    query = url.unescape(query)
    os.execute("echo \""..query.."\" >/tmp/query")
    local params = {}
    for w in string.gmatch(query,"(%a%w*%=[%w %(%)%-@/]*)") do
      table.insert(params,w)
      os.execute("echo \""..w.."\" >>/tmp/query")
    end
    
    -- TODO: More validation
    local nextloc = "index.html"
    local restart = 0;
    local restartdms = 0;
    for _,v in ipairs(params) do
      name,value = string.match(v,"(%w+)%=(.+)")
      if name == "msmode" then
         os.remove("/config/noswitch.enabled")
         WriteConfigFile("msmode",value);
         restart = 1;         
      elseif name == "boxname" then
         if value ~= "" then
            WriteConfigFile("boxname","OctopusNet:"..value);
         else
            os.remove("/config/boxname")
         end
         restart = 1;
         restartdms = 1;
      elseif( WriteSetting(name,value == "1") ) then
        if name == "telnet" then 
          os.execute("/etc/init.d/S91telnet restart") 
        end
        if name == "vlan" then restart = 1 end
        if name == "nodms" then restart = 1 end
        if name == "nodvbt" then restart = 1 end
        if name == "strict" then restart = 1 end
      end
    end
    if restart == 1 then 
      os.execute("/etc/init.d/S99octo restartoctonet > /dev/null 2>&1 &")
      nextloc = "wait.html?10"
    end
    if restartdms == 1 then
      os.execute("/etc/init.d/S92dms restart > /dev/null 2>&1 &")
      nextloc = "wait.html?10"
    end
    print(proto.." 303")
    print("Location: http://"..host.."/"..nextloc)
    print("")
else

  http_print(proto.." 200")
  http_print("Pragma: no-cache")
  http_print("Cache-Control: no-cache")
  http_print("Content-Type: application/json; charset=utf-8")
  http_print()

  http_print('{')
  
  http_print(' "BoxName":"' .. GetBoxName() .. '",')
  http_print(' "isMaxS8":' .. CheckMaxS8() .. ',')
  http_print(' "telnetEnabled":' .. ReadSetting('telnet') .. ',')
  http_print(' "vlanEnabled":' .. ReadSetting('vlan') .. ',')
  http_print(' "nodmsEnabled":' .. ReadSetting('nodms') .. ',')
  http_print(' "nodvbtEnabled":' .. ReadSetting('nodvbt') .. ',')
  http_print(' "MSMode":"' .. GetMSMode() .. '",')  
  http_print(' "strictEnabled":' .. ReadSetting('strict'))
  
  http_print('}')

  
end


