
./imgsize.pl $1 -f
	
#cut to the blocklist
dd if=$1 of=$2 bs=1 count=1420

#insert the length on the blocklist and complete 3068 bytes
cat length-s.hdb >> $2
dd if=$1 of=$2.sysimg.1 bs=1 skip=1422 count=1646	
cat $2.sysimg.1 >> $2

#write the size
cat length.hdb >> $2

#get the rest of the img from 3072
dd if=$1 of=$2.sysimg.1 bs=512 skip=6
cat $2.sysimg.1 >> $2
rm $2.sysimg.1

../tools/fill $2 -m 1474048	
rm -f zeros
rm length.hdb
rm length-s.hdb