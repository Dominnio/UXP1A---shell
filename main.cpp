#include <iostream>
#include <ctime>
#include <pwd.h>

#include "Terminal.h"

int main()
{
    const char *homedir;

    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }

    chdir(homedir);
    Terminal::getInstance().start();
    return 0;
}

