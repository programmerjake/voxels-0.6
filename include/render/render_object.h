#ifndef RENDER_OBJECT_H_INCLUDED
#define RENDER_OBJECT_H_INCLUDED

#include "render/mesh.h"
#include "render/renderer.h"
#include "stream/stream.h"
#include "util/variable_set.h"
#include "util/block_chunk.h"
#include "util/enum_traits.h"
#include "util/linked_map.h"
#include "util/cached_variable.h"
#include "physics/physics.h"
#include <functional>
#include <algorithm>
#include <mutex>
#include <iostream>

using namespace std;

enum class BlockFace : uint8_t
{
    NX,
    PX,
    NY,
    PY,
    NZ,
    PZ,
    DEFINE_ENUM_LIMITS(NX, PZ)
};

constexpr int getDX(BlockFace face)
{
    return face == BlockFace::NX ? -1 : (face == BlockFace::PX ? 1 : 0);
}

constexpr int getDY(BlockFace face)
{
    return face == BlockFace::NY ? -1 : (face == BlockFace::PY ? 1 : 0);
}

constexpr int getDZ(BlockFace face)
{
    return face == BlockFace::NZ ? -1 : (face == BlockFace::PZ ? 1 : 0);
}

constexpr VectorI getDelta(BlockFace face)
{
    return VectorI(getDX(face), getDY(face), getDZ(face));
}

typedef uint32_t BlockDrawClass;

struct RenderObjectBlockDescriptor
{
    shared_ptr<Mesh> center;
    enum_array<shared_ptr<Mesh>, BlockFace> faceMesh;
    enum_array<bool, BlockFace> faceBlocked;
    BlockDrawClass blockDrawClass;
    RenderLayer renderLayer = RenderLayer::Opaque;
    shared_ptr<PhysicsObjectConstructor> physicsObjectConstructor;
    VectorF physicsObjectOffset;
    static bool needRenderFace(BlockFace face, shared_ptr<RenderObjectBlockDescriptor> block, shared_ptr<RenderObjectBlockDescriptor> sideBlock)
    {
        if(!block)
            return false;
        if(!sideBlock)
            return false;
        if(block->blockDrawClass == sideBlock->blockDrawClass)
            return false;
        if(sideBlock->faceBlocked[face])
            return false;
        return true;
    }
    static void renderFace(BlockFace face, Mesh &dest, PositionI position, shared_ptr<RenderObjectBlockDescriptor> block, shared_ptr<RenderObjectBlockDescriptor> sideBlock)
    {
        if(needRenderFace(face, block, sideBlock))
            dest.append(transform(Matrix::translate((VectorF)position), block->faceMesh[face]));
    }
    static shared_ptr<RenderObjectBlockDescriptor> read(stream::Reader &reader, VariableSet &variableSet)
    {
        shared_ptr<RenderObjectBlockDescriptor> retval = make_shared<RenderObjectBlockDescriptor>();
        retval->center = stream::read<Mesh>(reader, variableSet);
        for(BlockFace face : enum_traits<BlockFace>())
        {
            retval->faceMesh[face] = stream::read<Mesh>(reader, variableSet);
            retval->faceBlocked[face] = stream::read<bool>(reader);
        }
        retval->blockDrawClass = stream::read<BlockDrawClass>(reader);
        retval->renderLayer = stream::read<RenderLayer>(reader);
        retval->physicsObjectConstructor = stream::read<PhysicsObjectConstructor>(reader, variableSet);
        retval->physicsObjectOffset = stream::read<VectorF>(reader);
        return retval;
    }
    void write(stream::Writer &writer, VariableSet &variableSet) const
    {
        stream::write<Mesh>(writer, variableSet, center);
        for(BlockFace face : enum_traits<BlockFace>())
        {
            stream::write<Mesh>(writer, variableSet, faceMesh[face]);
            stream::write<bool>(writer, faceBlocked[face]);
        }
        stream::write<BlockDrawClass>(writer, blockDrawClass);
        stream::write<RenderLayer>(writer, renderLayer);
        stream::write<PhysicsObjectConstructor>(writer, variableSet, physicsObjectConstructor);
        stream::write<VectorF>(writer, physicsObjectOffset);
    }
    shared_ptr<PhysicsObject> createPhysicsObject(PositionI blockPosition, shared_ptr<PhysicsWorld> pWorld)
    {
        return physicsObjectConstructor->make((PositionF)blockPosition + physicsObjectOffset, VectorF(0), pWorld);
    }
};

struct RenderObjectEntityPart
{
    shared_ptr<Mesh> mesh;
    shared_ptr<Script> script;
    RenderLayer renderLayer;
};

