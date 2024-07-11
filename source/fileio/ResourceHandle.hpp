#pragma once
#include <string>

namespace cereal
{
class access;
}

namespace KS
{

template <typename T>
class ResourceHandle
{
public:
    std::string path;

    bool operator==(const ResourceHandle<T>& other) const
    {
        return path == other.path;
    }

private:
    friend cereal::access;

    template <typename A>
    std::string save_minimal(A& a) const { return path; }

    template <typename A>
    void load_minimal(A& a, const std::string& value) { path = value; }
};

}

namespace std
{

template <typename T>
struct hash<KS::ResourceHandle<T>>
{
    size_t operator()(const KS::ResourceHandle<T>& k) const
    {
        return std::hash<std::string>()(k.path);
    }
};

}