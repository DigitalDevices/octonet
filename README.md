# OctopusNet Buildroot


### Prepare
For building on Debian/Ubuntu run the following commands(remove sudo if already root user):

```
sudo apt-get -y install build-essential bison flex gettext libncurses5-dev texinfo autoconf automake libtool \
libpng12-dev libglib2.0-dev libgtk2.0-dev gperf libxt-dev rsync git subversion mercurial bc \
#libxp-dev missing in Debian9/Ubuntu16.04
```
* Check if 'bash' is the default shell (Debian/Ubuntu standard is dash)?
```
echo $0
#or
echo $SHELL
```
* If output is "-dash" instead of "-bash" run following command and select "no"!
```
sudo dpkg-reconfigure dash
```

Clone the "octonet" and "dddvb" repositories and reset both to a certain commit that actually builds:

```
git clone -b master https://github.com/DigitalDevices/dddvb.git dddvb
cd dddvb
git reset --hard 933779a
#git show #check if current commit is indeed 933779a press 'q' to exit
cd ..

git clone -b master https://github.com/DigitalDevices/octonet.git octonet
cd octonet
git reset --hard cd19e9e
#git show #check if current commit is indeed cd19e9e
./mk.patch
```
* Fix Buildroot broken url for i2c-tools!
```
sed -i "s|http://dl.lm-sensors.org/i2c-tools/releases|http://oe-lite.org/mirror/i2c-tools|" buildroot/package/i2c-tools/i2c-tools.mk
```

### Building
Run the following command for a complete build (needed once):

```
./mk.all
```
* For rebuilding main firmware just run:
```
./mk
```

When building succeeds (after ~20 minutes on a old i7quadcore) the size of the default custom firmware image will be ~5.7MB big and can be found in:

```
ls -la buildroot/output-octonet/images
ls -la buildroot/output-oc
```

### Installing
For upgrading the firmware within the OctopusNet webinterface, you must point it to a local webserver holding these (custombuild) firmware files in a subdirectory "octonet" that uses directory listing. The following commands will let you install a webserver on the very same machine your were building firmware and serve the custom firmware files.

```
sudo apt-get -y install lighttpd
echo 'dir-listing.activate = "enable"' | sudo tee -a /etc/lighttpd/lighttpd.conf
sudo systemctl restart lighttpd.service

sudo mkdir /var/www/html/octonet
sudo cp buildroot/output-octonet/images/octonet.* /var/www/html/octonet/

hostname -I #<WebServer IP>
```
* Check with your webbrowser if the following URL will show a directory listing:
```
http://<WebServer IP>/octonet
```

Configure your OctopusNet(s) to use your buildmachine/webserver as a updateserver on the following URL:

```
http://<OctopusNet IP>/updateserver.html
```
Change the current updateserver address to ipaddress of the <WebServer IP> and Initiate update from the OctopusNet.


Note: for security reasons only private ip addresses (10.0.0.0/8, 172.16.0.0/12, 192.168,0.0/16) are accepted


You can find details about OctopusNet its hardware, flash memory, boot process and stock firmware: 
* [dddvb/docs/octopusnet](https://github.com/DigitalDevices/dddvb/blob/master/docs/octopusnet)
* [firmware archive](http://download.digital-devices.de/download/linux/octonet/)
