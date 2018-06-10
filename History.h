#ifndef SHELL_HISTORY_H
#define SHELL_HISTORY_H

#include <sys/file.h>
#include <fstream>
#include <cstring>
#include <iostream>
#include <list>

class History
{
public:
    static History& getInstance()
    {
        static History instance;
        return instance;
    }
    void push(std::string s)
    {
        list.push_back(s);
        list_to_add.push_back(s);
        iter = list.end();
    }
    std::string getprev()
    {
        if(list.empty()) {
            return "";
        }

        if(iter == list.begin())
        {
            return *iter;
        }
        return (*(--iter));
    }
    std::string getnext()
    {
        if(list.empty()) {
            return "";
        }

        if(iter == --list.end() || iter == list.end())
        {
            iter = list.end();
            return "";
        }
        return (*(++iter));
    }
    ~History()
    {
        std::string line;
        std::string path(getenv("HOME"));
        path += "/history_file.txt";
        file.open(path, std::ios::app);
        auto fd = open(path.c_str(), O_CREAT, 0600);
        flock(fd,LOCK_EX);
        for(auto iter = list_to_add.begin(); iter != list_to_add.end(); iter++)
        {
            file << *iter << std::endl;
        }
        flock(fd,LOCK_UN);
        close(fd);
        file.close();
    }
private:
    History()
    {
        char current;
        std::string line;
        std::string path(getenv("HOME"));
        path += "/history_file.txt";
        auto fd = open(path.c_str(), O_CREAT, 0600);
        file.open(path, std::fstream::in|std::fstream::out);
        flock(fd,LOCK_SH);
        file >> current;
        while(current != '\0' && current != EOF)
        {
            while(current != '\0' && current != '\n' && current != EOF)
            {
                line.push_back(current);
                current = file.get();
            }
            if(current == '\n')
            {
                list.push_front(line);
                line.clear();
                current = file.get();
            }

        }
        flock(fd,LOCK_UN);
        file.close();
        close(fd);
        list.reverse();
        iter = list.end();
    };
    History(History const&) {};
    History& operator=(History const&) {};
    static History* instance;

    std::fstream file;
    std::list<std::string> list;
    std::list<std::string> list_to_add;
    std::list<std::string>::iterator iter;
};

#endif //SHELL_HISTORY_H
