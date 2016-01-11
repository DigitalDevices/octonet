

local ContentDirectory = {}

-- local dlnaprofile = 'DLNA.ORG_PN=MPEG_TS;DLNA.ORG_OP=00;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=0D100000000000000000000000000000'
local dlnaprofile = 'DLNA.ORG_PN=MPEG_TS;DLNA.ORG_OP=00;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=8D100000000000000000000000000000'
local dlnaschema = ' xmlns:dlna="urn:schemas-dlna-org:metadata-1-0"'

if DisableDLNA then
  dlnaprofile = '*'
  dlnaschema = ''
end

local Schema = 'xmlns:u="urn:schemas-upnp-org:service:ContentDirectory:1"'

local DIDLStart = '' -- '&lt;?xml version="1.0" encoding="utf-8"?&gt;'
                ..'&lt;DIDL-Lite xmlns:dc="http://purl.org/dc/elements/1.1/"'
                ..' xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/"'
                ..' xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"'
                -- .. dlnaschema
                ..'&gt;'
local DIDLEnd = '&lt;/DIDL-Lite&gt;'

-- State variables
local SystemUpdateID = 36
-- ---------------
----------------------------------------------------------------------------------------------------------------------------------------

local AllFolders = {}
local RootFolders = {}
local Folders = {}
local AllItems = {}

local i,f,vi,ai

----- Hierarchical Folders

