// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

#include "harness.h"

#include "display/framerateaverage.h"
#include "util/awayclock.h"

#include <chrono>
#include <cmath>

namespace {

const double frame = 1.0 / 60;

/* Shows 60 frames a second from `start` for `count` frames, and gives
 * back the time of the last one. */
double showFrames(FrameRateAverage &rate, double start, int count)
{
    double now = start;
    for (int i = 0; i < count; ++i)
    {
        now += frame;
        rate.frameShown(now);
    }
    return now;
}

bool near60(double fps) { return std::fabs(fps - 60) < 0.01; }

using Clock = AwayClock::Clock;

Clock::time_point at(double seconds)
{
    return Clock::time_point() + std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(seconds));
}

bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

} // namespace

TEST(framerateaverage_reads_the_rate_of_steady_frames)
{
    FrameRateAverage rate;
    showFrames(rate, 0, 100);

    CHECK(near60(rate.average(30)));
}

TEST(framerateaverage_leaves_the_load_time_out_of_the_first_frame)
{
    FrameRateAverage rate;
    showFrames(rate, 20, 10);

    CHECK(near60(rate.average(30)));
}

TEST(framerateaverage_gives_the_fallback_until_two_frames_are_shown)
{
    FrameRateAverage rate;
    CHECK(rate.average(30) == 30);

    rate.frameShown(5);
    CHECK(rate.average(30) == 30);
}

TEST(framerateaverage_counts_a_slow_frame)
{
    FrameRateAverage rate;
    double now = showFrames(rate, 0, 40);

    rate.frameShown(now + 1);

    CHECK(rate.average(30) < 30);
}

TEST(awayclock_stands_still_while_away)
{
    AwayClock clock(at(0));
    clock.leave(at(10));

    CHECK(near(clock.seconds(at(10)), 10));
    CHECK(near(clock.seconds(at(40)), 10));

    clock.comeBack(at(40));
    CHECK(near(clock.seconds(at(41)), 11));
}

TEST(awayclock_counts_overlapping_spans_once)
{
    AwayClock clock(at(0));
    clock.leave(at(10));
    clock.leave(at(15));
    clock.comeBack(at(20));
    CHECK(near(clock.seconds(at(25)), 10));

    clock.comeBack(at(30));
    CHECK(near(clock.seconds(at(31)), 11));
    CHECK(clock.comebacks() == 1);
}

TEST(awayclock_ignores_a_come_back_with_no_leave)
{
    AwayClock clock(at(0));
    clock.comeBack(at(5));
    clock.leave(at(10));
    clock.comeBack(at(20));

    CHECK(near(clock.seconds(at(21)), 11));
    CHECK(clock.comebacks() == 1);
}

TEST(framerateaverage_on_the_away_clock_does_not_count_a_pause)
{
    AwayClock clock(at(0));
    FrameRateAverage rate;
    double now = 0;
    for (int i = 0; i < 100; ++i)
    {
        now += frame;
        rate.frameShown(clock.seconds(at(now)));
    }

    clock.leave(at(now));
    clock.comeBack(at(now + 30));
    now += 30 + frame;
    rate.frameShown(clock.seconds(at(now)));

    CHECK(near60(rate.average(30)));
}
