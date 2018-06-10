#ifndef TERMINAL_H
#define TERMINAL_H

#include <unistd.h>
#include <chrono>
#include <csignal>
#include "Parser.h"
#include "Executor.h"
#include "History.h"

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
#ifdef DEBUG_MODE
                cout<<"Process "<<p<<" finished with code "<<status<<endl;
#endif
            } else if(WIFSTOPPED(status)){
#ifdef DEBUG_MODE
                cout<<"stopped"<<endl;
#endif
                executor.stop();
            }
        }
    } else if(signum == SIGTSTP) {
#ifdef DEBUG_MODE
        cout<<"Got SIGTSTP"<<endl;
#endif
        executor.stop();
    } else if(signum == SIGINT) {
        executor.int_fg();
    }
#ifdef DEBUG_MODE
    cout << "Interrupt XDDXD signal (" << signum << ") received.\n";
#endif
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

        std::cout << "\r[" << currentDateTime() << "] " << getUserName() <<"@"<< getHostName()
                  << " " << getCurrentDir() << "> " <<input<<" \b"<<std::flush;

        string lastPrintedInput = "";

        while(true)
        {
            int c = getch();

//            cout<<endl<<"GOT: |"<<(int) c<<"|"<<endl;

            if(c == 10 || c == 13) {
                if(!input.empty()) {
                    history.push(input);
                    cout<<endl;
                    try {
                        parser.parse(input);
#ifdef DEBUG_MODE
                        parser.print();
#endif
                        auto &&commands = parser.getCommandList();
                        if (commands.size() > 1) {
                            executor.execute(commands, input);
                        } else {
                            executor.execute(*commands.begin(), input);
                        }
                        executor.wait_on_fg();
                        input.clear();
                    }
                    catch (ExitException &e) {
                        std::cout << e.what() << std::endl;
                        break;
                    }
                    catch (std::exception &e) {
                        std::cout << e.what() << std::endl;
                        input.clear();
                    }
                }
            } else if(c >= 32 && c < 127) {
                input.push_back(c);
            } else if(c == 4) {
                break;
            } else if(c == 127) {
                if(!input.empty()) {
                    input.pop_back();
                } else {
                    continue;
                }

            } else if(c == 27) {
                c = getch();
                if(c == '[') {
                    c = getch();
                    if(c == 'A') {
//                        cout<<endl<<"ARROW UP"<<endl;
//                        cout<<history.getprev()<<endl;
                        input = history.getprev();
                    } else if (c == 'B') {
//                        cout<<endl<<"ARROW DOWN"<<endl;
//                        cout<<history.getnext()<<endl;
                        input = history.getnext();
                    } else {
                        input.push_back('[');
                        input.push_back(c);
                    }
                } else {
                    input.push_back(c);
                }
            } else {
#ifdef DEBUG_MODE
                cout<<endl<<"GOT: |"<<c<<"|"<<endl;
#endif
            }

            std::cout << "\r[" << currentDateTime() << "] " << getUserName() <<"@"<< getHostName()
                      << " " << getCurrentDir() << "> " <<input<<std::flush;

            if(lastPrintedInput.size() > input.size()) {
                for (int i = 0; i < lastPrintedInput.size() - input.size(); i++) {
                    cout << " ";
                }
                for (int i = 0; i < lastPrintedInput.size() - input.size(); i++) {
                    cout << "\b";
                }
            }

            lastPrintedInput = input;
        }
    }

    char getch()
    {
        char ch;
        initTermios();
        ch = getchar();
        resetTermios();
        return ch;
    }

private:
    History& history = History::getInstance();

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

    struct termios old_modes, new_modes;

    void initTermios()
    {
        tcgetattr(0, &old_modes); /* grab old terminal i/o settings */
        new_modes = old_modes; /* make new settings same as old settings */
        new_modes.c_lflag &= ~ICANON; /* disable buffered i/o */
        new_modes.c_lflag &= ~ECHO; /* set no echo mode */
        tcsetattr(0, TCSANOW, &new_modes); /* use these new terminal i/o settings now */
    }

    void resetTermios()
    {
        tcsetattr(0, TCSANOW, &old_modes);
    }
};
#endif // TERMINAL_H