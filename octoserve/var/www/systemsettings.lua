#!/usr/bin/lua

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
  local enabled = false
  local tmp = io.open("/config/"..name..".enabled","r")
  if tmp then
    enabled = true
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


local host = os.getenv("HTTP_HOST")
local proto = os.getenv("SERVER_PROTOCOL")
local query = os.getenv("QUERY_STRING")

if arg[1] then
  query = arg[1]
  proto = "HTTP/1.0"
  host = "local"
end

if query ~= "" then
    os.execute("echo \""..query.."\" >/tmp/query")
    local params = {}
    for w in string.gmatch(query,"(%a%w+%=%d+%,?%d*%,?%d*)") do
      table.insert(params,w)
    end
    
    -- TODO: More validation
    local nextloc = "index.html"
    local restart = 0;
    for _,v in ipairs(params) do
      name,value = string.match(v,"(%w+)%=(%d+)")
      if( WriteSetting(name,value == "1") ) then
        if name == "telnet" then 
          os.rename("/etc/securetty","/etc/securetty.bak"); -- temp fix to allow root login on telnet
          os.execute("/etc/init.d/S91telnet restart") 
        end
--        if name == "vlan" then os.execute("/etc/init.d/S99octo restartoctonet&") end
--	if name == "nodms" then os.execute("/etc/init.d/S99octo restartoctonet&") end
--        if name == "nodvbt" then os.execute("/etc/init.d/S99octo restartoctonet&") end
        if name == "vlan" then restart = 1 end
	if name == "nodms" then restart = 1 end
        if name == "nodvbt" then restart = 1 end
        if name == "noswitch" then restart = 1 end
        nextloc = "wait.html?5"
      end
    end
    if restart == 1 then os.execute("/etc/init.d/S99octo restartoctonet&") end
    print(proto.." 303")
    print("Location: http://"..host.."/"..nextloc)
    print("")
else

  print(proto.." 200")
  print("Pragma: no-cache")
  print("Content-Type: application/x-javascript")
  print("")

  if ReadSetting("telnet") then
    print("telnetEnabled = true;")
  else
    print("telnetEnabled = false;")
  end

  if ReadSetting("vlan") then
    print("vlanEnabled = true;")
  else
    print("vlanEnabled = false;")
  end
    
  if ReadSetting("nodms") then
    print("nodmsEnabled = true;")
  else
    print("nodmsEnabled = false;")
  end
    
  if ReadSetting("nodvbt") then
    print("nodvbtEnabled = true;")
  else
    print("nodvbtEnabled = false;")
  end
    
  if ReadSetting("noswitch") then
    print("noswitchEnabled = true;")
  else
    print("noswitchEnabled = false;")
  end
    
end


