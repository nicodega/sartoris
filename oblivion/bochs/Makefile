all : 1.44a

1.44a : FORCE	
	#create sys image (modifying blocklist)
	
	./inject_size.sh "../bin/oblivion.img" "1.44a" 

clean : FORCE
	rm -f 1.44a
	rm -f zeros
	rm -f core
	rm -f *~
	rm -f *.out

clean_all: clean FORCE

FORCE :
