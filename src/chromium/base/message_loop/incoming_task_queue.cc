// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/incoming_task_queue.h"

#include <limits>
#include <utility>

#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {
namespace internal {

namespace {

#if DCHECK_IS_ON()
// Delays larger than this are often bogus, and a warning should be emitted in
// debug builds to warn developers.  http://crbug.com/450045
const int kTaskDelayWarningThresholdInSeconds =
    14 * 24 * 60 * 60;  // 14 days.
#endif

// Returns true if MessagePump::ScheduleWork() must be called one
// time for every task that is added to the MessageLoop incoming queue.
bool AlwaysNotifyPump(MessageLoop::Type type) {
#if defined(OS_ANDROID)
  // The Android UI message loop needs to get notified each time a task is
  // added
  // to the incoming queue.
  return type == MessageLoop::TYPE_UI || type == MessageLoop::TYPE_JAVA;
#else
  return false;
#endif
}

TimeTicks CalculateDelayedRuntime(TimeDelta delay) {
  TimeTicks delayed_run_time;
  if (delay > TimeDelta())
    delayed_run_time = TimeTicks::Now() + delay;
  else
    DCHECK_EQ(delay.InMilliseconds(), 0) << "delay should not be negative";
  return delayed_run_time;
}

}  // namespace

IncomingTaskQueue::IncomingTaskQueue(MessageLoop* message_loop)
    : always_schedule_work_(AlwaysNotifyPump(message_loop->type())),
      message_loop_(message_loop) {
  // The constructing sequence is not necessarily the running sequence in the
  // case of base::Thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

bool IncomingTaskQueue::AddToIncomingQueue(const Location& from_here,
                                           OnceClosure task,
                                           TimeDelta delay,
                                           bool nestable) {
  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task);
  DLOG_IF(WARNING,
          delay.InSeconds() > kTaskDelayWarningThresholdInSeconds)
      << "Requesting super-long task delay period of " << delay.InSeconds()
      << " seconds from here: " << from_here.ToString();

  PendingTask pending_task(from_here, std::move(task),
                           CalculateDelayedRuntime(delay), nestable);
#if defined(OS_WIN)
  // We consider the task needs a high resolution timer if the delay is
  // more than 0 and less than 32ms. This caps the relative error to
  // less than 50% : a 33ms wait can wake at 48ms since the default
  // resolution on Windows is between 10 and 15ms.
  if (delay > TimeDelta() &&
      delay.InMilliseconds() < (2 * Time::kMinLowResolutionThresholdMs)) {
    pending_task.is_high_res = true;
  }
#endif
  return PostPendingTask(&pending_task);
}

bool IncomingTaskQueue::IsIdleForTesting() {
  AutoLock lock(incoming_queue_lock_);
  return incoming_queue_.empty();
}

int IncomingTaskQueue::ReloadWorkQueue(TaskQueue* work_queue) {
  // Make sure no tasks are lost.
  DCHECK(work_queue->empty());

  // Acquire all we can from the inter-thread queue with one lock acquisition.
  AutoLock lock(incoming_queue_lock_);
  if (incoming_queue_.empty()) {
    // If the loop attempts to reload but there are no tasks in the incoming
    // queue, that means it will go to sleep waiting for more work. If the
    // incoming queue becomes nonempty we need to schedule it again.
    message_loop_scheduled_ = false;
  } else {
    incoming_queue_.swap(*work_queue);
  }
  // Reset the count of high resolution tasks since our queue is now empty.
  int high_res_tasks = high_res_task_count_;
  high_res_task_count_ = 0;
  return high_res_tasks;
}

void IncomingTaskQueue::WillDestroyCurrentMessageLoop() {
  {
    AutoLock auto_lock(incoming_queue_lock_);
    accept_new_tasks_ = false;
  }
  {
    AutoLock auto_lock(message_loop_lock_);
    message_loop_ = nullptr;
  }
}

void IncomingTaskQueue::StartScheduling() {
  bool schedule_work;
  {
    AutoLock lock(incoming_queue_lock_);
    DCHECK(!is_ready_for_scheduling_);
    DCHECK(!message_loop_scheduled_);
    is_ready_for_scheduling_ = true;
    schedule_work = !incoming_queue_.empty();
  }
  if (schedule_work) {
    DCHECK(message_loop_);
    // Don't need to lock |message_loop_lock_| here because this function is
    // called by MessageLoop on its thread.
    message_loop_->ScheduleWork();
  }
}

IncomingTaskQueue::~IncomingTaskQueue() {
  // Verify that WillDestroyCurrentMessageLoop() has been called.
  DCHECK(!message_loop_);
}

void IncomingTaskQueue::RunTask(PendingTask* pending_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_annotator_.RunTask("MessageLoop::PostTask", pending_task);
}

bool IncomingTaskQueue::PostPendingTask(PendingTask* pending_task) {
  // Warning: Don't try to short-circuit, and handle this thread's tasks more
  // directly, as it could starve handling of foreign threads.  Put every task
  // into this queue.
  bool accept_new_tasks;
  bool schedule_work = false;
  {
    AutoLock auto_lock(incoming_queue_lock_);
    accept_new_tasks = accept_new_tasks_;
    if (accept_new_tasks)
      schedule_work = PostPendingTaskLockRequired(pending_task);
  }

  if (!accept_new_tasks) {
    // Clear the pending task outside of |incoming_queue_lock_| to prevent any
    // chance of self-deadlock if destroying a task also posts a task to this
    // queue.
    DCHECK(!schedule_work);
    pending_task->task.Reset();
    return false;
  }

  // Wake up the message loop and schedule work. This is done outside
  // |incoming_queue_lock_| to allow for multiple post tasks to occur while
  // ScheduleWork() is running. For platforms (e.g. Android) that require one
  // call to ScheduleWork() for each task, all pending tasks may serialize
  // within the ScheduleWork() call. As a result, holding a lock to maintain the
  // lifetime of |message_loop_| is less of a concern.
  if (schedule_work) {
    // Ensures |message_loop_| isn't destroyed while running.
    AutoLock auto_lock(message_loop_lock_);
    if (message_loop_)
      message_loop_->ScheduleWork();
  }

  return true;
}

bool IncomingTaskQueue::PostPendingTaskLockRequired(PendingTask* pending_task) {
  incoming_queue_lock_.AssertAcquired();

#if defined(OS_WIN)
  if (pending_task->is_high_res)
    ++high_res_task_count_;
#endif

  // Initialize the sequence number. The sequence number is used for delayed
  // tasks (to facilitate FIFO sorting when two tasks have the same
  // delayed_run_time value) and for identifying the task in about:tracing.
  pending_task->sequence_num = next_sequence_num_++;

  task_annotator_.DidQueueTask("MessageLoop::PostTask", *pending_task);

  bool was_empty = incoming_queue_.empty();
  incoming_queue_.push(std::move(*pending_task));

  if (is_ready_for_scheduling_ &&
      (always_schedule_work_ || (!message_loop_scheduled_ && was_empty))) {
    // After we've scheduled the message loop, we do not need to do so again
    // until we know it has processed all of the work in our queue and is
    // waiting for more work again. The message loop will always attempt to
    // reload from the incoming queue before waiting again so we clear this
    // flag in ReloadWorkQueue().
    message_loop_scheduled_ = true;
    return true;
  }
  return false;
}

}  // namespace internal
}  // namespace base
