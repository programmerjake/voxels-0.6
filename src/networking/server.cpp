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
    static PositionF initialPositionF()
    {
        return PositionF(0.5, 64 + 10 + 0.5, 0.5, Dimension::Overworld);
    }
    static PositionI initialPositionI()
    {
        return (PositionI)initialPositionF();
    }
    static int32_t generateDistance()
    {
        return 50;
    }
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
        pwriter->flush();
    }
    void startConnection(shared_ptr<stream::StreamRW> streamRW)
    {
        shared_ptr<Connection> pconnection = shared_ptr<Connection>(new Connection(connectionCount, anyConnections));
        thread(&Server::reader, this, pconnection, streamRW->preader()).detach();
        thread(&Server::writer, this, pconnection, streamRW->pwriter()).detach();
    }
    void generateChunk(PositionI chunkPosition)
    {
        for(int32_t x = chunkPosition.x; x < chunkPosition.x + RenderObjectChunk::BlockChunkType::chunkSizeX; x++)
        {
            for(int32_t z = chunkPosition.z; z < chunkPosition.z + RenderObjectChunk::BlockChunkType::chunkSizeZ; z++)
            {
                int32_t landHeight = (int32_t)(64 + 16 * (sin((float)x / 3) * sin((float)z / 3)));
                for(int32_t y = chunkPosition.y; y < chunkPosition.y + RenderObjectChunk::BlockChunkType::chunkSizeY; y++)
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
    void generateWorld()
    {
        PositionI minPosition = RenderObjectChunk::BlockChunkType::getChunkBasePosition(initialPositionI() - VectorI(generateDistance()));
        PositionI maxPosition = RenderObjectChunk::BlockChunkType::getChunkBasePosition(initialPositionI() + VectorI(generateDistance()));
        for(PositionI chunkPosition = minPosition; chunkPosition.x <= maxPosition.x; chunkPosition.x += RenderObjectChunk::BlockChunkType::chunkSizeX)
        {
            for(chunkPosition.y = minPosition.y; chunkPosition.y <= maxPosition.y; chunkPosition.y += RenderObjectChunk::BlockChunkType::chunkSizeY)
            {
                for(chunkPosition.z = minPosition.z; chunkPosition.z <= maxPosition.z; chunkPosition.z += RenderObjectChunk::BlockChunkType::chunkSizeZ)
                {
                    float progress = (float)(chunkPosition.z - minPosition.z) / (maxPosition.z - minPosition.z + RenderObjectChunk::BlockChunkType::chunkSizeZ);
                    progress = (chunkPosition.y - minPosition.y + progress * RenderObjectChunk::BlockChunkType::chunkSizeY) / (maxPosition.y - minPosition.y + RenderObjectChunk::BlockChunkType::chunkSizeY);
                    progress = (chunkPosition.x - minPosition.x + progress * RenderObjectChunk::BlockChunkType::chunkSizeX) / (maxPosition.x - minPosition.x + RenderObjectChunk::BlockChunkType::chunkSizeX);
                    cout << "Generating World ... (" << (int)(100 * progress) << "%)\x1b[K\r" << flush;
                    generateChunk(chunkPosition);
                }
            }
        }
        cout << "Generating World ... Done." << endl;
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