struct RenderObjectEntityDescriptor
{
    vector<RenderObjectEntityPart> parts;
    shared_ptr<PhysicsObjectConstructor> physicsObjectConstructor;
    static shared_ptr<RenderObjectEntityDescriptor> read(stream::Reader &reader, VariableSet &variableSet)
    {
        size_t partCount = (size_t)(uint8_t)stream::read<uint8_t>(reader);
        shared_ptr<RenderObjectEntityDescriptor> retval = make_shared<RenderObjectEntityDescriptor>();
        RenderObjectEntityDescriptor & descriptor = *retval;
        descriptor.parts.resize(partCount);
        for(size_t i = 0; i < partCount; i++)
        {
            descriptor.parts[i].mesh = stream::read_nonnull<Mesh>(reader, variableSet);
            descriptor.parts[i].script = stream::read_nonnull<Script>(reader, variableSet);
            descriptor.parts[i].renderLayer = stream::read<RenderLayer>(reader);
        }
        descriptor.physicsObjectConstructor = stream::read_nonnull<PhysicsObjectConstructor>(reader, variableSet);
        return retval;
    }
    void write(stream::Writer &writer, VariableSet &variableSet)
    {
        uint8_t partCount = parts.size();
        assert((size_t)partCount == parts.size());
        stream::write<uint8_t>(writer, partCount);
        for(const RenderObjectEntityPart &part : parts)
        {
            stream::write<Mesh>(writer, variableSet, part.mesh);
            stream::write<Script>(writer, variableSet, part.script);
            stream::write<RenderLayer>(writer, part.renderLayer);
        }
        stream::write<PhysicsObjectConstructor>(writer, variableSet, physicsObjectConstructor);
    }
    void draw(Mesh &dest, RenderLayer renderLayer, PositionF position, VectorF velocity, float age, shared_ptr<Scripting::DataObject> ioObject)
    {
        for(RenderObjectEntityPart &part : parts)
        {
            if(part.renderLayer != renderLayer)
                continue;
            runEntityPartScript(dest, *part.mesh, part.script, position, velocity, age, ioObject);
        }
    }
};

struct RenderObjectEntity
{
    shared_ptr<RenderObjectEntityDescriptor> descriptor;
    shared_ptr<Scripting::DataObject> ioObject;
    PositionF position;
    VectorF velocity;
    float age = 0;
    shared_ptr<PhysicsObject> physicsObject;
    static shared_ptr<RenderObjectEntity> read(stream::Reader &reader, VariableSet &variableSet)
    {
        shared_ptr<RenderObjectEntity> retval = make_shared<RenderObjectEntity>();
        retval->descriptor = stream::read<RenderObjectEntityDescriptor>(reader, variableSet);
        retval->position = stream::read<PositionF>(reader);
        retval->velocity = stream::read<VectorF>(reader);
        retval->age = stream::read_finite<float32_t>(reader);
        retval->physicsObject = nullptr;
#warning add read/write ioObject
        return retval;
    }
    void write(stream::Writer &writer, VariableSet &variableSet)
    {
        stream::write<RenderObjectEntityDescriptor>(writer, variableSet, descriptor);
        stream::write<PositionF>(writer, position);
        stream::write<VectorF>(writer, velocity);
        stream::write<float32_t>(writer, age);
    }
    void draw(Mesh &dest, RenderLayer renderLayer, Dimension dimension)
    {
        if(dimension == position.d)
        {
            descriptor->draw(dest, renderLayer, position, velocity, age, ioObject);
        }
    }
};

namespace stream
{
template <>
struct is_value_changed<RenderObjectEntity>
{
    bool operator ()(std::shared_ptr<const RenderObjectEntity> value, VariableSet &) const
    {
        if(value == nullptr)
            return false;
        return true;
    }
};
}

