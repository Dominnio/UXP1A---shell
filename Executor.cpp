#include "Executor.h"

using std::cout;
using std::cerr;
using std::endl;

#define F_EXIST 0
#define F_EXECABLE 1

sig_atomic_t Executor::runningGID = 0;
sig_atomic_t Executor::stopped = false;

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

//void Executor::stop_handler(int sig) {
//    if(sig == SIGSTOP) {
//        cout << "STOP RECEIVED" << endl;
//        if (Executor::runningGID != 0) {
//            killpg(Executor::runningGID, SIGSTOP);
//            Executor::stopped = true;
//        }
//    }
//}

void Executor::handle_child_death(int pid, int status) {
    for(auto& job: jobs) {
        if(job.second.processes.erase(pid)) {
            job.second.dead_cnt++;
            cout<<"process "<<pid<<" from group "<<job.first<<" finished with code "<<status<<endl;
            if(pid == job.second.last_pid) {
                job.second.return_code = status;
                cout<<"it was the last launched process, saving return code"<<endl;
            }
            if(job.second.processes.empty()) {
                cout<<"all processes from group has finished"<<endl;
                currentRunningGroup = -1;
                //TODO clean up
            } else {
                cout<<job.second.processes.size();
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
        killpg(-currentRunningGroup, SIGINT);
    }
}

void Executor::stop() {
    cout<<"stopping"<<endl;
    if(currentRunningGroup == -1) {
        cout<<"no running processes"<<endl;
        return;
    } else {
        killpg(currentRunningGroup, SIGSTOP);
        jobs[currentRunningGroup].running = false;
        currentRunningGroup = -1;
    }
}

int Executor::execute(Command& cmd, string& cmdStr, int gid) {
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
            cout<<"["<<pos<<"]: "<<job.second.command<<" ";
            cout<<(job.second.running ? "running" : "stopped")<<endl;
        }
    } else if(cmd.app == "fg"){
        currentRunningGroup = -1;
        if(!jobs.empty()) {
            cout<<"Starting "<<(*jobs.begin()).first<<endl;
            currentRunningGroup = (*jobs.begin()).first;
            killpg((*jobs.begin()).first, SIGCONT);
        }
    } else if(cmd.app == "exit"){
        exit(0);
    } else { //not build-in command

        //app has path included - we do not perform search
        if(cmd.app.find('/') != string::npos) {
            uint8_t status;

            if((status = checkFile(cmd.app.c_str())) & (1<<F_EXIST)) {
                if(status & (1<<F_EXECABLE)) {
                    cout<<"executing local"<<endl;
                    int pid = fork();
                    if(pid == -1) {
                        cerr<<"internal error (1)"<<endl;
                    } else if(pid == 0) {
                        if(gid == -1) {
                            setpgid(0, 0);
                        } else {
                            setpgid(0, gid);
                        }
                        char** cmd_b = new char* [cmd.params.size() + 2];
                        cmd_b[0] = strdup(cmd.app.c_str());
                        int pos = 1;
                        for(auto& param: cmd.params) {
                            cmd_b[pos++] = strdup(param.c_str());
                        }
                        cmd_b[pos] = nullptr;
                        execv(cmd.app.c_str(), cmd_b);
                    } else {
                        Job tmp;
                        tmp.command = cmdStr;
                        if(gid == -1) {
                            tmp.group = pid;
                            gid = pid;
                        } else {
                            tmp.group = gid;
                        }
                        tmp.dead_cnt = 0;
                        tmp.processes.insert(pid);
                        tmp.running = true;
                        jobs[gid] = tmp;
                        currentRunningGroup = gid;

                        cout<<"running "<<jobs[gid].processes.size()<<" processes"<<endl;

//                        for(int i=0; i<jobs[gid].processes.size(); i++) {
//                            cout<<"waiting"<<endl;
//
//                            int ret;
//                            waitpid(-(gid), &ret, 0);
//                            if(!jobs[gid].running) {
//                                cout<<"job "<<gid<<" sleeped"<<endl;
//                                break;
//                            }
//                        }
//                        cout<<"finished waiting for group "<<gid<<endl;

//                        int returnCode;
//                        stopped = false;
//                        runningGID = pid;
//                        if(signal(SIGSTOP, Executor::stop_handler) == 0) {
//                            cout<<"signal attached"<<endl;
//                        } else {
//                            cout<<"signal not attached"<<endl;
//                        }
//                        waitpid(pid, &returnCode, 0);
//                        runningGID = 0;
//                        if(stopped) {
//                            cout<<"job "<<cmdStr<<" has been stopped"<<endl;
//                        } else {
//                            jobs.erase(jobs.find(pid));
//                            cout << "job " << cmdStr << " returned with code: " << returnCode << endl;
//                        }
                    }
                } else {
                    cerr<<cmd.app<<" is not executable"<<endl;
                }
            } else {
                cerr<<cmd.app<<" no such file"<<endl;
            }

        } else { // we have to search in $PATH

            char *path = getenv("PATH");
            char *item = NULL;
            bool found = false;

            if (!path)
                return 0;

            path = strdup(path);

            char real_path[4096];
            for (item = strtok(path, ":"); (!found) && item; item = strtok(NULL, ":"))
            {
                sprintf(real_path, "%s/%s", item, cmd.app.c_str());

                if(checkFile(real_path) & ((1<<F_EXIST) | (1<<F_EXECABLE))) {
                    found = true;
                }
            }

            free(path);

            if(found) {
                cout<<"executing global ("<<real_path<<")"<<endl;
                int pid = fork();
                if(pid == -1) {
                    cerr<<"internal error (1)"<<endl;
                } else if(pid == 0) {
                    setpgid(0, 0);
                    char** cmd_b = new char* [cmd.params.size() + 2];
                    cmd_b[0] = strdup(cmd.app.c_str());
                    int pos = 1;
                    for(auto& param: cmd.params) {
                        cmd_b[pos++] = strdup(param.c_str());
                    }
                    cmd_b[pos] = nullptr;
                    execv(real_path, cmd_b);
                } else {
                    Job tmp;
                    tmp.command = cmdStr;
                    if(gid == -1) {
                        tmp.group = pid;
                        gid = pid;
                    } else {
                        tmp.group = gid;
                    }
                    tmp.dead_cnt = 0;
                    tmp.processes.insert(pid);
                    tmp.running = true;
                    jobs[gid] = tmp;
                    currentRunningGroup = gid;

                    cout<<"running "<<jobs[gid].processes.size()<<" processes"<<endl;

//                    for(auto& proc: jobs[gid].processes) {
//                        cout<<"waiting"<<endl;
//
//                        int ret;
//                        waitpid(proc, &ret, 0);
//                        if(!jobs[gid].running) {
//                            cout<<"job "<<gid<<" sleeped"<<endl;
//                            break;
//                        }
//                    }
//                    cout<<"finished waiting for group "<<gid<<endl;
//                    int returnCode;
//                    stopped = false;
//                    runningGID = pid;
//                    if(signal(SIGINT, Executor::stop_handler) == 0) {
//                        cout<<"signal attached"<<endl;
//                    } else {
//                        cout<<"signal not attached"<<endl;
//                    }
//                    waitpid(pid, &returnCode, 0);
//
//                    runningGID = 0;
//                    if(stopped) {
//                        cout<<"job "<<cmdStr<<" has been stopped"<<endl;
//                    } else {
//                        jobs.erase(jobs.find(pid));
//                        cout << "job " << cmdStr << " returned with code: " << returnCode << endl;
//                    }
                }
            } else {
                cerr<<cmd.app<<" command not found"<<endl;
            }
        }
    }
}