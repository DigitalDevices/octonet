#!/bin/bash
# Alte Images und Kernel-Stempel bereinigen
rm -f output-octonet/images/octonet*
rm -f output-octonet/build/linux-custom/.stamp_*

DDDVB=../dddvb
LINUX=linux.octonet

if [ ! -e linux.octonet ]; then
  if [ ! -e linux-3.17.8.tar.xz ]; then
     wget https://www.kernel.org/pub/linux/kernel/v3.x/linux-3.17.8.tar.xz
  fi
  tar xf linux-3.17.8.tar.xz
  mv linux-3.17.8 linux.octonet
  patch -d linux.octonet -p1 < linux.patch
fi

if [ ! -e buildroot ]; then
  if [ ! -e buildroot-2015.02-rc2.tar.bz2 ]; then
     wget http://buildroot.org/downloads/buildroot-2015.02-rc2.tar.bz2
  fi
  tar xf buildroot-2015.02-rc2.tar.bz2
  mv buildroot-2015.02-rc2 buildroot
#  cp dvb-apps-0002-Fix-broken-bitops-on-arm.patch buildroot/package/dvb-apps/
#  cp busybox-0001-Fix-zcip-arp-compare.patch buildroot/package/busybox/
#  cp lzop-0001-ACC.patch buildroot/package/lzop/
  if [ -e dl ]; then
     ln -sf ../dl buildroot
  fi
fi

if [ ! -e ../dddvb ]; then
   git clone -b "0.9.38" "https://github.com/DigitalDevices/dddvb.git" ../dddvb
fi

if [ ! -e u-boot ]; then
  if [ ! -e u-boot-2014.07.tar.bz2 ]; then
     wget ftp://ftp.denx.de/pub/u-boot/u-boot-2014.07.tar.bz2
  fi
  tar xf u-boot-2014.07.tar.bz2
  mv u-boot-2014.07 u-boot
  patch -d u-boot -p1 < u-boot.patch
fi



# --- Phase 1: Sourcen kopieren (v0.9.38 Struktur) ---
cp $DDDVB/ddbridge/*.[ch] $LINUX/drivers/media/pci/ddbridge/
cp $DDDVB/ddbridge/Kconfig $LINUX/drivers/media/pci/ddbridge/
cp $DDDVB/ddbridge/Makefile.kernel $LINUX/drivers/media/pci/ddbridge/Makefile

cp $DDDVB/dvb-core/*.c $LINUX/drivers/media/dvb-core/
cp $DDDVB/dvb-core/Kconfig $LINUX/drivers/media/dvb-core/

# Header-Fix für Kernel 3.17.x Kompatibilität (Problem 8)
# Wir kopieren die modernen Header direkt in den Hauptordner
mkdir -p $LINUX/drivers/media/dvb-core/media/
cp $DDDVB/include/linux/media/*.h $LINUX/drivers/media/dvb-core/media/
cp $DDDVB/include/linux/media/*.h $LINUX/drivers/media/dvb-core/
cp $DDDVB/include/linux/dvb/*.h $LINUX/include/uapi/linux/dvb/
cp $DDDVB/include/*.h $LINUX/drivers/media/dvb-core/

cp $DDDVB/dvb-core/Makefile.kernel $LINUX/drivers/media/dvb-core/Makefile

# Frontends kopieren
cp $DDDVB/frontends/Kconfig $LINUX/drivers/media/dvb-frontends/
cp $DDDVB/frontends/Makefile.kernel $LINUX/drivers/media/dvb-frontends/Makefile
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

# Bereinigung alter App-Builds
rm -rf output-octonet/build/octonet*
rm -rf output-octonet/build/octoserve*
rm -rf output-octonet/build/octoscan*

# --- Phase 2: Buildroot Prozess (Reihenfolge nach mk.all) ---

cd buildroot

# 1. Toolchain Build (Sicherstellen, dass output-octonet-tc existiert)
make BR2_EXTERNAL=../buildroot.octonet O=output-octonet-tc digitaldevices_octonet_tc_defconfig
make BR2_EXTERNAL=../buildroot.octonet O=output-octonet-tc || exit 1

# 2. Init Build
make BR2_EXTERNAL=../buildroot.octonet O=output-octonet-init digitaldevices_octonet_init_defconfig
make BR2_EXTERNAL=../buildroot.octonet O=output-octonet-init || exit 1

# 3. Haupt-Target Build
make BR2_EXTERNAL=../buildroot.octonet O=output-octonet digitaldevices_octonet_defconfig
make BR2_EXTERNAL=../buildroot.octonet O=output-octonet || exit 1

# --- Phase 3: Packaging ---
TS=`date +%y%m%d%H%M`
echo "Erstelle Octonet Image: $TS"

if [ -f output-octonet/images/rootfs.tar.bz2 ]; then
    cp output-octonet/images/rootfs.tar.bz2 output-octonet/images/octonet.$TS.img
    sha256sum output-octonet/images/octonet.$TS.img > output-octonet/images/octonet.$TS.sha
    echo "Fertig: octonet.$TS.img wurde erstellt."
else
    echo "Fehler: rootfs.tar.bz2 nicht gefunden. Build fehlgeschlagen."
    exit 1
fi