struct RenderObjectBlock
{
    shared_ptr<RenderObjectBlockDescriptor> descriptor;
    shared_ptr<PhysicsObject> physicsObject;
    void draw(Mesh &dest, RenderLayer renderLayer, PositionI position, const RenderObjectBlock & nx, const RenderObjectBlock & px, const RenderObjectBlock & ny, const RenderObjectBlock & py, const RenderObjectBlock & nz, const RenderObjectBlock & pz)
    {
        if(descriptor == nullptr || descriptor->renderLayer != renderLayer)
            return;
        dest.append(transform(Matrix::translate((VectorF)position), descriptor->center));
        RenderObjectBlockDescriptor::renderFace(BlockFace::NX, dest, position, descriptor, nx.descriptor);
        RenderObjectBlockDescriptor::renderFace(BlockFace::PX, dest, position, descriptor, px.descriptor);
        RenderObjectBlockDescriptor::renderFace(BlockFace::NY, dest, position, descriptor, ny.descriptor);
        RenderObjectBlockDescriptor::renderFace(BlockFace::PY, dest, position, descriptor, py.descriptor);
        RenderObjectBlockDescriptor::renderFace(BlockFace::NZ, dest, position, descriptor, nz.descriptor);
        RenderObjectBlockDescriptor::renderFace(BlockFace::PZ, dest, position, descriptor, pz.descriptor);
    }
    static RenderObjectBlock read(stream::Reader &reader, VariableSet &variableSet)
    {
        RenderObjectBlock retval;
        retval.descriptor = stream::read<RenderObjectBlockDescriptor>(reader, variableSet);
        return retval;
    }
    void write(stream::Writer &writer, VariableSet &variableSet) const
    {
        stream::write<RenderObjectBlockDescriptor>(writer, variableSet, descriptor);
    }
    RenderObjectBlock(shared_ptr<RenderObjectBlockDescriptor> descriptor = nullptr)
        : descriptor(descriptor)
    {
    }
    explicit operator bool() const
    {
        return (bool)descriptor;
    }
    bool operator ~() const
    {
        return !descriptor;
    }
    void createPhysicsObject(shared_ptr<PhysicsWorld> pWorld, PositionI position)
    {
        if(physicsObject)
            physicsObject->destroy();
        if(descriptor)
            physicsObject = descriptor->createPhysicsObject(position, pWorld);
        else
            physicsObject = nullptr;
    }
    void destroyPhysicsObject()
    {
        if(physicsObject)
            physicsObject->destroy();
        physicsObject = nullptr;
    }
};

namespace stream
{
template <>
struct rw_cached_helper<RenderObjectBlock>
{
    typedef RenderObjectBlock value_type;
    static value_type read(Reader &reader, VariableSet &variableSet)
    {
        return RenderObjectBlock::read(reader, variableSet);
    }
    static void write(Writer &writer, VariableSet &variableSet, value_type value)
    {
        value.write(writer, variableSet);
    }
};
}

