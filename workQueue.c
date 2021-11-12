/*
 * workQueue.c -- part of SpaceRobots2
 * Copyright (C) 2020-2021 Michael Banack <github@banack.net>
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
    SDL_AtomicSet(&wq->anyFinishWaitingCount, 0);

    CMBVector_CreateEmpty(&wq->items, itemSize);

    MBLock_Create(&wq->lock);
    wq->workerSem = SDL_CreateSemaphore(0);
    wq->finishSem = SDL_CreateSemaphore(0);
    wq->anyFinishSem = SDL_CreateSemaphore(0);
}

void WorkQueue_Destroy(WorkQueue *wq)
{
    ASSERT(wq->itemSize != 0);
    ASSERT(SDL_AtomicGet(&wq->finishWaitingCount) == 0);
    ASSERT(SDL_AtomicGet(&wq->anyFinishWaitingCount) == 0);

    CMBVector_Destroy(&wq->items);
    MBLock_Destroy(&wq->lock);
    SDL_DestroySemaphore(wq->workerSem);
    SDL_DestroySemaphore(wq->finishSem);
    SDL_DestroySemaphore(wq->anyFinishSem);

    wq->workerSem = NULL;
    wq->finishSem = NULL;
    wq->anyFinishSem = NULL;
}

void WorkQueue_Lock(WorkQueue *wq)
{
    ASSERT(wq != NULL);
    MBLock_Lock(&wq->lock);
}

void WorkQueue_Unlock(WorkQueue *wq)
{
    ASSERT(wq != NULL);
    MBLock_Unlock(&wq->lock);
}

void WorkQueue_QueueItem(WorkQueue *wq, void *item, uint itemSize)
{
    WorkQueue_Lock(wq);
    WorkQueue_QueueItemLocked(wq, item, itemSize);
    WorkQueue_Unlock(wq);
}


void WorkQueue_QueueItemLocked(WorkQueue *wq, void *item, uint itemSize)
{
    ASSERT(wq != NULL);
    ASSERT(MBLock_IsLocked(&wq->lock));

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

    int res = SDL_SemPost(wq->workerSem);
    ASSERT(res == 0);
}

void WorkQueue_GetItemLocked(WorkQueue *wq, void *item, uint itemSize)
{
    ASSERT(wq != NULL);
    ASSERT(MBLock_IsLocked(&wq->lock));

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

    WorkQueue_Unlock(wq);
    int res = SDL_SemWait(wq->workerSem);
    ASSERT(res == 0);
    WorkQueue_Lock(wq);

    WorkQueue_GetItemLocked(wq, item, itemSize);

    WorkQueue_Unlock(wq);
}

void WorkQueue_FinishItem(WorkQueue *wq)
{
    bool pEmpty;
    bool doLock = FALSE;

    pEmpty = SDL_AtomicDecRef(&wq->numInProgress);

    if (pEmpty || SDL_AtomicGet(&wq->anyFinishWaitingCount) > 0) {
        doLock = TRUE;
    }

    if (doLock) {
        WorkQueue_Lock(wq);
        ASSERT(SDL_AtomicGet(&wq->numInProgress) >= 0);

        if (SDL_AtomicGet(&wq->anyFinishWaitingCount) > 0) {
            SDL_AtomicDecRef(&wq->anyFinishWaitingCount);
            SDL_SemPost(wq->anyFinishSem);
        }

        if (WorkQueue_IsIdle(wq) &&
            SDL_AtomicGet(&wq->finishWaitingCount) > 0) {
            SDL_AtomicDecRef(&wq->finishWaitingCount);
            SDL_SemPost(wq->finishSem);
        }

        WorkQueue_Unlock(wq);
    }
}

void WorkQueue_WaitForAnyFinished(WorkQueue *wq)
{
    bool wait = FALSE;
    ASSERT(wq != NULL);

    /*
     * We don't properly support multi-waiters.
     */
    ASSERT(SDL_AtomicGet(&wq->anyFinishWaitingCount) == 0);

    /*
     * If nothing is queued or in-progress, don't wait.
     */
    if (WorkQueue_IsIdle(wq)) {
        return;
    }

    WorkQueue_Lock(wq);
    if (!WorkQueue_IsIdle(wq)) {
        SDL_AtomicIncRef(&wq->anyFinishWaitingCount);
        wait = TRUE;
    }
    WorkQueue_Unlock(wq);

    if (wait) {
        int error = SDL_SemWait(wq->anyFinishSem);
        if (error != 0) {
            PANIC("Failed to wait for WorkQueue: %s\n", SDL_GetError());
        }
    }
}

void WorkQueue_WaitForAllFinished(WorkQueue *wq)
{
    bool wait = FALSE;
    ASSERT(wq != NULL);

    /*
     * We don't properly support multi-waiters.
     * If things are actively being queued while we're
     * waiting here, it's possible to racily miss a transient
     * state of being empty, or incorrectly detect being empty.
     */
    ASSERT(SDL_AtomicGet(&wq->finishWaitingCount) == 0);

    if (WorkQueue_IsIdle(wq)) {
        return;
    }

    WorkQueue_Lock(wq);
    if (!WorkQueue_IsIdle(wq)) {
        SDL_AtomicIncRef(&wq->finishWaitingCount);
        wait = TRUE;
    }
    WorkQueue_Unlock(wq);

    if (wait) {
        int error = SDL_SemWait(wq->finishSem);
        if (error != 0) {
            PANIC("Failed to wait for WorkQueue: %s\n", SDL_GetError());
        }
    }
}

void WorkQueue_WaitForCountBelow(WorkQueue *wq, uint count)
{
    uint32 waitCount;
    ASSERT(wq != NULL);
    ASSERT(count < MAX_INT32 / 2);

    /*
     * Doesn't work correctly if someone is actively queueing new
     * items, or we have multi-waiters.
     */
    ASSERT(SDL_AtomicGet(&wq->anyFinishWaitingCount) == 0);

    if (WorkQueue_IsCountBelow(wq, count)) {
        return;
    }

    WorkQueue_Lock(wq);
    if (WorkQueue_IsCountBelow(wq, count)) {
        waitCount = 0;
    } else {
        waitCount = WorkQueue_GetCount(wq) - count;
        SDL_AtomicAdd(&wq->anyFinishWaitingCount, waitCount);
    }
    WorkQueue_Unlock(wq);

    while (waitCount > 0) {
        int error = SDL_SemWait(wq->anyFinishSem);
        if (error != 0) {
            PANIC("Failed to wait for WorkQueue: %s\n", SDL_GetError());
        }
        waitCount--;
    }
}

void WorkQueue_MakeEmpty(WorkQueue *wq)
{
    WorkQueue_Lock(wq);
    SDL_AtomicSet(&wq->numInProgress, 0);
    SDL_AtomicSet(&wq->numQueued, 0);
    wq->nextItem = 0;
    WorkQueue_Unlock(wq);
}
