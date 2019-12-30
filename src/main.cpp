#include "application.h"

#define WIDTH 1024
#define HEIGHT 512

int main (int argc, char** argv)
{
    Application app (WIDTH, HEIGHT);
    app.run ();

    return 0;
}
