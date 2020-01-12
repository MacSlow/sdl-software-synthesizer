#include <SDL.h>
#include <GL/glew.h>

#include <algorithm>
#include <iostream>

#include "opengl.h"
#include "shaders.h"

enum VertexAttribs {
    PositionAttr = 0,
};

OpenGL::OpenGL (unsigned int width, unsigned int height)
	: _width {width}
	, _height {height}
{
    glewExperimental = GL_TRUE;
    int success = glewInit (); 
    if (success != GLEW_OK) {
        std::cout << "OpenGL initialization failed!\n";
        SDL_ShowSimpleMessageBox (SDL_MESSAGEBOX_ERROR,
                                  "Error",
                                  "OpenGL initialization failed!",
                                  NULL);
    }
}

OpenGL::~OpenGL ()
{
}
 
bool OpenGL::init (size_t audioBufferSize, size_t frequencyBins)
{
    _audioBufferSize = audioBufferSize;
    std::cout << "buffer size: " << _audioBufferSize << '\n';

    glClearColor (.075f, .075f, .075f, 1.);
    glViewport (0, 0, _width, _height);
    glLineWidth(2.f);
    glEnable (GL_BLEND);

    _program = createShaderProgram (vert, frag, true);

    glGenVertexArrays (1, &_vao);
    glGenBuffers (1, &_vbo);

    size_t size = _audioBufferSize;
    std::vector<GLfloat> quad (size);
    for (int i = 0; i < quad.size(); i += 2) {
        float x = static_cast<float>(i);
        quad[i] = (x/_audioBufferSize)*(_width/_height) - 1.f;
        quad[i+1] = .0f;
    }

    glBindVertexArray (_vao);
    glBindBuffer (GL_ARRAY_BUFFER, _vbo);
    glBufferData (GL_ARRAY_BUFFER, quad.size() * sizeof (GLfloat), quad.data(), GL_DYNAMIC_DRAW); 
    glBindAttribLocation (_program, PositionAttr, "aPosition");
    GLchar* offset = 0;
    glVertexAttribPointer (PositionAttr,
                           2,
                           GL_FLOAT,
                           GL_FALSE,
                           4 * sizeof (GLfloat),
                           offset);
    glEnableVertexAttribArray (PositionAttr);

    glBindVertexArray (0);
    glBindBuffer (GL_ARRAY_BUFFER, 0);

	return true;
}

bool OpenGL::resize (unsigned int width, unsigned int height)
{
    _width = width;
    _height = height;

    glViewport (0, 0, _width, _height);

    return true;
}

bool OpenGL::draw (std::vector<float>& sampleBufferForDrawing,
                   std::vector<float>& fftBufferForDrawing,
                   bool doFFT)
{
    glClear (GL_COLOR_BUFFER_BIT);

    glUseProgram (_program);
    glBindVertexArray (_vao);

    size_t size = sampleBufferForDrawing.size();
    std::vector<GLfloat> quad (size);
    for (int i = 0; i < quad.size(); i += 2) {
        float x = static_cast<float>(i);
        float y = sampleBufferForDrawing[i];
        if (doFFT) {
            y = fftBufferForDrawing[i];
        }
        quad[i] = (x/_audioBufferSize)*(_width/_height) - 1.f;
        quad[i+1] = y;
    }

    glNamedBufferSubData(_vbo, 0, quad.size() * sizeof (GLfloat), quad.data());

    std::vector<unsigned short> indices(_audioBufferSize/4);
    int index = 0;
    std::generate(indices.begin(), indices.end(), [&](){
        return index++;
    });
    glDrawElements (GL_LINE_STRIP,
                    indices.size(),
                    GL_UNSIGNED_SHORT,
                    indices.data());

    glUseProgram (0);
    glBindVertexArray (0);

	return true;
}

GLuint OpenGL::loadShader (const char* src, GLenum type)
{
    GLuint shader = glCreateShader (type);
    if (shader)
    {   
        GLint compiled;

        glShaderSource (shader, 1, &src, NULL);
        glCompileShader (shader);
        glGetShaderiv (shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            GLchar log[1024];
            glGetShaderInfoLog (shader, sizeof log - 1, NULL, log);
            log[sizeof log - 1] = '\0';
            std::cout << "loadShader compile failed: " << log << '\n'; 
            glDeleteShader (shader);
            shader = 0;
        }
    }   

    return shader;
}

GLuint OpenGL::createShaderProgram (const char* vertSrc,
                                    const char* fragSrc,
                                    bool link)
{
    GLuint program = _program;

    if (!vertSrc && !fragSrc)
        return 0;

    if (vertSrc) {
        _vShaderId = loadShader (vertSrc, GL_VERTEX_SHADER);
        if (!_vShaderId)
            return _program;
    }

    if (fragSrc) {
        _fShaderId = loadShader (fragSrc, GL_FRAGMENT_SHADER);
        if (!_fShaderId)
            return _program;
    }

    program = glCreateProgram ();
    if (!program)
        return _program;

    if (vertSrc) {
        glAttachShader (program, _vShaderId);
    }

    if (fragSrc) {
        glAttachShader (program, _fShaderId);
    }

    if (!link) {
        return program;
    }

    glProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

    glLinkProgram (program);

    GLint linked = 0;
    glGetProgramiv (program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLchar log[1024];
        glGetProgramInfoLog (program, sizeof log - 1, NULL, log);
        log[sizeof log - 1] = '\0';
        std::cout << "Link failed: " << log << '\n';
        glDeleteProgram (program);
        return _program;
    }

    return program;
}

void OpenGL::linkShaderProgram (GLuint progId)
{
    glLinkProgram (progId);

    GLint linked = 0;
    glGetProgramiv (progId, GL_LINK_STATUS, &linked);
    if (!linked)
    {   
        GLchar log[1024];
        glGetProgramInfoLog (progId, sizeof log - 1, NULL, log);
        log[sizeof log - 1] = '\0';
        std::cout << "Link failed: " << log << '\n'; 
        glDeleteProgram (progId);
    }   
}
