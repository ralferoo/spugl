all:	idea4.png

#idea4_sub.png: idea4.svg Makefile
#	inkscape -a 0:200:520:328 -d 11.125 -e idea4_sub.png idea4.svg

idea4.png: idea4_sub.png

%.png: %.svg Makefile
	inkscape -C -d 120 -e $*.png $*.svg



