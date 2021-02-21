.phony all:
all: sfs

sfs:
	gcc -o diskinfo diskinfo.c 
	gcc -o disklist disklist.c 
	gcc -o diskget diskget.c 
	gcc -o diskput diskput.c 

.PHONY clean:
clean:
	-rm -rf diskinfo
	-rm -rf disklist
	-rm -rf diskget
	-rm -rf diskput
