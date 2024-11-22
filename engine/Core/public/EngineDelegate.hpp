#pragma once
#include <Common.hpp>

#include <memory>

enum class ExecutionOrder : uint32_t
{
    FIRST = 0,
    PRE_UPDATE = 5,
    UPDATE = 10,
    POST_UPDATE = 15,
    PRE_RENDER = 20,
    RENDER = 25,
    POST_RENDER = 30,
    LAST = 35
};

class Engine;

// Delegate that models the function void(Engine&)
class EngineDelegate
{
public:
    using FunctionPtr = void (*)(Engine&);

    template <typename T>
    using MemberFunction = void (T::*)(Engine&);

private:
    struct ImplInterface
    {
        ImplInterface() = default;
        virtual ~ImplInterface() = default;
        virtual void Invoke(Engine&) = 0;
    };

    struct FreeFunctionImpl : public ImplInterface
    {
        FreeFunctionImpl(FunctionPtr ptr)
            : fn(ptr)
        {
        }
        virtual ~FreeFunctionImpl() = default;
        virtual void Invoke(Engine& e) { fn(e); }
        FunctionPtr fn {};
    };

    template <typename T>
    struct MemberFunctionImpl : public ImplInterface
    {
        MemberFunctionImpl(T* self, MemberFunction<T> ptr)
            : this_instante(self)
            , fn(ptr)
        {
        }
        virtual ~MemberFunctionImpl() = default;
        virtual void Invoke(Engine& e) { (this_instante->*fn)(e); }
        T* this_instante {};
        MemberFunction<T> fn {};
    };

public:
    EngineDelegate() = default;

    EngineDelegate(FunctionPtr ptr)
        : callable(std::make_unique<FreeFunctionImpl>(ptr))
    {
    }

    template <typename T>
    EngineDelegate(T* self, MemberFunction<T> ptr)
        : callable(std::make_unique<MemberFunctionImpl<T>>(self, ptr))
    {
    }

    void Invoke(Engine& e) { callable->Invoke(e); }

private:
    std::unique_ptr<ImplInterface> callable;
};

// namespace detail
// {

//     template <typename Ret, typename... Args>
//     class InvokeInterface
//     {
//     public:
//         virtual ~InvokeInterface() = default;
//         virtual Ret Invoke(Args&&... args) const = 0;
//     };

//     template <typename Class, typename Ret, typename... Args>
//     class MemberInvokeImpl : public InvokeInterface<Ret, Args...>
//     {
//     public:
//         MemberInvokeImpl(Class* self, Ret (Class::*mem_ptr)(Args...))
//             : self(self)
//             , mem_ptr(mem_ptr)
//         {
//         }
//         ~MemberInvokeImpl() override = default;

//         Class* self = nullptr;
//         Ret (Class::*mem_ptr)(Args...) = nullptr;

//         Ret Invoke(Args&&... args) const override
//         {
//             return (self->*mem_ptr)(std::forward<Args>(args)...);
//         }
//     };

//     template <typename Ret, typename... Args>
//     class FreeFunctionInvokeImpl : public InvokeInterface<Ret, Args...>
//     {
//     public:
//         FreeFunctionInvokeImpl(Ret (*ptr)(Args...))
//             : ptr(ptr)
//         {
//         }
//         ~FreeFunctionInvokeImpl() override = default;

//         Ret (*ptr)(Args...) = nullptr;

//         Ret Invoke(Args&&... args) const override
//         {
//             return ptr(std::forward<Args>(args)...);
//         }
//     };
// }

// template <typename T>
// class Delegate;

// template <typename Ret, typename... Args>
// class Delegate<Ret(Args...)>
// {
//     template <typename Class>
//     using MemberFunctionPtr = Ret (Class::*)(Args...);
//     using FreeFunctionPtr = Ret (*)(Args...);

// public:
//     Delegate(FreeFunctionPtr fn)
//         : invoke(std::make_unique<detail::FreeFunctionInvokeImpl<Ret, Args...>>(fn))
//     {
//     }

//     template <typename Class>
//     Delegate(Class* instance, MemberFunctionPtr<Class> mem_fn)
//     {
//         invoke = std::make_unique<detail::MemberInvokeImpl<Class, Ret, Args...>>(instance, mem_fn);
//     }

//     Ret operator()(Args&&... args) { return invoke->Invoke(std::forward<Args>(args)...); }

// private:
//     std::unique_ptr<detail::InvokeInterface<Ret, Args...>> invoke {};
// };