struct RenderObjectChunk
{
    typedef BlockChunk<RenderObjectBlock> BlockChunkType;
    BlockChunkType blockChunk;
    enum_array<shared_ptr<CachedMesh>, RenderLayer> cachedMesh;
    enum_array<atomic_bool, RenderLayer> cachedMeshValid;
    static constexpr int subChunkSizeShiftAmount = 2;
    static constexpr int32_t subChunkSize = (int32_t)1 << subChunkSizeShiftAmount;
    static_assert(BlockChunkType::chunkSizeX % subChunkSize == 0, "BlockChunkType::chunkSizeX is not divisible by subChunkSize");
    static_assert(BlockChunkType::chunkSizeY % subChunkSize == 0, "BlockChunkType::chunkSizeY is not divisible by subChunkSize");
    static_assert(BlockChunkType::chunkSizeZ % subChunkSize == 0, "BlockChunkType::chunkSizeZ is not divisible by subChunkSize");
    enum_array<array<array<array<Mesh, BlockChunkType::chunkSizeZ / subChunkSize>, BlockChunkType::chunkSizeY / subChunkSize>, BlockChunkType::chunkSizeX / subChunkSize>, RenderLayer> subChunkMeshes;
    enum_array<array<array<array<atomic_bool, BlockChunkType::chunkSizeZ / subChunkSize>, BlockChunkType::chunkSizeY / subChunkSize>, BlockChunkType::chunkSizeX / subChunkSize>, RenderLayer> subChunkMeshesValid;
    mutex generateMeshesLock;
    enum_array<CachedVariable<Mesh>, RenderLayer> drawMesh;
    atomic_bool meshesValid;
    RenderObjectChunk(PositionI position)
        : blockChunk(position), meshesValid(false)
    {
        for(atomic_bool &v : cachedMeshValid)
            v = false;
        for(auto &block : subChunkMeshesValid)
        {
            for(auto &slab : block)
            {
                for(auto &column : slab)
                {
                    for(atomic_bool &v : column)
                        v = false;
                }
            }
        }
    }
    RenderObjectChunk(const BlockChunkType & chunk)
        : blockChunk(chunk), meshesValid(false)
    {
        for(atomic_bool &v : cachedMeshValid)
            v = false;
        for(auto &block : subChunkMeshesValid)
        {
            for(auto &slab : block)
            {
                for(auto &column : slab)
                {
                    for(atomic_bool &v : column)
                        v = false;
                }
            }
        }
    }
    void invalidateMeshes()
    {
        meshesValid = false;
        for(atomic_bool &v : cachedMeshValid)
            v = false;
        for(auto &block : subChunkMeshesValid)
        {
            for(auto &slab : block)
            {
                for(auto &column : slab)
                {
                    for(atomic_bool &v : column)
                        v = false;
                }
            }
        }
    }
    void invalidate()
    {
        invalidateMeshes();
        blockChunk.onChange();
    }
    void invalidateMeshes(PositionI position)
    {
        assert(position.d == blockChunk.basePosition.d);
        meshesValid = false;
        for(atomic_bool &v : cachedMeshValid)
            v = false;
        VectorI relativePosition = position - blockChunk.basePosition;
        assert(relativePosition.x >= 0 && relativePosition.x < BlockChunkType::chunkSizeX);
        assert(relativePosition.y >= 0 && relativePosition.y < BlockChunkType::chunkSizeY);
        assert(relativePosition.z >= 0 && relativePosition.z < BlockChunkType::chunkSizeZ);
        VectorI subChunkPos = VectorI(relativePosition.x >> subChunkSizeShiftAmount,
                                      relativePosition.y >> subChunkSizeShiftAmount,
                                      relativePosition.z >> subChunkSizeShiftAmount);
        for(auto &chunk : subChunkMeshesValid)
            chunk[subChunkPos.x][subChunkPos.y][subChunkPos.z] = false;
    }
    void invalidate(PositionI position)
    {
        invalidateMeshes(position);
        blockChunk.onChange();
    }
private:
    const Mesh &generateSubChunkDrawMeshes(RenderLayer renderLayer, VectorI subChunkPosition, shared_ptr<RenderObjectChunk> nx, shared_ptr<RenderObjectChunk> px, shared_ptr<RenderObjectChunk> ny, shared_ptr<RenderObjectChunk> py, shared_ptr<RenderObjectChunk> nz, shared_ptr<RenderObjectChunk> pz)
    {
        atomic_bool &subChunkValid = subChunkMeshesValid[renderLayer][subChunkPosition.x >> subChunkSizeShiftAmount][subChunkPosition.y >> subChunkSizeShiftAmount][subChunkPosition.z >> subChunkSizeShiftAmount];
        Mesh &mesh = subChunkMeshes[renderLayer][subChunkPosition.x >> subChunkSizeShiftAmount][subChunkPosition.y >> subChunkSizeShiftAmount][subChunkPosition.z >> subChunkSizeShiftAmount];
        if(subChunkValid)
            return mesh;
        mesh.clear();
        for(int32_t dx = subChunkPosition.x; dx < subChunkPosition.x + subChunkSize; dx++)
        {
            for(int32_t dy = subChunkPosition.y; dy < subChunkPosition.y + subChunkSize; dy++)
            {
                for(int32_t dz = subChunkPosition.z; dz < subChunkPosition.z + subChunkSize; dz++)
                {
                    VectorI dpos = VectorI(dx, dy, dz);
                    PositionI pos = blockChunk.basePosition + dpos;
                    RenderObjectBlock bnx = (dx <= 0 ? (nx != nullptr ? nx->blockChunk.blocks[BlockChunkType::chunkSizeX - 1][dy][dz] : RenderObjectBlock()) : blockChunk.blocks[dx - 1][dy][dz]);
                    RenderObjectBlock bny = (dy <= 0 ? (ny != nullptr ? ny->blockChunk.blocks[dx][BlockChunkType::chunkSizeY - 1][dz] : RenderObjectBlock()) : blockChunk.blocks[dx][dy - 1][dz]);
                    RenderObjectBlock bnz = (dz <= 0 ? (nz != nullptr ? nz->blockChunk.blocks[dx][dy][BlockChunkType::chunkSizeZ - 1] : RenderObjectBlock()) : blockChunk.blocks[dx][dy][dz - 1]);
                    RenderObjectBlock bpx = (dx >= BlockChunkType::chunkSizeX - 1 ? (px != nullptr ? px->blockChunk.blocks[0][dy][dz] : RenderObjectBlock()) : blockChunk.blocks[dx + 1][dy][dz]);
                    RenderObjectBlock bpy = (dy >= BlockChunkType::chunkSizeY - 1 ? (py != nullptr ? py->blockChunk.blocks[dx][0][dz] : RenderObjectBlock()) : blockChunk.blocks[dx][dy + 1][dz]);
                    RenderObjectBlock bpz = (dz >= BlockChunkType::chunkSizeZ - 1 ? (pz != nullptr ? pz->blockChunk.blocks[dx][dy][0] : RenderObjectBlock()) : blockChunk.blocks[dx][dy][dz + 1]);
                    blockChunk.blocks[dx][dy][dz].draw(mesh, renderLayer, pos, bnx, bpx, bny, bpy, bnz, bpz);
                }
            }
        }
        subChunkValid = true;
        return mesh;
    }
public:
    bool generateDrawMeshes(shared_ptr<RenderObjectChunk> nx, shared_ptr<RenderObjectChunk> px, shared_ptr<RenderObjectChunk> ny, shared_ptr<RenderObjectChunk> py, shared_ptr<RenderObjectChunk> nz, shared_ptr<RenderObjectChunk> pz)
    {
        lock_guard<mutex> lockIt(generateMeshesLock);
        if(meshesValid)
            return false;
        for(RenderLayer renderLayer : enum_traits<RenderLayer>())
        {
            Mesh & mesh = drawMesh[renderLayer].writeRef();
            mesh.clear();
            for(int32_t dx = 0; dx < BlockChunkType::chunkSizeX; dx += subChunkSize)
            {
                for(int32_t dy = 0; dy < BlockChunkType::chunkSizeY; dy += subChunkSize)
                {
                    for(int32_t dz = 0; dz < BlockChunkType::chunkSizeZ; dz += subChunkSize)
                    {
                        mesh.append(generateSubChunkDrawMeshes(renderLayer, VectorI(dx, dy, dz), nx, px, ny, py, nz, pz));
                    }
                }
            }
            drawMesh[renderLayer].finishWrite();
            cachedMeshValid[renderLayer] = false;
        }
        meshesValid = true;
        return true;
    }
    const Mesh & getDrawMesh(RenderLayer renderLayer)
    {
        return drawMesh[renderLayer].read();
    }
    static shared_ptr<RenderObjectChunk> read(stream::Reader &reader, VariableSet &variableSet)
    {
        shared_ptr<BlockChunkType> readBlockChunk = BlockChunkType::read(reader, variableSet);
        shared_ptr<RenderObjectChunk> retval = make_shared<RenderObjectChunk>(readBlockChunk->basePosition);
        retval->blockChunk.blocks = readBlockChunk->blocks;
        retval->blockChunk.onChange();
        return retval;
    }
    void write(stream::Writer &writer, VariableSet &variableSet)
    {
        blockChunk.write(writer, variableSet);
    }
    void createPhysicsObjects(shared_ptr<PhysicsWorld> pWorld, VectorI minPosition, VectorI maxPosition)
    {
        minPosition -= (VectorI)blockChunk.basePosition;
        maxPosition -= (VectorI)blockChunk.basePosition;
        if(minPosition.x < 0)
            minPosition.x = 0;
        if(minPosition.y < 0)
            minPosition.y = 0;
        if(minPosition.z < 0)
            minPosition.z = 0;
        if(maxPosition.x > BlockChunkType::chunkSizeX - 1)
            maxPosition.x = BlockChunkType::chunkSizeX - 1;
        if(maxPosition.y > BlockChunkType::chunkSizeY - 1)
            maxPosition.y = BlockChunkType::chunkSizeY - 1;
        if(maxPosition.z > BlockChunkType::chunkSizeZ - 1)
            maxPosition.z = BlockChunkType::chunkSizeZ - 1;
        for(int32_t x = minPosition.x; x <= maxPosition.x; x++)
            for(int32_t y = minPosition.y; y <= maxPosition.y; y++)
                for(int32_t z = minPosition.z; z <= maxPosition.z; z++)
                    blockChunk.blocks[x][y][z].createPhysicsObject(pWorld, blockChunk.basePosition + VectorI(x, y, z));
    }
    void destroyPhysicsObjects(VectorI minPosition, VectorI maxPosition)
    {
        minPosition -= (VectorI)blockChunk.basePosition;
        maxPosition -= (VectorI)blockChunk.basePosition;
        if(minPosition.x < 0)
            minPosition.x = 0;
        if(minPosition.y < 0)
            minPosition.y = 0;
        if(minPosition.z < 0)
            minPosition.z = 0;
        if(maxPosition.x > BlockChunkType::chunkSizeX - 1)
            maxPosition.x = BlockChunkType::chunkSizeX - 1;
        if(maxPosition.y > BlockChunkType::chunkSizeY - 1)
            maxPosition.y = BlockChunkType::chunkSizeY - 1;
        if(maxPosition.z > BlockChunkType::chunkSizeZ - 1)
            maxPosition.z = BlockChunkType::chunkSizeZ - 1;
        for(int32_t x = minPosition.x; x <= maxPosition.x; x++)
            for(int32_t y = minPosition.y; y <= maxPosition.y; y++)
                for(int32_t z = minPosition.z; z <= maxPosition.z; z++)
                    blockChunk.blocks[x][y][z].destroyPhysicsObject();
    }
};

