TARGETDIR=$1

grep -q config $TARGETDIR/etc/fstab || echo -e "/dev/ubi0_0\t\t/config\tubifs\tdefaults\t0\t0" >> $TARGETDIR/etc/fstab
grep -q pts $TARGETDIR/etc/securetty || echo -e "pts/0\npts/1\npts/2\npts/3" >> $TARGETDIR/etc/securetty

mkdir -p $TARGETDIR/config

grep -q admin $TARGETDIR/etc/passwd || echo -e "admin:x:200:200:nobody:/home:/bin/false" >> $TARGETDIR/etc/passwd
grep -q admin $TARGETDIR/etc/shadow || echo -e "admin:WiBKbsJTSQ8dc:13514:0:99999:7:::" >> $TARGETDIR/etc/shadow

