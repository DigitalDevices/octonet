#!/usr/bin/lua

local SLAXML = require 'slaxdom'
local SCIFDataBase = io.open('SCIFDataBase.xml'):read("*a")
-- SLAXML:parse(SCIFDataBase,{stripWhitespace=true})
local dom = SLAXML:dom(SCIFDataBase,{ simple=false,stripWhitespace=true })

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

local child
local unit

local i
local j
local k
local l
local Frequency

local ManufacturerList = {}
local ManufacturerArray = {}
local ManufacturerCount = 0

for i,child in ipairs(dom.kids) do
  http_print (i,child.name)
  if child.name == "SCIFDataBase" then
    for j,unit in ipairs(child.kids) do
      if unit.name == "OutdoorUnit" then
        local Name = unit.attr["Name"];
        local Manufacturer = unit.attr["Manufacturer"];
        local Type = unit.attr["Type"];
	local Protocol = unit.attr["Protocol"];
        if not Protocol then Protocol = "EN50494" end
        if not Manufacturer then Manufacturer = "" end
        if not Type then Type = "LNB" end
        -- http_print ( "    ",Name,Manufacturer,Type)
        local CurManu = ManufacturerList[Manufacturer]
        if not CurManu then
          CurManu = { UnitList = {}, UnitCount = 0, Name = Manufacturer }
          ManufacturerCount = ManufacturerCount + 1
          ManufacturerList[Manufacturer] = CurManu
          ManufacturerArray[ManufacturerCount] = CurManu
        end
        CurManu.UnitCount = CurManu.UnitCount + 1
        local CurUnit = { Name = Name, Type = Type, Protocol = Protocol, Frequencies = {} } 
        CurManu.UnitList[CurManu.UnitCount] = CurUnit
        local fcount = 0
        for k,Frequency in ipairs(unit.kids) do
          if Frequency.name == "UBSlot" then
             fcount = fcount + 1
             CurUnit.Frequencies[fcount] = Frequency.attr["Frequency"]
             
             -- http_print(" -------------------------", Frequency.type, Frequency.name, Frequency.attr["Frequency"])
          end
        end
      end
    end
  end
end

-- http_print(ManufacturerCount)

http_print("HTTP/1.1 200 ")
http_print("Pragma: no-cache")
http_print("Cache-Control: no-cache")
http_print("Content-Type: application/x-javascript")
http_print()

http_print("ManufacturerList = new Array();")

for i,CurManu in ipairs(ManufacturerArray) do
  http_print()
  http_print(string.format("ManufacturerList[%d] = new Object();",i-1))
  http_print(string.format("ManufacturerList[%d].Name = \"%s\";",i-1,CurManu.Name))
  http_print(string.format("ManufacturerList[%d].UnitList = new Array();",i-1))

  for j,CurUnit in ipairs(CurManu.UnitList) do
    http_print()
    http_print(string.format("ManufacturerList[%d].UnitList[%d] = new Object();",i-1,j-1))
    http_print(string.format("ManufacturerList[%d].UnitList[%d].Name = \"%s\";",i-1,j-1,CurUnit.Name))
    http_print(string.format("ManufacturerList[%d].UnitList[%d].Protocol = \"%s\";",i-1,j-1,CurUnit.Protocol))
    http_print(string.format("ManufacturerList[%d].UnitList[%d].Frequencies = new Array();",i-1,j-1))
    for k,Frequency in ipairs(CurUnit.Frequencies) do
      http_print(string.format("ManufacturerList[%d].UnitList[%d].Frequencies[%d] = %d;",i-1,j-1,k-1,Frequency))
    end
  end
  
  i = i + 1
end
http_print()
