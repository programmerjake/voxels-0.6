#ifndef RENDER_OBJECT_H_INCLUDED
#define RENDER_OBJECT_H_INCLUDED

#include "render/mesh.h"
#include "stream/stream.h"
#include "util/variable_set.h"
#include "util/block_chunk.h"
#include "util/enum_traits.h"

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
    return face == NX ? -1 : (face == PX ? 1 : 0);
}

constexpr int getDY(BlockFace face)
{
    return face == NY ? -1 : (face == PY ? 1 : 0);
}

constexpr int getDZ(BlockFace face)
{
    return face == NZ ? -1 : (face == PZ ? 1 : 0);
}

constexpr VectorI getDelta(BlockFace face)
{
    return VectorI(getDX(face), getDY(face), getDZ(face));
}

typedef uint32_t BlockDrawClass;

class RenderObjectBlockDescriptor
{
    shared_ptr<Mesh> center;
    enum_array<shared_ptr<Mesh>, BlockFace> faceMesh;
    enum_array<bool, BlockFace> faceBlocked;
    BlockDrawClass blockDrawClass;
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
            dest.append(transform(Matrix::translate((VectorF)position), faceMesh[face]));
    }
    static shared_ptr<RenderObjectBlockDescriptor> read(stream::Reader &reader, VariableSet &variableSet)
    {
        shared_ptr<RenderObjectBlockDescriptor> retval = make_shared<RenderObjectBlockDescriptor>();
        retval->center = stream::read<Mesh>(reader, variableSet);
        for(BlockFace face : enum_traits<BlockFace>)
        {
            retval->faceMesh[face] = stream::read<Mesh>(reader, variableSet);
            retval->faceBlocked[face] = stream::read<bool>(reader);
        }
        retval->blockDrawClass = stream::read<BlockDrawClass>(reader);
        return retval;
    }
    void write(stream::Writer &writer, VariableSet &variableSet) const
    {
        stream::write<Mesh>(writer, variableSet, center);
        for(BlockFace face : enum_traits<BlockFace>)
        {
            stream::write<Mesh>(writer, variableSet, faceMesh[face]);
            stream::write<bool>(writer, faceBlocked[face]);
        }
        stream::write<BlockDrawClass>(writer, blockDrawClass);
    }
};

class RenderObjectBlock
{
    shared_ptr<RenderObjectBlockDescriptor> descriptor;
    void draw(Mesh &dest, PositionI position, const RenderObjectBlock & nx, const RenderObjectBlock & px, const RenderObjectBlock & ny, const RenderObjectBlock & py, const RenderObjectBlock & nz, const RenderObjectBlock & pz)
    {
        if(descriptor == nullptr)
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
#warning finish
};

#endif // RENDER_OBJECT_H_INCLUDED
