#!/usr/bin/lua


print("HTTP/1.0 200 ")
print("Pragma: no-cache")
print("Content-Type: application/x-javascript")
print("")

local query = os.getenv("QUERY_STRING")

if( query == "sjiwjsiowjs" ) then
  print("Rebooting = true")
  local uImage = io.open("/boot/uImage")
  if( uImage ) then
    uImage:close()
  else
    -- Cleanup server home
    os.execute("rm -rf /var/www/*")
    os.execute("rm -rf /var/dms/*")
    os.execute("rm -rf /var/channels/*")
  end
    
  os.execute("/etc/init.d/S99octo stop")
  os.execute("sync")
  os.execute("reboot")
elseif( query == "restart_octo" ) then
  print("Rebooting = true")
  os.execute("/etc/init.d/S99octo restartoctonet")
elseif( query == "restart_dms" ) then
  print("Rebooting = true")
  os.execute("/etc/init.d/S92dms restart")
else
  print("Rebooting = false")
end
