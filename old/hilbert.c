#include <stdio.h>

#define DIR_DRU 0
#define DIR_RDL 1
#define DIR_LUR 2
#define DIR_ULD 3

int x=0, y=0, n=0;

void emit(int d, int o)
{
	if (d==0)
		printf("[%03x] %2d,%2d {%d}\t", n++, x, y, o);
	else {
		d--;
		switch(o) {
			case DIR_DRU:
				emit(d, DIR_RDL);
				y++;
				emit(d, o);
				x++;
				emit(d, o);
				y--;
				emit(d, DIR_LUR);
				break;
			case DIR_RDL:
				emit(d, DIR_DRU);
				x++;
				emit(d, o);
				y++;
				emit(d, o);
				x--;
				emit(d, DIR_ULD);
				break;
			case DIR_LUR:
				emit(d, DIR_ULD);
				x--;
				emit(d, o);
				y--;
				emit(d, o);
				x++;
				emit(d, DIR_DRU);
				break;
			case DIR_ULD:
				emit(d, DIR_LUR);
				y--;
				emit(d, o);
				x--;
				emit(d, o);
				y++;
				emit(d, DIR_RDL);
				break;
		}
		printf("\n");
	}
}

int main(int argc, char* argv[])
{
	int d = argc>1?atoi(argv[1]):1;
	printf("d is %d\n", d);
	printf("Table 0:\n");
	x=0, y=0,n=0; emit(d, 0);
	printf("Table 1:\n");
	x=0, y=0,n=0; emit(d, 1);
	printf("Table 2:\n");
	x=0, y=0,n=0; emit(d, 2);
	printf("Table 3:\n");
	x=0, y=0,n=0; emit(d, 3);
}

