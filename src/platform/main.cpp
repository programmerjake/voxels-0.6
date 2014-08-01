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
#include "texture/texture_atlas.h"
#include "render/generate.h"
#include <iostream>
#include <thread>
using namespace std;

shared_ptr<RenderObjectBlockDescriptor> makeBlockDescriptor(RenderLayer renderLayer, BlockDrawClass blockDrawClass, bool isSolid, TextureDescriptor nx, TextureDescriptor px, TextureDescriptor ny, TextureDescriptor py, TextureDescriptor nz, TextureDescriptor pz)
{
    shared_ptr<RenderObjectBlockDescriptor> retval = make_shared<RenderObjectBlockDescriptor>();
    retval->center = make_shared<Mesh>();
    retval->faceMesh[BlockFace::NX] = make_shared<Mesh>(Generate::unitBox(nx, TextureDescriptor(), TextureDescriptor(), TextureDescriptor(), TextureDescriptor(), TextureDescriptor()));
    retval->faceMesh[BlockFace::PX] = make_shared<Mesh>(Generate::unitBox(TextureDescriptor(), px, TextureDescriptor(), TextureDescriptor(), TextureDescriptor(), TextureDescriptor()));
    retval->faceMesh[BlockFace::NY] = make_shared<Mesh>(Generate::unitBox(TextureDescriptor(), TextureDescriptor(), ny, TextureDescriptor(), TextureDescriptor(), TextureDescriptor()));
    retval->faceMesh[BlockFace::PY] = make_shared<Mesh>(Generate::unitBox(TextureDescriptor(), TextureDescriptor(), TextureDescriptor(), py, TextureDescriptor(), TextureDescriptor()));
    retval->faceMesh[BlockFace::NZ] = make_shared<Mesh>(Generate::unitBox(TextureDescriptor(), TextureDescriptor(), TextureDescriptor(), TextureDescriptor(), nz, TextureDescriptor()));
    retval->faceMesh[BlockFace::PZ] = make_shared<Mesh>(Generate::unitBox(TextureDescriptor(), TextureDescriptor(), TextureDescriptor(), TextureDescriptor(), TextureDescriptor(), pz));
    for(BlockFace face : enum_traits<BlockFace>())
    {
        retval->faceBlocked[face] = isSolid;
    }
    retval->blockDrawClass = blockDrawClass;
    retval->renderLayer = renderLayer;
    return retval;
}

shared_ptr<RenderObjectWorld> makeWorld()
{
    shared_ptr<RenderObjectWorld> world = make_shared<RenderObjectWorld>();
    shared_ptr<RenderObjectBlockDescriptor> airDescriptor = makeBlockDescriptor(RenderLayer::Opaque, 0, false, TextureDescriptor(), TextureDescriptor(), TextureDescriptor(), TextureDescriptor(), TextureDescriptor(), TextureDescriptor());
    shared_ptr<RenderObjectBlockDescriptor> glassDescriptor = makeBlockDescriptor(RenderLayer::Opaque, 1, false, TextureAtlas::Glass.td(), TextureAtlas::Glass.td(), TextureAtlas::Glass.td(), TextureAtlas::Glass.td(), TextureAtlas::Glass.td(), TextureAtlas::Glass.td());
    shared_ptr<RenderObjectBlockDescriptor> oakWoodDescriptor = makeBlockDescriptor(RenderLayer::Opaque, 2, true, TextureAtlas::OakWood.td(), TextureAtlas::OakWood.td(), TextureAtlas::WoodEnd.td(), TextureAtlas::WoodEnd.td(), TextureAtlas::OakWood.td(), TextureAtlas::OakWood.td());
    for(int32_t x = -100; x < 100; x++)
    {
        for(int32_t y = -100; y < 256 - 100; y++)
        {
            for(int32_t z = -100; z < 100; z++)
            {
                world->setBlock(PositionI(x, y + 100, z, Dimension::Overworld), RenderObjectBlock(airDescriptor));
                if(x * x + y * y + z * z > 25)
                    world->setBlock(PositionI(x, y + 100, z, Dimension::Overworld), RenderObjectBlock(x * y * z <= 0 ? glassDescriptor : oakWoodDescriptor));
            }
        }
    }
    return world;
}

shared_ptr<stream::Reader> worldReader()
{
    stream::StreamPipe pipe;
    thread([](shared_ptr<stream::Writer> pwriter)
    {
        stream::Writer &writer = *pwriter;
        VariableSet variableSet;
        stream::write<RenderObjectWorld>(writer, variableSet, makeWorld());
    }, pipe.pwriter()).detach();
    return pipe.preader();
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
    PositionF position = PositionF(0.5, 100.5, 0.5, Dimension::Overworld);
    startGraphics();
    shared_ptr<PlayingAudio> playingAudio = audio.play(0.5f, true);
    Renderer r;
    while(true)
    {
        Display::clear();
        Matrix tform = Matrix::rotateY(Display::timer() / 5 * M_PI).concat(Matrix::translate((VectorF)position));
        size_t triangleCount = 0;
        for(RenderLayer renderLayer : enum_traits<RenderLayer>())
        {
            r << renderLayer;
            world->draw(r, inverse(tform), renderLayer, (PositionI)position, 32);
        }
        Display::flip(60);
        Display::handleEvents(nullptr);
        cout << "FPS: " << Display::averageFPS() << "\x1b[K\r";
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
