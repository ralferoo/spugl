/****************************************************************************
 *
 * SPU GL - 3d rasterisation library for the PS3
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> 
 *
 * This library may not be used or distributed without a licence, please
 * contact me for information if you wish to use it.
 *
 ****************************************************************************/

#define SCALE 0.9
//#define SCALE 1.0
//#define SCALE 1.7
//#define SCALE 2.0
//#define SCALE 2.5
//#define SCALE 3.0

#define SYNC_WITH_FRAME
// #define DOUBLE_SYNC
// #define BLACK_MIDDLES

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <GL/gl.h>
#include <GL/glspu.h>

void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);

#include "joystick.h"

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

static inline double getTimeSince(struct timespec startPoint) {
	struct timespec endPoint;
	clock_gettime(CLOCK_MONOTONIC,&endPoint);
	double secs = endPoint.tv_sec-startPoint.tv_sec +
		((endPoint.tv_nsec-startPoint.tv_nsec)/1000000000.0);
	return secs;
}

unsigned long flag=0;

int main(int argc, char* argv[]) {
	float scale = SCALE;
	int magicScale = 1;

	if (argc>1 && argv[1][0]=='-') {
		scale=atof(&argv[1][1]);
		magicScale = 0;
		argc--;
		argv++;
	}

	glspuSetup(argc>1 ? argv[1] : NULL);

	// NOTE THIS ISN'T A REAL OPENGL TRANSFORM!!!

	GLfloat projectionMatrix[] = {
		1.0,				0.0,				0.0,	0.0,
		0.0,				1.0,				0.0,	0.0,
		640.0f/420.0f,			360.0/420.0f,			1.0,	1.0/420.f,
		(640.0f*-282.0f)/420.0f,	(360.f*-282.0f)/420.0f,		0.0,	-282.0f/420.f,
	};
	glLoadMatrixf(&projectionMatrix);

	float onesec = 42670000.0;

	float a=0.0,b=0.0,c=0.0;

	int f,v;
	int cnt = 0;

	stick_init();

	// initial debounce
	while (stick_button(3));

	int last_button = 0;

	while (!stick_button(3)) {
		struct timespec startPoint;
		clock_gettime(CLOCK_MONOTONIC,&startPoint);

		unsigned long blks_start = glspuBlocksProduced();
		unsigned long caches_start = glspuCacheMisses();

		int but = stick_buttons() & ((1<<9) | (1<<8) | (1<<10) | (1<<11) | (1<<14) | (1<<13));
		if (but) {
//		if (a!=last_button) {
//			last_button = a;
//			magicScale = !magicScale;
//		}
//
//		if (magicScale) {
//			scale = (sin(flag/50.0)+1.0)*0.95 + SCALE;
			scale = 1.0/(1.0 + stick_axis(3) * 0.00002);

//			a += stick_axis(2) * 0.000002;
			b += stick_axis(0) * 0.000002;
			c += stick_axis(1) * 0.000002;
		} else {
			a += 0.011;
			b += 0.037;
			c += 0.017;
		}

//	printf("scale %8.6f a %10.6f b %10.6f c %10.6f\n", scale, a, b, c);

//if (cnt<16990) goto skip;

		float sa = sin(a);
		float ca = cos(a);

		float sb = sin(b);
		float cb = cos(b);

		float sc = sin(c);
		float cc = cos(c);

		float x,y,z,t;

		glspuClear();
		unsigned long _start = glspuCounter();
		unsigned long _startBlocked = glspuBlockedCounter();
	if(1) {
		glBegin(GL_TRIANGLES);
		for (f=0; f<6; f++) {
			glBindTexture(0,f);
			float sx[5], sy[5], sz[5];
			float tx=0, ty=0, tz=0;
			float tr=0, tg=0, tb=0;
			for (v=0; v<4; v++) {
				x = vertices[faces[f][v]][0] / scale;
				y = vertices[faces[f][v]][1] / scale;
				z = vertices[faces[f][v]][2] / scale;

				t = ca*x+sa*y;
				y = ca*y-sa*x;
				x = t;

				t = cb*y+sb*z;
				z = cb*z-sb*y;
				y = t;
				
				t = cc*x+sc*z;
				z = cc*z-sc*x;
				x = t;

				z-=25;

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
				glTexCoord2f( (v^(v>>1))&1?(256.0f/256.0f):0.0f,
						(v&2)?0.0f:(256.0f/256.0f));
				glColor3ub(vertices[faces[f][v]][4],
					   vertices[faces[f][v]][5],
					   vertices[faces[f][v]][3]);
				glVertex3f(sx[v],sy[v],sz[v]);

				glTexCoord2f( ((v+1)^((v+1)>>1))&1?(256.0f/256.0f):0.0f,
						((v+1)&2)?0.0f:(256.0f/256.0f));
				glColor3ub(vertices[faces[f][(v+1)%4]][4],
					   vertices[faces[f][(v+1)%4]][5],
					   vertices[faces[f][(v+1)%4]][3]);
				glVertex3f(sx[(v+1)%4],sy[(v+1)%4],sz[(v+1)%4]);

				glTexCoord2f(0.5f, 0.5f);
#ifdef BLACK_MIDDLES
				glColor3ub(0,0,0);
#else
				glColor3ub(tr/4, tg/4, tb/4);
#endif
				glVertex3f(tx/4, ty/4, tz/4);
//goto cheat;
			}
		}
cheat:
		glEnd();	
	}
		glFlush();

		double uptoFlip = getTimeSince(startPoint);

		glspuFlip();
#ifdef SYNC_WITH_FRAME
		glspuWait();
#ifdef DOUBLE_SYNC
		double x1 = getTimeSince(startPoint);
		if (x1<(1.0/45.0))
			glspuWait();
#endif
#endif
		unsigned long _end = glspuCounter();
		unsigned long _endBlocked = glspuBlockedCounter();
		GLenum error = glGetError();
		if (error)
			printf("glGetError() returned %d\n", error);
skip:
		cnt++;

		double uptoLoop = getTimeSince(startPoint);

		unsigned long blks_end = glspuBlocksProduced();
		unsigned long caches_end = glspuCacheMisses();

		// test the set flag functionality - this variable is updated by the SPU
		glspuSetFlag(&flag, flag+1);

		// bah humbug, stdio buffering, bah!
		char buffer[256];
		sprintf(buffer,"\r[%5d] %4.1f FPS (actual %5.1f) %5d blocks %6d misses %8.1f block/s ",
			flag, //cnt,
			(float) 1.0/uptoLoop,
			(float) 1.0/uptoFlip,
			blks_end-blks_start, caches_end-caches_start,
			((float)blks_end-blks_start)/uptoLoop,
			(float) (100.0*(_end-_start)/onesec/uptoLoop),
			(float) (100.0*(_endBlocked-_startBlocked)/onesec/uptoLoop));
		write(2,buffer,strlen(buffer));
		if (uptoLoop > (1.0/58.4f))
			write(2, "*\n", 2);
	}
	stick_close();
	exit(0);

//	usleep(250000);
	glspuDestroy();

	// quick hack so that SPU debugging messages have a chance to come out
	usleep(150000);
	exit(0);
}
