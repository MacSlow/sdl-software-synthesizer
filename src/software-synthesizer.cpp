#include <random>
#include "software-synthesizer.h"

using namespace std;

#define NUM_CHANNELS 4
#define NO_INTENSITY 0
#define FULL_INTENSITY 255

SoftwareSynthesizer::SoftwareSynthesizer (size_t width, size_t height)
    : _width (width), _height (height)
{
}

void SoftwareSynthesizer::update ()
{
}

void SoftwareSynthesizer::paint (const SDL_Window* window) const
{
    size_t pitch = _width * NUM_CHANNELS;
    size_t size = _height * pitch;
    std::vector<unsigned char> buffer;
    buffer.reserve (size);

    size_t indexSurface = 0;
    unsigned char value = 0;

    for (size_t y = 0; y < _height; ++y) {
        for (size_t x = 0; x < _width; ++x) {
            indexSurface = x * NUM_CHANNELS + y * pitch;
            value = 0;
            buffer[indexSurface] = value;
            buffer[indexSurface + 1] = value;
            buffer[indexSurface + 2] = value;
            buffer[indexSurface + 3] = FULL_INTENSITY;
        }
    }

    SDL_Surface* src = nullptr;
    src = SDL_CreateRGBSurfaceWithFormatFrom (buffer.data (),
                                              _width,
                                              _height,
                                              32,
                                              pitch,
                                              SDL_PIXELFORMAT_RGB888);
    SDL_Surface* dst = SDL_GetWindowSurface (const_cast<SDL_Window*> (window));
    SDL_BlitSurface (src, NULL, dst, NULL);
    SDL_FreeSurface (src);
    SDL_UpdateWindowSurface (const_cast<SDL_Window*> (window));
}
