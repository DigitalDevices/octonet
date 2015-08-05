
ConnectionManager = {}

function ConnectionManager:Description()
  t = ""
  local f = io.open("ConnectionManager.xml","r")
  if not f then os.exit() end
  while true do
    local line = f:read()
    if not line then break end
    t = t .. line .. "\r\n"  
  end
  f:close()
  return t
end

local dlnaprofile = 'DLNA.ORG_PN=MPEG_TS;DLNA.ORG_OP=00;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01500000000000000000000000000000'
local dlnaschema = ' xmlns:dlna="urn:schemas-dlna-org:metadata-1-0"'

if DisableDLNA then
  dlnaprofile = '*'
  dlnaschema = ''
end

local Schema = 'xmlns:u="urn:schemas-upnp-org:service:ConnectionManager:1"'


local ProtocolInfo = 'rtsp-rtp-udp:*:video/mp2t:'..dlnaprofile
              .. ','..'rtsp-rtp-udp:*:audio/mp2t:'..dlnaprofile
              -- ','..'http-get:*:video/x-ms-wmv:DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01700000000000000000000000000000'  -- TEST

                            
function ConnectionManager:Invoke(client,Attributes,Request)
  local Action = string.match(Attributes["SOAPACTION"],".+%#([%a%d%-_]+)")
  local Host = tostring(Attributes["host"])
  
  print(Host,"ConnectionManager",Action)
  
  if Action == "GetProtocolInfo" then
    UPnP:SendResponse(client,UPnP:CreateResponse(Schema,Action,{{ n = "Source", v = ProtocolInfo }, { n = "Sink", v = "" }}))
  elseif Action == "GetCurrentConnectionIDs" then
    UPnP:SendResponse(client,UPnP:CreateResponse(Schema,Action,{{ n = "ConnectionIDs", v = "0" }}))
  elseif Action == "GetCurrentConnectionInfo" then
    UPnP:SendSoapError(client,706)
  else
    UPnP:SendSoapError(client,401)
  end
  return
end


function ConnectionManager:Subscribe(client,callback,timeout)
  local r = "HTTP/1.1 200 OK\r\n"
          .. 'Content-Type: text/xml; charset="utf-8"\r\n'
          .. "Server: "..UPnP.Server.."\r\n"
          .. "SID: uuid:50c95801-e839-4b96-b7ae-779d989e1399\r\n"
          .. "Timeout: Second-1800\r\n"
          .. "Content-Length: 0\r\n"
          .. "Connection: close\r\n"
          .. "EXT:\r\n"
          .. "\r\n"
  client:send(r)  
  
  local ipaddr,port = client:getpeername()  
  local Args = { { n = "SourceProtocolInfo", v = ProtocolInfo }, { n = "SourceProtocolInfo", v = "" }, { n = "CurrentConnectionIDs", v = "0" } }
  UPnP:SendEvent(callback,"50c95801-e839-4b96-b7ae-779d989e1399",0,Args)
end

function ConnectionManager:Renew(client,sid,timeout)
  local r = "HTTP/1.1 200 OK\r\n"
          .. 'Content-Type: text/xml; charset="utf-8"\r\n'
          .. "Server: "..UPnP.Server.."\r\n"
          .. "SID: uuid:50c95801-e839-4b96-b7ae-779d989e1399\r\n"
          .. "Timeout: Second-1800\r\n"
          .. "Content-Length: 0\r\n"
          .. "Connection: close\r\n"
          .. "EXT:\r\n"
          .. "\r\n"
  client:send(r)  
end

function ConnectionManager:Unsubscribe(client,sid)
  local r = "HTTP/1.1 200 OK\r\n"
          .. "Server: "..UPnP.Server.."\r\n"
          .. "Connection: close\r\n"
          .. "\r\n"
  client:send(r)  
end

return ConnectionManager