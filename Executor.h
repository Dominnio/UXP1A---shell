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
#include <unordered_map>
#include <unordered_set>
#include <wait.h>
#include <climits>
#include <lcms.h>
#include <condition_variable>

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

struct Job {
    string command;
    int group;
    int dead_cnt = 0;
    int last_pid;
    int return_code;
    bool running;
//    bool valid = false;
    std::unordered_set<int> processes; //<pid>
};

class Executor {
private:
    std::unordered_map<int, Job> jobs;
    int currentRunningGroup; //on fg
//    static Executor* executor;

public:

//    Executor() {
//        g_mutex.lock();
//    }
    static sig_atomic_t runningGID;
    static sig_atomic_t stopped;
    int execute(Command&, string&, int gid = -1);
    int execute(vector<Command>);
    void handle_child_death(int pid, int status);
    void stop();
    void wait_on_fg();
    void int_fg();
};


#endif //SHELL_EXECUTOR_H