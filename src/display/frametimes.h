// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

#ifndef FRAMETIMES_H
#define FRAMETIMES_H

#include <cstdio>
#include <string>

class FrameTimes
{
public:
    enum Part
    {
        Scripts,
        Draw,
        Sleep,
        Swap,
        PartCount
    };

    void mark(Part part, double seconds)
    {
        if (running)
            frame[part] += seconds - last;
        last = seconds;
        running = true;
    }

    /* The report does not include this frame. */
    std::string frameDone(double budget, bool drawn)
    {
        std::string report;
        if (frames > 0 && (budget != frameBudget || seconds() >= reportSeconds))
            report = takeReport();
        frameBudget = budget;

        double period = 0;
        for (int part = 0; part < PartCount; ++part)
            period += frame[part];
        bool late = period > lateFactor * budget;
        for (int part = 0; part < PartCount; ++part)
        {
            total[part] += frame[part];
            if (late)
                lateTotal[part] += frame[part];
            frame[part] = 0;
        }
        ++frames;
        if (!drawn)
            ++notDrawn;
        if (late)
            ++lateFrames;
        if (period > longest)
            longest = period;
        return report;
    }

private:
    static constexpr double reportSeconds = 10;
    static constexpr double lateFactor = 1.5;

    double seconds() const
    {
        double sum = 0;
        for (int part = 0; part < PartCount; ++part)
            sum += total[part];
        return sum;
    }

    std::string takeReport()
    {
        double span = seconds();
        double toMeanMs = 1000.0 / frames;
        char text[200];
        std::snprintf(text, sizeof(text),
                      "%d frames in %.1f s, %.1f of %.0f fps. "
                      "Mean ms: scripts %.1f, draw %.1f, sleep %.1f, swap %.1f.",
                      frames, span, span > 0 ? frames / span : 0, 1 / frameBudget,
                      total[Scripts] * toMeanMs, total[Draw] * toMeanMs,
                      total[Sleep] * toMeanMs, total[Swap] * toMeanMs);
        std::string report = text;
        if (notDrawn > 0)
        {
            std::snprintf(text, sizeof(text), " %d not drawn.", notDrawn);
            report += text;
        }
        if (lateFrames > 0)
        {
            std::snprintf(text, sizeof(text),
                          " Late: %d frames, longest %.1f ms. "
                          "Their sum in ms: scripts %.1f, draw %.1f, sleep %.1f, swap %.1f.",
                          lateFrames, longest * 1000,
                          lateTotal[Scripts] * 1000, lateTotal[Draw] * 1000,
                          lateTotal[Sleep] * 1000, lateTotal[Swap] * 1000);
            report += text;
        }

        frames = notDrawn = lateFrames = 0;
        longest = 0;
        for (int part = 0; part < PartCount; ++part)
            total[part] = lateTotal[part] = 0;
        return report;
    }

    double last = 0;
    bool running = false;
    double frame[PartCount] = {};

    double frameBudget = 0;
    int frames = 0;
    int notDrawn = 0;
    int lateFrames = 0;
    double longest = 0;
    double total[PartCount] = {};
    double lateTotal[PartCount] = {};
};

#endif // FRAMETIMES_H
