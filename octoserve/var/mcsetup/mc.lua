#!/usr/bin/lua

mc = {}

local DebugFlag = true

function mc.CheckSignal()
  local tmp = io.open("/tmp/mc.signal")
  if tmp then
    tmp:close()
    os.remove("/tmp/mc.signal")
    return true
  end
  return nil  
end

function mc:ReadSetupFile(filename)
  local list = {}
  local file = io.open(filename,"r")
  print(filename,file)
  if file then
    local line = file:read()
    if line then
      if DebugFlag then print(line) end
      line = string.gsub(line,'"','')
      if line:match("TITLE,REQUEST,PIDS,LANPORTS") then
        while true do
          line = file:read()
          if not line then break end
          if DebugFlag then print(line) end
          local title,request,pids,lanports = line:match('%"(.*)%",%"(.*)%",%"P(%a-[%d%:]-)%",%"L(%a-[%d%:]-)%"')
          if not title then
            title,request,pids = line:match('%"(.*)%",%"(.*)%",%"P(%a-[%d%:]-)%"')
          end
          if not title or not request or not pids then
            break
          end
          if pids == "" then break end
          
          pids = string.gsub(pids,":",",")
          if lanports then 
            lanports = string.gsub(lanports,":",",") 
            if lanports == "" then lanports = nil end
            if lanports == "all" then lanports = "1,2,3,4,5" end
          end
          local url 
          if request == "" or string.sub(request,1,1) ~= "?" then
            url = "stream=%d?pids=" .. pids
          else
            url = request .. "&pids=" .. pids
          end
          table.insert(list,{ url = url, lanports = lanports })
        end
      elseif line:match("TITLE,REQUEST,PIDS,PROTO,IP,PORT,TTL,LANPORTS") then
        while true do
          line = file:read()
          if not line then break end
          if DebugFlag then print(line) end
          local title,request,pids,proto,ip,port,ttl,lanports = line:match('%"(.*)%",%"(.*)%",%"P(%a-[%d%:]-)%",%"(%u%u%u)%",%"(%d*%.%d*%.%d*%.%d*)%",(%d*),(%d*),%"L(%a-[%d%:]-)%"')
          if not title then
            title,request,pids,proto,ip,port,ttl = line:match('%"(.*)%",%"(.*)%",%"P(%a-[%d%:]-)%",%"(%u%u%u)%",%"(%d*%.%d*%.%d*%.%d*)%",(%d*),(%d*)')
          end
          if not title or not request or not pids then
            break
          end
          if pids == "" then break end
          if proto ~= "UDP" and proto ~= "RTP" then break end
           
          pids = string.gsub(pids,":",",")
          if lanports then 
            lanports = string.gsub(lanports,":",",") 
            if lanports == "" then lanports = nil end
            if lanports == "all" then lanports = "1,2,3,4,5" end
          end
          local url 
          if request == "" or string.sub(request,1,1) ~= "?" then
            url = "stream=%d?pids=" .. pids
          else
            url = request .. "&pids=" .. pids
          end
          table.insert(list,{ url = url, proto = proto, ip = ip, port = port, ttl = ttl, lanports = lanports })
        end
      end
    end
    file:close()
  end
  return list
end

