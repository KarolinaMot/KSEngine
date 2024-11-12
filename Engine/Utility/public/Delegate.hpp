#pragma once
#include <Common.hpp>

#include <memory>

namespace detail
{

template <typename Ret, typename... Args>
class InvokeInterface
{
public:
    virtual ~InvokeInterface() = default;
    virtual Ret Invoke(Args&&... args) const = 0;
};

template <typename Class, typename Ret, typename... Args>
class MemberInvokeImpl : public InvokeInterface<Ret, Args...>
{
public:
    MemberInvokeImpl(Class* self, Ret (Class::*mem_ptr)(Args...))
        : self(self)
        , mem_ptr(mem_ptr)
    {
    }
    ~MemberInvokeImpl() override = default;

    Class* self = nullptr;
    Ret (Class::*mem_ptr)(Args...) = nullptr;

    Ret Invoke(Args&&... args) const override
    {
        return (self->*mem_ptr)(std::forward<Args>(args)...);
    }
};

template <typename Ret, typename... Args>
class FreeFunctionInvokeImpl : public InvokeInterface<Ret, Args...>
{
public:
    FreeFunctionInvokeImpl(Ret (*ptr)(Args...))
        : ptr(ptr)
    {
    }
    ~FreeFunctionInvokeImpl() override = default;

    Ret (*ptr)(Args...) = nullptr;

    Ret Invoke(Args&&... args) const override
    {
        return ptr(std::forward<Args>(args)...);
    }
};

}

template <typename T>
class Delegate;

template <typename Ret, typename... Args>
class Delegate<Ret(Args...)>
{
    template <typename Class>
    using MemberFunctionPtr = Ret (Class::*)(Args...);
    using FreeFunctionPtr = Ret (*)(Args...);

public:
    Delegate(FreeFunctionPtr fn)
        : invoke(std::make_unique<detail::FreeFunctionInvokeImpl<Ret, Args...>>(fn))
    {
    }

    template <typename Class>
    Delegate(Class* instance, MemberFunctionPtr<Class> mem_fn)
    {
        invoke = std::make_unique<detail::MemberInvokeImpl<Class, Ret, Args...>>(instance, mem_fn);
    }

    Ret operator()(Args&&... args) { return invoke->Invoke(std::forward<Args>(args)...); }

private:
    std::unique_ptr<detail::InvokeInterface<Ret, Args...>> invoke {};
};
