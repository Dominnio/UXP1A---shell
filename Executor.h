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
#include <termios.h>

#define REDIR_IN true
#define REDIR_OUT false

using std::string;
using std::vector;

struct Redirection {
    string path;
    int descriptor;
    int opened_file = -1;
    bool direction;
    Redirection(string s, int de, bool di): path(std::move(s)), descriptor(de), direction(di) {}
};

struct Command {
    string app;
    string path = "";
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
    std::unordered_set<int> processes; //<pid>
};

class Executor {
private:
    struct termios shell_tmodes;
    std::vector<Job> jobs;
    int currentRunningGroup = -1; //on fg
    bool getExecPath(string& cmd, string& res);
    bool isCmdInternal(string& cmd);
    void executeInternal(Command& cmd);
    void executeExternal(Command& cmd, string&, int& gid, int in_pipe = -1, int out_pipe = -1, bool last_one = true);
    void getControl();
    bool insertExecPath(Command& cmd);
public:
    Executor() {
        tcgetattr (STDIN_FILENO, &shell_tmodes);
        getControl();
    }
    int getCurrentGID(){return currentRunningGroup;}
    static sig_atomic_t runningGID;
    static sig_atomic_t stopped;
    int execute(Command&, string&);
    int execute(vector<Command>, string&);
    void handle_child_death(int pid, int status);
    void stop();
    void wait_on_fg();
    void int_fg();
};


#endif //SHELL_EXECUTOR_H