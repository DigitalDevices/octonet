#!/usr/bin/lua

local ClientTimeout = 20
local Backlog = 32
DisableDLNA = true

local socket = require("socket")
local mime = require("mime")

local upnp = require("UPnP")
local cman = require("ConnectionManager")
local cdir = require("ContentDirectory")
local mreg = require("MediaReceiverRegistrar")




function sendXMLFile(client,data)
  local r = "HTTP/1.1 200 OK\r\n"
        .. 'Content-Type: text/xml; charset="utf-8"\r\n'
        .. "Connection: close\r\n"
        .. "Content-Length: "..string.format("%d",#data).."\r\n"
        .. "Server: "..UPnP.Server.."\r\n"
        .. "Date: "..os.date("!%a, %d %b %Y %H:%M:%S GMT").."\r\n"
        .. "EXT:\r\n"
        .. "\r\n"
        .. data  
  client:send(r)
end

function sendImage(client,path)
  local p = string.gsub(path,"%.+",".")
  if string.sub(p,1,7) == "/icons/" then
    local f = io.open("."..p,"r")
    if f then
      local image = f:read(100000)
      f:close()
      local t = "jpeg"
      if p.sub(p,-3) == "png" then t = "png" end
    
      local r = "HTTP/1.1 200 OK\r\n"
              .. "Content-Type: image/"..t.."\r\n"
              .. "Content-Length: "..string.format("%d",#image).."\r\n"
              .. "Connection: Close\r\n"
              .. "Server: "..UPnP.Server.."\r\n"
              .. "Date: "..os.date("!%a, %d %b %Y %H:%M:%S GMT").."\r\n"
              .. "\r\n"
              .. image  
      client:send(r)
    else
      upnp:SendHTTPError(client,404)
    end
  else
    upnp:SendHTTPError(client,404)
  end
end

function sendRedirect(client,host)
  local r = "HTTP/1.1 200 OK\r\n"
          .. "Content-Type: text/html\r\n"
          .. "Connection: Close\r\n"
          .. "Server: "..UPnP.Server.."\r\n"
          .. "Refresh: 0; url=http://"..host.."/\r\n"
          .. "\r\n"
          .. '<html><body><a href="http://'..host..'/">Click</a></body></html>\r\n'
  client:send(r)
end

function LoadFile(fname)
  print(fname)
  t = ""
  local f = io.open(fname,"r")
  if not f then os.exit() end
  while true do
    local line = f:read()
    if not line then break end
    t = t .. line .. "\r\n"  
  end
  f:close()
  return t
end

upnp:SetDebug(true)

local port = 8080
-- local RootLocation = "http://10.0.4.31:8080/dms.xml"

local uuid,sernbr,myip = upnp:SystemParameters("f0287290-e1e1-11e2-9a21-000000000000")

local Desc = LoadFile("dms.xml")
Desc = string.gsub(Desc,"##(%a+)##",{ UUID = uuid, SERNBR = sernbr, HOST = myip })

if DisableDLNA then
  Desc = string.gsub(Desc,"(%<dlna:.+DOC%>)","")
end

local server = socket.tcp()
assert(server:setoption("reuseaddr", true))
assert(server:bind("*", port))

local ip, port = server:getsockname()
print("Listen: " .. ip ..  ":" .. port)

assert(server:listen(Backlog))

while true do
  local client = server:accept()  
  client:settimeout(ClientTimeout)
  
  local method,path,proto,attributes = upnp:ReadHTTPHeader(client)
  if method then 
    if method == "GET" then
      if path == "/dms.xml" then
        sendXMLFile(client,Desc)
      elseif path == "/cman.xml" then
        sendXMLFile(client,cman:Description())
      elseif path == "/cdir.xml" then
        sendXMLFile(client,cdir:Description())
      elseif path == "/mreg.xml" then
        sendXMLFile(client,mreg:Description())
      elseif path == "/" then
        sendRedirect(client,attributes["host"])
      else
        sendImage(client,path)
      end      
    elseif method == "SUBSCRIBE" then
      if attributes["NT"] then
        if attributes["NT"] == "upnp:event" and attributes["CALLBACK"] ~= "" then
          if path == "/cde" then
            cdir:Subscribe(client,attributes["CALLBACK"],attributes["TIMEOUT"])
          elseif path == "/cme" then
            cman:Subscribe(client,attributes["CALLBACK"],attributes["TIMEOUT"])
          elseif path == "/mre" then
            mreg:Subscribe(client,attributes["CALLBACK"],attributes["TIMEOUT"])
          else
            upnp:SendSoapError(client,401)
          end
        else
          upnp:SendSoapError(client,412)
        end
      else
        if attributes["SID"] and attributes["SID"] ~= "" then
          if path == "/cde" then
            cdir:Renew(client,attributes["SID"],attributes["TIMEOUT"])
          elseif path == "/cme" then
            cman:Renew(client,attributes["SID"],attributes["TIMEOUT"])
          elseif path == "/mre" then
            mreg:Renew(client,attributes["SID"],attributes["TIMEOUT"])
          else
            upnp:SendSoapError(client,401)
          end
        else
          upnp:SendSoapError(client,412)
        end
      end
    elseif method == "UNSUBSCRIBE" then
      if attributes["SID"] and attributes["SID"] ~= "" then
        if path == "/cde" then
          cdir:Unsubscribe(client,attributes["SID"])
        elseif path == "/cme" then
          cman:Unsubscribe(client,attributes["SID"])
        elseif path == "/mre" then
          mreg:Unsubscribe(client,attributes["SID"])
        else
          upnp:SendSoapError(client,401)
        end
      else
        upnp:SendSoapError(client,412)
      end
    elseif method == "POST" then
      local soap = upnp:ReadHTTPBody(client,attributes["CONTENT-LENGTH"])
        
      if path == "/cdc" then
        cdir:Invoke(client,attributes,soap)
      elseif path == "/cmc" then
        cman:Invoke(client,attributes,soap)
      elseif path == "/mrc" then
        mreg:Invoke(client,attributes,soap)
      else
        upnp:SendSoapError(client,404)
      end
    else
      upnp:SendHTTPError(client,"500")
    end
  end
    
  client:close()
  collectgarbage()

end
