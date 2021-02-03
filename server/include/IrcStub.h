#ifndef LISTSERVER_IRCSTUB_H
#define LISTSERVER_IRCSTUB_H
#include "ServerPlayer.h"
class ServerPlayer;

class IrcStub
{
public:
    virtual bool sendMessage(const std::string& channel, ServerPlayer *from, const std::string& message) = 0;
};
#endif //LISTSERVER_IRCSTUB_H
