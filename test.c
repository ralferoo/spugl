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

	float a=0.0,b=0.0,c=0.0;

	int f,v;
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
		glBegin(GL_QUADS);
		for (f=0; f<6; f++) {
			for (v=0; v<4; v++) {
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
		}
		glEnd();	
		glFlush();
		glspuFlip();
		glspuWait();
//		sleep(1);
		GLenum error = glGetError();
		if (error)
			printf("glGetError() returned %d\n", error);
	}
	usleep(250000);
	glspuDestroy();

	// quick hack so that SPU debugging messages have a chance to come out
	usleep(150000);
	exit(0);
}

/*

(-100, -100, -100) -> (398.45660187346448, 104.07111201173004, -0.0030490745677122363)
(100, -100, -100) -> (512.56654128163802, 238.04501471973302, -0.0022350053725772832)
(-100, 100, -100) -> (261.10380347126193, 176.85473244416573, -0.0021322073205141427)
(100, 100, -100) -> (375.74910901004222, 263.94520976132742, -0.0016993641366900908)
(-100, -100, 100) -> (323.15703793504952, 344.27300717003413, -0.0039754381268609033)
(100, -100, 100) -> (485.01823879536835, 428.50273001381419, -0.0026953999578720796)
(-100, 100, 100) -> (186.1160741399809, 344.93495365263112, -0.0025472918586032477)
(100, 100, 100) -> (335.36763753928147, 405.81080010218307, -0.0019530043285876076)

*/
