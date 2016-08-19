# OctopusNet


###Prepare for Building
On Debian/Ubuntu (as root or using sudo):

```
 >apt-get install build-essential bison flex gettext libncurses5-dev texinfo autoconf automake libtool
 >apt-get install libpng12-dev libglib2.0-dev libgtk2.0-dev gperf libxt-dev libxp-dev
 >apt-get install rsync git subversion mercurial
```

* Ensure bash ist default shell (Debian/Ubuntu standard is dash):

```
  >dpkg-reconfigure dash
```
  and select no.

* Clone the octonet and ddvb repositories:

```
  >git clone -b master https://github.com/DigitalDevices/octonet.git octonet
  >git clone -b master https://github.com/DigitalDevices/dddvb.git dddvb
  >cd octonet  
  >./mk.patch
```
If needed replace branch (master) and repository path with your own.

###Building

Complete build (needed once)
```
  >./mk.all
```

Rebuild main firmware
```
  >./mk
```

###Installing

* Create a subdirectory octonet on a local webserver, enable directory listing.

```
  >cp buildroot/output-octonet/images/octonet.* <your webserver root>/octonet
```

On some servers a .htaccess file with:
```
Options +Indexes
```
in the octonet directory might be necessary.

* Configure your OctopusNet(s) to use your webserver as update server:
```
http://<OctopusNet IP>/updateserver.html
```
Initiate update from the OctopusNet 


Note: for security reasons only private ip addresses (10.0.0.0/8, 172.16.0.0/12, 192.168,0.0/16) are accepted


You can find details about the OctopusNet hardware, the flash memory map and the boot process
in dddvb/docs/octopusnet in the dddvb repo!

