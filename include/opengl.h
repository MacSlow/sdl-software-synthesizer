#ifndef _OPENGL_H
#define _OPENGL_H

#include <SDL_opengl.h>

class OpenGL
{
    public:
        OpenGL (unsigned int width,
                unsigned int height);
        ~OpenGL ();
 
        bool init ();
        bool resize (unsigned int width, unsigned int height);
        bool draw (std::vector<float>& sampleBufferForDrawing);

    private:
        GLuint loadShader (const char *src, GLenum type);
        GLuint createShaderProgram (const char* vertSrc,
                                    const char* fragSrc,
                                    bool link);
        void linkShaderProgram (GLuint progId);

    private:
        unsigned int _width;
        unsigned int _height;
        GLuint _vShaderId;
        GLuint _fShaderId;
        GLuint _program;
        GLuint _vao;
        GLuint _vbo;
};
#endif // _OPENGL_H
