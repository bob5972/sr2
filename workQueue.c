/*
 * workQueue.c -- part of SpaceRobots2
 * Copyright (C) 2020 Michael Banack <github@banack.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "workQueue.h"

void WorkQueue_Create(WorkQueue *wq, uint itemSize)
{
    ASSERT(wq != NULL);
    MBUtil_Zero(wq, sizeof(wq));

    ASSERT(itemSize != 0);

    wq->itemSize = itemSize;
    wq->nextItem = 0;
    wq->numQueued = 0;
    SDL_AtomicSet(&wq->numInProgress, 0);
    wq->workerWaitingCount = 0;
    wq->finishWaitingCount = 0;

    CMBVector_CreateEmpty(&wq->items, itemSize);

    wq->lock = SDL_CreateMutex();
    wq->workerSignal = SDL_CreateCond();
    wq->finishSignal = SDL_CreateCond();
}

void WorkQueue_Destroy(WorkQueue *wq)
{
    ASSERT(wq->itemSize != 0);
    ASSERT(wq->workerWaitingCount == 0);
    ASSERT(wq->finishWaitingCount == 0);

    CMBVector_Destroy(&wq->items);
    SDL_DestroyMutex(wq->lock);
    SDL_DestroyCond(wq->workerSignal);
    SDL_DestroyCond(wq->finishSignal);

    wq->lock = NULL;
    wq->workerSignal = NULL;
    wq->finishSignal = NULL;
}

void WorkQueue_Lock(WorkQueue *wq)
{
    int c;

    ASSERT(wq != NULL);
    c = SDL_LockMutex(wq->lock);
    if (c != 0) {
        PANIC("Failed to lock mutex: error=%d\n", c);
    }
}

void WorkQueue_Unlock(WorkQueue *wq)
{
    int c;

    ASSERT(wq != NULL);
    c = SDL_UnlockMutex(wq->lock);
    if (c != 0) {
        PANIC("Failed to unlock mutex: error=%d\n", c);
    }
}

void WorkQueue_QueueItem(WorkQueue *wq, void *item, uint itemSize)
{
    WorkQueue_Lock(wq);
    WorkQueue_QueueItemLocked(wq, item, itemSize);
    WorkQueue_Unlock(wq);
}


void WorkQueue_QueueItemLocked(WorkQueue *wq, void *item, uint itemSize)
{
    //XXX ASSERT isLocked?

    ASSERT(wq->itemSize == itemSize);
    uint32 newLastIndex = wq->nextItem + wq->numQueued;

    if (newLastIndex >= CMBVector_Size(&wq->items)) {
        CMBVector_Grow(&wq->items);
    }

    ASSERT(newLastIndex < CMBVector_Size(&wq->items));
    void *ptr = CMBVector_GetPtr(&wq->items, newLastIndex);
    memcpy(ptr, item, wq->itemSize);

    wq->numQueued++;

    if (wq->workerWaitingCount > 0) {
        wq->workerWaitingCount--;
        SDL_CondSignal(wq->workerSignal);
    }
}

void WorkQueue_GetItemLocked(WorkQueue *wq, void *item, uint itemSize)
{
    //XXX ASSERT isLocked ?

    ASSERT(wq->itemSize == itemSize);
    ASSERT(wq->numQueued + wq->nextItem <= CMBVector_Size(&wq->items));
    ASSERT(wq->numQueued > 0);

    void *ptr = CMBVector_GetPtr(&wq->items, wq->nextItem);
    memcpy(item, ptr, wq->itemSize);
    wq->nextItem++;
    wq->numQueued--;

    if (wq->numQueued == 0) {
        wq->nextItem = 0;
    }

    SDL_AtomicIncRef(&wq->numInProgress);
}

void WorkQueue_WaitForItem(WorkQueue *wq, void *item, uint itemSize)
{
    WorkQueue_Lock(wq);

    ASSERT(wq->itemSize == itemSize);
    ASSERT(wq->numQueued + wq->nextItem <= CMBVector_Size(&wq->items));

    while (wq->numQueued == 0) {
        wq->workerWaitingCount++;
        SDL_CondWait(wq->workerSignal, wq->lock);
    }

    WorkQueue_GetItemLocked(wq, item, itemSize);

    WorkQueue_Unlock(wq);
}

void WorkQueue_FinishItem(WorkQueue *wq)
{
    bool pEmpty;

    pEmpty = SDL_AtomicDecRef(&wq->numInProgress);

    if (pEmpty) {
        WorkQueue_Lock(wq);
        ASSERT(SDL_AtomicGet(&wq->numInProgress) >= 0);

        if (wq->numQueued == 0 &&
            SDL_AtomicGet(&wq->numInProgress) == 0 &&
            wq->finishWaitingCount > 0) {
            wq->finishWaitingCount = 0;
            SDL_CondBroadcast(wq->finishSignal);
        }

        WorkQueue_Unlock(wq);
    }
}

void WorkQueue_WaitForAllFinished(WorkQueue *wq)
{
    WorkQueue_Lock(wq);

    /*
     * If we were briefly finished, and then a new work unit
     * gets queued, we might not actually wake-up here.
     */
    while (wq->numQueued > 0 ||
           SDL_AtomicGet(&wq->numInProgress) > 0) {
        wq->finishWaitingCount++;
        SDL_CondWait(wq->finishSignal, wq->lock);
    }

    WorkQueue_Unlock(wq);
}

int WorkQueue_QueueSizeLocked(WorkQueue *wq)
{
    //XXX: ASSERT isLocked ?
    return wq->numQueued;
}

int WorkQueue_QueueSize(WorkQueue *wq)
{
    int retVal;
    WorkQueue_Lock(wq);
    retVal = WorkQueue_QueueSizeLocked(wq);
    WorkQueue_Unlock(wq);
    return retVal;
}

bool WorkQueue_IsEmpty(WorkQueue *wq)
{
    return WorkQueue_QueueSize(wq) == 0;
}

void WorkQueue_MakeEmpty(WorkQueue *wq)
{
    WorkQueue_Lock(wq);
    SDL_AtomicSet(&wq->numInProgress, 0);
    wq->numQueued = 0;
    wq->nextItem = 0;
    WorkQueue_Unlock(wq);
}
