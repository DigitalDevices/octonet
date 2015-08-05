
local DataBase = {}

local SourceList = {}
local ChannelList = {}

local function AddSource(refid,title,system,src)
  table.insert(SourceList, { refid = refid, title = title, src = src, system = system } )
end

local function AddChannel(sourceid,title,request,tracks)
  table.insert(ChannelList, { refid = sourceid, title = title, request = request, tracks = tracks } )
end

local function ReadFile(sourceid,file,system)
  while true do
    line = file:read()
    if not line then break end
    local cat,title,sys,freq,sr,pol,mtype,bw,ds,plp,t2lite,radio,enc,pids,tracks
    local request = ""
    
    if system == "dvbs" or system == "dvbs2" then
      cat,title,sys,freq,sr,pol,radio,enc,pids,tracks = string.match(line,'%"(.*)%",%"(.*)%",%"(.*)%",(%d+),(%d+),%"(.*)%",(%d+),(%d+),%"P(.*)%",%"A(.*)%"')
      if not tracks then break end
      if sys == "dvbs" or "dvbs2" then
        pids = string.gsub(pids,":",",")
        tracks = string.gsub(tracks,":",",") 
        request = "freq="..freq.."&pol="..pol.."&msys="..sys.."&sr="..sr.."&pids=0,"..pids      
      end
    elseif system == "dvbc" then
      cat,title,sys,freq,sr,mtype,radio,enc,pids,tracks = string.match(line,'%"(.*)%",%"(.*)%",%"(.*)%",(%d+),(%d+),%"(.*)%",(%d+),(%d+),%"P(.*)%",%"A(.*)%"')
      if not tracks then break end
      pids = string.gsub(pids,":",",")
      tracks = string.gsub(tracks,":",",") 
      local f = string.sub(freq,1,3).."."..string.sub(freq,4,6)
      if sys == "dvbc" then
        request = "freq="..f.."&msys=dvbc&sr="..sr.."&mtype="..mtype.."&pids=0,"..pids      
      end
    elseif system == "dvbc2" then
      cat,title,sys,freq,bw,ds,plp,mtype,radio,enc,pids,tracks = string.match(line,'%"(.*)%",%"(.*)%",%"(.*)%",(%d+),(%d+),(%d+),(%d+),%"(.*)%",(%d+),(%d+),%"P(.*)%",%"A(.*)%"')
      if not tracks then break end
      pids = string.gsub(pids,":",",")
      tracks = string.gsub(tracks,":",",") 
      local f = string.sub(freq,1,3).."."..string.sub(freq,4,6)
      if sys == "dvbc" then
        request = "freq="..f.."&msys=dvbc&sr="..bw.."&mtype="..ds.."&pids=0,"..pids      
      elseif sys == "dvbc2" then
        request = "freq="..f.."&msys=dvbc2&bw="..bw.."&x_ds="..ds.."&plp="..plp.."&pids=0,"..pids      
      end      
    elseif system == "dvbt" or system == "dvbt2" then
      cat,title,sys,freq,bw,plp,t2lite,radio,enc,pids,tracks = string.match(line,'%"(.*)%",%"(.*)%",%"(.*)%",(%d+),(%d+),(%d+),(%d+),(%d+),(%d+),%"P(.*)%",%"A(.*)%"')
      if not tracks then break end
      pids = string.gsub(pids,":",",")
      tracks = string.gsub(tracks,":",",") 
      local f = string.sub(freq,1,3).."."..string.sub(freq,4,6)
      if sys == "dvbt" then
        request = "freq="..f.."&msys=dvbt&bw="..bw.."&pids=0,"..pids      
      elseif sys == "dvbt2" then
        if bw == "1" then bw = "1.712" end
        request = "freq="..f.."&msys=dvbt2&bw="..bw.."&plp="..plp.."&pids=0,"..pids      
      end      
    end

    if request ~= "" then
      if enc ~= "0" then request = request.."&x_pmt="..enc end
      AddChannel(sourceid,title,request, tracks)
    end
  end

  file:close()
end

local function OpenCSV(name,line1)
  local file = io.open("/config/channels/"..name..".csv","r")
  if file then 
    local line = file:read()
    line = string.gsub(line,'"','')
    if string.match(line,line1) then
      return file
    end
    file:close()
  end
  file = io.open("/var/channels/"..name..".csv","r")
  if file then 
    local line = file:read()
  end
  return file
end

local srcfile = OpenCSV("sourcelist","KEY,CSVFILE,NAME,SYSTEM,SRC")
if srcfile then
  while true do
    line = srcfile:read()
    if not line then break end
    local key,csvfile,name,system,src = string.match(line,'%"(.*)%",%"(.*)%",%"(.*)%",%"(.*)%",(%d+)')
    if not key or not csvfile or not name or not system or not src then
      break
    end
    if system == "dvbs" or system == "dvbs2" then 
      AddSource(key,name,system,src)
    elseif system == "dvbc" or system == "dvbc2" or system == "dvbt" or system == "dvbt2" then 
      AddSource(key,name,system,nil)
    else
      csvfile = nil
    end
    if csvfile then
      local header = "?"
      if system == "dvbs" or system == "dvbs2" then 
        header = "CATEGORY,TITLE,SYSTEM,FREQ,SR,POL,RADIO,ENC,PIDS,TRACKS"
      elseif system == "dvbc" then 
        header = "CATEGORY,TITLE,SYSTEM,FREQ,SR,MOD,RADIO,ENC,PIDS,TRACKS"
      elseif system == "dvbc2" then 
        header = "CATEGORY,TITLE,SYSTEM,FREQ,BW/SR,DS,PLP,MOD,RADIO,ENC,PIDS,TRACKS"
      elseif system == "dvbt" or system == "dvbt2" then 
        header = "CATEGORY,TITLE,SYSTEM,FREQ,BW,PLP,LITE,RADIO,ENC,PIDS,TRACKS"
      end
    
      local file = OpenCSV(csvfile,header)
      if file then
        ReadFile(key,file,system)
      end
    end
  end
  srcfile:close()
end

DataBase.SourceList = SourceList
DataBase.ChannelList = ChannelList

return DataBase