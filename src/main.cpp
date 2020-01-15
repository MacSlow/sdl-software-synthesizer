#include <iostream>

#include "application.h"

#define WIDTH 1024*1.5
#define HEIGHT 512*1.5

using std::string;

int main (int argc, char** argv)
{
	string midiPort = "hw:1,0,0";

	if (argc == 2) {
		midiPort = argv[1];
	}

    Application app (WIDTH, HEIGHT, midiPort);
    app.run ();

    return 0;
}
