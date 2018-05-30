#ifndef STATEMENT_H
#define STATEMENT_H

#include "Exception.h"
#include "Arg.h"

enum SymType { cdsy, lssy, exportsy, pwdsy, exitsy, fgsy, bgsy, identsy, EOLsy, STARTsy, othersy };

std::map<std::string,SymType> KeyWordMap
    {
        std::make_pair("cd", cdsy),
        std::make_pair("ls", lssy),
        std::make_pair("export", exportsy),
        std::make_pair("pwd", pwdsy),
        std::make_pair("exit", exitsy),
        std::make_pair("fg", fgsy),
        std::make_pair("bg", bgsy)
    };

std::map<std::string,SymType> KeySignMap {};

class Statement
{
public:
    Statement() {};
    Statement(Arg* _order, std::list<Arg*> _argList, std::list<Arg*> _redirectList, bool _isAmpersand)
    {
        order = _order;
        argList = _argList;
        redirectList =_redirectList;
        isBackground = _isAmpersand;
    }
    virtual ~Statement() {}
    virtual void execute()
    {
        order->print();
        for(auto iter = argList.begin(); iter != argList.end(); iter++)
        {
            (*iter)->print();
        }
        for(auto iter = redirectList.begin(); iter != redirectList.end(); iter++)
        {
            (*iter)->print();
        }
        if(isBackground)
        {
            std::cout << "Proces ma sie wykonać w tle";
        }

    };
protected:
    Arg*                order;
    std::list<Arg*>     argList;
    std::list<Arg*>     redirectList;
    bool                isBackground;
};


#endif // STATEMENT_H
