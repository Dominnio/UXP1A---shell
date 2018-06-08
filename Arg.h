#ifndef UXP1A_ARG_H
#define UXP1A_ARG_H

#include <algorithm>
#include "Executor.h"

class Arg
{
public:
    Arg() {};
    Arg(std::string s)
    {
        str = s;
    };
    const std::string getStr() const{
        return str;
    }
    virtual void print() {};
    virtual Redirection toRedirection() {};
    void dequota()
    {
        std::string newstr;
        for(auto i = str.begin(); i != str.end(); i++)
        {
            if(*i == 39)
            {
                continue;
            } else{
                newstr.push_back(*i);
            }
        }
        str = newstr;

    }
    void envsub()
    {
        bool isQuota = false;
        std::string envname;
        for(auto i = str.begin(); i != str.end(); i++)
        {
            if(*i == 39)
            {
                isQuota = !isQuota;
                continue;
            }
            if(isQuota)
            {
                continue;
            }else{
                if(*i == '$')
                {
                    i++;
                    auto b = i;
                    while(isalnum(*i))
                    {
                        envname.push_back(*i);
                        i++;
                    }
                    auto e = i;
                    str.erase(b,e);
                    auto j = str.begin();
                    while(*j != '$') j++;
                    auto name = getenv(envname.c_str());
                    if(name == nullptr)
                    {
                        std::cout << "Uzyto nieistniejacej zmiennej srodowiskowej\n";
                        throw SemanticException();
                    }
                    str.replace(j,j++,name);
                    j = str.begin();
                    while(*j != '$') j++;
                    str.erase(j);
                    i = j;
                }
            }
        }
    }
protected:
    std::string str;
};

class RedirectArg : public Arg
{
public:
    RedirectArg() {};
    RedirectArg(int n, std::string s) {};
    virtual void print() {};
protected:
    int num;
};

class OrderArg : public Arg
{
public:
    OrderArg(std::string s)
    {
        str = s;
    };
    virtual void print()
    {
        std::cout << "Wykonuje polecenie : " << str << std::endl;
    };
};

class SimpleArg : public Arg
{
public:
    SimpleArg(std::string s)
    {
        str = s;
    };
    virtual void print()
    {
        std::cout << "Argument : " << str << std::endl;
    };
};

class InRedirectArg : public RedirectArg
{
public:
    InRedirectArg(int n, std::string s)
    {
        str = s;
        num = n;
    }
    virtual void print()
    {
        std::cout << "Przekierowanie wejścia : numer " << num << " do pliku " << str << std::endl;
    };

    Redirection toRedirection() {
        return Redirection(str, num, REDIR_IN);
    }
};

class OutRedirectArg : public RedirectArg
{
public:
    OutRedirectArg(int n, std::string s)
    {
        str = s;
        num = n;
    }
    virtual void print()
    {
        std::cout << "Przekierowanie wyjścia : numer " << num << " do pliku " << str << std::endl;
    };

    Redirection toRedirection() {
        return Redirection(str, num, REDIR_OUT);
    }
};
#endif //UXP1A_ARG_H
