#include "log.hpp"
#include "threads.hpp"
#include <__chrono/duration.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <concurrentqueue.h>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <latch>
#include <ratio>
#include <sys/qos.h>
#include <thread>

namespace util
{

    const char* getStringOfLoggingLevel(LoggingLevel level) // NOLINT
    {
        switch (level)
        {
        case LoggingLevel::Trace:
            return "Trace";
        case LoggingLevel::Debug:
            return "Debug";
        case LoggingLevel::Log:
            return " Log ";
        case LoggingLevel::Warn:
            return "Warn ";
        case LoggingLevel::Fatal:
            return "Fatal";
        case LoggingLevel::Panic:
            return "Panic";
        default:
            return "Unknown level";
        }
    }

    class Logger
    {
    public:

        Logger();
        ~Logger();

        Logger(const Logger&)             = delete;
        Logger(Logger&&)                  = delete;
        Logger& operator= (const Logger&) = delete;
        Logger& operator= (Logger&&)      = delete;

        void send(std::string);
        void sendBlocking(std::string);

    private:
        std::unique_ptr<std::atomic<bool>> should_thread_close;
        std::thread                        worker_thread;
        std::unique_ptr<moodycamel::ConcurrentQueue<std::string>> message_queue;
        std::unique_ptr<Mutex<std::ofstream>> log_file_handle;
    };

    Logger::Logger()
        : should_thread_close {std::make_unique<std::atomic<bool>>(false)}
        , message_queue {std::make_unique<
              moodycamel::ConcurrentQueue<std::string>>()}
        , log_file_handle {std::make_unique<util::Mutex<std::ofstream>>(
              std::ofstream {"verdigris_log.txt"})}
    {
        std::latch threadStartLatch {1};

        this->worker_thread = std::thread {
            [this, loggerConstructionLatch = &threadStartLatch]
            {
                const std::ofstream logFileHandle {"verdigris_log.txt"};
                std::string         temporaryString {"INVALID MESSAGE"};

                std::atomic<bool>* shouldThreadStop {
                    this->should_thread_close.get()};

                moodycamel::ConcurrentQueue<std::string>* messageQueue {
                    this->message_queue.get()};

                loggerConstructionLatch->count_down();
                // after this latch:
                // `this` may be dangling
                // loggerConstructionLatch is dangling

                // Main loop
                while (!shouldThreadStop->load(std::memory_order_acquire))
                {
                    if (messageQueue->try_dequeue(temporaryString))
                    {
                        std::ignore = std::fwrite(
                            temporaryString.data(),
                            sizeof(char),
                            temporaryString.size(),
                            stdout);

                        this->log_file_handle->lock(
                            [&](std::ofstream& stream)
                            {
                                std::ignore = stream.write(
                                    temporaryString.c_str(),
                                    static_cast<std::streamsize>(
                                        temporaryString.size()));
                            });
                    }
                }

                // Cleanup loop
                while (messageQueue->try_dequeue(temporaryString))
                {
                    std::ignore = std::fwrite(
                        temporaryString.data(),
                        sizeof(char),
                        temporaryString.size(),
                        stdout);

                    this->log_file_handle->lock(
                        [&](std::ofstream& stream)
                        {
                            std::ignore = stream.write(
                                temporaryString.c_str(),
                                static_cast<std::streamsize>(
                                    temporaryString.size()));
                        });
                }
            }};

        threadStartLatch.wait();
    }
    Logger::~Logger()
    {
        if (this->should_thread_close != nullptr)
        {
            this->should_thread_close->store(true, std::memory_order_release);

            this->worker_thread.join();
        }
    }

    void Logger::send(std::string string)
    {
        if (!this->message_queue->enqueue(std::move(string)))
        {
            std::puts("Unable to send message to worker thread!\n");
            std::ignore =
                std::fwrite(string.data(), sizeof(char), string.size(), stderr);

            this->log_file_handle->lock(
                [&](std::ofstream& stream)
                {
                    std::ignore = stream.write(
                        string.c_str(),
                        static_cast<std::streamsize>(string.size()));
                });
        }
    }

