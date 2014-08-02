#ifndef CLIENT_H_INCLUDED
#define CLIENT_H_INCLUDED

#include "stream/stream.h"

void runClient(shared_ptr<stream::StreamRW> streamRW);

#endif // CLIENT_H_INCLUDED
