// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

#ifndef FRAMEDEADLINE_H
#define FRAMEDEADLINE_H

#include <cstdint>

/* Call both on the thread that runs the frames. */
void startFrameWork(uint64_t deadlineNanoseconds);
void endFrameWork();

#endif // FRAMEDEADLINE_H
