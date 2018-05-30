#ifndef EXCEPTION_H
#define EXCEPTION_H

/*
 *  Created by Dominik on 26.05.2018.
 *
 * ================= WYJĄTKOWY OPIS =================
 *
 * W tej klasie definiujemy wyjątki które chemy rzucać podczas parsowania lub wykonania.
 *
 * Jeśli wyjątek jest jakiś niesamomity to wymaga dopisania bloku catch w Terminal.h.
 *
 * Wyjątki łapane są w Terminal::start().
 *
 * Wyjątek powinien dostarczać metodę what().
 *
 * ================= OPIS WYJĄTKÓW =================
 *
 * ExitException - wyjątek rzucany przy zamknięciu terminala (np. poprzez polecenie exit)
 *
 * ExecuteException - błędy wykonania
 *
 * SemanticException - błędy składni
 */

#include <cstdlib>
#include <exception>
#include <map>
#include <cstring>
#include <list>
#include <iostream>
#include <ctime>

class ExitException : public std::exception
{
public:
    virtual const char* what() const throw()
    {
        return "Zamkniecie terminala";
    }
};

class ExecuteException : public std::exception
{
public:
    virtual const char* what() const throw()
    {
        return "Błąd wykonania";
    }
};

class SemanticException : public std::exception
{
public:
    virtual const char* what() const throw()
    {
        return "Błąd składni";
    }
};


#endif //EXCEPTION_H
