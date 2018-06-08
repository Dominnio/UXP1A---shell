#include <string.h>
#include <wait.h>
#include <climits>
#include <lcms.h>
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

int Executor::execute(Command& cmd) {
    if(cmd.app == "cd") {
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
                        char** cmd_b = new char* [cmd.params.size() + 2];
                        cmd_b[0] = strdup(cmd.app.c_str());
                        int pos = 1;
                        for(auto& param: cmd.params) {
                            cmd_b[pos++] = strdup(param.c_str());
                        }
                        cmd_b[pos] = nullptr;
                        execv(cmd.app.c_str(), cmd_b);
                    } else {
                        int returnCode;
                        waitpid(pid, &returnCode, 0);
                        cout<<"process returned with code: "<<returnCode<<endl;
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
                    char** cmd_b = new char* [cmd.params.size() + 2];
                    cmd_b[0] = strdup(cmd.app.c_str());
                    int pos = 1;
                    for(auto& param: cmd.params) {
                        cmd_b[pos++] = strdup(param.c_str());
                    }
                    cmd_b[pos] = nullptr;
                    execv(real_path, cmd_b);
                } else {
                    int returnCode;
                    waitpid(pid, &returnCode, 0);
                    cout<<"process returned with code: "<<returnCode<<endl;
                }
            } else {
                cerr<<cmd.app<<" command not found"<<endl;
            }
        }
    }
}