#!/usr/bin/lua

rtsp = {}

local DebugFlag = true
local CRLF = "\r\n"

local function SplitLines(s)
  local lines = {}
  local line = nil
  for line in string.gmatch(s,"(.-)"..CRLF) do
    table.insert(lines,line)
  end
  return lines
end

function rtsp:ReadResponse(server)

  local linenum = 0
  local line, err = server:receive()
  
  if err then
    if DebugFlag then print("RTSP Error "..err) end
    return 
  end

  if DebugFlag then print(string.format("%2d:%s",linenum,line)) end
  local rc = string.match(line,"%s+(%d+)")
  local attributes = {}

  while true do
    local line, err = server:receive()
    if err then 
      if DebugFlag then print("RTSP Error "..err.." Line: "..tostring(linenum)) end
      return 
    end
    linenum = linenum + 1
    if DebugFlag then print(string.format("%2d:%s",linenum,line)) end
    if line == "" then break end
    
    if linenum == 30 then
      if DebugFlag then print("RTSP Error "..tostring(linenum)) end
      return 
    end
    
    local n,v = string.match(line,"([%a%-%.]+)%s*%:%s*(.*)")
    if n and v then
      attributes[string.upper(n)] = v
    end
    
  end
  
  local sdp = nil
  local ctype = attributes["CONTENT-TYPE"]
  if ctype then
    if string.lower(ctype) == "application/sdp" then
      local clen = attributes["CONTENT-LENGTH"]
      line, err = server:receive(tonumber(clen))
      if err then 
        if DebugFlag then print("RTSP Error "..err.." Line: "..tostring(linenum)) end
        return 
      else
       sdp = SplitLines(line)
      end
    end
  end
  return rc,attributes,sdp
end

function rtsp:SendRequest(server,ip,command,request,cseq,session_transport,x_octonet)
  if not request then request = "" end
  local s = command .. " rtsp://"..ip..":554/"..request.." RTSP/1.0" .. CRLF
         .. "User-Agent: DD_MCSetup" .. CRLF
  if cseq then s = s .. "Cseq: ".. tostring(cseq)..CRLF end
  if session_transport then 
    if command == "SETUP" then
      s = s .. "Transport: "..session_transport..CRLF         
    else
      s = s .. "Session:" ..session_transport..CRLF 
    end
  end
  if x_octonet then
    s = s .. "x_octonet: "..x_octonet..CRLF
  end
  s = s .. CRLF
  if DebugFlag then print(s) end
  local r,err = server:send(s)  
  if not r then
    return err
  end
  return nil
end

return rtsp
