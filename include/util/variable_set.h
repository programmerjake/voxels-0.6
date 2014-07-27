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
    unordered_map<const void *, size_t> reverseMap;
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
            reverseMap[static_cast<const void *>(value.get())] = descriptor.descriptorIndex;
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
        return Descriptor<T>(std::get<1>(*iter));
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
        return make_pair(Descriptor<T>(std::get<1>(*iter)), true);
    }
};

#endif // VARIABLE_SET_H_INCLUDED
