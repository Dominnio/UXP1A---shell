#ifndef SHELL_HISTORY_H
#define SHELL_HISTORY_H

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
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
        if(list.empty())
        {
            return "";
        }
        if(iter == list.begin())
        {
            return (*(list.begin()));
        }
        return (*(--iter));
    }
    std::string getnext()
    {
        if(list.empty())
        {
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
        path += "/.history_file";
        auto fd = open(path.c_str(), O_RDWR);
        flock(fd,LOCK_EX);
        for(auto iter = list_to_add.begin(); iter != list_to_add.end(); iter++)
        {
            write(fd,((*iter) + (char)'\n').c_str(),(*iter).size() + 1);
        }
        flock(fd,LOCK_UN);
        close(fd);
    }
private:
    History()
    {
        char current[1];
        std::string line;
        std::string path(getenv("HOME"));
        path += "/.history_file";
        auto fd = open(path.c_str(), O_RDWR | O_CREAT | O_EXCL, 0644);
        if((fd == -1) && (EEXIST == errno))
        {
            fd = open(path.c_str(), O_RDWR);
        }
        flock(fd,LOCK_SH);
        struct stat buf;
        fstat(fd,&buf);
        if(buf.st_size != 0)
        {
            read(fd, current, 1);
            while (current[0] != '\0' && current[0] != EOF)
            {
                while (current[0] != '\0' && current[0] != '\n' && current[0] != '\r'  && current[0] != EOF)
                {
                    line.push_back(current[0]);
                    read(fd, current, 1);
                }
                if (current[0] == '\n')
                {
                    list.push_front(line);
                    line.clear();
                    read(fd, current, 1);
                }
                if(lseek(fd,0,SEEK_CUR) == buf.st_size) {
                    break;
                }
            }
        }
        flock(fd,LOCK_UN);
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
