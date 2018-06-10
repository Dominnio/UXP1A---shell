#ifndef PARSER_H
#define PARSER_H

#include "Statement.h"
#include "Arg.h"

const int MAXIDENTSIZE = 100;
const int MAXLINESIZE = 500;

class Parser
{

    friend class ExportStatement;

public :
    char nextChar()
    {
        return *(activeChar++);
    }

    char prevChar()
    {
        return *(activeChar--);
    }

    void skipWhiteSpace()
    {
        while(isspace((int)(*activeChar)))
        {
            if(activeChar == input.end())
                break;
            nextChar();
        }
    }

    void accept(char c)
    {
        if(*activeChar == c)
            nextChar();
        else
            throw SemanticException();
    }

    void parse(std::string _input)
    {
        input = _input;
        activeChar = input.begin();
        if(input.length() > MAXLINESIZE)
        {
            std::cout << "Polecenie jest za dlugie" << std::endl  << std::flush;
            throw SemanticException();
        }
        while(*activeChar != '\0' && activeChar != input.end())
        {
            statementList.push_back(parseStatement());
            if(*activeChar == '|')
            {
                nextChar();
            }
        }
    }

    Statement* parseStatement()
    {
        std::list<Arg*> args;
        while(*activeChar != '\0' && activeChar != input.end() && *activeChar != '|')
        {
            auto arg = parseArg();
            if(arg != nullptr)
                args.push_back(arg);
        }
        Arg* order;
        std::list<Arg*> argList;
        std::list<Arg*> redirectList;
        bool isAmpersand = false;

        if((*(--args.end()))->getStr() == "&")
        {
            isAmpersand = true;
            args.pop_back();
        }
        for(auto i = args.begin(); i != args.end(); )
        {
            std::string num;
            bool isNumber = false;
            auto j = (*i)->getStr().begin();
            while(isdigit(*j))
            {
                isNumber = true;
                num.push_back(*j);
                j++;
            }
            if(*j == '>' || *j == '<')
            {
                if(!isNumber)
                {
                    if(*j == '>')
                        num.push_back('1');
                    else
                        num.push_back('0');
                }
                auto remember = j;
                auto todelete = i;
                ++j;
                if((j) == (*i)->getStr().end() || *j == '\0')
                {
                    if(++i != args.end())
                    {
                        (*i)->envsub();
                        (*i)->dequota();
                        if(*remember == '>')
                            redirectList.push_back(new OutRedirectArg(atoi(num.c_str()),(*i)->getStr()));
                        if(*remember == '<')
                            redirectList.push_back(new InRedirectArg(atoi(num.c_str()),(*i)->getStr()));
                        args.erase(todelete);
                        args.erase(i);
                        i = args.begin();
                    }else{
                        throw SemanticException();
                    }
                }else{
                    std::string filename;
                    while(j != ((*i)->getStr()).end() && *j != '\0')
                    {
                        filename.push_back(*j);
                        j++;
                    }
                    (*i)->envsub();
                    (*i)->dequota();
                    if(*remember == '>')
                        redirectList.push_back(new OutRedirectArg(atoi(num.c_str()),filename));
                    if(*remember == '<')
                        redirectList.push_back(new InRedirectArg(atoi(num.c_str()),filename));
                    args.erase(i);
                    i = args.begin();
                }
            }else{
                i++;
                continue;
            }
        }
        for(auto i = args.begin(); i != args.end(); i++)
        {
            (*i)->envsub();
            (*i)->dequota();
        }
        auto i = args.begin();
        if(*i == NULL)
        {
            throw SemanticException();
        }
        order = new OrderArg((*i)->getStr());
        i++;
        while(i != args.end())
        {
            argList.push_back(new SimpleArg((*i)->getStr()));
            i++;
        }
        return new Statement(order, argList, redirectList, isAmpersand);
    }

    Arg* parseArg()
    {
        skipWhiteSpace();
        bool isQuota = false;
        std::string spellArg;
        while(((!(isspace((int)(*activeChar))) && *activeChar != '|') || isQuota) && *activeChar != '\0' && activeChar != input.end())
        {
            if(*activeChar == 39)
            {
                isQuota = !isQuota;
                spellArg.push_back(nextChar());
                continue;
            }
            if(isQuota)
            {
                spellArg.push_back(nextChar());
                continue;
            }
            spellArg.push_back(nextChar());
        }
        if(spellArg.empty())
        {
            return nullptr;
        }
        return new Arg(spellArg);
    }

    void print()
    {
        for(auto iter = statementList.begin(); iter != statementList.end(); iter++)
        {
            (*iter)->execute();
        }
//        statementList.clear();
    }

    vector<Command> getCommandList()
    {
        vector<Command> res;
        for(auto iter = statementList.begin(); iter != statementList.end(); iter++)
        {
            res.emplace_back((*iter)->toCommand());
        }
        statementList.clear();
        return res;
    }

private:
    std::string                         input;
    std::string::iterator               activeChar;
    std::list<Statement*>               statementList;
    std::map<std::string,std::string>   enviroment;
};



#endif // PARSER_H
