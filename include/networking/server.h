#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

#include "stream/stream.h"

void runServer(shared_ptr<stream::StreamServer> streamServer);

#endif // SERVER_H_INCLUDED
