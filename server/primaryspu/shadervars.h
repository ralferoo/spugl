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


/// THIS FILE IS NOT USED FOR ANYTHING!




/*
 * Built in vertex shader vars:  (The OpenGL Shading Language, chapter 7)
 *
 * highp   vec4 gl_Position;   // should be written to
 * mediump float gl_PointSize; // may be written to
 *
 * Can be read back after writing, if not written to they are undefined.
 *
 * Fragment shaders special vars:
 *
 * mediump vec4 gl_FragCoord;  // read-only
 *         bool gl_FrontFacing; // read-only
 * mediump vec4 gl_FragColor;
 * mediump vec4 gl_FragData[gl_MaxDrawBuffers];
 * mediump vec2 gl_PointCoord; // read-only
 *
 * Can write to gl_FragDepth, but then must always write it.
 * Choice of gl_FragColor or gl_FragData[n]
 * 
 * Special constants
 *
 * const mediump int gl_MaxVertexAttribs = 8;
 * const mediump int gl_MaxVertexUniformVectors = 128;   // < maybe not all in regs
 * const mediump int gl_MaxVaryingVectors = 8;
 * const mediump int gl_MaxVertexTextureImageUnits = 0;
 * const mediump int gl_MaxCombinedTextureImageUnits = 8;
 * const mediump int gl_MaxTextureImageUnits = 8;
 * const mediump int gl_MaxFragmentUniformVectors = 16;
 * const mediump int gl_MaxDrawBuffers = 1;
 *
 * Built-in uniform state
 *
 * struct gl_DepthRangeParameters {
 *	highp float near;        // n
 *	highp float far;         // f
 *	highp float diff;        // f - n
 * };
 * uniform gl_DepthRangeParameters gl_DepthRange;
 *
 */

