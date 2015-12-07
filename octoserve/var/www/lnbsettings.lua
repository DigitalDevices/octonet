#!/usr/bin/lua

function SaveOctoserveConf(Section,Values)
  local ConfStart = ""
  local f = io.open("/config/octoserve.conf","r")
  local line
  if f then
    while true do
      while true do
        line = f:read()
        if not line then break end
        if string.match(line,"^%["..Section.."%]") then break end
        ConfStart = ConfStart .. line .. "\n"
      end
      while true do
        line = f:read()
        if not line then break end
        if not string.match(line,"^%["..Section.."%]") then
          if string.match(line,"^%[%w+%]") then
            ConfStart = ConfStart .. line .. "\n"
            break 
          end
        end
      end
      if not line then break end
    end
    f:close()
    os.remove("/config/octoserve.bak")
    os.rename("/config/octoserve.conf","/config/octoserve.bak")
  end
    
  f = io.open("/config/octoserve.conf","w")
  if ConfStart then 
    f:write(ConfStart)
  end
  f:write(Values)
  f:close()
end

function LoadOctoserveConf(Section)
  local f = io.open("/config/octoserve.conf","r")
  local line
  local Sections = {}
  local curSection = {}
  if f then
    while true do
      while true do
        line = f:read()
        if not line then break end
        if string.match(line,"^%["..Section.."%]") then
          break
        end
      end
      while true do
        line = f:read()
        if not line then break end
        if string.match(line,"^%["..Section.."%]") then
          if #curSection > 0 then
            table.insert(Sections,curSection)
            curSection = {}        
          end          
        else
          if string.match(line,"^%[%w+%]") then
            break
          else
            if #line > 0 and not string.match(line,"^%#") then 
              table.insert(curSection,line)
            end
          end
        end
      end
      if #curSection > 0 then
        table.insert(Sections,curSection)
        curSection = {}        
      end
      if not line then break end
    end
    f:close()
  end
  return Sections
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
  if query == "get" then query = "" end
  proto = "HTTP/1.0"
  host = "local"
end

if query ~= "" then
  local nextloc = "wait.html?5"    
  local params = io.stdin:read("*a")
  local p,v
  local auto = false
  local conf = ""

  http_print(proto.." 303")
  http_print("Location: http://"..host.."/"..nextloc)
  http_print("")
  
  for p,v in string.gmatch(params,"(%a+)=([0123456789%.]+)") do
    http_print(p,v)
    if p == "auto" and p == "1" then
      auto = true
      break
    end
    if p == "LNB" then
      local lnb,tuner,source,lof1,lof2,lofs = string.match(v,"(%d+)%.(%d+)%.(%d+)%.(%d+)%.(%d+)%.(%d+)")
      
      conf = conf .. "[LNB]\n"
      conf = conf .. "#\n# LNB " .. lnb .. " Setting\n#\n"
      if tuner > "0" then conf = conf .. string.format("Tuner=%d\n",tuner) end
      if source > "0" then conf = conf .. string.format("Source=%d\n",source) end
      conf = conf .. string.format("LOF1=%d\n",lof1) 
      if lof2 > "0" then conf = conf .. string.format("LOF2=%d\n",lof2) end
      if lofs > "0" then conf = conf .. string.format("LOFS=%d\n",lofs) end
      conf = conf .. "\n"
      
    end
  end
  
  SaveOctoserveConf("LNB",conf)
  os.execute("/etc/init.d/S99octo restartoctoserve&")      
else

  http_print(proto.." 200")
  http_print("Pragma: no-cache")
  http_print("Cache-Control: no-cache")
  http_print("Content-Type: application/x-javascript")
  http_print("")

  http_print("LNBList = new Array();")
  
  local i,lnb
  local Conf = LoadOctoserveConf("LNB")
 
  i = 0
  for _,lnb in pairs(Conf) do
    local Tuner  = 0
    local Source = 0
    local LOF1   = 0
    local LOF2   = 0
    local LOFS   = 0
    
    for _,line in pairs(lnb) do
      local n,v = string.match(line,"%s-(%w+)%s-=%s-(%d+)")
      if n == "Tuner" then Tuner = v end
      if n == "Source" then Source = v end
      if n == "LOF1" then LOF1 = v end
      if n == "LOF2" then LOF2 = v end
      if n == "LOFS" then LOFS = v end
      http_print("// " .. n .. " = " .. v);
    end
  
    http_print(string.format("LNBList[%d] = new Object();" ,i))
    http_print(string.format("LNBList[%d].Tuner = %d;"     ,i,Tuner ))
    http_print(string.format("LNBList[%d].Source = %d;"    ,i,Source))
    http_print(string.format("LNBList[%d].LOF1 = %d;"      ,i,LOF1  ))
    http_print(string.format("LNBList[%d].LOF2 = %d;"      ,i,LOF2  ))
    http_print(string.format("LNBList[%d].LOFS = %d;"      ,i,LOFS  ))
  
  
    i = i + 1
  end
  
    
    
end