function mc:Setup(mclist,serverip,devid)
  local streamid = 0
  local port = 6670
  local req = nil
  local dest = 1
  local SessionList = {}
  local entry
    
  for _,entry in ipairs(mclist) do
    print("*************************************************************************")
    req = entry.url
    local rc = nil
    local attributes = nil
    local sdp = nil
    local server = socket.tcp()
    server:settimeout(2)
    local cseq = 1
    if server:connect(serverip,554) then
      --local transport = string.format("RTP/AVP;multicast;port=%d-%d;ttl=5",port,port + 1)
      local destip = string.format("239.%d.2.%d",devid,dest);
      local destport = port;
      local ttl = 5
      if entry.ip then
        destip = entry.ip
      end
      if entry.port then 
        destport = tonumber(entry.port)
      end
      if entry.ttl then 
        ttl = tonumber(entry.ttl)
      end
      local transport = nil
      if entry.proto and entry.proto == "UDP" then
        transport = string.format("UDP;multicast;destination=%s;port=%d;ttl=%d",destip,destport,ttl)
      else
        transport = string.format("RTP/AVP;multicast;destination=%s;port=%d-%d;ttl=%d",destip,destport,destport + 1,ttl)
      end
      
      -- port = port + 2
      if string.match(req,"stream") then
        if streamid > 0 then
          req = string.format(req,streamid)
          rtsp:SendRequest(server,serverip,"SETUP",req,cseq,transport)
          rc,attributes,sdp = rtsp:ReadResponse(server)
          cseq = cseq + 1
          if rc == "200" then
            session = string.match(attributes["SESSION"],"^(%w*);")
            table.insert(SessionList,{ id = session, streamid = streamid, lanports = entry.lanports })
          end
        end
      else
        rtsp:SendRequest(server,serverip,"SETUP",req,cseq,transport)
        rc,attributes,sdp = rtsp:ReadResponse(server)      
        cseq = cseq + 1      
        if rc == "200" then
          streamid = tonumber(attributes["COM.SES.STREAMID"])
          session = string.match(attributes["SESSION"],"^(%w*);")
          table.insert(SessionList,{ id = session, streamid = streamid, lanports = entry.lanports })
        end
      end
      server:close()
    else
      print("Connect?")
      break
    end
    if rc ~= "200" then
      break
    end 
    dest = dest + 1
  end
  return SessionList
end

function mc:Play(SessionList,serverip)
  if #SessionList > 0 then
    local index,session
    for index,session in ipairs(SessionList) do
      print("*************************************************************************",index)
      if session.lanports then
        local server = socket.tcp()
        server:settimeout(2)
        if server:connect(serverip,554) then
          local req = string.format("stream=%d",session.streamid)
          rtsp:SendRequest(server,serverip,"PLAY",req,1,session.id, "switch="..session.lanports)
          local rc,attributes,sdp = rtsp:ReadResponse(server)
          server:close()
        end
      end
    end
  end
end

local function pKeepAlive(mc,SessionList,ip)
  local index = 1
  local server = socket.tcp()
  server:settimeout(2)
  local cseq = 1 
  print(SessionList,ip)
  local status = 0
  if server:connect(ip,554) then
    while status do
      socket.select(nil, nil, 3)
      if mc.CheckSignal() then
        status = 2
        break
      end
      print("*************************************************",index,SessionList[index].id)
      local err = rtsp:SendRequest(server,ip,"DESCRIBE",nil,cseq,SessionList[index].id)
      if err then
        print(err)
        break
      end
      local rc,attributes,sdp = rtsp:ReadResponse(server)
      if not rc or rc ~= "200" then 
        break
      end
      cseq = cseq + 1
      index = index + 1
      if index > #SessionList then
        index = 1
      end
      collectgarbage()
    end
    server:close()
  end
  return status
end

function mc:KeepAlive(SessionList,serverip)
  if #SessionList > 0 then 
    local rc,status = pcall(pKeepAlive,mc,SessionList,serverip)
    if not rc then
      print(status)
      return 1
    else
      return status
    end
  end
  return 0
end

function mc:TearDown(SessionList,serverip)
  if #SessionList > 0 then
    local index,session
    for index,session in ipairs(SessionList) do
      print("*************************************************************************",index)
      local server = socket.tcp()
      server:settimeout(2)
      if server:connect(serverip,554) then
        local req = string.format("stream=%d",session.streamid)
        rtsp:SendRequest(server,serverip,"TEARDOWN",req,1,session.id)
        local rc,attributes,sdp = rtsp:ReadResponse(server)
        server:close()
        if rc ~= "200" then
          print("error",rc)
        end
      end    
    end
  end
end

return mc
