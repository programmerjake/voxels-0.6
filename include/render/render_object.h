#ifndef RENDER_OBJECT_H_INCLUDED
#define RENDER_OBJECT_H_INCLUDED

#include "render/mesh.h"
#include "stream/stream.h"
#include "util/variable_set.h"
#include "util/block_chunk.h"
#include "util/enum_traits.h"
#include "util/linked_map.h"

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

class RenderObjectWorld
{
    struct Chunk
    {
        typedef BlockChunk<RenderObjectBlock> BlockChunkType;
        BlockChunkType blockChunk;
        enum_array<shared_ptr<Mesh>, RenderLayer> drawMesh;
        atomic_bool meshesValid;
        Chunk(PositionI position)
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
        void drawAll(shared_ptr<Chunk> nx, shared_ptr<Chunk> px, shared_ptr<Chunk> ny, shared_ptr<Chunk> py, shared_ptr<Chunk> nz, shared_ptr<Chunk> pz)
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
        const Mesh & getDrawMesh(RenderLayer renderLayer, shared_ptr<Chunk> nx, shared_ptr<Chunk> px, shared_ptr<Chunk> ny, shared_ptr<Chunk> py, shared_ptr<Chunk> nz, shared_ptr<Chunk> pz)
        {
            if(!meshesValid.exchange(true))
            {
                drawAll(nx, px, ny, py, nz, pz);
            }
            return *drawMesh[renderLayer];
        }
    };
    linked_map<PositionI, shared_ptr<Chunk>> chunks;
    shared_ptr<Chunk> getChunk(PositionI pos)
    {
        auto iter = chunks.find(pos);
        if(iter == chunks.end())
            return nullptr;
        return std::get<1>(*iter);
    }
public:
    void draw(Mesh & dest, RenderLayer renderLayer, PositionI pos, int32_t viewDistance)
    {
        assert(viewDistance > 0);
        PositionI minPosition = pos - VectorI(viewDistance);
        PositionI maxPosition = pos + VectorI(viewDistance) + VectorI(Chunk::BlockChunkType::chunkSizeX, Chunk::BlockChunkType::chunkSizeY, Chunk::BlockChunkType::chunkSizeZ);
        PositionI minBlockPosition = Chunk::BlockChunkType::getChunkBasePosition(minPosition);
        PositionI maxBlockPosition = Chunk::BlockChunkType::getChunkBasePosition(maxPosition);

        for(PositionI blockPosition = minBlockPosition; blockPosition.x <= maxBlockPosition.x; blockPosition.x += Chunk::BlockChunkType::chunkSizeX)
        {
            for(blockPosition.y = minBlockPosition.y; blockPosition.y <= maxBlockPosition.y; blockPosition.y += Chunk::BlockChunkType::chunkSizeY)
            {
                for(blockPosition.z = minBlockPosition.z; blockPosition.z <= maxBlockPosition.z; blockPosition.z += Chunk::BlockChunkType::chunkSizeZ)
                {
                    PositionI nxPos = blockPosition - VectorI(Chunk::BlockChunkType::chunkSizeX, 0, 0);
                    PositionI pxPos = blockPosition + VectorI(Chunk::BlockChunkType::chunkSizeX, 0, 0);
                    PositionI nyPos = blockPosition - VectorI(0, Chunk::BlockChunkType::chunkSizeY, 0);
                    PositionI pyPos = blockPosition + VectorI(0, Chunk::BlockChunkType::chunkSizeY, 0);
                    PositionI nzPos = blockPosition - VectorI(0, 0, Chunk::BlockChunkType::chunkSizeZ);
                    PositionI pzPos = blockPosition + VectorI(0, 0, Chunk::BlockChunkType::chunkSizeZ);
                    shared_ptr<Chunk> chunk = getChunk(blockPosition);
                    if(chunk != nullptr)
                    {
                        dest.append(chunk->getDrawMesh(renderLayer, getChunk(nxPos), getChunk(pxPos), getChunk(nyPos), getChunk(pyPos), getChunk(nzPos), getChunk(pzPos)));
                    }
                }
            }
        }
    }
private:
    void invalidateChunkMeshes(PositionI position)
    {
        shared_ptr<Chunk> chunk = getChunk(Chunk::BlockChunkType::getChunkBasePosition(position));
        if(chunk != nullptr)
            chunk->invalidateMeshes();
    }
    void invalidateBlock(PositionI position)
    {
        PositionI chunkBasePosition = Chunk::BlockChunkType::getChunkBasePosition(position);
        shared_ptr<Chunk> chunk = getChunk(chunkBasePosition);
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
        shared_ptr<Chunk> pchunk = getChunk(Chunk::BlockChunkType::getChunkBasePosition(position));
        if(pchunk == nullptr)
            return RenderObjectBlock();
        Chunk & chunk = *pchunk;
        PositionI relativePosition = Chunk::BlockChunkType::getChunkRelativePosition(position);
        return chunk.blockChunk.blocks[relativePosition.x][relativePosition.y][relativePosition.z];
    }
    void setBlock(PositionI position, RenderObjectBlock block)
    {
        shared_ptr<Chunk> & pchunk = chunks[Chunk::BlockChunkType::getChunkBasePosition(position)];
        if(pchunk == nullptr)
        {
            pchunk = shared_ptr<Chunk>(new Chunk(Chunk::BlockChunkType::getChunkBasePosition(position)));
        }
        Chunk & chunk = *pchunk;
        PositionI relativePosition = Chunk::BlockChunkType::getChunkRelativePosition(position);
        chunk.blockChunk.blocks[relativePosition.x][relativePosition.y][relativePosition.z] = block;
        invalidateBlock(position);
    }
};

#endif // RENDER_OBJECT_H_INCLUDED
