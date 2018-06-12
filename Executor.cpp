#include <fcntl.h>
#include "Executor.h"
#include "Parser.h"

using std::cout;
using std::cerr;
using std::endl;

#define F_EXIST 0
#define F_EXECABLE 1

#define PIPE_READ 0
#define PIPE_WRITE 1

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
#ifndef DEBUG_MODE
            if(currentRunningGroup == -1) {
#endif
                cout << endl << "process " << pid << " from job '" << job.command << "' has finished with code "
                     << WEXITSTATUS(status) << " (status: " << status << ")" << endl;
#ifndef DEBUG_MODE
            }
#endif
            if(pid == job.last_pid) {
                job.return_code = status;
#ifdef DEBUG_MODE
                cout<<"it was the last launched process, saving return code"<<endl;
#endif
                if(setenv("?", std::to_string(WEXITSTATUS(status)).c_str(), 1) == -1) {
                    perror("setting return code variable");
                }
            }
            if(job.processes.empty()) {
                if(currentRunningGroup == -1) {
                    cout << "Job '" << job.command << "' has finished with code " << WEXITSTATUS(job.return_code) << endl;
                }
#ifdef DEBUG_MODE
                cout<<"all processes from group has finished"<<endl;
#endif
                getControl();
#ifdef DEBUG_MODE
                cout<<"Got control"<<endl;
#endif
                currentRunningGroup = -1;
                jobs.erase(it);
            } else {
#ifdef DEBUG_MODE
                cout<<job.processes.size();
                cout<<" processes from group are still running"<<endl;
#endif
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
#ifdef DEBUG_MODE
        cout<<"sending SIGINT to "<<currentRunningGroup<<endl;
#endif
        killpg(currentRunningGroup, SIGINT);
    }
}