RootFolders[#RootFolders+1] = { title = "All", id = "64" }
RootFolders[#RootFolders+1] = { title = "Audio", id = "1" }
RootFolders[#RootFolders+1] = { title = "Images", id = "3" }
RootFolders[#RootFolders+1] = { title = "Video", id = "2" }
for _,f in ipairs(RootFolders) do
  f.VideoItems = {}
  f.AudioItems = {}
  f.ChildFolders = {}
  AllFolders[f.id] = f
end
AllFolders["2"].ChildFolders = Folders
AllFolders["64"].ChildFolders = Folders

----
local file = io.open("/config/ChannelList.json")
if file then
   local json = file:read("*a")
   file:close()
   local newdecoder = require("lunajson.decoder")
   local decode = newdecoder()
   local channellist = decode(json)

   for _,group in ipairs(channellist.GroupList) do
      local f = {}
      f.id = "$"..tostring(#Folders)
      f.VideoItems = {}
      f.AudioItems = {}
      f.ChildFolders = {}
      f.title = string.gsub(group.Title,'&','&amp;amp;')
      f.title = string.gsub(f.title,'<','&amp;lt;')
      f.title = string.gsub(f.title,'>','&amp;gt;')
      table.insert(Folders,f)
      --  table.insert(RootFolders,f)
      AllFolders[f.id] = f
      for _,channel in ipairs(group.ChannelList) do
         local vi = {}
         vi.id = f.id.."$"..tostring(#f.VideoItems)
         vi.src = f.src
         vi.parentID = f.id
         vi.request = string.gsub(channel.Request:sub(2),'&','&amp;amp;')
         vi.title = string.gsub(channel.Title,'&','&amp;amp;')
         vi.title = string.gsub(vi.title,'<','&amp;lt;')
         vi.title = string.gsub(vi.title,'>','&amp;gt;')
         table.insert(f.VideoItems,vi)
         AllItems[vi.id] = vi
      end
   end
else
   local db = require("DataBase")
   for _,f in ipairs(db.SourceList) do
     f.id = f.refid
     f.VideoItems = {}
     f.AudioItems = {}
     f.ChildFolders = {}
     table.insert(Folders,f)
   --  table.insert(RootFolders,f)
     AllFolders[f.id] = f
   end

   for _,vi in ipairs(db.ChannelList) do
     f = AllFolders[vi.refid]
     vi.id = f.id.."$"..tostring(#f.VideoItems)
     vi.src = f.src
     vi.parentID = f.id
     vi.request = string.gsub(vi.request,'&','&amp;amp;')
     vi.title = string.gsub(vi.title,'&','&amp;amp;')
     vi.title = string.gsub(vi.title,'<','&amp;lt;')
     vi.title = string.gsub(vi.title,'>','&amp;gt;')
     table.insert(f.VideoItems,vi)
     AllItems[vi.id] = vi
   end
end

----- Add Stream Folder

f = {}
f.id = "STRM"
f.title = "Current Streams"
f.VideoItems = {}
f.AudioItems = {}
f.ChildFolders = {}
table.insert(Folders,f)
AllFolders[f.id] = f
for i = 1,4,1 do
  vi = { id = f.id.."$"..tostring(i-1), parentID = f.id, title = "Stream "..tostring(i), stream = tostring(i) }
  table.insert(f.VideoItems,vi)
  AllItems[vi.id] = vi
end

----------------------------------------------------------------------------------------------------------------------------------------

local function Folder(title,id,parentid,childCount)
  local F = '&lt;container id="'..id..'" parentID="'..parentid..'"'
        -- ..' childCount="'..childCount..'"'
        ..' restricted="1"'
        -- ..' searchable="1"'
        ..'&gt;'
        ..'&lt;dc:title&gt;'..title..'&lt;/dc:title&gt;'
        ..'&lt;upnp:class&gt;object.container.storageFolder&lt;/upnp:class&gt;'
        ..'&lt;/container&gt;'
  return F
end

local function VideoItem(Host,Item,nCompat)
  local rtspreq = ''
  if Item.stream then
    rtspreq = 'stream='..Item.stream
  else
    -- Some clients don't like a long request url, or an url with '&' in it
    -- Fail them for now
    -- if nCompat then
      -- rtspreq = "stream_99"
    -- elseif Item.src then
    if Item.src then
      rtspreq = '?src='..Item.src..'&amp;amp;'..Item.request
    else
      rtspreq = '?'..Item.request
    end
  end

  local didl = '&lt;item id="'..Item.id..'" parentID="'..Item.parentID
  didl = didl .. '" restricted="1"&gt;'
              ..'&lt;dc:title&gt;'..Item.title..'&lt;/dc:title&gt;'
              ..'&lt;upnp:class&gt;object.item.videoItem.videoBroadcast&lt;/upnp:class&gt;'
  if Item.channelNr then
    didl = didl ..'&lt;upnp:channelNr&gt;'..Item.channelNr..'&lt;/upnp:channelNr&gt;'
  end
  didl = didl ..'&lt;res'
              ..' protocolInfo="rtsp-rtp-udp:*:video/mpeg:'..dlnaprofile..'"&gt;'
              ..'rtsp://'..Host..':554/'..rtspreq
              ..'&lt;/res&gt;'
  didl = didl ..'&lt;/item&gt;'
  return didl

end


local function BrowseChildren(client,Host,Request,nCompat)
  local ObjectID = UPnP:GetRequestParam(Request,"ObjectID")
  local BrowseFlag = UPnP:GetRequestParam(Request,"BrowseFlag")
  local Filter = UPnP:GetRequestParam(Request,"Filter")
  local StartingIndex = tonumber(UPnP:GetRequestParam(Request,"StartingIndex"))
  local RequestedCount = tonumber(UPnP:GetRequestParam(Request,"RequestedCount"))
  print("BrowseChildren",ObjectID,Filter,StartingIndex,RequestedCount)

  local didl = DIDLStart;
  local Error = 0
  local NumberReturned = 0
  local TotalMatches = 0
  local UpdateID = SystemUpdateID
  local f,vi,ai

  if ObjectID == "0" then
    if nCompat then
      for _,f in ipairs(RootFolders) do
        didl = didl..Folder(f.title,f.id,ObjectID,tostring(#f.VideoItems + #f.AudioItems + #f.ChildFolders))
        NumberReturned = NumberReturned + 1
        TotalMatches = TotalMatches +1
      end
    else
      for _,f in ipairs(Folders) do
        didl = didl..Folder(f.title,f.id,ObjectID,tostring(#f.VideoItems + #f.AudioItems + #f.ChildFolders))
        NumberReturned = NumberReturned + 1
        TotalMatches = TotalMatches +1
      end
    end

  else
    local f = AllFolders[ObjectID]
    if f then

      local Index = 0
      for i,cf in ipairs(f.ChildFolders) do
        if Index >= StartingIndex and (RequestedCount == 0 or NumberReturned < RequestedCount) then
          didl = didl..Folder(cf.title,cf.id,ObjectID,tostring(#cf.VideoItems + #cf.AudioItems + #cf.ChildFolders))
          NumberReturned = NumberReturned + 1
        end
        Index = Index + 1
        TotalMatches = TotalMatches +1
      end

      for i,vi in ipairs(f.VideoItems) do
        if Index >= StartingIndex and (RequestedCount == 0 or NumberReturned < RequestedCount) then
          didl = didl..VideoItem(Host,vi,nCompat)
          NumberReturned = NumberReturned + 1
        end
        Index = Index + 1
        TotalMatches = TotalMatches +1
      end

    else
      Error = 710
    end
  end

  didl = didl..DIDLEnd
  print("Returned",StartingIndex,NumberReturned,TotalMatches,Error)

  if Error == 0 then
    local Args = { { n = "Result", v = didl }, { n = "NumberReturned", v = tostring(NumberReturned)},
                   { n = "TotalMatches", v = tostring(TotalMatches)}, { n = "UpdateID", v = tostring(UpdateID) } }
    UPnP:SendResponse(client,UPnP:CreateResponse(Schema,"Browse",Args))
  else
    UPnP:SendSoapError(client,Error)
  end

  return
end

local function BrowseMetaData(client,Host,Request,nCompat)
  local ObjectID = UPnP:GetRequestParam(Request,"ObjectID")
  local didl = DIDLStart;
  local UpdateID = SystemUpdateID
  local Error = 0

  if ObjectID == "0" then
    local ChildCount = #Folders
    if nCompat then ChildCount = #RootFolders end
    didl = didl .. '&lt;container id="1" parentID = "-1" childCount="'..tostring(ChildCount)..'" restricted="true"&gt>'
                .. '&lt:dc:title&gt;OctopusNet&lt;/dc:title&gt;'
                ..'&lt;upnp:class&gt;object.container.storageFolder&lt;/upnp:class&gt;'
                ..'&lt;/container&gt;'
  else
    local f = AllFolders[ObjectID]
    if f then
      didl = didl..Folder(f.title,f.id,ObjectID,tostring(#f.VideoItems + #f.AudioItems + #f.ChildFolders))
    else
      local item = AllItems[ObjectID]
      if item then
        didl = didl..VideoItem(Host,item,nCompat)
      else
        Error = 710
      end
    end
  end

  didl = didl..DIDLEnd

  if Error == 0 then
    local Args = { { n = "Result", v = didl }, { n = "NumberReturned", v = "1"},
                   { n = "TotalMatches", v = "1"}, { n = "UpdateID", v = tostring(UpdateID) } }
    UPnP:SendResponse(client,UPnP:CreateResponse(Schema,"Browse",Args))
  else
    UPnP:SendSoapError(client,Error)
  end

  didl = DIDLEnd;
end

local function Search(client,Host,Request,nCompat)
  local ContainerID = UPnP:GetRequestParam(Request,"ContainerID")
  local SearchCriteria = UPnP:GetRequestParam(Request,"SearchCriteria")
  local Filter = UPnP:GetRequestParam(Request,"Filter")
  local StartingIndex = tonumber(UPnP:GetRequestParam(Request,"StartingIndex"))
  local RequestedCount = tonumber(UPnP:GetRequestParam(Request,"RequestedCount"))
  local SortCriteria = UPnP:GetRequestParam(Request,"SortCriteria")
  print(SearchCriteria,ContainerID,Filter,StartingIndex,RequestedCount,SortCriteria)

  local didl = DIDLStart;
  local Error = 0
  local NumberReturned = 0
  local TotalMatches = 0
  local UpdateID = SystemUpdateID

  if string.match(SearchCriteria,"videoItem") then

    if ContainerID == "0" then
      if not nCompat or nCompat ~= "WMP" then
        local Index = 0
        for _,vi in pairs(AllItems) do
          if Index >= StartingIndex and (RequestedCount == 0 or NumberReturned < RequestedCount) then
            didl = didl..VideoItem(Host,vi,nCompat)
            NumberReturned = NumberReturned + 1
          end
          Index = Index + 1
          TotalMatches = TotalMatches + 1
          -- if nCompat and TotalMatches > 19 then break end
        end
      end

    else
      local f = AllFolders[ContainerID]
      if f then
        local src = f.src
        if src then src = "src="..src.."&"  else src = "" end

        local Index = 0
        for _,vi in ipairs(f.VideoItems) do
          if Index >= StartingIndex and (RequestedCount == 0 or NumberReturned < RequestedCount) then
            didl = didl..VideoItem(Host,vi,nCompat)
            NumberReturned = NumberReturned + 1
          end
          Index = Index + 1
          TotalMatches = TotalMatches +1
        end
      else
        Error = 710
      end
    end
  end

  didl = didl..DIDLEnd
  -- didl = TestDidl
    -- NumberReturned = 1
    -- TotalMatches = 1
  print("Returned",StartingIndex,NumberReturned,TotalMatches,Error)


  if Error == 0 then
    local Args = { { n = "Result", v = didl }, { n = "NumberReturned", v = tostring(NumberReturned)},
                   { n = "TotalMatches", v = tostring(TotalMatches)}, { n = "UpdateID", v = tostring(UpdateID) } }
    UPnP:SendResponse(client,UPnP:CreateResponse(Schema,"Search",Args))
  else
    UPnP:SendSoapError(client,Error)
  end

  return
end

local function SendResult(client,Action,VarName,Result)
  local Args = { { n = VarName, v = Result } }
  UPnP:SendResponse(client,UPnP:CreateResponse(Schema,Action,Args))
end

function ContentDirectory:Invoke(client,Attributes,Request)
  local Action = string.match(Attributes["SOAPACTION"],".+%#([%a%d%-_]+)")
  local Host = tostring(Attributes["host"])

  local Compability = nil
  if Attributes["USER-AGENT"] then
    if string.match(Attributes["USER-AGENT"],"Windows%-Media%-Player") then Compability = "WMP"
    elseif string.match(Attributes["USER-AGENT"],"IPI%/1%.0") then Compability = "IPI"
    end
  end
  print("Compability=",Compability)

  print(Host,"ContentDirectory",Action)
  if Action == "Browse" then
    local BrowseFlag = tostring(UPnP:GetRequestParam(Request,"BrowseFlag"))
    if BrowseFlag == "BrowseDirectChildren" then
      BrowseChildren(client,Host,Request,Compability)
    elseif BrowseFlag == "BrowseMetadata" then
      BrowseMetaData(client,Host,Request,Compability)
    else
      UPnP:SendSoapError(client,710)
    end
  elseif Action == "Search" then
    Search(client,Host,Request,Compability)
  -- elseif Action == "X_GetRemoteSharingStatus" then
    -- SendResult(client,Action,"0")
  elseif Action == "GetSortCapabilities" then
    SendResult(client,Action,"SortCaps","dc:title,upnp:class,upnp:originalTrackNumber")
  elseif Action == "GetSearchCapabilities" then
    SendResult(client,Action,"SearchCaps","dc:title")
  elseif Action == "GetSystemUpdateID" then
    SendResult(client,Action,"Id","1")
  else
    UPnP:SendSoapError(client,401)
  end
end

function ContentDirectory:Subscribe(client,callback,timeout)
  local r = "HTTP/1.1 200 OK\r\n"
          .. 'Content-Type: text/xml; charset="utf-8"\r\n'
          .. "Server: "..UPnP.Server.."\r\n"
          .. "SID: uuid:50c95800-e839-4b96-b7ae-779d989e1399\r\n"
          .. "Timeout: Second-1800\r\n"
          .. "Content-Length: 0\r\n"
          .. "Connection: close\r\n"
          .. "EXT:\r\n"
          .. "\r\n"
  client:send(r)

  local ipaddr,port = client:getpeername()
  local Args = { { n = "TransferIDs", v = "" }, { n = "SystemUpdateID", v = tostring(SystemUpdateID) } }
  UPnP:SendEvent(callback,"50c95800-e839-4b96-b7ae-779d989e1399",0,Args)

end

function ContentDirectory:Renew(client,sid,timeout)
  local r = "HTTP/1.1 200 OK\r\n"
          .. 'Content-Type: text/xml; charset="utf-8"\r\n'
          .. "Server: "..UPnP.Server.."\r\n"
          .. "SID: uuid:50c95800-e839-4b96-b7ae-779d989e1399\r\n"
          .. "Timeout: Second-1800\r\n"
          .. "Content-Length: 0\r\n"
          .. "Connection: close\r\n"
          .. "EXT:\r\n"
          .. "\r\n"
  client:send(r)
end

function ContentDirectory:Unsubscribe(client,sid)
  local r = "HTTP/1.1 200 OK\r\n"
          .. 'Content-Type: text/xml; charset="utf-8"\r\n'
          .. "Server: "..UPnP.Server.."\r\n"
          .. "Content-Length: 0\r\n"
          .. "Connection: close\r\n"
          .. "EXT:\r\n"
          .. "\r\n"
  client:send(r)
end


function ContentDirectory:Description()
  t = ""
  local f = io.open("ContentDirectory.xml","r")
  if not f then os.exit() end
  while true do
    local line = f:read()
    if not line then break end
    t = t .. line .. "\r\n"
  end
  f:close()
  return t
end

return ContentDirectory