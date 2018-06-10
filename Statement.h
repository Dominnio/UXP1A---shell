#ifndef STATEMENT_H
#define STATEMENT_H

#include "Exception.h"
#include "Arg.h"
#include "Executor.h"

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

    Command toCommand()
    {
        Command cmd;
        cmd.app = order->getStr();
        for(auto iter = argList.begin(); iter != argList.end(); iter++)
        {
            cmd.params.push_back((*iter)->getStr());
        }
        for(auto iter = redirectList.begin(); iter != redirectList.end(); iter++)
        {
            cmd.redirections.emplace_back((*iter)->toRedirection());
        }
        cmd.foreground = !isBackground;
        return cmd;
    };

protected:
    Arg*                order;
    std::list<Arg*>     argList;
    std::list<Arg*>     redirectList;
    bool                isBackground;
};

#endif // STATEMENT_H