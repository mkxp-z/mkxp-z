// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

#ifndef FRAMERATEAVERAGE_H
#define FRAMERATEAVERAGE_H

#include <deque>
#include <numeric>

class FrameRateAverage
{
public:
    void frameShown(double seconds)
    {
        if (intervals.size() > 40)
            intervals.pop_front();

        intervals.push_back(seconds - lastFrame);
        lastFrame = seconds;
    }

    /* Leaves a pause out of the next frame, so that the pause does not
     * count as one long frame. Games divide by
     * Graphics.average_frame_rate to keep a speed per second, and a rate
     * near 0 makes that step huge. */
    void skip(double seconds) { lastFrame += seconds; }

    double average() const
    {
        double total = std::accumulate(intervals.begin(), intervals.end(), 0.0);
        return intervals.size() / total;
    }

private:
    std::deque<double> intervals;
    double lastFrame = 0;
};

#endif // FRAMERATEAVERAGE_H
