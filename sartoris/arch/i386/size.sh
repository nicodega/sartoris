
echo "Checking Kernel Size..."
a=$(grep -n "PROVIDE (end, .)" kernel.map | sed -n 's/.*\(0x[0,1,2,3,4,5,6,7,8,9,a,b,c,d,e,f]*\).*/\1/p')
b=0xA0000
if [ $((a)) -gt $((b)) ]; then
	echo "KERNEL SIZE IS TOO BIG! (Greater than 0xA0000)"
	exit -1
fi
echo "OK"