void Executor::stop() {
#ifdef DEBUG_MODE
    cout<<"stopping"<<endl;
#endif
    if(currentRunningGroup == -1) {
#ifdef DEBUG_MODE
        cout<<"no running processes"<<endl;
#endif
        return;
    } else {
        killpg(currentRunningGroup, SIGSTOP);
        for(auto& job: jobs){
            if(job.group == currentRunningGroup) {
                job.running = false;
                cout<<"Stopped '"<<job.command<<"'"<<endl;
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
    static std::unordered_set<string> internals{"exit", "jobs", "fg", "bg", "cd", "pwd", "export", VARIABLE_SET_CMD};
    return internals.find(cmd) != internals.end();
}

void Executor::getControl() {
    //tcsetattr (STDIN_FILENO, TCSADRAIN, &shell_tmodes);
    if (tcsetpgrp(STDIN_FILENO, getpid())) {
        perror("tcsetpgrp failed");
    }
}

void Executor::executeExternal(Command &cmd, string& cmdStr, int& gid, int in_pipe, int out_pipe, bool last_one) {
    bool in_redir = false;
    bool out_redir = false;
    for(auto& redir: cmd.redirections) {
        if(redir.direction == REDIR_IN) {
            redir.opened_file = open(redir.path.c_str(), O_RDONLY | O_CREAT, 0644);
            if(redir.descriptor == STDIN_FILENO) {
                in_redir = true;
            }
        } else {
            redir.opened_file = open(redir.path.c_str(), O_WRONLY | O_CREAT, 0644);
            if(redir.descriptor == STDOUT_FILENO) {
                out_redir = true;
            }
        }

        if(redir.opened_file == -1) {
            perror(string("Cannot open file for redirection: " + redir.path).c_str());
            return;
        }
    }

    if(in_redir && in_pipe != -1) {
#ifdef DEBUG_MODE
        cout<<"double input redirection!"<<endl;
#endif
        close(in_pipe);
        in_pipe = -1;
    }

#ifdef DEBUG_MODE
    cout<<"out_redir: "<<out_redir<<" out_pipe: "<<out_pipe<<endl;
#endif

    if(out_redir && out_pipe != -1) {
#ifdef DEBUG_MODE
        cout<<"double output redirection!"<<endl;
#endif
        close(out_pipe);
        out_pipe = -1;
    }

    char sync_buf[1];
    int sync_pipe[2];
    pipe(sync_pipe);

    int pid = fork();
    if (pid == -1) {
        cerr << "INTERNAL ERROR: cannot fork" << endl;
    } else if (pid == 0) {
        setDefaultSignals();

        for(auto& redir: cmd.redirections) {
            dup2(redir.opened_file, redir.descriptor);
            close(redir.opened_file);
        }

        if(in_pipe != -1) {
            dup2(in_pipe, STDIN_FILENO);
            close(in_pipe);
        }

        if(out_pipe != -1) {
            dup2(out_pipe, STDOUT_FILENO);
            close(out_pipe);
        }

        char **cmd_b = new char *[cmd.params.size() + 2];
        cmd_b[0] = strdup(cmd.app.c_str());
        int pos = 1;
        for (auto &param: cmd.params) {
            cmd_b[pos++] = strdup(param.c_str());
        }
        cmd_b[pos] = nullptr;

        read(sync_pipe[PIPE_READ], sync_buf, 1);

        close(sync_pipe[PIPE_READ]);
        close(sync_pipe[PIPE_WRITE]);

        execv(cmd.path.c_str(), cmd_b);
    } else {
        if (gid == -1) {
            gid = pid;

            Job tmp;
            tmp.command = cmdStr;
            tmp.group = gid;
            tmp.processes.insert(pid);
            tmp.running = true;
            if(last_one) {
                tmp.last_pid = pid;
            }
            jobs.emplace_back(tmp);
        } else {
            bool found = false;
            for(auto& job: jobs) {
                if(job.group == gid) {
                    job.processes.insert(pid);
                    if(last_one) {
                        job.last_pid = pid;
                    }
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
            tcsetpgrp(STDIN_FILENO, gid);
            currentRunningGroup = gid;
        } else {
            currentRunningGroup = -1;
        }

        for(auto& redir: cmd.redirections) {
            close(redir.opened_file);
        }

        if(in_pipe != -1) {
            close(in_pipe);
        }
        if(out_pipe != -1) {
            close(out_pipe);
        }

        sync_buf[0] = 'r';
        write(sync_pipe[PIPE_WRITE], sync_buf, 1);

        close(sync_pipe[PIPE_READ]);
        close(sync_pipe[PIPE_WRITE]);
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
            cout<<(job.running ? "running" : "stopped")<<" (";
            cout<<job.group<<")"<<endl;
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
#ifdef DEBUG_MODE
        cout<<"Starting "<<job.group<<endl;
#endif
        job.running = true;
        if(cmd.app == "fg") {
            currentRunningGroup = job.group;
            tcsetpgrp(STDIN_FILENO, job.group);
        }
        killpg(job.group, SIGCONT);

    } else if(cmd.app == "exit") {
        exit(0);
    } else if(cmd.app == VARIABLE_SET_CMD) {
//        cout<<"setting variable: "<<cmd.params[0]<<":"<<cmd.params[1]<<endl;
        if(!cmd.params[1].empty()) {
            setenv(cmd.params[0].c_str(), cmd.params[1].c_str(), 1);
        } else {
            unsetenv(cmd.params[0].c_str());
        }
    } else if(cmd.app == "pwd") {
        char path[300];
        if(getcwd(path, 299) != NULL) {
            path[299] = 0;
            cout<<path<<endl;
        } else {
            cout<<"Error while getting current path"<<endl;
        }
    } else if(cmd.app == "export") {
        if(cmd.params.size() != 1) {
            cout<<"wrong params for export provided"<<endl;
        } else {
            string val, name;
            if(Parser::parseSetVariableString(cmd.params[0], name, val)) {
                if(val.size() > 0) {
                    setenv(name.c_str(), val.c_str(), 1);
                } else {
                    unsetenv(name.c_str());
                }
            }
        }
    }
}

bool Executor::insertExecPath(Command& cmd) {
    return (!cmd.app.empty() && getExecPath(cmd.app, cmd.path));
}

// execute single command
int Executor::execute(Command& cmd, string& cmdStr) {
    if(isCmdInternal(cmd.app)) {
        executeInternal(cmd);
    } else {
        int gid = -1;
        if(insertExecPath(cmd)) {
            executeExternal(cmd, cmdStr, gid);
        } else {
            cerr<<"Cannot execute: '"<<cmd.app<<"'"<<endl;
        }
    }
}

// execute piped commands
int Executor::execute(vector<Command> cmds, string& cmd_str) {
    bool is_group_in_background = false;
    for(int i = 0; i < cmds.size(); i++){
        auto& cmd = cmds[i];
        if(isCmdInternal(cmd.app)) {
            cerr<<"cannot use internal command ("<<cmd.app<<") with pipes"<<endl;
            return 0;
        }
        if(!cmd.foreground) {
            if(i != cmds.size() - 1) {
                cerr << "cannot launch intermediate pipe command in background" << endl;
                return 0;
            } else {
                is_group_in_background = true;
            }
        }
        if(!insertExecPath(cmd)) {
            cerr<<"Cannot execute: '"<<cmd.app<<"'"<<endl;
            return 0;
        }
    }
    int prevInPipe = -1;
    int gid = -1;
    for(int i = 0; i < cmds.size(); i++) {
        if(is_group_in_background) {
            cmds[i].foreground = false;
        }

        if(i != cmds.size() - 1) {
            int pipefd[2];
            pipe(pipefd);
            executeExternal(cmds[i], cmd_str, gid, prevInPipe, pipefd[PIPE_WRITE], false);
            prevInPipe = pipefd[PIPE_READ];
        } else {
            executeExternal(cmds[i], cmd_str, gid, prevInPipe);
        }
    }
#ifdef DEBUG_MODE
    cout<<"started group: "<<gid<<endl;
#endif
}