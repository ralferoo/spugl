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

	int f,v;
	int cnt = 0;
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
		glBegin(GL_TRIANGLES);
		for (f=0; f<6; f++) {
			float sx[5], sy[5], sz[5];
			float tx=0, ty=0, tz=0;
			float tr=0, tg=0, tb=0;
			for (v=0; v<4; v++) {
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

				sx[v] = x;
				sy[v] = y;
				sz[v] = z;

				tx += x;
				ty += y;
				tz += z;

				tr += vertices[faces[f][v]][4];
				tg += vertices[faces[f][v]][5];
				tb += vertices[faces[f][v]][3];
			}

			for (v=0; v<4; v++) {
				glColor3ub(vertices[faces[f][v]][4],
					   vertices[faces[f][v]][5],
					   vertices[faces[f][v]][3]);
				glVertex3f(sx[v],sy[v],sz[v]);

				glColor3ub(vertices[faces[f][(v+1)%4]][4],
					   vertices[faces[f][(v+1)%4]][5],
					   vertices[faces[f][(v+1)%4]][3]);
				glVertex3f(sx[(v+1)%4],sy[(v+1)%4],sz[(v+1)%4]);

				glColor3ub(tr/4, tg/4, tb/4);
				glVertex3f(tx/4, ty/4, tz/4);
//goto cheat;
			}
		}
cheat:
		glEnd();	
		glFlush();
		glspuFlip();
		unsigned long _end = glspuCounter();
//		glspuWait();
		GLenum error = glGetError();
		if (error)
			printf("glGetError() returned %d\n", error);

		cnt++;
		printf("[%d] Currently idling %2.2f%% SPU capacity    \r",
			cnt, (_end-_start));
//((_end-_start)/onesec)*100.0, cnt);
	}
	usleep(250000);
	glspuDestroy();

	// quick hack so that SPU debugging messages have a chance to come out
	usleep(150000);
	exit(0);
}
