#include "application.h"

#define WIDTH 1024*1.5
#define HEIGHT 512*1.5

int main (int argc, char** argv)
{
    Application app (WIDTH, HEIGHT);
    app.run ();

    return 0;
}
