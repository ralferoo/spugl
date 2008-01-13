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

int main(int argc, char* argv[]) {
	glspuSetup();
	int y,z;
	for (y=1; y<100; y++)
	for (z=250; z>-300; z-=5) {
//		printf("z=%d\n", z);
		glspuClear();
		glBegin(GL_QUADS);
//		glColor3ub(127, 15, 192);
		glVertex3f(-100.0f, 100.0f, -50.0f+z);
		glColor3f(1.0, 0.0, 0.25);
		glVertex3f(-100.0f,-100.0f, -50.0f+z);
		glVertex3f( 100.0f,-100.0f,  50.0f+z);
		glColor3f(0.0, 1.0, 0.25);
		glVertex3f( 100.0f, 100.0f,  50.0f+z);
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
