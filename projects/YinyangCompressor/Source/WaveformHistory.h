#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>

// Audio-thread min/max buckets preserve transients without copying audio to the
// editor. Atomic slots let the UI skip a concurrent overwrite without waiting.
class WaveformHistory
{
public:
    static constexpr double duration = 3;
    struct Frame { double time = 0; float inputMin = 0, inputMax = 0, outputMin = 0, outputMax = 0; };
    void prepare (double rate) noexcept
    {
        samplePeriod = 1 / (std::isfinite (rate) && rate >= 8000 ? rate : 48000);
        bucketSize = std::max (1, static_cast<int> (std::round (1 / (200 * samplePeriod))));
        position = 0; count = 0; pending = {}; latest.store (0);
        for (auto& slot : slots) slot.sequence.store (0);
    }
    void beginBlock (double endTime, int samples) noexcept { time = endTime - samples * samplePeriod; }
    void push (const std::array<float,2>& input, const std::array<float,2>& output, int channels) noexcept
    {
        time += samplePeriod;
        for (int ch = 0; ch < channels; ++ch)
        {
            const auto c = static_cast<size_t> (ch);
            const auto in = std::isfinite (input[c]) ? input[c] : 0.f;
            const auto out = std::isfinite (output[c]) ? output[c] : 0.f;
            if (count == 0 && ch == 0) pending = { time, in, in, out, out };
            pending.inputMin = std::min (pending.inputMin,in); pending.inputMax = std::max (pending.inputMax,in);
            pending.outputMin = std::min (pending.outputMin,out); pending.outputMax = std::max (pending.outputMax,out);
        }
        if (++count < bucketSize) return;
        pending.time = time;
        auto& slot = slots[static_cast<size_t> (position % capacity)];
        slot.sequence.store (position * 2 + 1);
        slot.time.store (pending.time);
        slot.inputMin.store (pending.inputMin); slot.inputMax.store (pending.inputMax);
        slot.outputMin.store (pending.outputMin); slot.outputMax.store (pending.outputMax);
        slot.sequence.store (position * 2 + 2);
        latest.store (++position); count = 0;
    }
    template<typename Visitor> void visit (double now, Visitor&& visitor) const
    {
        const auto end = latest.load();
        const auto begin = end > capacity ? end-capacity : 0;
        for (auto i = begin; i < end; ++i)
        {
            const auto& slot = slots[static_cast<size_t> (i % capacity)];
            const auto expected = i*2+2;
            if (slot.sequence.load() != expected) continue;
            Frame frame {slot.time.load(),slot.inputMin.load(),slot.inputMax.load(),slot.outputMin.load(),slot.outputMax.load()};
            if (slot.sequence.load() != expected) continue;
            const auto age = now-frame.time;
            if (age >= 0 && age <= duration) visitor (static_cast<float> (1-age/duration),frame);
        }
    }
private:
    static constexpr uint64_t capacity = 1024;
    struct Slot
    {
        std::atomic<uint64_t> sequence {0};
        std::atomic<double> time {0};
        std::atomic<float> inputMin {0}, inputMax {0}, outputMin {0}, outputMax {0};
    };
    std::array<Slot,capacity> slots;
    std::atomic<uint64_t> latest {0};
    uint64_t position = 0;
    int count = 0, bucketSize = 240;
    double time = 0, samplePeriod = 1./48000;
    Frame pending;
};
