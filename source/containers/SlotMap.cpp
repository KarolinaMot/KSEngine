#include "SlotMap.hpp"

#include <memory>

void KS::Tests::TestSlotMap()
{
    SlotMap<int> IntCache;

    auto key = IntCache.Insert(14);

    if (auto val = IntCache.Get(key))
    {
        if (*val != 14)
        {
            throw;
        }
    }
    else
    {
        throw;
    }

    IntCache.Erase(key);

    if (IntCache.Contains(key))
    {
        throw;
    }

    auto key2 = IntCache.Insert(31);

    if (IntCache.Size() != 1)
    {
        throw;
    }

    SlotMap<std::shared_ptr<float>> FloatCache;

    auto float_elem = std::make_shared<float>(13.8f);
    auto key3 = FloatCache.Insert(float_elem);

    if (float_elem.use_count() != 2)
    {
        throw;
    }

    FloatCache.Erase(key3);

    if (float_elem.use_count() != 1)
    {
        throw;
    }
}
