#ifndef _SHADERS_H
#define _SHADERS_H

#define GLSL(src) "#version 130\n" #src

const char vert[] = GLSL(
    attribute vec2 aPosition;
    attribute vec2 aTexCoord;
    out vec2 fragCoord;
    void main()
    {    
        gl_Position = vec4 (aPosition, -1.0, 1.0);
        fragCoord = aTexCoord;
    }    
);

const char frag[] = GLSL(
	in vec2 fragCoord;
    out vec4 fragColor;

    void main()
    {    
        fragColor = vec4 (.05, 1., .05, 1.);
    }    
);

#endif // _SHADERS_H
