#pragma once

#include <atomic>
#include <memory>
#include <unordered_map>
#include <stddef.h>
#include <Core/Types.h>
#include <Common/LabelledMetrics.h>
#include <Common/VariableContext.h>
#include <metric_helper.h>

/** Implements global counters for various events happening in the application
  *  - for high level profiling.
  * See .cpp for list of events.
  */

namespace ProfileEvents
{
    /// Event identifier (index in array).
    using Event = size_t;
    using Count = size_t;
    using Counter = std::atomic<Count>;
    class Counters;

    /// Counters - how many times each event happened
    extern Counters global_counters;

    class Counters
    {
        Counter * counters = nullptr;
        std::unique_ptr<Counter[]> counters_holder;
        /// Used to propagate increments
        Counters * parent = nullptr;

    public:

        VariableContext level = VariableContext::Thread;

        /// By default, any instance have to increment global counters
        Counters(VariableContext level = VariableContext::Thread, Counters * parent = &global_counters);

        /// Global level static initializer
        Counters(Counter * allocated_counters)
            : counters(allocated_counters), parent(nullptr), level(VariableContext::Global) {}

        Counter & operator[] (Event event)
        {
            return counters[event];
        }

        const Counter & operator[] (Event event) const
        {
            return counters[event];
        }

        inline void increment(Event event, Count amount = 1)
        {
            Counters * current = this;
            do
            {
                current->counters[event].fetch_add(amount, std::memory_order_relaxed);
                current = current->parent;
            } while (current != nullptr);
        }

        uint64_t getIOReadTime(bool use_async_read) const;

        struct Snapshot
        {
            Snapshot();

            const Count & operator[] (Event event) const
            {
                return counters_holder[event];
            }

        private:
            std::unique_ptr<Count[]> counters_holder;

            friend class Counters;
        };

        /// Every single value is fetched atomically, but not all values as a whole.
        Snapshot getPartiallyAtomicSnapshot() const;

        /// Reset all counters to zero and reset parent.
        void reset();

        /// Get parent (thread unsafe)
        Counters * getParent()
        {
            return parent;
        }

        /// Set parent (thread unsafe)
        void setParent(Counters * parent_)
        {
            parent = parent_;
        }

        /// Set all counters to zero
        void resetCounters();

        static const Event num_counters;
    };

    /// Increment a counter for event. Thread-safe
    void increment(Event event, Count amount = 1);

    /// Get name of event by identifier. Returns statically allocated string.
    const char * getName(Event event);

    /// Get name of metric by identifier in snake_case. Returns statically allocated string.
    const DB::String getSnakeName(Event event);

    /// Get description of event by identifier. Returns statically allocated string.
    const char * getDocumentation(Event event);

    /// Get index just after last event identifier.
    Event end();
}
