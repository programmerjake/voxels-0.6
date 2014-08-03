#include "networking/server.h"
#include "stream/network_event.h"
#include "util/variable_set.h"
#include "render/render_object.h"
#include "util/flag.h"
#include "texture/texture_atlas.h"
#include "render/generate.h"
#include <thread>
#include <cmath>
#include <mutex>
#include <condition_variable>
#include "util/unlock_guard.h"

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
        return 16;
    }
    struct Connection
    {
        atomic_uint &connectionCount;
        flag &anyConnections;
        VariableSet variableSet;
        PositionF viewPosition;
        atomic_bool hasViewPosition;
        atomic_bool done;
        atomic_bool needKeepalive;
        unordered_set<PositionI> requestedChunks;
        mutex requestedChunksLock;
        mutex eventWaitMutex;
        condition_variable_any eventWaitCond;
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
    list<weak_ptr<Connection>> connectionsList;
    mutex connectionsListLock;
    void reader(shared_ptr<Connection> pconnection, shared_ptr<stream::Reader> preader)
    {
        Connection &connection = *pconnection;
        connection.viewPosition = PositionF(0.5, 0.5 + 64 + 10, 0.5, Dimension::Overworld);
        connection.hasViewPosition = true;
        NetworkEvent event;
        while(running && !connection.done)
        {
            try
            {
                event = stream::read<NetworkEvent>(*preader);
                switch(event.type)
                {
                case NetworkEventType::Keepalive:
                    connection.needKeepalive = true;
                    connection.eventWaitCond.notify_all();
                    break;
                case NetworkEventType::SendNewChunk:
                    break;
                case NetworkEventType::SendBlockUpdate:
                    break;
                case NetworkEventType::RequestChunk:
                {
                    shared_ptr<stream::Reader> pEventReader = event.getReader();
                    PositionI chunkPosition = stream::read<PositionI>(*pEventReader);
                    if(chunkPosition != RenderObjectChunk::BlockChunkType::getChunkBasePosition(chunkPosition))
                        break;
                    lock_guard<mutex> lockIt(connection.requestedChunksLock);
                    connection.requestedChunks.insert(chunkPosition);
                    connection.eventWaitCond.notify_all();
                    break;
                }
                }
            }
            catch(stream::IOException &e)
            {
                cerr << "IO Error : " << e.what() << endl;
                connection.done = true;
            }
        }
        connection.eventWaitCond.notify_all();
        connection.done = true;
    }
    bool writeRequestedChunks(Connection &connection, stream::Writer &writer)
    {
        lock_guard<mutex> lockIt(connection.requestedChunksLock);
        vector<PositionI> requestedChunks;
        requestedChunks.reserve(connection.requestedChunks.size());
        for(PositionI pos : connection.requestedChunks)
        {
            if(!queueGenerateChunk(pos)) // then the chunk exists
            {
                requestedChunks.push_back(pos);
            }
        }
        if(requestedChunks.empty())
            return false;
        PositionF playerPos = connection.viewPosition;
        std::sort(requestedChunks.begin(), requestedChunks.end(), [&](PositionI a, PositionI b)
        {
            return chunkDistanceMetric(a, playerPos) < chunkDistanceMetric(b, playerPos);
        });
        stream::MemoryWriter chunkDataWriter;
        stream::write<RenderObjectChunk>(chunkDataWriter, connection.variableSet, world->getChunk(requestedChunks.front()));
        NetworkEvent event(NetworkEventType::SendNewChunk, std::move(chunkDataWriter));
        stream::write<NetworkEvent>(writer, std::move(event));
        writer.flush();
        return true;
    }
    void writer(shared_ptr<Connection> pconnection, shared_ptr<stream::Writer> pwriter)
    {
        VariableSet &variableSet = pconnection->variableSet;
        Connection &connection = *pconnection;
        try
        {
            stream::write<RenderObjectWorld>(*pwriter, variableSet, world);
            pwriter->flush();
            while(running && !connection.done)
            {
                if(connection.needKeepalive.exchange(false))
                {
                    NetworkEvent event(NetworkEventType::Keepalive);
                    stream::write<NetworkEvent>(*pwriter, event);
                    pwriter->flush();
                }
                else if(writeRequestedChunks(connection, *pwriter))
                {
                }
                else
                {
                    lock_guard<mutex> lockIt(connection.eventWaitMutex);
                    connection.eventWaitCond.wait(connection.eventWaitMutex);
                }
            }
        }
        catch(stream::IOException &e)
        {
            cerr << "IO Error : " << e.what() << endl;
            connection.done = true;
        }
        connection.eventWaitCond.notify_all();
        connection.done = true;
    }
    void startConnection(shared_ptr<stream::StreamRW> streamRW)
    {
        shared_ptr<Connection> pconnection = shared_ptr<Connection>(new Connection(connectionCount, anyConnections));
        {
            lock_guard<mutex> lockIt(connectionsListLock);
            connectionsList.push_back(pconnection);
        }
        thread(&Server::reader, this, pconnection, streamRW->preader()).detach();
        thread(&Server::writer, this, pconnection, streamRW->pwriter()).detach();
    }
    void generateChunk(PositionI chunkPosition)
    {
        {
            RenderObjectChunk::BlockChunkType blockChunk(chunkPosition);
            for(int32_t x = chunkPosition.x; x < chunkPosition.x + RenderObjectChunk::BlockChunkType::chunkSizeX; x++)
            {
                for(int32_t z = chunkPosition.z; z < chunkPosition.z + RenderObjectChunk::BlockChunkType::chunkSizeZ; z++)
                {
                    int32_t landHeight = (int32_t)(64 + 16 * (sin((float)x / 3) * sin((float)z / 3)));
                    for(int32_t y = chunkPosition.y; y < chunkPosition.y + RenderObjectChunk::BlockChunkType::chunkSizeY; y++)
                    {
                        if(y < landHeight)
                        {
                            blockChunk.blocks[x - chunkPosition.x][y - chunkPosition.y][z - chunkPosition.z] = getStone();
                        }
                        else if(y <= landHeight)
                        {
                            blockChunk.blocks[x - chunkPosition.x][y - chunkPosition.y][z - chunkPosition.z] = getDirt();
                        }
                        else if(y == landHeight + 1)
                        {
                            blockChunk.blocks[x - chunkPosition.x][y - chunkPosition.y][z - chunkPosition.z] = getGlass();
                        }
                        else
                        {
                            blockChunk.blocks[x - chunkPosition.x][y - chunkPosition.y][z - chunkPosition.z] = getAir();
                        }
                    }
                }
            }
            shared_ptr<RenderObjectChunk> chunk = make_shared<RenderObjectChunk>(blockChunk);
            world->setChunk(chunk);
        }
        lock_guard<mutex> lockIt(generateChunksLock);
        generatedChunks.insert(chunkPosition);
        cout << "Generated : " << (VectorI)chunkPosition << endl;
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
    static float chunkDistanceMetric(PositionI chunkPos, PositionF playerPos)
    {
        return (chunkPos.d != playerPos.d ? 16 : 1) * absSquared((VectorI)chunkPos - 0.5 * VectorF(RenderObjectChunk::BlockChunkType::chunkSizeX, RenderObjectChunk::BlockChunkType::chunkSizeY, RenderObjectChunk::BlockChunkType::chunkSizeZ) - (VectorF)playerPos);
    }
    flag starting;
    void simulate()
    {
        generateWorld();
        starting = false;
        running.wait(false);
    }
    unordered_set<PositionI> needGenerateChunks;
    unordered_set<PositionI> generatingChunks;
    unordered_set<PositionI> generatedChunks;
    mutex generateChunksLock;
    condition_variable_any generateChunksCond;
    bool queueGenerateChunk(PositionI chunkPosition) // returns if the chunk is not generated
    {
        lock_guard<mutex> lockIt(generateChunksLock);
        if(generatedChunks.find(chunkPosition) != generatedChunks.end())
            return false;
        if(generatingChunks.find(chunkPosition) != generatingChunks.end())
            return true;
        bool sizeWasZero = needGenerateChunks.empty();
        needGenerateChunks.insert(chunkPosition);
        if(sizeWasZero)
            generateChunksCond.notify_all();
        return true;
    }
    void chunkGenerator()
    {
        vector<pair<PositionI, float>> chunksList;
        vector<PositionF> playerPositions;
        starting.wait(false);
        lock_guard<mutex> lockIt(generateChunksLock);
        while(running)
        {
            while(running && needGenerateChunks.empty())
            {
                generateChunksCond.wait(generateChunksLock);
            }
            if(!running)
                break;
            playerPositions.clear();
            {
                lock_guard<mutex> lockIt2(connectionsListLock);
                for(auto i = connectionsList.begin(); i != connectionsList.end();)
                {
                    weak_ptr<Connection> wpConnection = *i;
                    shared_ptr<Connection> pConnection = wpConnection.lock();
                    if(!pConnection)
                    {
                        i = connectionsList.erase(i);
                        continue;
                    }
                    else
                        i++;
                    if(pConnection->hasViewPosition)
                    {
                        playerPositions.push_back(pConnection->viewPosition);
                    }
                }
            }
            chunksList.clear();
            chunksList.reserve(needGenerateChunks.size());
            for(PositionI p : needGenerateChunks)
            {
                float minDistanceSquared = 1e20;
                for(PositionF playerPosition : playerPositions)
                {
                    float distanceSquared = chunkDistanceMetric(p, playerPosition);
                    if(distanceSquared < minDistanceSquared)
                        minDistanceSquared = distanceSquared;
                }
                chunksList.push_back(make_pair(p, minDistanceSquared));
            }
            sort(chunksList.begin(), chunksList.end(), [](pair<PositionI, float> a, pair<PositionI, float> b)->bool
            {
                return std::get<1>(a) < std::get<1>(b);
            });
            PositionI chunkPosition = std::get<0>(chunksList.front());
            needGenerateChunks.erase(chunkPosition);
            generatingChunks.insert(chunkPosition);
            {
                unlock_guard<mutex> unlockIt(generateChunksLock);
                generateChunk(chunkPosition);
            }
            generatingChunks.erase(chunkPosition);
        }
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
        constexpr size_t generateChunkThreadCount = 5;
        for(size_t i = 0; i < generateChunkThreadCount; i++)
            thread(&Server::chunkGenerator, this).detach();
        starting.wait(false);
        while(running)
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
        generateChunksCond.notify_all();
    }
};
}

void runServer(shared_ptr<stream::StreamServer> streamServer)
{
    Server(streamServer).run();
}
