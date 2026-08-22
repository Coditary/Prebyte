#include "runtime/compiled/CompiledTemplateWriter.h"

#include "io/OutputWriter.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <thread>

namespace prebyte {

struct CompiledTemplateWriter::Impl {
    std::mutex mutex;
    std::condition_variable condition;
    std::condition_variable idle_condition;
    std::deque<std::pair<std::filesystem::path, std::string>> queue;
    std::set<std::filesystem::path> queued_paths;
    bool stop = false;
    bool processing = false;
    std::thread worker;
};

CompiledTemplateWriter& CompiledTemplateWriter::instance() {
    static CompiledTemplateWriter writer;
    return writer;
}

CompiledTemplateWriter::CompiledTemplateWriter()
    : impl_(new Impl()) {
    impl_->worker = std::thread([this]() { run(); });
}

CompiledTemplateWriter::~CompiledTemplateWriter() {
    {
        std::lock_guard lock(impl_->mutex);
        impl_->stop = true;
    }
    impl_->condition.notify_all();
    if (impl_->worker.joinable()) {
        impl_->worker.join();
    }
    delete impl_;
}

void CompiledTemplateWriter::enqueue(std::filesystem::path output_path, std::string bytes) {
    if (output_path.empty()) {
        return;
    }

    {
        std::lock_guard lock(impl_->mutex);
        if (!impl_->queued_paths.insert(output_path).second) {
            return;
        }
        impl_->queue.emplace_back(std::move(output_path), std::move(bytes));
    }
    impl_->condition.notify_one();
}

void CompiledTemplateWriter::wait_idle() {
    std::unique_lock lock(impl_->mutex);
    impl_->idle_condition.wait(lock, [&]() { return impl_->queue.empty() && !impl_->processing; });
}

void CompiledTemplateWriter::run() {
    OutputWriter writer;
    while (true) {
        std::pair<std::filesystem::path, std::string> job;
        {
            std::unique_lock lock(impl_->mutex);
            impl_->condition.wait(lock, [&]() { return impl_->stop || !impl_->queue.empty(); });
            if (impl_->stop && impl_->queue.empty()) {
                return;
            }
            job = std::move(impl_->queue.front());
            impl_->queue.pop_front();
            impl_->processing = true;
        }

        std::error_code error;
        std::filesystem::create_directories(job.first.parent_path(), error);
        try {
            writer.write(job.second, job.first);
        } catch (const std::exception&) { // NOLINT(bugprone-empty-catch)
        }
        {
            std::lock_guard lock(impl_->mutex);
            impl_->queued_paths.erase(job.first);
            impl_->processing = false;
            if (impl_->queue.empty()) {
                impl_->idle_condition.notify_all();
            }
        }
    }
}

}
