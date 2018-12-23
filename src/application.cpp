#include <iostream>
#include <sstream>
#include <memory>
#include "application.h"

using namespace std;

#define WIN_TITLE "Audio software synthesizer by MacSlow"
#define newline '\n'

void Application::initialize ()
{
    if (_initialized)
        return;

    int result = 0;
    SDL_ClearError ();
    result = SDL_Init (SDL_INIT_VIDEO|SDL_INIT_VIDEO|SDL_INIT_EVENTS);
    if (result != 0) {
        cout << "SDL_Init() failed: " << SDL_GetError () << newline;
        _initialized = false;
        return;
    }

    _initialized = true;
}

Application::Application (size_t width, size_t height)
    : _initialized (false), _window (NULL), _running (false)
{
    initialize ();

    SDL_ClearError ();
    _window = SDL_CreateWindow (WIN_TITLE, SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED, width, height, 0);
    if (!_window) {
        cout << "window creation failed: " << SDL_GetError () << newline;
        return;
    }

    _softwareSynthesizer = make_unique<SoftwareSynthesizer> (width, height);
}

Application::~Application ()
{
    SDL_DestroyWindow (_window);
    SDL_Quit ();
}

void Application::handle_events ()
{
    SDL_Event event;
    while (SDL_PollEvent (&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                _running = false;
			}
            break;

        case SDL_QUIT:
            _running = false;
            break;
        }
    }
}

void Application::run ()
{
    if (!_initialized)
        return;

    _running = true;

    while (_running) {
        handle_events ();
        update ();
    }
}

void Application::update ()
{
    if (!_initialized)
        return;

    _softwareSynthesizer->update ();
    _softwareSynthesizer->paint (_window);
}
