#!/bin/bash
# Make an .opk file for rg350

cp ./bin/rgquake ./opk_data/
cd opk_data

FLIST="rgquake"
FLIST="${FLIST} default.gcw0.desktop"
FLIST="${FLIST} hipnotic.gcw0.desktop"
FLIST="${FLIST} rogue.gcw0.desktop"
FLIST="${FLIST} rgquake.sh"
FLIST="${FLIST} hipnotic.sh"
FLIST="${FLIST} rogue.sh"
FLIST="${FLIST} logo.png"
FLIST="${FLIST} mp1.png"
FLIST="${FLIST} mp2.png"
FLIST="${FLIST} manual-gcw0.txt"
FLIST="${FLIST} id1"
FLIST="${FLIST} hipnotic"

OPK_NAME=rgquake.opk
rm -f ${OPK_NAME}
mksquashfs ${FLIST} ${OPK_NAME} -all-root -noappend -no-exports -no-xattrs -force-uid 1000 -force-gid 100