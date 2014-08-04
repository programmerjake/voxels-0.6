#include "networking/client.h"
#include "platform/platform.h"
#include "platform/event.h"
#include "platform/audio.h"
#include "render/render_object.h"
#include "util/variable_set.h"
#include "util/flag.h"
#include "render/generate.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include "stream/network_event.h"
#include "util/cached_variable.h"

using namespace std;

namespace
{
class Client
{
    shared_ptr<RenderObjectWorld> world;
    flag running, starting;
    shared_ptr<stream::StreamRW> streamRW;
    VariableSet variableSet;
    CachedVariable<PositionF> viewPosition = PositionF(0.5, 0.5 + 64 + 10, 0.5, Dimension::Overworld);
    float viewPhi = 0, viewTheta = 0;
    float deltaPhi = 0, deltaTheta = 0;
    atomic_bool positionChanged;
    unordered_set<PositionI> neededChunks;
    mutex neededChunksLock;
    flag somethingToWrite;
    PositionF getViewPosition() const
    {
        return viewPosition;
    }
    float getViewPhi() const
    {
        return viewPhi;
    }
    float getViewTheta() const
    {
        return viewTheta;
    }
    void setViewTheta(float v)
    {
        viewTheta = v;
    }
    void setViewPhi(float v)
    {
        viewPhi = v;
    }
    void setViewPosition(PositionF v)
    {
        if(viewPosition.read() == v)
            return;
        viewPosition = v;
        positionChanged = true;
        somethingToWrite.set();
    }
    void reader(shared_ptr<stream::Reader> preader)
    {
        try
        {
            world = stream::read<RenderObjectWorld>(*preader, variableSet);
            starting = false;
            NetworkEvent event;
            while(running)
            {
                event = stream::read<NetworkEvent>(*preader);
                switch(event.type)
                {
                case NetworkEventType::Keepalive:
                    break;
                case NetworkEventType::SendNewChunk:
                {
                    shared_ptr<RenderObjectChunk> chunk = stream::read<RenderObjectChunk>(*event.getReader(), variableSet);
                    if(!chunk)
                        break;
                    world->setChunk(chunk);
                    lock_guard<mutex> lockIt(neededChunksLock);
                    neededChunks.erase(chunk->blockChunk.basePosition);
                    break;
                }
                case NetworkEventType::SendBlockUpdate:
                {
                    shared_ptr<stream::Reader> pEventReader = event.getReader();
                    stream::Reader &eventReader = *pEventReader;
                    PositionI blockPosition = stream::read<PositionI>(eventReader);
                    RenderObjectBlock block = stream::read<RenderObjectBlock>(eventReader, variableSet);
                    world->setBlock(blockPosition, block);
                    break;
                }
                case NetworkEventType::RequestChunk:
                    break;
                case NetworkEventType::SendPlayerProperties:
                    break;
                }
            }
        }
        catch(stream::IOException &e)
        {
            cerr << "IO Error : " << e.what() << endl;
            starting = false;
            running = false;
        }
        //running = false;
    }
    void writer(shared_ptr<stream::Writer> pwriter)
    {
        unordered_set<PositionI> sentChunkRequests;
        try
        {
            while(running)
            {
                bool didAnything = false;
                {
                    PositionI chunkPosition;
                    bool gotChunk = false;
                    {
                        lock_guard<mutex> lockIt(neededChunksLock);
                        for(PositionI cPos : neededChunks)
                        {
                            if(std::get<1>(sentChunkRequests.insert(cPos)))
                            {
                                gotChunk = true;
                                chunkPosition = cPos;
                                break;
                            }
                        }
                    }
                    if(gotChunk)
                    {
                        didAnything = true;
                        stream::MemoryWriter eventWriter;
                        stream::write<PositionI>(eventWriter, chunkPosition);
                        stream::write<NetworkEvent>(*pwriter, NetworkEvent(NetworkEventType::RequestChunk, std::move(eventWriter)));
                        pwriter->flush();
                    }
                }
                {
                    if(positionChanged.exchange(false))
                    {
                        stream::MemoryWriter eventWriter;
                        stream::write<PositionF>(eventWriter, getViewPosition());
                        stream::write<NetworkEvent>(*pwriter, NetworkEvent(NetworkEventType::SendPlayerProperties, std::move(eventWriter)));
                        pwriter->flush();
                        didAnything = true;
                    }
                }
                if(!didAnything)
                {
                    somethingToWrite.waitThenReset(true);
                }
            }
        }
        catch(stream::IOException &e)
        {
            cerr << "IO Error : " << e.what() << endl;
            running = false;
        }
    }
    void meshGenerator()
    {
        starting.wait(false);
        while(running)
        {
            assert(world);
            if(!world->generateMeshes((PositionI)getViewPosition()));
                this_thread::sleep_for(chrono::milliseconds(5));
        }
    }
    bool isWDown = false;
    bool isADown = false;
    bool isSDown = false;
    bool isDDown = false;
    bool isQDown = false;
    bool isSpaceDown = false;
    bool isEDown = false;
    bool gotSpaceDown = false;
    bool isLShiftDown = false;
    bool isRShiftDown = false;
    bool isShiftDown = false;
    bool isFDown = false;
    struct MyEventHandler : public EventHandler
    {
        Client & client;
        MyEventHandler(Client & client)
            : client(client)
        {
        }
        virtual bool handleMouseUp(MouseUpEvent &event) override
        {
            return false;
        }
        virtual bool handleMouseDown(MouseDownEvent &event) override
        {
            return false;
        }
        virtual bool handleMouseMove(MouseMoveEvent &event) override
        {
            client.deltaTheta -= event.deltaX / 300;
            client.deltaPhi -= event.deltaY / 300;
            return true;
        }
        virtual bool handleMouseScroll(MouseScrollEvent &event) override
        {
            return false;
        }
        virtual bool handleKeyUp(KeyUpEvent &event) override
        {
            if(event.key == KeyboardKey_A)
            {
                client.isADown = false;
                return true;
            }
            if(event.key == KeyboardKey_S)
            {
                client.isSDown = false;
                return true;
            }
            if(event.key == KeyboardKey_D)
            {
                client.isDDown = false;
                return true;
            }
            if(event.key == KeyboardKey_Q)
            {
                client.isQDown = false;
                return true;
            }
            if(event.key == KeyboardKey_W)
            {
                client.isWDown = false;
                return true;
            }
            if(event.key == KeyboardKey_E)
            {
                client.isEDown = false;
                return true;
            }
            if(event.key == KeyboardKey_F)
            {
                client.isFDown = false;
                return true;
            }
            if(event.key == KeyboardKey_Space)
            {
                client.isSpaceDown = false;
                return true;
            }
            if(event.key == KeyboardKey_LShift)
            {
                client.isLShiftDown = false;
                client.isShiftDown = client.isRShiftDown;
                return true;
            }
            if(event.key == KeyboardKey_RShift)
            {
                client.isRShiftDown = false;
                client.isShiftDown = client.isLShiftDown;
                return true;
            }
            return false;
        }
        virtual bool handleKeyDown(KeyDownEvent &event) override
        {
            if(event.key == KeyboardKey_A)
            {
                client.isADown = true;
                return true;
            }
            if(event.key == KeyboardKey_S)
            {
                client.isSDown = true;
                return true;
            }
            if(event.key == KeyboardKey_D)
            {
                client.isDDown = true;
                return true;
            }
            if(event.key == KeyboardKey_Q)
            {
                client.isQDown = true;
                return true;
            }
            if(event.key == KeyboardKey_W)
            {
                client.isWDown = true;
                return true;
            }
            if(event.key == KeyboardKey_E)
            {
                client.isEDown = true;
                return true;
            }
            if(event.key == KeyboardKey_F)
            {
                client.isFDown = true;
                return true;
            }
            if(event.key == KeyboardKey_Space)
            {
                client.isSpaceDown = true;
                client.gotSpaceDown = true;
                return true;
            }
            if(event.key == KeyboardKey_LShift)
            {
                client.isLShiftDown = true;
                client.isShiftDown = true;
                return true;
            }
            if(event.key == KeyboardKey_RShift)
            {
                client.isRShiftDown = true;
                client.isShiftDown = true;
                return true;
            }
            return false;
        }
        virtual bool handleKeyPress(KeyPressEvent &event) override
        {
            return false;
        }
        virtual bool handleQuit(QuitEvent &event) override
        {
            return client.running.exchange(false);
        }
    };
    static ColorF lightVertex(ColorF color, VectorF position, VectorF normal)
    {
        float scale = dot(normal, VectorF(0, 1, 0)) * 0.3 + 0.4;
        return scaleF(scale, color);
    }

public:
    Client(shared_ptr<stream::StreamRW> streamRW)
        : streamRW(streamRW), positionChanged(true)
    {
    }
    void run()
    {
        running = true;
        starting = true;
        thread(&Client::reader, this, streamRW->preader()).detach();
        thread(&Client::writer, this, streamRW->pwriter()).detach();
        thread(&Client::meshGenerator, this).detach();
        starting.wait(false);
        startGraphics();
        Display::grabMouse(true);
        Renderer r;
        while(running)
        {
            Display::clear();
            Matrix tform = Matrix::rotateX(getViewPhi()).concat(Matrix::rotateY(getViewTheta())).concat(Matrix::translate((VectorF)getViewPosition()));
            bool anyNeededChunks = false;
            for(RenderLayer renderLayer : enum_traits<RenderLayer>())
            {
                r << renderLayer;
                world->draw(r, inverse(tform), renderLayer, (PositionI)getViewPosition(), 50, [&](Mesh m, PositionI chunkBasePosition)->Mesh
                {
                    return lightMesh(m, lightVertex);
                }, false, [&](PositionI chunkPos)
                {
                    lock_guard<mutex> lockIt(neededChunksLock);
                    neededChunks.insert(chunkPos);
                    anyNeededChunks = true;
                });
            }
            if(anyNeededChunks)
            {
                somethingToWrite.set();
            }
            Display::flip(60);
            Display::handleEvents(shared_ptr<EventHandler>(new MyEventHandler(*this)));
            setViewTheta(fmod(getViewTheta() + deltaTheta * 0.5, 2 * M_PI));
            deltaTheta *= 0.5;
            setViewPhi(limit<float>(getViewPhi() + deltaPhi * 0.5, -M_PI / 2, M_PI / 2));
            deltaPhi *= 0.5;
            VectorF deltaPosition = VectorF(0);
            VectorF forwardVector = Matrix::rotateY(getViewTheta()).applyNoTranslate(VectorF(0, 0, -1));
            VectorF leftVector = Matrix::rotateY(getViewTheta()).applyNoTranslate(VectorF(-1, 0, 0));
            VectorF upVector = VectorF(0, 1, 0);
            if(isWDown)
                deltaPosition += forwardVector;
            if(isSDown)
                deltaPosition -= forwardVector;
            if(isADown)
                deltaPosition += leftVector;
            if(isDDown)
                deltaPosition -= leftVector;
            if(isSpaceDown)
                deltaPosition += upVector;
            if(isShiftDown)
                deltaPosition -= upVector;
            deltaPosition *= Display::frameDeltaTime() * 1.5;
            if(isFDown)
                deltaPosition *= 5;
            setViewPosition(getViewPosition() + deltaPosition);
        }
        running = false;
        somethingToWrite.set();
        endGraphics();
    }
};
}

void runClient(shared_ptr<stream::StreamRW> streamRW)
{
    (new Client(streamRW))->run();
}
