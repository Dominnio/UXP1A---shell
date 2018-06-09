#include "Executor.h"

using std::cout;
using std::cerr;
using std::endl;

#define F_EXIST 0
#define F_EXECABLE 1

uint8_t checkFile(const char* path) {
    struct stat sb;
    uint8_t res = 0;

    if (stat(path, &sb) == 0 && S_ISREG(sb.st_mode)) {
        res |= (1<<F_EXIST);
        if (S_IXUSR & sb.st_mode) {
            res |= (1<<F_EXECABLE);
        }
    }

    return res;
}

uint8_t checkFile(string& path) {
    return checkFile(path.c_str());
}

void setDefaultSignals() {
    signal (SIGINT, SIG_DFL);
    signal (SIGQUIT, SIG_DFL);
    signal (SIGTSTP, SIG_DFL);
    signal (SIGTTIN, SIG_DFL);
    signal (SIGTTOU, SIG_DFL);
    signal (SIGCHLD, SIG_DFL);
}

void Executor::handle_child_death(int pid, int status) {
    for(auto it = jobs.begin(); it != jobs.end(); ++it) {
        auto& job = *it;
        if(job.processes.erase(pid)) {
            job.dead_cnt++;
            cout<<"process "<<pid<<" from group "<<job.group<<" finished with code "<<status<<endl;
            if(pid == job.last_pid) {
                job.return_code = status;
                cout<<"it was the last launched process, saving return code"<<endl;
            }
            if(job.processes.empty()) {
                getControl();
                cout<<"all processes from group has finished"<<endl;
                getControl();
                cout<<"Got control"<<endl;
                currentRunningGroup = -1;
                jobs.erase(it);
                //TODO clean up
            } else {
                cout<<job.processes.size();
                cout<<" processes from group are still running"<<endl;
            }
            break;
        }
    }
}

void Executor::wait_on_fg() {
    sigset_t myset;
    sigemptyset(&myset);
    while(currentRunningGroup != -1) {
        sigsuspend(&myset);
    }
}

void Executor::int_fg() {
    if(currentRunningGroup != -1) {
        cout<<"sending SIGINT to "<<currentRunningGroup<<endl;
        killpg(currentRunningGroup, SIGINT);
    }
}

void Executor::stop() {
    cout<<"stopping"<<endl;
    if(currentRunningGroup == -1) {
        cout<<"no running processes"<<endl;
        return;
    } else {
        killpg(currentRunningGroup, SIGSTOP);
        for(auto& job: jobs){
            if(job.group == currentRunningGroup) {
                job.running = false;
            }
        }
        currentRunningGroup = -1;
        getControl();
    }
}

bool Executor::getExecPath(string& cmd, string& res) {
    //app has path included - we do not perform search
    if(cmd.find('/') != string::npos) {
        uint8_t status;

        if((status = checkFile(cmd.c_str())) & (1<<F_EXIST)) {
            if(status & (1 << F_EXECABLE)) {
                res = cmd;
                return true;
            }
        }
    } else {
        char *path = getenv("PATH");
        char *item = NULL;
        bool found = false;

        if (!path)
            return false;

        path = strdup(path);

        char real_path[4096];
        for (item = strtok(path, ":"); (!found) && item; item = strtok(NULL, ":"))
        {
            sprintf(real_path, "%s/%s", item, cmd.c_str());

            if(checkFile(real_path) & ((1<<F_EXIST) | (1<<F_EXECABLE))) {
                found = true;
            }
        }

        free(path);

        if(found) {
            res = string(real_path);
            return true;
        }
    }
    return false;
}

bool Executor::isCmdInternal(string& cmd) {
    static std::unordered_set<string> internals{"exit", "jobs", "fg", "bg", "cd"};
    return internals.find(cmd) != internals.end();
}

void Executor::getControl() {
    tcsetattr (STDIN_FILENO, TCSADRAIN, &shell_tmodes);
    if (tcsetpgrp(STDIN_FILENO, getpid())) {
        perror("tcsetpgrp failed");
    }
}

