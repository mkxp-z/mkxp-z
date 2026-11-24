// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

#include "framedeadline.h"

#include "app_bridge.h"

/* This header declares the `Rect` of MacTypes.h, so no file that
 * includes etc.h can include it. */
#include <AudioToolbox/AudioWorkInterval.h>
#include <cstdio>
#include <mach/mach_time.h>
#include <os/object.h>
#include <os/workgroup.h>

namespace
{

/* The game thread sleeps for most of each frame. iOS reads that as
 * light work and keeps the thread on the efficiency cores at a low
 * clock speed. The user-interactive QoS class, a fixed priority and the
 * real-time policy do not change that. A work interval does: iOS sets
 * the speed of the thread from the time that the work has until its
 * deadline. On an A15 at 40 frames per second, the scripts of a map
 * frame take 7 ms with no interval, 8 ms with a deadline of one frame,
 * 2.6 ms with a quarter of a frame, and 1.4 ms with an eighth.
 * The interval must not hold the sleep of the limiter. With the sleep,
 * each interval ends at its deadline and iOS has no steady signal: the
 * same scripts then take from 2 ms to 11 ms. */
class FrameDeadline
{
public:
    /* A thread that ends while it is joined is undefined behavior. */
    ~FrameDeadline()
    {
        if (state == Open)
            (void)os_workgroup_interval_finish(interval, nullptr);
        if (state == Joined || state == Open)
            os_workgroup_leave(interval, &token);
        if (interval)
            os_release(interval);
    }

    void start(uint64_t deadlineNanoseconds)
    {
        /* Open: the last interval did not finish, and iOS does not
         * start a second one. */
        if (state == Off || state == Open)
            return;

        bool first = state == New;
        int error = first ? join() : 0;
        if (error == 0) {
            uint64_t now = mach_absolute_time();
            uint64_t deadline = now + deadlineNanoseconds * timebase.denom / timebase.numer;
            error = os_workgroup_interval_start(interval, now, deadline, nullptr);
            if (error == 0)
                state = Open;
        }
        if (first) {
            char message[64];
            std::snprintf(message, sizeof(message), "frame work interval: %s (%d)",
                          error == 0 ? "on" : "off", error);
            mkxp_debugLog("INFO", "frame-deadline", message);
        }
    }

    void end()
    {
        if (state == Open && os_workgroup_interval_finish(interval, nullptr) == 0)
            state = Joined;
    }

private:
    enum State
    {
        New,
        Off,
        Joined,
        Open
    };

    int join()
    {
        mach_timebase_info(&timebase);
        interval = AudioWorkIntervalCreate("frame", OS_CLOCK_MACH_ABSOLUTE_TIME, nullptr);
        int error = interval ? os_workgroup_join(interval, &token) : -1;
        state = error == 0 ? Joined : Off;
        return error;
    }

    State state = New;
    os_workgroup_interval_t interval = nullptr;
    os_workgroup_join_token_s token{};
    mach_timebase_info_data_t timebase{};
};

thread_local FrameDeadline frameDeadline;

} // namespace

void startFrameWork(uint64_t deadlineNanoseconds)
{
    frameDeadline.start(deadlineNanoseconds);
}

void endFrameWork()
{
    frameDeadline.end();
}
