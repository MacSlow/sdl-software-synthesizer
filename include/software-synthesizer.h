#ifndef _SOFTWARE_SYNTHESIZER_H
#define _SOFTWARE_SYNTHESIZER_H

#include <SDL.h>
#include <vector>

class SoftwareSynthesizer {
  public:
    SoftwareSynthesizer (size_t width, size_t height);

    void update ();
    void paint (const SDL_Window* window) const;

  private:
    size_t _width = 0;
    size_t _height = 0;
};

#endif // _SOFTWARE_SYNTHESIZER_H
