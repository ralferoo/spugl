[SPU_COMMAND_NOP] = (SPU_COMMAND*) &imp_nop,
[SPU_COMMAND_JUMP] = (SPU_COMMAND*) &imp_jump,
[SPU_COMMAND_ADD_CHILD] = (SPU_COMMAND*) &impAddChild,
[SPU_COMMAND_DELETE_CHILD] = (SPU_COMMAND*) &impDeleteChild,
[SPU_COMMAND_GL_BEGIN] = (SPU_COMMAND*) &imp_glBegin,
[SPU_COMMAND_GL_END] = (SPU_COMMAND*) &imp_glEnd,
[SPU_COMMAND_GL_VERTEX2] = (SPU_COMMAND*) &imp_glVertex2,
[SPU_COMMAND_GL_VERTEX3] = (SPU_COMMAND*) &imp_glVertex3,
[SPU_COMMAND_GL_VERTEX4] = (SPU_COMMAND*) &imp_glVertex4,
[SPU_COMMAND_GL_COLOR3] = (SPU_COMMAND*) &imp_glColor3,
[SPU_COMMAND_GL_COLOR4] = (SPU_COMMAND*) &imp_glColor4,
[SPU_COMMAND_GL_TEX_COORD2] = (SPU_COMMAND*) &imp_glTexCoord2,
[SPU_COMMAND_GL_TEX_COORD3] = (SPU_COMMAND*) &imp_glTexCoord3,
[SPU_COMMAND_GL_TEX_COORD4] = (SPU_COMMAND*) &imp_glTexCoord4,
[SPU_COMMAND_GL_BIND_TEXTURE] = (SPU_COMMAND*) &imp_glBindTexture,
[SPU_COMMAND_SCREEN_INFO] = (SPU_COMMAND*) &impScreenInfo,
[SPU_COMMAND_CLEAR_SCREEN] = (SPU_COMMAND*) &impClearScreen,
