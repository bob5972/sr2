/*
 * workQueue.h -- part of SpaceRobots2
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

#ifndef _WORKQUEUE_H_202006241219
#define _WORKQUEUE_H_202006241219

#include "MBVector.h"
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_atomic.h>

typedef struct WorkQueue {
    uint itemSize;
    int nextItem;
    SDL_atomic_t numQueued;
    SDL_atomic_t numInProgress;
    int workerWaitingCount;
    int finishWaitingCount;
    CMBVector items;
    SDL_mutex *lock;
    SDL_cond *workerSignal;
    SDL_cond *finishSignal;
} WorkQueue;

void WorkQueue_Create(WorkQueue *wq, uint itemSize);
void WorkQueue_Destroy(WorkQueue *wq);

void WorkQueue_QueueItem(WorkQueue *wq, void *item, uint itemSize);
void WorkQueue_WaitForItem(WorkQueue *wq, void *item, uint itemSize);
void WorkQueue_FinishItem(WorkQueue *wq);
void WorkQueue_WaitForAllFinished(WorkQueue *wq);

void WorkQueue_Lock(WorkQueue *wq);
void WorkQueue_Unlock(WorkQueue *wq);

void WorkQueue_GetItemLocked(WorkQueue *wq, void *item, uint itemSize);
void WorkQueue_QueueItemLocked(WorkQueue *wq, void *item, uint itemSize);

int WorkQueue_QueueSize(WorkQueue *wq);
int WorkQueue_QueueSizeLocked(WorkQueue *wq);
bool WorkQueue_IsEmpty(WorkQueue *wq);
void WorkQueue_MakeEmpty(WorkQueue *wq);


#endif // _WORKQUEUE_H_202006241219

