#ifndef _APPLICATION_H
#define _APPLICATION_H

#include <SDL.h>
#include <memory>

#include "software-synthesizer.h"

class Application {
  public:
    Application (size_t width, size_t height);
    ~Application ();

    void run ();
    void update ();

  private:
    void initialize ();
    void handle_events ();

  private:
    bool _initialized = false;
    SDL_Window* _window = nullptr;
    bool _running = true;
    std::unique_ptr<SoftwareSynthesizer> _softwareSynthesizer;
};

#endif // _APPLICATION_H
