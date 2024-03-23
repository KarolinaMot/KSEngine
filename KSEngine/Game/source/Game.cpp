// Game.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <Engine.hpp>
#include <utils/Log.hpp>
#include <iostream>
#include <thread>
#include <chrono>

class TimeModule : public KSE::IModule {
public:
    TimeModule(KSE::EngineClass& e) : KSE::IModule(e) { ResetFrameTime(); }

    void ResetFrameTime() {
        last_tick = std::chrono::high_resolution_clock::now();
    }

    std::chrono::duration<float, std::milli> GetDeltaTime() {
        return std::chrono::high_resolution_clock::now() - last_tick;
    }

private:
    std::chrono::high_resolution_clock::time_point last_tick;

};

class TestSystem : public KSE::ISystem {
public:
    TestSystem(KSE::EngineClass& e) : KSE::ISystem(e) {}

    virtual void Update() override {

        auto time = Engine.GetModule<TimeModule>().lock();

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(10us);

        auto tick_time = time->GetDeltaTime();
        time->ResetFrameTime();
        LOG(KSE::LogType::ERROR, "Seconds since last tick {}", tick_time.count());
    }
};

int main()
{
    KSE::EngineClass engine;
 
    return KSE::EngineClass()
        .AddSystem<TestSystem>()
        .AddModule<TimeModule>()
        .Run();
}
