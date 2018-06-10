#ifndef TERMINAL_H
#define TERMINAL_H

#include <unistd.h>
#include <chrono>
#include <csignal>
#include "Parser.h"
#include "Executor.h"

using std::string;
using std::cout;
using std::endl;

Executor executor;


void signalHandler( int signum ) {
    if(signum == SIGCHLD) {
        pid_t p;
        int status = -1;

        while ((p=waitpid(-1, &status, WUNTRACED|WNOHANG)) > 0)
        {
            if(WCOREDUMP(status) || WIFSIGNALED(status) || WIFEXITED(status)) {
                executor.handle_child_death(p, status);
                cout<<"Process "<<p<<" finished with code "<<status<<endl;
            } else if(WIFSTOPPED(status)){
                cout<<"stopped"<<endl;
                executor.stop();
            }
        }
    } else if(signum == SIGTSTP) {
        cout<<"Got SIGTSTP"<<endl;
        executor.stop();
    } else if(signum == SIGINT) {
        executor.int_fg();
    }
    cout << "Interrupt XDDXD signal (" << signum << ") received.\n";
}

class Terminal
{
private:

public :
    static Terminal& getInstance()
    {
        static Terminal instance;
        return instance;
    }
    void start()
    {
        Parser parser;
        signal(SIGCHLD, signalHandler);
        signal(SIGTSTP, signalHandler);
        signal(SIGINT, signalHandler);

        signal (SIGTTIN, SIG_IGN);
        signal (SIGTTOU, SIG_IGN);
        while(true)
        {
            try {
                std::cout << "[" << currentDateTime() << "] " << getUserName() <<"@"<< getHostName() << " " << getCurrentDir() <<"> ";
                std::getline(std::cin,input);
                parser.parse(input);
                parser.print();
                auto&& commands = parser.getCommandList();
                if(commands.size() > 1) {
                    executor.execute(commands, input);
                } else {
                    executor.execute(*commands.begin(), input);
                }
                executor.wait_on_fg();
                input.clear();
            }
            catch(ExitException &e)
            {
                std::cout << e.what() << std::endl;
                break;
            }
            catch(std::exception &e)
            {
                std::cout << e.what() << std::endl;
            }
        }
    }
private:
    Terminal() {
        setsid();
    };
    Terminal(Terminal const&) {};
    Terminal& operator=(Terminal const&) {};
    static Terminal* instance;

    std::string input;

    const string currentDateTime()
    {
        time_t     now = time(0);
        struct tm  tstruct;
        char       buf[10];
        tstruct = *localtime(&now);
        strftime(buf, 10, "%X", &tstruct);

        return string(buf);
    }

    const string getUserName() {
        return string(getlogin());
    }

    const string getHostName() {
        char hostname[64];
        if(gethostname(hostname, 63) != -1) {
            hostname[63] = 0;
            return string(hostname);
        }

        return "unknown";
    }

    const string getCurrentDir() {
        char path[300];
        if(getcwd(path, 299) != NULL) {
            path[299] = 0;
            return string(path);
        }

        return "error"; // xD
    }
};
#endif // TERMINAL_H