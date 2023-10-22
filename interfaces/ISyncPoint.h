//
// Created by fcors on 10/21/2023.
//

#pragma once


class ISyncPoint {
public:
    virtual ~ISyncPoint() = default;

    /* Used by eventFilter to control sleep/wakeup */
    virtual void haltThreads() = 0;

    virtual void resumeThreads() = 0;

    /* Used by RGSS thread */
    virtual bool mainSyncLocked() = 0;

    virtual void waitMainSync() = 0;

    /* Used by secondary (audio) threads */
    virtual void passSecondarySync() = 0;
};
