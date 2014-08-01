/*
 * Voxels is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Voxels is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Voxels; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */
#if 1
#warning finish main.cpp
#include "platform/audio.h"
#include "platform/platform.h"
#include "render/render_object.h"
#include "networking/server.h"
#include "render/generate.h"
#include <iostream>
#include <thread>
#include "util/linked_map.h"
using namespace std;

shared_ptr<stream::Reader> worldReader()
{
    stream::StreamBidirectionalPipe pipe;
    shared_ptr<stream::StreamServer> streamServer = make_shared<stream::StreamServerWrapper>(vector<shared_ptr<stream::StreamRW>>{pipe.pport2()});
    thread(runServer, streamServer).detach();
    return pipe.port1().preader();
}

inline ColorF lightVertex(ColorF color, VectorF position, VectorF normal)
{
    float scale = dot(normal, VectorF(0, 1, 0)) * 0.3 + 0.4;
    return scaleF(scale, color);
}

int main()
{
    Audio audio(L"background13.ogg", true);
    shared_ptr<RenderObjectWorld> world;
    {
        VariableSet variableSet;
        world = stream::read<RenderObjectWorld>(*worldReader(), variableSet);
    }
    cout << "Read World" << endl;
    PositionF position = PositionF(0.5, 64 + 10.5, 0.5, Dimension::Overworld);
    startGraphics();
    shared_ptr<PlayingAudio> playingAudio = audio.play(0.5f, true);
    Renderer r;
    enum_array<linked_map<PositionI, size_t>, RenderLayer> triangleCounts;
    while(true)
    {
        Display::clear();
        Matrix tform = Matrix::rotateY(Display::timer() / 5 * M_PI).concat(Matrix::translate((VectorF)position));
        size_t triangleCount = 0;
        for(RenderLayer renderLayer : enum_traits<RenderLayer>())
        {
            r << renderLayer;
            world->draw(r, inverse(tform), renderLayer, (PositionI)position, 64, [&](Mesh m, PositionI chunkBasePosition)->Mesh
            {
                triangleCounts[renderLayer][chunkBasePosition] = m.size();
                return lightMesh(m, lightVertex);
            }, false);
            for(pair<PositionI, size_t> e : triangleCounts[renderLayer])
            {
                triangleCount += std::get<1>(e);
            }
        }
        Display::flip(60);
        Display::handleEvents(nullptr);
        cout << "FPS: " << Display::averageFPS() << "    Triangle Count : " << triangleCount << "\x1b[K\r" << flush;
    }
}
#else
#include "client.h"
#include "server.h"
#include "stream/stream.h"
#include "stream/network.h"
#include "util/util.h"
#include "util/game_version.h"
#include <thread>
#include <vector>
#include <iostream>

using namespace std;

namespace
{
void serverThreadFn(shared_ptr<StreamServer> server)
{
    runServer(*server);
}

bool isQuiet = false;

void outputVersion()
{
    if(isQuiet)
        return;
    static bool didOutputVersion = false;
    if(didOutputVersion)
        return;
    didOutputVersion = true;
    cout << "voxels " << wcsrtombs(GameVersion::VERSION) << "\n";
}

void help()
{
    isQuiet = false;
    outputVersion();
    cout << "usage : voxels [-h | --help] [-q | --quiet] [--server] [--client <server url>]\n";
}

int error(wstring msg)
{
    outputVersion();
    cout << "error : " << wcsrtombs(msg) << "\n";
    if(!isQuiet)
        help();
    return 1;
}
}

int myMain(vector<wstring> args)
{
    args.erase(args.begin());
    bool isServer = false, isClient = false;
    wstring clientAddr;
    for(auto i = args.begin(); i != args.end(); i++)
    {
        wstring arg = *i;
        if(arg == L"--help" || arg == L"-h")
        {
            help();
            return 0;
        }
        else if(arg == L"-q" || arg == L"--quiet")
        {
            isQuiet = true;
        }
        else if(arg == L"--server")
        {
            if(isServer)
                return error(L"can't specify two server flags");
            if(isClient)
                return error(L"can't specify both server and client");
            isServer = true;
        }
        else if(arg == L"--client")
        {
            if(isServer)
                return error(L"can't specify both server and client");
            if(isClient)
                return error(L"can't specify two client flags");
            isClient = true;
            i++;
            if(i == args.end())
                return error(L"--client missing server url");
            arg = *i;
            clientAddr = arg;
        }
        else
            return error(L"unrecognized argument : " + arg);
    }
    thread serverThread;
    try
    {
        if(isServer)
        {
            cout << "Voxels " << wcsrtombs(GameVersion::VERSION) << " (c) 2014 Jacob R. Lifshay" << endl;
            NetworkServer server(GameVersion::port);
            cout << "Connected to port " << GameVersion::port << endl;
            runServer(server);
            return 0;
        }
        if(isClient)
        {
            NetworkConnection connection(clientAddr, GameVersion::port);
            clientProcess(connection);
            return 0;
        }
        StreamBidirectionalPipe pipe;
        shared_ptr<NetworkServer> server = nullptr;
        try
        {
            server = make_shared<NetworkServer>(GameVersion::port);
            cout << "Connected to port " << GameVersion::port << endl;
        }
        catch(IOException & e)
        {
            cout << e.what() << endl;
        }
        serverThread = thread(serverThreadFn, shared_ptr<StreamServer>(new StreamServerWrapper(list<shared_ptr<StreamRW>>{pipe.pport1()}, server)));
        clientProcess(pipe.port2());
    }
    catch(exception & e)
    {
        return error(mbsrtowcs(e.what()));
    }
    serverThread.join();
    return 0;
}

int main(int argc, char ** argv)
{
    vector<wstring> args;
    args.resize(argc);
    for(int i = 0; i < argc; i++)
    {
        args[i] = mbsrtowcs(argv[i]);
    }
    return myMain(args);
}
#endif
