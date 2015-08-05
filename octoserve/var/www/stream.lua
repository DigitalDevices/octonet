#!/usr/bin/lua


print("HTTP/1.0 200\r")
print("Content-type: application/sdp\r")
print("\r")

local query = os.getenv("QUERY_STRING")

print("v=0\r")
print("o=- 2890844526 2890842807 IN IP4 10.0.4.31\r")
print("s=RTSP Session\r")
print("t=0 0\r")
print("m=video 0 RTP/AVP 33\r")
print("a=control:stream=1\r")
print("a=fmtp:33\r")
