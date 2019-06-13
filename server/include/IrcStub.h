//
// Created by marlon on 6/13/19.
//

#ifndef LISTSERVER_IRCSTUB_H
#define LISTSERVER_IRCSTUB_H

//! Base class that handles socket functions.
//! Derive from this class and define the functions.
//! Then, pass the class to CSocketManager::registerSocket().
class IrcStub
{
public:
    virtual bool sendMessage(const std::string& channel, const std::string& from, const std::string& message) = 0;
};
#endif //LISTSERVER_IRCSTUB_H
