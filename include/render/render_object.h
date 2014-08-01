#ifndef RENDER_OBJECT_H_INCLUDED
#define RENDER_OBJECT_H_INCLUDED

#include "render/mesh.h"
#include "render/renderer.h"
#include "stream/stream.h"
#include "util/variable_set.h"
#include "util/block_chunk.h"
#include "util/enum_traits.h"
#include "util/linked_map.h"
#include <functional>

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
    }
};

struct RenderObjectBlock
{
    shared_ptr<RenderObjectBlockDescriptor> descriptor;
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
private:
    enum_array<shared_ptr<Mesh>, RenderLayer> drawMesh;
    atomic_bool meshesValid;
public:
    RenderObjectChunk(PositionI position)
        : blockChunk(position), meshesValid(false)
    {
    }
    void invalidateMeshes()
    {
        meshesValid = false;
    }
    void invalidate()
    {
        invalidateMeshes();
        blockChunk.onChange();
    }
private:
    void drawAll(shared_ptr<RenderObjectChunk> nx, shared_ptr<RenderObjectChunk> px, shared_ptr<RenderObjectChunk> ny, shared_ptr<RenderObjectChunk> py, shared_ptr<RenderObjectChunk> nz, shared_ptr<RenderObjectChunk> pz)
    {
        for(RenderLayer renderLayer : enum_traits<RenderLayer>())
        {
            if(!drawMesh[renderLayer])
                drawMesh[renderLayer] = make_shared<Mesh>();
            drawMesh[renderLayer]->clear();
            for(int32_t dx = 0; dx < BlockChunkType::chunkSizeX; dx++)
            {
                for(int32_t dy = 0; dy < BlockChunkType::chunkSizeY; dy++)
                {
                    for(int32_t dz = 0; dz < BlockChunkType::chunkSizeZ; dz++)
                    {
                        VectorI dpos = VectorI(dx, dy, dz);
                        PositionI pos = blockChunk.basePosition + dpos;
                        RenderObjectBlock bnx = (dx <= 0 ? (nx != nullptr ? nx->blockChunk.blocks[BlockChunkType::chunkSizeX - 1][dy][dz] : RenderObjectBlock()) : blockChunk.blocks[dx - 1][dy][dz]);
                        RenderObjectBlock bny = (dy <= 0 ? (ny != nullptr ? ny->blockChunk.blocks[dx][BlockChunkType::chunkSizeY - 1][dz] : RenderObjectBlock()) : blockChunk.blocks[dx][dy - 1][dz]);
                        RenderObjectBlock bnz = (dz <= 0 ? (nz != nullptr ? nz->blockChunk.blocks[dx][dy][BlockChunkType::chunkSizeZ - 1] : RenderObjectBlock()) : blockChunk.blocks[dx][dy][dz - 1]);
                        RenderObjectBlock bpx = (dx >= BlockChunkType::chunkSizeX - 1 ? (px != nullptr ? px->blockChunk.blocks[0][dy][dz] : RenderObjectBlock()) : blockChunk.blocks[dx + 1][dy][dz]);
                        RenderObjectBlock bpy = (dy >= BlockChunkType::chunkSizeY - 1 ? (py != nullptr ? py->blockChunk.blocks[dx][0][dz] : RenderObjectBlock()) : blockChunk.blocks[dx][dy + 1][dz]);
                        RenderObjectBlock bpz = (dz >= BlockChunkType::chunkSizeZ - 1 ? (pz != nullptr ? pz->blockChunk.blocks[dx][dy][0] : RenderObjectBlock()) : blockChunk.blocks[dx][dy][dz + 1]);
                        blockChunk.blocks[dx][dy][dz].draw(*drawMesh[renderLayer], renderLayer, pos, bnx, bpx, bny, bpy, bnz, bpz);
                    }
                }
            }
        }
    }
public:
    const Mesh & getDrawMesh(RenderLayer renderLayer, shared_ptr<RenderObjectChunk> nx, shared_ptr<RenderObjectChunk> px, shared_ptr<RenderObjectChunk> ny, shared_ptr<RenderObjectChunk> py, shared_ptr<RenderObjectChunk> nz, shared_ptr<RenderObjectChunk> pz, bool * didGenerateMeshes = nullptr)
    {
        if(didGenerateMeshes)
            *didGenerateMeshes = false;
        if(!meshesValid.exchange(true))
        {
            drawAll(nx, px, ny, py, nz, pz);
            if(didGenerateMeshes)
                *didGenerateMeshes = true;
        }
        return *drawMesh[renderLayer];
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
    linked_map<PositionI, shared_ptr<RenderObjectChunk>> chunks;
    shared_ptr<RenderObjectChunk> getChunk(PositionI pos)
    {
        auto iter = chunks.find(pos);
        if(iter == chunks.end())
            return nullptr;
        return std::get<1>(*iter);
    }
public:
    void draw(Renderer & renderer, Matrix tform, RenderLayer renderLayer, PositionI pos, int32_t viewDistance, function<Mesh(Mesh mesh, PositionI chunkBasePosition)> filterFn, bool needFilterUpdate)
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
                    PositionI nxPos = blockPosition - VectorI(RenderObjectChunk::BlockChunkType::chunkSizeX, 0, 0);
                    PositionI pxPos = blockPosition + VectorI(RenderObjectChunk::BlockChunkType::chunkSizeX, 0, 0);
                    PositionI nyPos = blockPosition - VectorI(0, RenderObjectChunk::BlockChunkType::chunkSizeY, 0);
                    PositionI pyPos = blockPosition + VectorI(0, RenderObjectChunk::BlockChunkType::chunkSizeY, 0);
                    PositionI nzPos = blockPosition - VectorI(0, 0, RenderObjectChunk::BlockChunkType::chunkSizeZ);
                    PositionI pzPos = blockPosition + VectorI(0, 0, RenderObjectChunk::BlockChunkType::chunkSizeZ);
                    shared_ptr<RenderObjectChunk> chunk = getChunk(blockPosition);
                    if(chunk != nullptr)
                    {
                        bool isNewMesh;
                        const Mesh &mesh = chunk->getDrawMesh(renderLayer, getChunk(nxPos), getChunk(pxPos), getChunk(nyPos), getChunk(pyPos), getChunk(nzPos), getChunk(pzPos), &isNewMesh);
                        if(isNewMesh || !chunk->cachedMesh[renderLayer] || needFilterUpdate)
                            chunk->cachedMesh[renderLayer] = renderer.cacheMesh(filterFn(mesh, blockPosition));
                        renderer << transform(tform, chunk->cachedMesh[renderLayer]);
                    }
                }
            }
        }
    }
private:
    void invalidateChunkMeshes(PositionI position)
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
        chunk->invalidate();
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
        shared_ptr<RenderObjectChunk> & pchunk = chunks[RenderObjectChunk::BlockChunkType::getChunkBasePosition(position)];
        if(pchunk == nullptr)
        {
            pchunk = shared_ptr<RenderObjectChunk>(new RenderObjectChunk(RenderObjectChunk::BlockChunkType::getChunkBasePosition(position)));
        }
        RenderObjectChunk & chunk = *pchunk;
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
            shared_ptr<RenderObjectChunk> chunk = stream::read<RenderObjectChunk>(reader, variableSet);
            if(!chunk || retval->chunks.count(chunk->blockChunk.basePosition) > 0)
                throw stream::InvalidDataValueException("chunk already in world");
            retval->chunks[chunk->blockChunk.basePosition] = chunk;
        }
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
    }
    bool getChanged(VariableSet &variableSet) const
    {
        return changeTracker.getChanged(variableSet);
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
