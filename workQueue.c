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
    SDL_AtomicSet(&wq->numQueued, 0);
    SDL_AtomicSet(&wq->numInProgress, 0);
    SDL_AtomicSet(&wq->finishWaitingCount, 0);
    wq->workerWaitingCount = 0;

    CMBVector_CreateEmpty(&wq->items, itemSize);

    wq->lock = SDL_CreateMutex();
    wq->workerSignal = SDL_CreateCond();
    wq->finishSem = SDL_CreateSemaphore(0);
}

void WorkQueue_Destroy(WorkQueue *wq)
{
    ASSERT(wq->itemSize != 0);
    ASSERT(wq->workerWaitingCount == 0);
    ASSERT(SDL_AtomicGet(&wq->finishWaitingCount) == 0);

    CMBVector_Destroy(&wq->items);
    SDL_DestroyMutex(wq->lock);
    SDL_DestroyCond(wq->workerSignal);
    SDL_DestroySemaphore(wq->finishSem);

    wq->lock = NULL;
    wq->workerSignal = NULL;
    wq->finishSem = NULL;
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
    uint numQueued = SDL_AtomicGet(&wq->numQueued);
    int32 newLastIndex = wq->nextItem + numQueued;

    ASSERT(newLastIndex >= 0);
    ASSERT(newLastIndex >= wq->nextItem);
    ASSERT(newLastIndex >= numQueued);

    if (newLastIndex >= CMBVector_Size(&wq->items)) {
        CMBVector_Grow(&wq->items);
    }
    ASSERT(newLastIndex < CMBVector_Size(&wq->items));

    void *ptr = CMBVector_GetPtr(&wq->items, newLastIndex);
    memcpy(ptr, item, wq->itemSize);

    numQueued++;
    SDL_AtomicSet(&wq->numQueued, numQueued);

    if (wq->workerWaitingCount > 0) {
        wq->workerWaitingCount--;
        SDL_CondSignal(wq->workerSignal);
    }
}

void WorkQueue_GetItemLocked(WorkQueue *wq, void *item, uint itemSize)
{
    //XXX ASSERT isLocked ?
    uint numQueued = SDL_AtomicGet(&wq->numQueued);

    ASSERT(wq->itemSize == itemSize);
    ASSERT(numQueued + wq->nextItem <= CMBVector_Size(&wq->items));
    ASSERT(numQueued > 0);

    void *ptr = CMBVector_GetPtr(&wq->items, wq->nextItem);
    memcpy(item, ptr, wq->itemSize);
    wq->nextItem++;
    numQueued--;
    SDL_AtomicSet(&wq->numQueued, numQueued);

    if (numQueued == 0) {
        wq->nextItem = 0;
    }

    SDL_AtomicIncRef(&wq->numInProgress);
}

void WorkQueue_WaitForItem(WorkQueue *wq, void *item, uint itemSize)
{
    WorkQueue_Lock(wq);

    ASSERT(wq->itemSize == itemSize);
    ASSERT(SDL_AtomicGet(&wq->numQueued) + wq->nextItem <=
           CMBVector_Size(&wq->items));

    while (SDL_AtomicGet(&wq->numQueued) == 0) {
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

        if (SDL_AtomicGet(&wq->numQueued) == 0 &&
            SDL_AtomicGet(&wq->numInProgress) == 0 &&
            SDL_AtomicGet(&wq->finishWaitingCount) > 0) {
            uint count = SDL_AtomicGet(&wq->finishWaitingCount);
            while (count > 0) {
                SDL_AtomicAdd(&wq->finishWaitingCount, -1);
                SDL_SemPost(wq->finishSem);
                count--;
            }
        }

        WorkQueue_Unlock(wq);
    }
}

void WorkQueue_WaitForAllFinished(WorkQueue *wq)
{
    ASSERT(wq != NULL);

    /*
     * If things are actively being queued while we're
     * waiting here, it's possible to racily miss a transient
     * state of being empty, or incorrectly detect being empty.
     */
    if (SDL_AtomicGet(&wq->numQueued) == 0 &&
        SDL_AtomicGet(&wq->numInProgress) == 0) {
        return;
    }

    SDL_AtomicAdd(&wq->finishWaitingCount, 1);
    int error = SDL_SemWait(wq->finishSem);
    if (error != 0) {
        PANIC("Failed to wait for WorkQueue: %s\n", SDL_GetError());
    }
}

int WorkQueue_QueueSizeLocked(WorkQueue *wq)
{
    return SDL_AtomicGet(&wq->numQueued);
}

int WorkQueue_QueueSize(WorkQueue *wq)
{
    return WorkQueue_QueueSizeLocked(wq);
}

bool WorkQueue_IsEmpty(WorkQueue *wq)
{
    return WorkQueue_QueueSize(wq) == 0;
}

void WorkQueue_MakeEmpty(WorkQueue *wq)
{
    WorkQueue_Lock(wq);
    SDL_AtomicSet(&wq->numInProgress, 0);
    SDL_AtomicSet(&wq->numQueued, 0);
    wq->nextItem = 0;
    WorkQueue_Unlock(wq);
}
