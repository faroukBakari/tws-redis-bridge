#ifndef IERRORHANDLER_H
#define IERRORHANDLER_H

#include <string>
#include <ctime>

class IErrorHandler
{
public:
    virtual ~IErrorHandler() {}
    
    virtual void error(int id, time_t errorTime, int errorCode, const std::string &errorString, const std::string &advancedOrderRejectJson) = 0;
};

#endif // IERRORHANDLER_H
