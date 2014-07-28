#ifndef VARIABLE_SET_H_INCLUDED
#define VARIABLE_SET_H_INCLUDED

#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <tuple>
#include "stream/stream.h"

using namespace std;

class VariableSet final
{
private:
    class VariableDescriptorBase
    {
    protected:
        static uint64_t makeIndex()
        {
            static atomic_uint_fast64_t nextIndex(0);
            return ++nextIndex;
        }
    };
    struct VariableBase
    {
        virtual ~VariableBase()
        {
        }
    };
    template <typename T>
    struct Variable : public VariableBase
    {
        shared_ptr<T> ptr;
        Variable(shared_ptr<T> ptr)
            : ptr(ptr)
        {
        }
    };
    unordered_map<size_t, shared_ptr<VariableBase>> variables;
    unordered_map<const void *, pair<size_t, weak_ptr<void>>> reverseMap;
public:
    template <typename T>
    class Descriptor final : public VariableDescriptorBase
    {
        friend class VariableSet;
    private:
        uint64_t descriptorIndex;
        shared_ptr<T> & unwrap(shared_ptr<VariableBase> ptr) const
        {
            return dynamic_pointer_cast<Variable<T>>(ptr)->ptr;
        }
        Descriptor(uint64_t descriptorIndex)
            : descriptorIndex(descriptorIndex)
        {
        }
    public:
        Descriptor()
            : descriptorIndex(makeIndex())
        {
        }
        bool operator !() const
        {
            return descriptorIndex != 0;
        }
        operator bool() const
        {
            return descriptorIndex == 0;
        }
        static Descriptor read(Reader & reader)
        {
            return Descriptor(reader.readU64());
        }
        void write(Writer & writer) const
        {
            writer.writeU64(descriptorIndex);
        }
        static Descriptor null()
        {
            return Descriptor(0);
        }
    };
    recursive_mutex theLock;
    template <typename T>
    shared_ptr<T> get(const Descriptor<T> & descriptor)
    {
        lock_guard<recursive_mutex> lockIt(theLock);
        auto iter = variables.find(descriptor.descriptorIndex);
        if(iter == variables.end())
            return nullptr;
        if(std::get<1>(*iter) == nullptr)
            return nullptr;
        return descriptor.unwrap(std::get<1>(*iter));
    }
    template <typename T>
    bool set(const Descriptor<T> & descriptor, shared_ptr<T> value)
    {
        lock_guard<recursive_mutex> lockIt(theLock);
        reverseMap.erase(static_cast<const void *>(get<T>(descriptor).get()));
        if(value == nullptr)
        {
            return variables.erase(descriptor.descriptorIndex) != 0;
        }
        else
        {
            shared_ptr<VariableBase> & variable = variables[descriptor.descriptorIndex];
            bool retval = true;
            if(variable == nullptr)
            {
                variable = shared_ptr<VariableBase>(new Variable<T>(value));
                retval = false;
            }
            descriptor.unwrap(variable) = value;
            reverseMap[static_cast<const void *>(value.get())] = make_pair(descriptor.descriptorIndex, weak_ptr<void>(static_pointer_cast<void>(value)));
            return retval;
        }
    }
    template <typename T>
    Descriptor<T> find(shared_ptr<T> value)
    {
        lock_guard<recursive_mutex> lockIt(theLock);
        auto iter = reverseMap.find(static_cast<const void *>(value.get()));
        if(iter == reverseMap.end())
            return Descriptor<T>::null();
        if(std::get<1>(std::get<1>(*iter)).expired())
        {
            reverseMap.erase(iter);
            return Descriptor<T>::null();
        }
        return Descriptor<T>(std::get<0>(std::get<1>(*iter)));
    }
    template <typename T>
    pair<Descriptor<T>, bool> findOrMake(shared_ptr<T> value)
    {
        lock_guard<recursive_mutex> lockIt(theLock);
        auto iter = reverseMap.find(static_cast<const void *>(value.get()));
        if(iter == reverseMap.end())
        {
            Descriptor<T> descriptor;
            return make_pair(descriptor, set<T>(descriptor, value));
        }
        std::get<1>(std::get<1>(*iter)) = value;
        return make_pair(Descriptor<T>(std::get<0>(std::get<1>(*iter))), true);
    }
};

template <typename T>
inline shared_ptr<T> read(Reader &reader, VariableSet &variableSet)
{
    VariableSet::Descriptor<T> descriptor = read<VariableSet::Descriptor<T>>(reader);
    if(!descriptor)
        return nullptr;
    shared_ptr<T> retval = variableSet.get(descriptor);
    bool modified = read<bool>(reader);
    if(retval != nullptr && !modified)
        return retval;
    retval = T::read(reader, variableSet);
    variableSet.set(descriptor, retval);
    return retval;
}

template <typename T>
inline void write(Writer &writer, VariableSet &variableSet, shared_ptr<T> value, bool modified = false)
{
    if(value == nullptr)
    {
        write<VariableSet::Descriptor<T>>(writer, VariableSet::Descriptor<T>::null());
        return;
    }
    pair<VariableSet::Descriptor<T>, bool> findOrMakeReturnValue = variableSet.findOrMake<T>(value);
    write<VariableSet::Descriptor<T>>(writer, std::get<0>(findOrMakeReturnValue));
    write<bool>(writer, modified || !std::get<1>(findOrMakeReturnValue));
    if(std::get<1>(findOrMakeReturnValue) && !modified)
    {
        return;
    }
    value->write(writer, variableSet);
}


#endif // VARIABLE_SET_H_INCLUDED
