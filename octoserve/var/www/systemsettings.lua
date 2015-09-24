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
--~           os.rename("/etc/securetty","/etc/securetty.bak"); -- temp fix to allow root login on telnet
          os.execute("/etc/init.d/S91telnet restart") 
        end
        if name == "vlan" then restart = 1 end
        if name == "nodms" then restart = 1 end
        if name == "nodvbt" then restart = 1 end
        if name == "noswitch" then restart = 1 end
        if name == "strict" then restart = 1 end
        nextloc = "wait.html?10"
      end
    end
    if restart == 1 then os.execute("/etc/init.d/S99octo restartoctonet > /dev/null 2>&1 &") end
    print(proto.." 303")
    print("Location: http://"..host.."/"..nextloc)
    print("")
else

  http_print(proto.." 200")
  http_print("Pragma: no-cache")
  http_print("Cache-Control: no-cache")
  http_print("Content-Type: application/x-javascript")
  http_print()

  if ReadSetting("telnet") then
    http_print("telnetEnabled = true;")
  else
    http_print("telnetEnabled = false;")
  end

  if ReadSetting("vlan") then
    http_print("vlanEnabled = true;")
  else
    http_print("vlanEnabled = false;")
  end
    
  if ReadSetting("nodms") then
    http_print("nodmsEnabled = true;")
  else
    http_print("nodmsEnabled = false;")
  end
    
  if ReadSetting("nodvbt") then
    http_print("nodvbtEnabled = true;")
  else
    http_print("nodvbtEnabled = false;")
  end
    
  if ReadSetting("noswitch") then
    http_print("noswitchEnabled = true;")
  else
    http_print("noswitchEnabled = false;")
  end

  if ReadSetting("strict") then
    http_print("strictEnabled = true;")
  else
    http_print("strictEnabled = false;")
  end

  
end