namespace stream
{
template <>
struct is_value_changed<RenderObjectChunk>
{
    bool operator ()(std::shared_ptr<const RenderObjectChunk> value, VariableSet &variableSet) const
    {
        if(value == nullptr)
            return false;
        return value->blockChunk.changeTracker.getChanged(variableSet);
    }
};
}

class RenderObjectWorld
{
    mutable ChangeTracker changeTracker;
    typedef linked_map<PositionI, shared_ptr<RenderObjectChunk>> ChunksMap;
    ChunksMap chunks;
    mutex chunksLock;
    list<shared_ptr<RenderObjectEntity>> entities;
    mutex entitiesLock;
public:
    shared_ptr<RenderObjectChunk> getChunk(PositionI pos)
    {
        lock_guard<mutex> lockIt(chunksLock);
        auto iter = chunks.find(pos);
        if(iter == chunks.end())
            return nullptr;
        return std::get<1>(*iter);
    }
private:
    bool generateMesh(PositionI chunkPosition, shared_ptr<RenderObjectChunk> chunk = nullptr)
    {
        PositionI nxPos = chunkPosition - VectorI(RenderObjectChunk::BlockChunkType::chunkSizeX, 0, 0);
        PositionI pxPos = chunkPosition + VectorI(RenderObjectChunk::BlockChunkType::chunkSizeX, 0, 0);
        PositionI nyPos = chunkPosition - VectorI(0, RenderObjectChunk::BlockChunkType::chunkSizeY, 0);
        PositionI pyPos = chunkPosition + VectorI(0, RenderObjectChunk::BlockChunkType::chunkSizeY, 0);
        PositionI nzPos = chunkPosition - VectorI(0, 0, RenderObjectChunk::BlockChunkType::chunkSizeZ);
        PositionI pzPos = chunkPosition + VectorI(0, 0, RenderObjectChunk::BlockChunkType::chunkSizeZ);
        if(chunk == nullptr)
            chunk = getChunk(chunkPosition);
        if(chunk == nullptr)
            return false;
        return chunk->generateDrawMeshes(getChunk(nxPos), getChunk(pxPos), getChunk(nyPos), getChunk(pyPos), getChunk(nzPos), getChunk(pzPos));
    }
public:
    bool generateMeshes(PositionI pos)
    {
        vector<ChunksMap::iterator> chunksList;
        {
            lock_guard<mutex> lockIt(chunksLock);
            chunksList.reserve(chunks.size());
            for(auto iter = chunks.begin(); iter != chunks.end(); iter++)
            {
                if(std::get<1>(*iter)->meshesValid)
                    continue;
                chunksList.push_back(iter);
            }
        }
        std::sort(chunksList.begin(), chunksList.end(), [&pos](ChunksMap::iterator a, ChunksMap::iterator b)->bool
        {
            return absSquared((VectorI)pos - (VectorI)std::get<0>(*a)) < absSquared((VectorI)pos - (VectorI)std::get<0>(*b));
        });
        if(chunksList.empty())
            return false;
        return generateMesh(std::get<0>(*chunksList.front()), std::get<1>(*chunksList.front()));
    }
    void draw(Renderer & renderer, Matrix tform, RenderLayer renderLayer, PositionI pos, int32_t viewDistance, function<Mesh(Mesh mesh, PositionI chunkBasePosition)> filterFn, bool needFilterUpdate, function<void(PositionI chunkBasePosition)> needChunkCallback = nullptr)
    {
        assert(viewDistance > 0);
        PositionI minPosition = pos - VectorI(viewDistance);
        PositionI maxPosition = pos + VectorI(viewDistance) + VectorI(RenderObjectChunk::BlockChunkType::chunkSizeX, RenderObjectChunk::BlockChunkType::chunkSizeY, RenderObjectChunk::BlockChunkType::chunkSizeZ);
        PositionI minBlockPosition = RenderObjectChunk::BlockChunkType::getChunkBasePosition(minPosition);
        PositionI maxBlockPosition = RenderObjectChunk::BlockChunkType::getChunkBasePosition(maxPosition);

        for(PositionI blockPosition = minBlockPosition; blockPosition.x <= maxBlockPosition.x; blockPosition.x += RenderObjectChunk::BlockChunkType::chunkSizeX)
        {
            for(blockPosition.y = minBlockPosition.y; blockPosition.y <= maxBlockPosition.y; blockPosition.y += RenderObjectChunk::BlockChunkType::chunkSizeY)
            {
                for(blockPosition.z = minBlockPosition.z; blockPosition.z <= maxBlockPosition.z; blockPosition.z += RenderObjectChunk::BlockChunkType::chunkSizeZ)
                {
                    shared_ptr<RenderObjectChunk> chunk = getChunk(blockPosition);
                    if(chunk != nullptr)
                    {
                        const Mesh &mesh = chunk->getDrawMesh(renderLayer);
                        if(!chunk->cachedMesh[renderLayer] || !chunk->cachedMeshValid[renderLayer] || needFilterUpdate)
                        {
                            chunk->cachedMesh[renderLayer] = renderer.cacheMesh(filterFn(mesh, blockPosition));
                            chunk->cachedMeshValid[renderLayer] = true;
                        }
                        renderer << transform(tform, chunk->cachedMesh[renderLayer]);
                    }
                    else if(needChunkCallback)
                        needChunkCallback(blockPosition);
                }
            }
        }
        vector<shared_ptr<RenderObjectEntity>> entitiesList;
        {
            lock_guard<mutex> lockIt(entitiesLock);
            entitiesList.reserve(entities.size());
            for(auto v : entities)
                entitiesList.push_back(v);
        }
        Mesh drawMesh;
        for(shared_ptr<RenderObjectEntity> entity : entitiesList)
        {
            entity->draw(drawMesh, renderLayer, pos.d);
        }
        drawMesh = filterFn(std::move(drawMesh), PositionI(0, 0, 0, pos.d));
        renderer << transform(tform, drawMesh);
    }
private:
    void invalidateChunkMeshes(PositionI position)
    {
        shared_ptr<RenderObjectChunk> chunk = getChunk(RenderObjectChunk::BlockChunkType::getChunkBasePosition(position));
        if(chunk != nullptr)
            chunk->invalidateMeshes(position);
    }
    void invalidateChunkMeshesAll(PositionI position)
    {
        shared_ptr<RenderObjectChunk> chunk = getChunk(RenderObjectChunk::BlockChunkType::getChunkBasePosition(position));
        if(chunk != nullptr)
            chunk->invalidateMeshes();
    }
    void invalidateBlock(PositionI position)
    {
        PositionI chunkBasePosition = RenderObjectChunk::BlockChunkType::getChunkBasePosition(position);
        shared_ptr<RenderObjectChunk> chunk = getChunk(chunkBasePosition);
        if(chunk == nullptr)
            return;
        chunk->invalidate(position);
        for(BlockFace face : enum_traits<BlockFace>())
        {
            invalidateChunkMeshes(position + getDelta(face));
        }
    }
public:
    RenderObjectBlock getBlock(PositionI position)
    {
        shared_ptr<RenderObjectChunk> pchunk = getChunk(RenderObjectChunk::BlockChunkType::getChunkBasePosition(position));
        if(pchunk == nullptr)
            return RenderObjectBlock();
        RenderObjectChunk & chunk = *pchunk;
        PositionI relativePosition = RenderObjectChunk::BlockChunkType::getChunkRelativePosition(position);
        return chunk.blockChunk.blocks[relativePosition.x][relativePosition.y][relativePosition.z];
    }
    void setBlock(PositionI position, RenderObjectBlock block)
    {
        shared_ptr<RenderObjectChunk> *ppchunk;
        {
            lock_guard<mutex> lockIt(chunksLock);
            ppchunk = &chunks[RenderObjectChunk::BlockChunkType::getChunkBasePosition(position)];
            if(*ppchunk == nullptr)
            {
                *ppchunk = shared_ptr<RenderObjectChunk>(new RenderObjectChunk(RenderObjectChunk::BlockChunkType::getChunkBasePosition(position)));
            }
        }
        RenderObjectChunk &chunk = **ppchunk;
        PositionI relativePosition = RenderObjectChunk::BlockChunkType::getChunkRelativePosition(position);
        chunk.blockChunk.blocks[relativePosition.x][relativePosition.y][relativePosition.z] = block;
        invalidateBlock(position);
        changeTracker.onChange();
    }
    static shared_ptr<RenderObjectWorld> read(stream::Reader &reader, VariableSet &variableSet)
    {
        uint32_t chunkCount = stream::read<uint32_t>(reader);
        shared_ptr<RenderObjectWorld> retval = make_shared<RenderObjectWorld>();
        for(uint32_t i = 0; i < chunkCount; i++)
        {
            cout << "Reading World ... (" << 100 * i / chunkCount << "%)\x1b[K\r" << flush;
            shared_ptr<RenderObjectChunk> chunk = stream::read<RenderObjectChunk>(reader, variableSet);
            if(!chunk || retval->chunks.count(chunk->blockChunk.basePosition) > 0)
            {
                cout << "Reading World ... Error!" << endl;
                throw stream::InvalidDataValueException("chunk already in world");
            }
            retval->chunks[chunk->blockChunk.basePosition] = chunk;
        }
        uint32_t entityCount = stream::read<uint32_t>(reader);
        for(uint32_t i = 0; i < entityCount; i++)
        {
            shared_ptr<RenderObjectEntity> entity = stream::read_nonnull<RenderObjectEntity>(reader, variableSet);
            retval->entities.push_back(entity);
        }
        cout << "Reading World ... Done." << endl;
        return retval;
    }
    void write(stream::Writer &writer, VariableSet &variableSet)
    {
        uint32_t chunkCount = (uint32_t)chunks.size();
        assert((size_t)chunkCount == chunks.size());
        stream::write<uint32_t>(writer, chunkCount);
        for(auto iter = chunks.begin(); iter != chunks.end(); iter++)
        {
            stream::write<RenderObjectChunk>(writer, variableSet, std::get<1>(*iter));
        }
        uint32_t entityCount = (uint32_t)entities.size();
        assert((size_t)entityCount == entities.size());
        stream::write<uint32_t>(writer, entityCount);
        for(shared_ptr<RenderObjectEntity> & e : entities)
        {
            stream::write<RenderObjectEntity>(writer, variableSet, e);
        }
        changeTracker.onWrite(variableSet);
    }
    bool getChanged(VariableSet &variableSet) const
    {
        return changeTracker.getChanged(variableSet);
    }
    void setChunk(shared_ptr<RenderObjectChunk> chunk)
    {
        assert(chunk);
        PositionI chunkPosition = chunk->blockChunk.basePosition;
        assert(RenderObjectChunk::BlockChunkType::getChunkBasePosition(chunkPosition) == chunkPosition);
        {
            lock_guard<mutex> lockIt(chunksLock);
            chunks[chunkPosition] = chunk;
        }
        changeTracker.onChange();
        invalidateChunkMeshesAll(chunkPosition - VectorI(RenderObjectChunk::BlockChunkType::chunkSizeX, 0, 0));
        invalidateChunkMeshesAll(chunkPosition + VectorI(RenderObjectChunk::BlockChunkType::chunkSizeX, 0, 0));
        invalidateChunkMeshesAll(chunkPosition - VectorI(0, RenderObjectChunk::BlockChunkType::chunkSizeY, 0));
        invalidateChunkMeshesAll(chunkPosition + VectorI(0, RenderObjectChunk::BlockChunkType::chunkSizeY, 0));
        invalidateChunkMeshesAll(chunkPosition - VectorI(0, 0, RenderObjectChunk::BlockChunkType::chunkSizeZ));
        invalidateChunkMeshesAll(chunkPosition + VectorI(0, 0, RenderObjectChunk::BlockChunkType::chunkSizeZ));
    }
    void createPhysicsObjects(shared_ptr<PhysicsWorld> pWorld, PositionI center, VectorI extents)
    {
        PositionI minPos = center - extents;
        PositionI maxPos = center + extents;
        PositionI beginChunkPos = RenderObjectChunk::BlockChunkType::getChunkBasePosition(minPos);
        PositionI endChunkPos = RenderObjectChunk::BlockChunkType::getChunkBasePosition(maxPos + VectorI(RenderObjectChunk::BlockChunkType::chunkSizeX, RenderObjectChunk::BlockChunkType::chunkSizeY, RenderObjectChunk::BlockChunkType::chunkSizeZ));
        for(PositionI pos = beginChunkPos; pos.x < endChunkPos.x; pos.x += RenderObjectChunk::BlockChunkType::chunkSizeX)
        {
            for(pos.y = beginChunkPos.y; pos.y < endChunkPos.y; pos.y += RenderObjectChunk::BlockChunkType::chunkSizeY)
            {
                for(pos.z = beginChunkPos.z; pos.z < endChunkPos.z; pos.z += RenderObjectChunk::BlockChunkType::chunkSizeZ)
                {
                    shared_ptr<RenderObjectChunk> chunk = getChunk(pos);
                    chunk->createPhysicsObjects(pWorld, (VectorI)minPos, (VectorI)maxPos);
                }
            }
        }
    }
    void destroyPhysicsObjects(PositionI center, VectorI extents)
    {
        PositionI minPos = center - extents;
        PositionI maxPos = center + extents;
        PositionI beginChunkPos = RenderObjectChunk::BlockChunkType::getChunkBasePosition(minPos);
        PositionI endChunkPos = RenderObjectChunk::BlockChunkType::getChunkBasePosition(maxPos + VectorI(RenderObjectChunk::BlockChunkType::chunkSizeX, RenderObjectChunk::BlockChunkType::chunkSizeY, RenderObjectChunk::BlockChunkType::chunkSizeZ));
        for(PositionI pos = beginChunkPos; pos.x < endChunkPos.x; pos.x += RenderObjectChunk::BlockChunkType::chunkSizeX)
        {
            for(pos.y = beginChunkPos.y; pos.y < endChunkPos.y; pos.y += RenderObjectChunk::BlockChunkType::chunkSizeY)
            {
                for(pos.z = beginChunkPos.z; pos.z < endChunkPos.z; pos.z += RenderObjectChunk::BlockChunkType::chunkSizeZ)
                {
                    shared_ptr<RenderObjectChunk> chunk = getChunk(pos);
                    chunk->destroyPhysicsObjects((VectorI)minPos, (VectorI)maxPos);
                }
            }
        }
    }
};

namespace stream
{
template <>
struct is_value_changed<RenderObjectWorld>
{
    bool operator ()(std::shared_ptr<const RenderObjectWorld> value, VariableSet &variableSet) const
    {
        if(value == nullptr)
            return false;
        return value->getChanged(variableSet);
    }
};
}

#endif // RENDER_OBJECT_H_INCLUDED
