#include "application.h"

#define WIDTH 768
#define HEIGHT 384 

int main (int argc, char** argv)
{
    Application app (WIDTH, HEIGHT);
    app.run ();

    return 0;
}
