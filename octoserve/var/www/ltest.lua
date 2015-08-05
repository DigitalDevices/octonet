#!/usr/bin/lua

-- HTTP header
print [[
Content-Type: text/plain
Set-Cookie: foo=bar
Set-Cookie: wonder=always

]]

-- body of page

-- find all environment variables using bash and a temporary file

fname = os.tmpname ()
os.execute ("/bin/sh -c set > " .. fname)
f = io.open (fname, "r")  -- open it
s = f:read ("*a")  -- read all of it
print (s)
f:close ()  -- close it
os.remove (fname)

print("")
params = io.stdin:read("*a")
print(params)