/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include <GL/gl.h>
#include <GL/glspu.h>

float vertices[][6] = {
	{ -100, -100, -100,	0,127,0},
	{  100, -100, -100,	187,107,33},
	{ -100,  100, -100,	127,0,127},
	{  100,  100, -100,	27,64,96},
	{ -100, -100,  100,	30,90,199},
	{  100, -100,  100,	0,127,155},
	{ -100,  100,  100,	64,255,127},
	{  100,  100,  100,	127,42,0},
};

int faces[6][7] = {
	{ 4,5,7,6, 0,128,128},
	{ 1,0,2,3, 0,128,64},

	{ 0,4,6,2, 128,64,0},
	{ 5,1,3,7, 128,128,0},

	{ 5,4,0,1, 64,0,128},
	{ 3,2,6,7, 128,0,128},
};

int main(int argc, char* argv[]) {
	glspuSetup();

/*
	printf("Calibrating SPU speed...\n");
	unsigned long start = glspuCounter();
	sleep(1);
//	unsigned long one = glspuCounter();
//	sleep(1);
	unsigned long two = glspuCounter();
	float onesec = (two-start); // /2;
//	printf("Time taken: %d, %d: avg %f\n", one-start, two-one, onesec);
	printf("Time taken: %f\n", onesec);
*/
	float onesec = 42670000;

	float a=0.0,b=0.0,c=0.0;

	int f,v,vv;
	for (;;) {
		a += 0.011;
		b += 0.037;
		c += 0.017;

		float sa = sin(a);
		float ca = cos(a);

		float sb = sin(b);
		float cb = cos(b);

		float sc = sin(c);
		float cc = cos(c);

		float x,y,z,t;

		glspuClear();
		unsigned long _start = glspuCounter();
		for (f=0; f<6; f++) {
		glBegin(GL_QUADS);
		//glBegin(GL_QUAD_STRIP);
			for (vv=0; vv<4; vv++) {
				v = vv;
		//		v = vv^(vv>>1); // swap 2 and 3
			    if (0)
				glColor3ub(faces[f][4],
					   faces[f][5],
					   faces[f][6]);
			    else
				glColor3ub(vertices[faces[f][v]][4],
					   vertices[faces[f][v]][5],
					   vertices[faces[f][v]][3]);

				x = vertices[faces[f][v]][0];
				y = vertices[faces[f][v]][1];
				z = vertices[faces[f][v]][2];

				t = ca*x+sa*y;
				y = ca*y-sa*x;
				x = t;

				t = cb*y+sb*z;
				z = cb*z-sb*y;
				y = t;
				
				t = cc*x+sc*z;
				z = cc*z-sc*x;
				x = t;

				glVertex3f(x,y,z);
			}
		glEnd();	
		}
		glFlush();
		glspuFlip();
		unsigned long _end = glspuCounter();
		glspuWait();
		GLenum error = glGetError();
		if (error)
			printf("glGetError() returned %d\n", error);

		printf("Currently idling %2.2f%% SPU capacity    \r",
			(_end-_start), ((_end-_start)/onesec)*100.0);
	}
	usleep(250000);
	glspuDestroy();

	// quick hack so that SPU debugging messages have a chance to come out
	usleep(150000);
	exit(0);
}
