#!/usr/bin/lua

local query = ""
local DoUpdate = false
local DoCheck = false

if arg[1] then
  if arg[1] == "doupdate" then
    DoUpdate = true
    DoCheck = true
  else
    query = arg[1]
  end
else
  query = os.getenv("QUERY_STRING")
end

if query == "check" then
  DoCheck = true
end

local userver = "download.digital-devices.de/download/linux"
local NewImage = "octonet.0000000000.img"
local CurImage = "octonet.0000000000.img"

if DoCheck then

  local tmp = io.open("/config/updateserver")
  if tmp then
    userver = tmp:read()
    tmp:close()
  end

  local tmp = io.popen(string.format("wget http://%s/octonet/ -q -O -",userver),"r")
  local serverdir = tmp:read("*a")
  tmp:close()

  local tmp = io.popen("ls /config/octonet.*.img","r")
  local configdir = tmp:read("*a")
  tmp:close()

  for v in string.gmatch(serverdir,"\"(octonet%.%d+%.img)\"") do
    if v > NewImage then
      NewImage = v
    end
  end

  for v in string.gmatch(configdir,"(octonet%.%d+%.img)") do
    if v > CurImage then
      CurImage = v
    end
  end
end

if DoUpdate then
  if NewImage > CurImage then
    os.execute("echo CheckDone >/tmp/updatestatus")
    os.remove("/config/tmpimage")
    local wgetstatus = os.execute(string.format("wget -q -P /config http://%s/octonet/%s -O /config/tmpimage >/dev/null 2>/dev/null",userver,NewImage))
    if wgetstatus == 0 then
      local ShaFile = string.gsub(NewImage,".img",".sha")
      wgetstatus = os.execute(string.format("wget -q -P /config http://%s/octonet/%s -O /config/%s >/dev/null 2>/dev/null",userver,ShaFile,ShaFile))
      os.execute("echo DownloadDone >/tmp/updatestatus")
      if wgetstatus == 0 then
        local tmp = io.popen(string.format("sha256sum /config/tmpimage","r"))
        local csum1 = tmp:read()
        tmp:close()
        local csum2 = io.open(string.format("/config/%s",ShaFile)):read()
        csum1 = string.match(csum1,"(%x+)")
        csum2 = string.match(csum2,"(%x+)")
        if csum1 == csum2 then
          os.execute("lua /etc/switch/swsetup.lua gb yb");
          os.rename("/config/tmpimage",string.format("/config/%s",NewImage))
          os.remove(string.format("/config/%s",CurImage))
          os.remove(string.format("/config/%s",string.gsub(CurImage,".img",".sha")))
          os.remove("/boot/uImage")
          os.execute("echo VerifyDone >/tmp/updatestatus")
        else
          print("Error")
          os.remove("/config/tmpimage")
          os.remove(string.format("/config/%s",ShaFile))
          os.execute("echo VerifyError >/tmp/updatestatus")
          return
        end
      else
        os.remove("/config/tmpimage")
        os.remove(string.format("/config/%s",ShaFile))
        os.execute("echo DownloadError >/tmp/updatestatus")
      end
    else
      os.remove("/config/tmpimage")
      os.execute("echo DownloadError >/tmp/updatestatus")
    end
  else
    os.execute("echo CheckError >/tmp/updatestatus")
  end
  return
end

print("HTTP/1.0 200 ")
print("Pragma: no-cache")
print("Content-Type: application/x-javascript")
print("")

if query == "check" then

  if NewImage > CurImage then
    print(string.format("UpdateInfo = \"%s\";",string.match(NewImage,"octonet%.(%d+)")))
  else
    print("UpdateInfo = \"\";")
  end
  print("CheckDone = true;")
elseif query == "update" then
  os.remove("/tmp/updatestatus")
  print("UpdateStarted = true;")
  os.execute("lua update.lua doupdate >/dev/null 2>/dev/null &")
elseif query == "updatestatus" then
  local tmp = io.open("/tmp/updatestatus")
  local updatestatus = ""
  if tmp then
    updatestatus = tmp:read()
    tmp:close()
  end
  if updatestatus == "CheckDone" then
    print("CheckDone = true;")    
  elseif updatestatus == "DownloadDone" then
    print("DownloadDone = true;")    
  elseif updatestatus == "VerifyDone" then
    print("VerifyDone = true;")    
  elseif string.match(updatestatus,"(Error)") then
    print(string.format("UpdateInfo = \'%s\';",updatestatus))
    print("UpdateError = true;")    
  end
else
  print(string.format("UpdateInfo = \'%s\';",query))
  print("UpdateError = true;")
end






