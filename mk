cd buildroot
rm output-octonet/images/octonet*
rm output-octonet/build/linux-custom/.stamp_*

DDDVB=../dddvb
LINUX=../linux.octonet

#if [ -e /home/rjkm/projects ]; then
#   rm -rf ../dddvb
#   cp -rpdu /home/rjkm/projects/dddvb ../
#   rm -rf ../octoserve
#   cp -rpdu /home/rjkm/projects/octoserve ../
#fi

cp $DDDVB/ddbridge/*.[ch] $LINUX/drivers/media/pci/ddbridge/
cp $DDDVB/dvb-core/*.[ch] $LINUX/drivers/media/dvb-core/
cp $DDDVB/include/linux/dvb/*.h $LINUX/include/uapi/linux/dvb/

cp $DDDVB/frontends/drxk*.[ch] $LINUX/drivers/media/dvb-frontends/
cp $DDDVB/frontends/lnb*.[ch] $LINUX/drivers/media/dvb-frontends/
cp $DDDVB/frontends/stv0367dd*.[ch] $LINUX/drivers/media/dvb-frontends/
cp $DDDVB/frontends/stv090x*.[ch] $LINUX/drivers/media/dvb-frontends/
cp $DDDVB/frontends/stv6110*.[ch] $LINUX/drivers/media/dvb-frontends/
cp $DDDVB/frontends/tda18*dd*.[ch] $LINUX/drivers/media/dvb-frontends/
cp $DDDVB/frontends/cxd2099.[ch] $LINUX/drivers/media/dvb-frontends/
cp $DDDVB/frontends/cxd2843.[ch] $LINUX/drivers/media/dvb-frontends/
cp $DDDVB/frontends/stv6111.[ch] $LINUX/drivers/media/dvb-frontends/
cp $DDDVB/frontends/stv0910*.[ch] $LINUX/drivers/media/dvb-frontends/
cp $DDDVB/frontends/mxl5xx.[ch] $LINUX/drivers/media/dvb-frontends/
cp $DDDVB/frontends/mxl5xx_regs.h $LINUX/drivers/media/dvb-frontends/
cp $DDDVB/frontends/mxl5xx_defs.h $LINUX/drivers/media/dvb-frontends/

#cp $DDDVB/apps/ddtest.c package/octonet/octonet/
#cp $DDDVB/apps/ddflash.c package/octonet/octonet/
#cp $DDDVB/apps/octonet/octonet.c package/octonet/octonet/
#cp $DDDVB/apps/octonet/octokey.c package/octonet/octonet/
#cp $DDDVB/include/linux/dvb/ns.h package/octonet/octonet/
rm -rf output-octonet/build/octonet*
rm -rf output-octonet/build/octoserve*

#chmod 577 board/digitaldevices/octonet/overlay/var/dms/dms.lua
#chmod 577 board/digitaldevices/octonet/overlay/var/mcsetup/mcsetup.lua

make BR2_EXTERNAL=../buildroot.octonet O=output-octonet digitaldevices_octonet_defconfig
make O=output-octonet

TS=`date +%y%m%d%H%M`
echo $TS
cp output-octonet/images/rootfs.tar.bz2 output-octonet/images/octonet.$TS.img
sha256sum output-octonet/images/octonet.$TS.img > output-octonet/images/octonet.$TS.sha