    void Logger::sendBlocking(std::string string)
    {
        std::ignore =
            std::fwrite(string.data(), sizeof(char), string.size(), stderr);

        this->log_file_handle->lock(
            [&](std::ofstream& stream)
            {
                std::ignore = stream.write(
                    string.c_str(),
                    static_cast<std::streamsize>(string.size()));
            });
    }

    namespace
    {
        std::atomic<Logger*>      LOGGER {nullptr};                    // NOLINT
        std::atomic<LoggingLevel> LOGGING_LEVEL {LoggingLevel::Trace}; // NOLINT
    } // namespace

    void installGlobalLoggerRacy()
    {
        LOGGER.store(
            new Logger {}, std::memory_order_seq_cst); // NOLINT: Owning pointer

        // Bro, I don't even remember anymore, I benchmarked it and it was
        // actually like a 10% improvement on reads from being able to use
        // relaxed
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    void removeGlobalLoggerRacy()
    {
        Logger* const currentLogger =
            LOGGER.exchange(nullptr, std::memory_order_seq_cst);

        // doing very similar things with this fence here, we want relaxed
        // reading, but still to actually flush the change
        std::atomic_thread_fence(std::memory_order_seq_cst);

        // NOLINTNEXTLINE
        assert(currentLogger != nullptr && "Logger was already nullptr!");

        delete currentLogger; // NOLINT: Owning pointer
    }

    void setLoggingLevel(LoggingLevel level)
    {
        LOGGING_LEVEL.store(level, std::memory_order_seq_cst);

        // Used for similar reasons as above, we want relaxed loading
        std::atomic_thread_fence(std::memory_order_seq_cst);

        logLog("Set logging level to {}", getStringOfLoggingLevel(level));
    }

    LoggingLevel getCurrentLevel()
    {
        return LOGGING_LEVEL.load(std::memory_order_relaxed);
    }

    void asynchronouslyLog(
        std::string                           message,
        LoggingLevel                          level,
        std::source_location                  location,
        std::chrono::system_clock::time_point time)
    {
        std::string output = std::format(
            "[{0}] [{1}] [{2}] {3}\n",
            [&] // 0 time
            {
                // NOLINTBEGIN
                // I love not having proper string utilities :)
                std::array<char, 32> buf {};

                const std::time_t t =
                    std::chrono::system_clock::to_time_t(time);

                const std::size_t idx = std::strftime(
                    buf.data(),
                    buf.size(),
                    "%b %m/%d/%Y %I:%M:%S",
                    std::localtime(&t)); // NOLINT

                std::array<char, 32> outBuffer {};

                const int milis =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        time.time_since_epoch())
                        .count()
                    % 1000;
                const int micros = time.time_since_epoch().count() % 1000;

                const std::size_t dateStringLength = std::snprintf(
                    outBuffer.data(),
                    outBuffer.size(),
                    "%s:%03u:%03u",
                    buf.data(),
                    milis,
                    micros);

                return std::string {outBuffer.data(), dateStringLength};
                // NOLINTEND
            }(),
            [&] // 1 location
            {
                constexpr std::array<std::string_view, 2> FolderIdentifiers {
                    "/src/", "/inc/"};
                const std::string rawFileName {location.file_name()};

                std::size_t index = std::string::npos;

                for (const std::string_view str : FolderIdentifiers)
                {
                    if (index != std::string::npos)
                    {
                        break;
                    }

                    index = rawFileName.find(str);
                }

                return std::format(
                    "{}:{}:{}",
                    rawFileName.substr(index + 1),
                    location.line(),
                    location.column());
            }(),
            [&] // 2 message level
            {
                return getStringOfLoggingLevel(level);
            }(),
            message);

        if (level >= LoggingLevel::Warn || level == LoggingLevel::Debug)
        {
            LOGGER.load(std::memory_order_relaxed)
                ->sendBlocking(std::move(output));
        }
        else
        {
            // the previous memory fences mean this is fine.
            LOGGER.load(std::memory_order_relaxed)->send(std::move(output));
        }
    }
} // namespace util
