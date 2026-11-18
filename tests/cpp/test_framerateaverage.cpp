// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

#include "harness.h"

#include "display/framerateaverage.h"

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

} // namespace

TEST(framerateaverage_reads_the_rate_of_steady_frames)
{
    FrameRateAverage rate;
    rate.skip(5);
    showFrames(rate, 5, 100);

    CHECK(near60(rate.average()));
}

TEST(framerateaverage_does_not_count_a_pause_as_a_frame)
{
    FrameRateAverage rate;
    double now = showFrames(rate, 0, 100);

    now += 30;
    rate.skip(30);
    showFrames(rate, now, 1);

    CHECK(near60(rate.average()));
}

TEST(framerateaverage_counts_the_work_before_a_pause)
{
    FrameRateAverage rate;
    double now = showFrames(rate, 0, 40);

    now += 1 + 30;
    rate.skip(30);
    rate.frameShown(now);

    CHECK(rate.average() < 30);
}

TEST(framerateaverage_counts_a_slow_frame)
{
    FrameRateAverage rate;
    double now = showFrames(rate, 0, 40);

    rate.frameShown(now + 1);

    CHECK(rate.average() < 30);
}
