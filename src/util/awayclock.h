// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

#ifndef AWAYCLOCK_H
#define AWAYCLOCK_H

#include <chrono>

/* Seconds since start, less the time the engine was away. Spans that
 * overlap, such as a pause during a game dialog, count once. */
class AwayClock
{
public:
    using Clock = std::chrono::steady_clock;

    explicit AwayClock(Clock::time_point start) : start(start) {}

    void leave(Clock::time_point now)
    {
        if (depth++ == 0)
            leftAt = now;
    }

    void comeBack(Clock::time_point now)
    {
        if (depth == 0)
            return;
        if (--depth == 0)
        {
            away += now - leftAt;
            ++comebackCount;
        }
    }

    unsigned comebacks() const { return comebackCount; }

    double seconds(Clock::time_point now) const
    {
        Clock::duration total = away;
        if (depth > 0)
            total += now - leftAt;
        return std::chrono::duration<double>(now - start - total).count();
    }

private:
    Clock::time_point start;
    Clock::time_point leftAt;
    Clock::duration away{};
    int depth = 0;
    unsigned comebackCount = 0;
};

#endif // AWAYCLOCK_H
