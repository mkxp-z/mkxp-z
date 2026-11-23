// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

#include "harness.h"

#include "display/frametimes.h"

#include <string>

namespace {

/* Each steady time is a sum of powers of 2, so the sums of the frames
 * are exact and a period ends on the same frame from any start time. */
const double budget = 1.0 / 32;

struct Frame
{
    double scripts, draw, sleep, swap;
};

const Frame steady = {8.0 / 1024, 2.0 / 1024, 20.0 / 1024, 2.0 / 1024};

std::string runFrame(FrameTimes &times, double &now, const Frame &frame,
                     double frameBudget = budget, bool drawn = true)
{
    times.mark(FrameTimes::Scripts, now += frame.scripts);
    times.mark(FrameTimes::Draw, now += frame.draw);
    times.mark(FrameTimes::Sleep, now += frame.sleep);
    times.mark(FrameTimes::Swap, now += frame.swap);
    return times.frameDone(frameBudget, drawn);
}

/* Starts the clock of `times` at `now`, as the end of a frame does. */
void start(FrameTimes &times, double now)
{
    times.mark(FrameTimes::Swap, now);
}

std::string runUntilReport(FrameTimes &times, double &now)
{
    for (int i = 0; i < 1000; ++i)
    {
        std::string report = runFrame(times, now, steady);
        if (!report.empty())
            return report;
    }
    return "";
}

bool has(const std::string &text, const char *part)
{
    return text.find(part) != std::string::npos;
}

} // namespace

TEST(frametimes_reports_the_mean_of_each_part)
{
    FrameTimes times;
    double now = 100;
    start(times, now);

    std::string report = runUntilReport(times, now);

    CHECK_EQ(std::string("320 frames in 10.0 s, 32.0 of 32 fps. "
                         "Mean ms: scripts 7.8, draw 2.0, sleep 19.5, swap 2.0."),
             report);
}

TEST(frametimes_gives_no_report_before_the_period_ends)
{
    FrameTimes times;
    double now = 0;
    start(times, now);

    for (int i = 0; i < 320; ++i)
        CHECK(runFrame(times, now, steady).empty());
    CHECK(!runFrame(times, now, steady).empty());
}

TEST(frametimes_leaves_the_time_before_the_first_mark_out)
{
    FrameTimes times;
    double now = 500;

    std::string report = runUntilReport(times, now);

    CHECK(has(report, "32.0 of 32 fps"));
}

TEST(frametimes_gives_the_parts_of_the_late_frames)
{
    FrameTimes times;
    double now = 0;
    start(times, now);

    runFrame(times, now, {0.050, 0.002, 0, 0.008});
    runFrame(times, now, {0.040, 0.002, 0, 0.008});
    std::string report = runUntilReport(times, now);

    CHECK(has(report, " Late: 2 frames, longest 60.0 ms. "
                      "Their sum in ms: scripts 90.0, draw 4.0, sleep 0.0, swap 16.0."));
}

TEST(frametimes_does_not_call_a_frame_late_below_the_limit)
{
    FrameTimes times;
    double now = 0;
    start(times, now);

    runFrame(times, now, {0.040, 0.002, 0, 0.004});
    std::string report = runUntilReport(times, now);

    CHECK(!has(report, "Late"));
}

TEST(frametimes_counts_the_frames_that_were_not_drawn)
{
    FrameTimes times;
    double now = 0;
    start(times, now);

    runFrame(times, now, steady, budget, false);
    runFrame(times, now, steady, budget, false);
    std::string report = runUntilReport(times, now);

    CHECK(has(report, " 2 not drawn."));
}

TEST(frametimes_reports_when_the_frame_rate_changes)
{
    FrameTimes times;
    double now = 0;
    start(times, now);

    for (int i = 0; i < 32; ++i)
        runFrame(times, now, steady);
    std::string report = runFrame(times, now, steady, budget / 3);

    CHECK(has(report, "32 frames in 1.0 s, 32.0 of 32 fps."));

    for (int i = 0; i < 9; ++i)
        runFrame(times, now, steady, budget / 3);
    report = runFrame(times, now, steady, budget);

    CHECK(has(report, "10 frames in 0.3 s, 32.0 of 96 fps."));
    CHECK(has(report, "Late: 10 frames"));
}
