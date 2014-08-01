#include "networking/server.h"
#include "stream/network_event.h"
#include "util/variable_set.h"
#include "render/render_object.h"
#include "util/flag.h"
#include "texture/texture_atlas.h"
#include "render/generate.h"
#include <thread>
#include <cmath>

using namespace std;

namespace
{
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

shared_ptr<RenderObjectBlockDescriptor> getDirt()
{
    static shared_ptr<RenderObjectBlockDescriptor> retval;
    if(retval == nullptr)
        retval = makeBlockDescriptor(RenderLayer::Opaque, 0, true, TextureAtlas::Dirt.td(), TextureAtlas::Dirt.td(), TextureAtlas::Dirt.td(), TextureAtlas::Dirt.td(), TextureAtlas::Dirt.td(), TextureAtlas::Dirt.td());
    return retval;
}

shared_ptr<RenderObjectBlockDescriptor> getGlass()
{
    static shared_ptr<RenderObjectBlockDescriptor> retval;
    if(retval == nullptr)
        retval = makeBlockDescriptor(RenderLayer::Opaque, 1, false, TextureAtlas::Glass.td(), TextureAtlas::Glass.td(), TextureAtlas::Glass.td(), TextureAtlas::Glass.td(), TextureAtlas::Glass.td(), TextureAtlas::Glass.td());
    return retval;
}

shared_ptr<RenderObjectBlockDescriptor> getAir()
{
    static shared_ptr<RenderObjectBlockDescriptor> retval;
    if(retval == nullptr)
        retval = makeBlockDescriptor(RenderLayer::Opaque, 2, false, TextureDescriptor(), TextureDescriptor(), TextureDescriptor(), TextureDescriptor(), TextureDescriptor(), TextureDescriptor());
    return retval;
}

shared_ptr<RenderObjectBlockDescriptor> getStone()
{
    static shared_ptr<RenderObjectBlockDescriptor> retval;
    if(retval == nullptr)
        retval = makeBlockDescriptor(RenderLayer::Opaque, 0, true, TextureAtlas::Stone.td(), TextureAtlas::Stone.td(), TextureAtlas::Stone.td(), TextureAtlas::Stone.td(), TextureAtlas::Stone.td(), TextureAtlas::Stone.td());
    return retval;
}

class Server
{
    shared_ptr<stream::StreamServer> streamServer;
    shared_ptr<RenderObjectWorld> world;
    atomic_uint connectionCount;
    flag anyConnections, running;
    struct Connection
    {
        atomic_uint &connectionCount;
        flag &anyConnections;
        VariableSet variableSet;
        Connection(atomic_uint &connectionCount, flag &anyConnections)
            : connectionCount(connectionCount), anyConnections(anyConnections)
        {
            connectionCount++;
            anyConnections = true;
        }
        ~Connection()
        {
            if(--connectionCount <= 0)
                anyConnections = false;
        }
    };
    void reader(shared_ptr<Connection> pconnection, shared_ptr<stream::Reader> preader)
    {

    }
    void writer(shared_ptr<Connection> pconnection, shared_ptr<stream::Writer> pwriter)
    {
        VariableSet &variableSet = pconnection->variableSet;
        stream::write<RenderObjectWorld>(*pwriter, variableSet, world);
    }
    void startConnection(shared_ptr<stream::StreamRW> streamRW)
    {
        shared_ptr<Connection> pconnection = shared_ptr<Connection>(new Connection(connectionCount, anyConnections));
        thread(&Server::reader, this, pconnection, streamRW->preader()).detach();
        thread(&Server::writer, this, pconnection, streamRW->pwriter()).detach();
    }
    void generateWorld()
    {
        for(int32_t x = -64; x < 64; x++)
        {
            for(int32_t z = -64; z < 64; z++)
            {
                int32_t landHeight = (int32_t)(64 + 16 * (sin((float)x / 3) * sin((float)z / 3)));
                for(int32_t y = 0; y < 256; y++)
                {
                    if(y < landHeight)
                    {
                        world->setBlock(PositionI(x, y, z, Dimension::Overworld), getStone());
                    }
                    else if(y <= landHeight)
                        world->setBlock(PositionI(x, y, z, Dimension::Overworld), getDirt());
                    else if(y == landHeight + 1)
                        world->setBlock(PositionI(x, y, z, Dimension::Overworld), getGlass());
                    else
                        world->setBlock(PositionI(x, y, z, Dimension::Overworld), getAir());
                }
            }
        }
    }
    flag starting;
    void simulate()
    {
        generateWorld();
        starting = false;
        running.wait(false);
    }
public:
    Server(shared_ptr<stream::StreamServer> streamServer)
        : streamServer(streamServer), world(make_shared<RenderObjectWorld>())
    {
    }
    void run()
    {
        running = true;
        starting = true;
        thread(&Server::simulate, this).detach();
        starting.wait(false);
        while(true)
        {
            try
            {
                startConnection(streamServer->accept());
            }
            catch(stream::NoStreamsLeftException &)
            {
                break;
            }
        }
        anyConnections.wait(false);
        running = false;
    }
};
}

void runServer(shared_ptr<stream::StreamServer> streamServer)
{
    Server(streamServer).run();
}
