#ifndef SHELL_EXECUTOR_H
#define SHELL_EXECUTOR_H

#include <string>
#include <utility>
#include <pwd.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/stat.h>
#include <iostream>
#include <vector>

#define REDIR_IN true
#define REDIR_OUT false

using std::string;
using std::vector;

struct Redirection {
    string path;
    int descriptor;
    bool direction;
    Redirection(string s, int de, bool di): path(std::move(s)), descriptor(de), direction(di) {}
};

struct Command {
    string app;
    vector<string> params;
    vector<Redirection> redirections;
    bool foreground;
};

class Executor {
public:
    int execute(Command&);
    int execute(vector<Command>);
};


#endif //SHELL_EXECUTOR_H