void Executor::executeExternal(Command &cmd, string& cmdStr, int& gid) {
    string execable;
    if(getExecPath(cmd.app, execable)) {
        int pid = fork();
        if (pid == -1) {
            cerr << "internal error: cannot fork" << endl;
        } else if (pid == 0) {
            setDefaultSignals();

            if (gid == -1) {
                gid = getpid();
            }

            setpgid(0, gid);

            if(cmd.foreground) {
                tcsetpgrp(STDIN_FILENO, getpid());
            }

            char **cmd_b = new char *[cmd.params.size() + 2];
            cmd_b[0] = strdup(cmd.app.c_str());
            int pos = 1;
            for (auto &param: cmd.params) {
                cmd_b[pos++] = strdup(param.c_str());
            }
            cmd_b[pos] = nullptr;
            execv(execable.c_str(), cmd_b);
        } else {
            if (gid == -1) {
                gid = pid;

                Job tmp;
                tmp.command = cmdStr;
                tmp.group = gid;
                tmp.processes.insert(pid);
                tmp.running = true;
                jobs.emplace_back(tmp);
            } else {
                bool found = false;
                for(auto& job: jobs) {
                    if(job.group == gid) {
                        job.processes.insert(pid);
                        found = true;
                        break;
                    }
                }

                if(!found) {
                    cerr<<"INTERNAL ERROR: wrong gid provided!"<<endl;
                    return;
                }
            }

            setpgid(pid, gid);

            if(cmd.foreground) {
                tcsetpgrp(STDIN_FILENO, pid);
                currentRunningGroup = gid;
            } else {
                currentRunningGroup = -1;
            }

        }
    } else {
        cerr<<"Cannot execute: '"<<cmd.app<<"'"<<endl;
    }
}

void Executor::executeInternal(Command& cmd) {
    if(cmd.app == "cd") {
        currentRunningGroup = -1;
        if(cmd.params.empty()) {
            const char *homedir;

            if ((homedir = getenv("HOME")) == NULL) {
                homedir = getpwuid(getuid())->pw_dir;
            }

            chdir(homedir);
        } else {
            struct stat sb;

            if (stat(cmd.params[0].c_str(), &sb) == 0 && S_ISDIR(sb.st_mode))
            {
                chdir(cmd.params[0].c_str());
            } else {
                cerr<<cmd.params[0]<<" is not a directory"<<endl;
            }
        }
    } else if(cmd.app == "jobs"){
        int pos = 0;
        currentRunningGroup = -1;
        for(auto& job: jobs) {
            cout<<"["<<pos<<"]: "<<job.command<<" ";
            cout<<(job.running ? "running" : "stopped")<<endl;
            pos++;
        }
    } else if(cmd.app == "bg" || cmd.app == "fg"){
        if(jobs.empty()) {
            return;
        }

        int job_num = jobs.size() - 1;

        if(!cmd.params.empty()) {
            try {
                job_num = std::stoi(cmd.params[0]);
            } catch (...) {
                job_num = -1;
            }

            if(job_num >= jobs.size()) {
                cerr<<"Provided job number is too high"<<endl;
                return;
            }
            if(job_num < 0) {
                cerr<<"Provided job number is not a number ("<<cmd.params[0]<<endl;
                return;
            }
        }

        Job& job = jobs[job_num];

        cout<<"Starting "<<job.group<<endl;
        job.running = true;
        if(cmd.app == "fg") {
            currentRunningGroup = job.group;
            tcsetpgrp(STDIN_FILENO, job.group);
        }
        killpg(job.group, SIGCONT);

    } else if(cmd.app == "exit"){
        exit(0);
    }
}

// execute single command
int Executor::execute(Command& cmd, string& cmdStr) {
    if(isCmdInternal(cmd.app)) {
        executeInternal(cmd);
    } else {
        int gid = -1;
        executeExternal(cmd, cmdStr, gid);
    }
}

// execute piped commands
int Executor::execute(vector<Command>, string&) {

}