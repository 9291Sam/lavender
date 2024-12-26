#include "ecs/entity.hpp"
#include "ecs/entity_component_system_manager.hpp"
#include "ecs/raw_entity.hpp"
#include "game/game.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include "util/thread_pool.hpp"
#include "verdigris/verdigris.hpp"
#include "voxel/structures.hpp"
#include <atomic>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <cassert>
#include <ctti/type_id.hpp>
#include <glm/gtx/hash.hpp>
#include <string_view>
#include <utility>

//

// // Threadsafe, efficieint
// class EntityManager
// {
// public:

//     [[nodiscard]] u32 createEntity() const;
//     void              destroyEntity(u32) const;

//     [[nodiscard]] bool isEntityAlive() const;

//     void withEntityALiveGuard(u32 entity, std::invocable<> auto func) {}

// private:
//     std::atomic<u32> next_valid_entity_id;
// };

// Entity Id Allocator
// Entity -> Component Map
// Component Data Storage

// class ConcreteEntity final : DERIVE_INHERENT_ENTITY(ConcreteEntity, entity)
// {
// public:
//     explicit ConcreteEntity(ecs::UniqueEntity entity_, int health_ = 100)
//         : name {"Unknown"}
//         , health {health_}
//         , entity {std::move(entity_)}
//     {}

//     [[nodiscard]] int getHealth() const
//     {
//         return this->health;
//     }
//     void setHealth(int health_)
//     {
//         this->health = health_;
//     }

//     [[nodiscard]] std::string_view getName() const
//     {
//         return this->name;
//     }
//     void setName(std::string_view newName)
//     {
//         this->name = newName;
//     }

// private:
//     std::string       name;
//     int               health;
//     ecs::UniqueEntity entity;
// };

// int main()
// {
//     util::installGlobalLoggerRacy();
//     util::installGlobalThreadPoolRacy();
//     ecs::installGlobalECSManagerRacy();

//     try
//     {
//         int f = 0;

//         util::logLog(
//             "starting lavender {}.{}.{}.{}{} {}",
//             LAVENDER_VERSION_MAJOR,
//             LAVENDER_VERSION_MINOR,
//             LAVENDER_VERSION_PATCH,
//             LAVENDER_VERSION_TWEAK,
//             util::isDebugBuild() ? " | Debug Build" : "",
//             f);

//         game::Game game {};

//         game.loadGameState<verdigris::Verdigris>();

//         game.run();
//     }
//     catch (const std::exception& e)
//     {
//         util::logFatal("Lavender has crashed! | {} {}", e.what(), typeid(e).name());
//     }
//     catch (...)
//     {
//         util::logFatal("Lavender has crashed! | Non Exception Thrown!");
//     }

//     util::logLog("lavender exited successfully!");

//     ecs::removeGlobalECSManagerRacy();
//     util::removeGlobalThreadPoolRacy();
//     util::removeGlobalLoggerRacy();

//     return EXIT_SUCCESS;
// }

#include <chrono>
#include <iostream>
#include <numeric>
#include <shared_mutex>
#include <thread>
#include <vector>

i64 a = 4;

class ReadLockBenchmark
{
    std::shared_mutex    mutex;
    static constexpr int ITERATIONS      = 1000000;
    static constexpr int THREAD_COUNTS[] = {1, 2, 4, 8, 16};

public:
    double runBenchmark(int numThreads, bool shared)
    {
        std::vector<std::thread> threads;
        std::vector<double>      durations(numThreads);

        for (int i = 0; i < numThreads; i++)
        {
            threads.emplace_back(
                [&, i]
                {
                    auto start = std::chrono::high_resolution_clock::now();

                    for (int j = 0; j < ITERATIONS; j++)
                    {
                        // foo = ITERATIONS;

                        if (shared)
                        {
                            std::shared_lock<decltype(this->mutex)> lock(mutex);
                        }
                        else
                        {
                            std::unique_lock<decltype(this->mutex)> lock(mutex);

                            for (int iters = 0; iters < 64; ++iters)
                            {
                                if (a % 2 == 0)
                                {
                                    a += j;
                                }
                            }
                        }
                        // Simulate brief read operation
                        // std::this_thread::yield();
                    }

                    auto end = std::chrono::high_resolution_clock::now();
                    durations[i] =
                        std::chrono::duration<double, std::nano>(end - start).count() / ITERATIONS;
                });
        }

        for (auto& thread : threads)
        {
            thread.join();
        }

        return std::accumulate(durations.begin(), durations.end(), 0.0) / numThreads;
    }

    void runAllBenchmarks(bool shared)
    {
        std::cout << "Thread Count | Avg Time (ns)\n";
        std::cout << "------------|-------------\n";

        for (int threadCount : THREAD_COUNTS)
        {
            double avgTime = runBenchmark(threadCount, shared);
            std::cout << std::setw(11) << threadCount << " | " << std::setw(11) << std::fixed
                      << std::setprecision(2) << avgTime << "\n";
        }
    }
};

int main()
{
    ReadLockBenchmark benchmark;
    benchmark.runAllBenchmarks(false);
    benchmark.runAllBenchmarks(true);
    return 0;
}