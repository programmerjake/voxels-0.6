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

using namespace std;

namespace
{
class Client
{
    shared_ptr<RenderObjectWorld> world;
    flag running, starting;
    shared_ptr<stream::StreamRW> streamRW;
    VariableSet variableSet;
    PositionF position = PositionF(0.5, 0.5 + 64 + 10, 0.5, Dimension::Overworld);
    PositionI getViewPosition() const
    {
        return (PositionI)position;
    }
    void reader(shared_ptr<stream::Reader> preader)
    {
        try
        {
            world = stream::read<RenderObjectWorld>(*preader, variableSet);
            starting = false;
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
        try
        {
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
            if(!world->generateMeshes(getViewPosition()));
                this_thread::sleep_for(chrono::milliseconds(1));
        }
    }
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
            return false;
        }
        virtual bool handleMouseScroll(MouseScrollEvent &event) override
        {
            return false;
        }
        virtual bool handleKeyUp(KeyUpEvent &event) override
        {
            return false;
        }
        virtual bool handleKeyDown(KeyDownEvent &event) override
        {
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
        : streamRW(streamRW)
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
        Renderer r;
        while(running)
        {
            Display::clear();
            Matrix tform = Matrix::rotateY(Display::timer() / 5 * M_PI).concat(Matrix::translate((VectorF)position));
            for(RenderLayer renderLayer : enum_traits<RenderLayer>())
            {
                r << renderLayer;
                world->draw(r, inverse(tform), renderLayer, (PositionI)position, 64, [&](Mesh m, PositionI chunkBasePosition)->Mesh
                {
                    return lightMesh(m, lightVertex);
                }, false);
            }
            Display::flip(60);
            Display::handleEvents(shared_ptr<EventHandler>(new MyEventHandler(*this)));
        }
        running = false;
        endGraphics();
    }
};
}

void runClient(shared_ptr<stream::StreamRW> streamRW)
{
    Client(streamRW).run();
}
