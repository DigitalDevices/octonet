#!/usr/bin/lua

local db = require("DataBase")

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


http_print(proto.." 200")
--http_print("Pragma: no-cache")
--http_print("Cache-Control: no-cache")
http_print("Content-Type: application/x-javascript")
http_print("")

local SourceList = {}

for _,f in ipairs(db.SourceList) do
  f.ChannelList = {}
  SourceList[f.refid] = f
  http_print("// " .. f.refid .. " " .. f.title )
end

for _,c in ipairs(db.ChannelList) do
  local f = SourceList[c.refid]
  if f then
    table.insert(f.ChannelList,c)
    -- http_print("// " .. c.refid .. " " .. c.title .. " " .. c.request .. " " .. c.tracks )
  end
end

local isat = 0
local icable = 0
local iter = 0
local ichannel = 0

http_print("var SourceListSat = new Array();")
http_print("var SourceListCable = new Array();")
http_print("var SourceListTer = new Array();")

for _,f in pairs(SourceList) do
  if f.system == "dvbs" or f.system == "dvbs2" then
    http_print("")
    http_print(string.format("SourceListSat[%d] = new Object();",isat))
    http_print(string.format("SourceListSat[%d].name = '%s';",isat,f.title))
    http_print(string.format("SourceListSat[%d].ChannelList = new Array();",isat))
    
    ichannel = 0
    for _,c in ipairs(f.ChannelList) do
      http_print("")
      http_print(string.format("SourceListSat[%d].ChannelList[%d] = new Object();",isat,ichannel))
      http_print(string.format("SourceListSat[%d].ChannelList[%d].name = '%s';",isat,ichannel,string.gsub(c.title,"'","\\'")))
      http_print(string.format("SourceListSat[%d].ChannelList[%d].request = '?src=%s&%s';",isat,ichannel,f.src,c.request))
      http_print(string.format("SourceListSat[%d].ChannelList[%d].tracks = new Array(%s);",isat,ichannel,c.tracks))
      ichannel = ichannel + 1
    end
    isat = isat + 1
  end
  if f.system == "dvbc" or f.system == "dvbc2" then
    http_print("")
    http_print(string.format("SourceListCable[%d] = new Object();",icable))
    http_print(string.format("SourceListCable[%d].name = '%s';",icable,f.title))
    http_print(string.format("SourceListCable[%d].ChannelList = new Array();",icable))
    
    ichannel = 0
    for _,c in ipairs(f.ChannelList) do
      http_print("")
      http_print(string.format("SourceListCable[%d].ChannelList[%d] = new Object();",icable,ichannel))
      http_print(string.format("SourceListCable[%d].ChannelList[%d].name = '%s';",icable,ichannel,string.gsub(c.title,"'","\\'")))
      http_print(string.format("SourceListCable[%d].ChannelList[%d].request = '?%s';",icable,ichannel,c.request))
      http_print(string.format("SourceListCable[%d].ChannelList[%d].tracks = new Array(%s);",icable,ichannel,c.tracks))
      ichannel = ichannel + 1
    end
    icable = icable + 1
  end
end

-- var ChannelListSat = new Array();

-- ChannelListSat[0] = new Object();
-- ChannelListSat[0].name = "Astra 19E: Das Erste HD";
-- ChannelListSat[0].request = "?src=1&freq=11494&pol=h&msys=dvbs&sr=22000&pids=0,5100,5101,5102,5103,5106,5104,5105";
-- ChannelListSat[0].tracks = new Array( 5102, 5103, 5106 );

-- ChannelListSat[1] = new Object();
-- ChannelListSat[1].name = "Astra 19E: ZDF HD";
-- ChannelListSat[1].request = "?src=1&freq=11362&pol=h&msys=dvbs&sr=22000&pids=0,6100,6110,6120,6121,6122,6123,6131,6130";
-- ChannelListSat[1].tracks = new Array( 6120, 6121, 6123, 6122 );

-- ChannelListSat[2] = new Object();
-- ChannelListSat[2].name = "Eutelsat 9E: TF1 HD Suisse";
-- ChannelListSat[2].request = "?src=1&freq=12034&pol=v&msys=dvbs&sr=27500&pids=0,800,810,820,821,822,850,814,815";
-- ChannelListSat[2].tracks = new Array( 820, 821, 822 );

-- ChannelListSat[3] = new Object();
-- ChannelListSat[3].name = "Eutelsat 9E: FTV HD Europe";
-- ChannelListSat[3].request = "?src=1&freq=11881&pol=v&msys=dvbs&sr=27500&pids=0,1001,2001,3001,4001";
-- ChannelListSat[3].tracks = new Array( 3001, 4001 );


-- var ChannelListCable = new Array();

-- ChannelListCable[0] = new Object();
-- ChannelListCable[0].name = "KabelBW: Das Erste HD";
-- ChannelListCable[0].request = "?freq=362&msys=dvbc&sr=6900&mtype=256qam&pids=0,6000,6010,6020,6021,6030,6022,6031";
-- ChannelListCable[0].tracks = new Array( 6020, 6021, 6022 );

-- ChannelListCable[1] = new Object();
-- ChannelListCable[1].name = "KabelBW: ZDF HD";
-- ChannelListCable[1].request = "?freq=370&msys=dvbc&sr=6900&mtype=256qam&pids=0,6100,6110,6120,6121,6123,6130,6122,6131";
-- ChannelListCable[1].tracks = new Array( 6120, 6121, 6123, 6122 );

-- ChannelListCable[2] = new Object();
-- ChannelListCable[2].name = "UnityMedia: Das Erste HD";
-- ChannelListCable[2].request = "?freq=418&msys=dvbc&sr=6900&mtype=256qam&pids=0,6000,6010,6020,6021,6030,6022,6031";
-- ChannelListCable[2].tracks = new Array( 6020, 6021, 6022 );

-- ChannelListCable[3] = new Object();
-- ChannelListCable[3].name = "UnityMedia: ZDF HD";
-- ChannelListCable[3].request = "?freq=394&msys=dvbc&sr=6900&mtype=256qam&pids=0,0,6100,6110,6120,6121,6123,6130,6122,6131";
-- ChannelListCable[3].tracks = new Array( 6120, 6121, 6123, 6122 );

-- ChannelListCable[4] = new Object();
-- ChannelListCable[4].name = "KDG: Das Erste HD";
-- ChannelListCable[4].request = "?freq=330&msys=dvbc&sr=6900&mtype=256qam&pids=0,5100,5101,5102,5103,5105,5104";
-- ChannelListCable[4].tracks = new Array( 5102, 5103 );

-- ChannelListCable[5] = new Object();
-- ChannelListCable[5].name = "KDG: ZDF HD";
-- ChannelListCable[5].request = "?freq=450&msys=dvbc&sr=6900&mtype=256qam&pids=0,6100,6110,6120,6121,6130,6123,6131";
-- ChannelListCable[5].tracks = new Array( 6120, 6121, 6123 );
