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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "client/daemon.h"
#include "client/client.h"
#include "client/fifodefs.h"

#include "GL/gl.h"
//#include "GL/spugl.h"

extern void _binary_pixelshaders_flat_shader_start;
extern void _binary_pixelshaders_flat_shader_size;

extern void _binary_pixelshaders_texture_shader_start;
extern void _binary_pixelshaders_texture_shader_size;

extern void _binary_pixelshaders_mask_shader_start;
extern void _binary_pixelshaders_mask_shader_size;

int main(int argc, char* argv[]) {

	int server = spuglConnect();
	if (server<0) {
		printf("Cannot connect to spugl server\n");
		exit(1);
	}

	int width, height;
	spuglScreenSize(server, &width, &height);
	printf("Connected to spugl, screen size is %dx%d\n", width, height);

	CommandQueue* queue = spuglAllocateCommandQueue(server, 24*1024);
	if (queue==NULL) { printf("Out of memory\n"); exit(1); }

	void* buffer = spuglAllocateBuffer(server, 24*1024);
	if (buffer==NULL) { printf("Out of memory\n"); exit(1); }

	void* flatShader = buffer;
	memcpy(flatShader, &_binary_pixelshaders_flat_shader_start, (int)&_binary_pixelshaders_flat_shader_size);
	buffer += ((int)&_binary_pixelshaders_flat_shader_size+127)&~127;

	void* maskShader = buffer;
	memcpy(maskShader, &_binary_pixelshaders_mask_shader_start, (int)&_binary_pixelshaders_mask_shader_size);
	buffer += ((int)&_binary_pixelshaders_mask_shader_size+127)&~127;

	void* textureShader = buffer;
	memcpy(textureShader, &_binary_pixelshaders_texture_shader_start, (int)&_binary_pixelshaders_texture_shader_size);
	buffer += ((int)&_binary_pixelshaders_texture_shader_size+127)&~127;

	printf("flat shader: 0x%8x + %d bytes -> %x\n", &_binary_pixelshaders_flat_shader_start, &_binary_pixelshaders_flat_shader_size, flatShader);
	printf("mask shader: 0x%8x + %d bytes -> %x\n", &_binary_pixelshaders_mask_shader_start, &_binary_pixelshaders_mask_shader_size, maskShader);
	printf("texture shader: 0x%8x + %d bytes -> %x\n", &_binary_pixelshaders_texture_shader_start, &_binary_pixelshaders_texture_shader_size, textureShader);
	printf("rest of buffer at %x\n", buffer);

	unsigned int context = spuglFlip(queue);
	printf("context = %x\n", context);

	spuglSetCurrentContext(queue);

	glLoadIdentity();
	spuglNop();
	spuglDrawContext(context);

	// spuglJump(start);


	// NOTE THIS ISN'T A REAL OPENGL TRANSFORM!!!
	GLfloat projectionMatrix[] = {
		1.0,				0.0,				0.0,	0.0,
		0.0,				1.0,				0.0,	0.0,
		640.0f/420.0f,			360.0/420.0f,			1.0,	1.0/420.f,
		(640.0f*-282.0f)/420.0f,	(360.f*-282.0f)/420.0f,		0.0,	-282.0f/420.f,
	};
	glLoadMatrixf(projectionMatrix);

	int nf = 0;
	struct timespec a,b;
	clock_gettime(CLOCK_MONOTONIC,&a);


	unsigned int start = spuglTarget();

	int* n = (int*) buffer;

	int qqq = 0;

	int i = -130;
	//int i = -338;

	for (int j=0; j<9; j++)
	for (i=-350; i<130; i++)
	//for (int k=0; k<429; k++)
	{
		char buffer[256];
		sprintf(buffer,"   %d\r", *n);
		write(1,buffer,strlen(buffer));
		spuglDrawContext(context);

		qqq++;
		spuglClearScreen( (qqq&31)*8,((qqq>>5)&31)*8,((qqq>>10)&31)*8,0);

		spuglSelectPixelShader(flatShader, (int)&_binary_pixelshaders_flat_shader_size);
		glBegin(GL_TRIANGLES);
			float ofs = i * 1.0f;

/////////////////////
				glTexCoord2f( 0, 0 );
				glColor3ub(0, 0, 255);
				glVertex3f(20+ofs, 0, 100);

				glTexCoord2f( 256, 0 );
				glColor3ub(255, 0, 0);
				glVertex3f(130+ofs, 40, 100);

				glTexCoord2f( 256, 256 );
				glColor3ub(255, 0, 255);
				glVertex3f(50+ofs, 100, 100);
/////////////////////
				glTexCoord2f( 256, 0 );
				glColor3ub(255, 0, 0);
				glVertex3f(130+ofs, 40, 100);

				glTexCoord2f( 0, 0 );
				glColor3ub(0, 0, 255);
				glVertex3f(20+ofs, 0, 100);

				glTexCoord2f( 256, 256 );
				glColor3ub(255, 255, 0);
				glVertex3f(170+ofs, 20, 100);
		glEnd();
/////////////////////
		spuglSelectPixelShader(textureShader, (int)&_binary_pixelshaders_texture_shader_size);
		glBegin(GL_TRIANGLES);
				glTexCoord2f( 256, 0 );
				glColor3ub(0, 55, 255);
				glVertex3f(-230-ofs, 60, 100);

				glTexCoord2f( 0, 0 );
				glColor3ub(0, 255, 0);
				glVertex3f(-150-ofs, -50, 100);

				glTexCoord2f( 256, 256 );
				glColor3ub(255, 0, 55);
				glVertex3f(-110-ofs, 20, 100);
/////////////////////
		glEnd();

		glFlush();

		spuglPoke(n, (*n)+1);

		spuglJump(start);
		spuglSetTarget(start);
		glFlush();

		spuglWait(queue);
		context = spuglFlip(queue);

		nf++;
		write(1,".",1);
	}

	clock_gettime(CLOCK_MONOTONIC,&b);

	float ta = a.tv_sec + (a.tv_nsec * 0.000000001f);
	float tb = b.tv_sec + (b.tv_nsec * 0.000000001f);

	printf("Render of %d frames took %f seconds or %f FPS\n", nf, (tb-ta), ((float)nf)/(tb-ta) );

	//sleep(1);

	spuglFreeBuffer(buffer);
	spuglFreeCommandQueue(queue);
	spuglDisconnect(server);

	exit(0);
}
