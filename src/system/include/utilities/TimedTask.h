/*
 * Copyright (c) 2008 James Molloy, Jörg Pfähler, Matthew Iselin
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef TIMED_TASK_H
#define TIMED_TASK_H

#include <process/Thread.h>
#include <processor/state.h>
#include <processor/types.h>
#include <process/Semaphore.h>
#include <process/Scheduler.h>

/** TimedTask defines a specific task that needs to be performed
  * within a specific timeframe.
  * Inherit from this and define your own task() function in order
  * to perform different tasks.
  */
class TimedTask : public TimerHandler
{
    public:
    
        TimedTask() :
            m_Nanoseconds(0), m_Seconds(0), m_Timeout(30), m_Success(false),
            m_Active(false), m_ReturnValue(0), m_Task(0), m_WaitSem(0)
        {};
        
        virtual ~TimedTask()
        {
            if(m_Active)
                unregisterMe();
            if(m_WaitSem)
                m_WaitSem->release();
            if(m_Task)
            {
                Process *pProcess = Processor::information().getCurrentThread()->getParent();
                pProcess->removeThread(m_Task);
                delete m_Task;
            }
        }
        
        virtual int task() = 0;
        
        void begin()
        {
            if(m_WaitSem)
            {
                m_Seconds = m_Nanoseconds = 0;
                m_Active = true;
                
                if(m_Timeout)
                {
                    registerMe();
                    
                    Process *pProcess = Processor::information().getCurrentThread()->getParent();
                    m_Task = new Thread(
                                    pProcess,
                                    reinterpret_cast<Thread::ThreadStartFunc>(taskTrampoline),
                                    reinterpret_cast<void *>(this)
                                    );
                }
                else
                {
                    // no timeout!
                    m_ReturnValue = task();
                    m_Success = true;
                    m_WaitSem->release();
                }
            }
            else
                WARNING("TimedTask::begin() called with no semaphore");
        }
        
        void cancel()
        {
            if(m_Task)
            {
                m_Active = false;
                m_Success = false;
            
                unregisterMe();
                
                delete m_Task;
                m_Task = 0;
                
                m_WaitSem->release();
                m_WaitSem = 0;
            }
            else
                ERROR("TimedTask::end() called but no thread is running");
        }
        
        /** Called when the task completes successfully */
        void end(int returnValue)
        {
            m_Active = false;
            
            unregisterMe();
            
            m_Success = true;
            m_ReturnValue = returnValue;
            
            m_WaitSem->release();
            m_WaitSem = 0;
            
            m_Task->setStatus(Thread::Zombie);
            
            while(1)
                Scheduler::instance().yield(0);
        }
        
        static int taskTrampoline(void *ptr)
        {
            TimedTask *t = reinterpret_cast<TimedTask *>(ptr);
            int ret = t->task();
            
            // We don't actually know if we'll ever reach this point, but if we do,
            // we're done :)
            t->end(ret);
            
            // won't reach here, we get killed by end()
            return 0;
        }

        void timer(uint64_t delta, InterruptState &state)
        {
            if(m_Active)
            {
                if(UNLIKELY(m_Seconds < m_Timeout))
                {
                    m_Nanoseconds += delta;
                    if(UNLIKELY(m_Nanoseconds >= 1000000000ULL))
                    {
                        ++m_Seconds;
                        m_Nanoseconds -= 1000000000ULL;
                    }

                    if(UNLIKELY(m_Seconds >= m_Timeout))
                    {
                        cancel();
                    }
                }
            }
        }
        
        void setSemaphore(Semaphore *mySem)
        {
            m_WaitSem = mySem;
        }
        
        bool wasSuccessful()
        {
            return m_Success;
        }
        
        void setTimeout(uint32_t timeout)
        {
            m_Timeout = timeout;
        }
        
        /** Don't use this if wasSuccessful() == false */
        int getReturnValue()
        {
            return m_ReturnValue;
        }
      
    private:
        TimedTask(const TimedTask&);
        const TimedTask& operator = (const TimedTask&);

        void registerMe()
        {
            Timer* t = Machine::instance().getTimer();
            if(t)
                t->registerHandler(this);
            else
            {
                m_WaitSem->release();
                ERROR("TimedTask::begin() with no machine timer!");
            }
        }
        
        void unregisterMe()
        {
            Timer* t = Machine::instance().getTimer();
            if(t)
                t->unregisterHandler(this);
        }
    
        uint64_t m_Nanoseconds;
        uint64_t m_Seconds;

    protected:
        uint32_t m_Timeout; // defaults to 30 seconds
        
    private:

        bool m_Success;
        bool m_Active;

        int m_ReturnValue;

        Thread *m_Task;

        /** Passed to us by the caller, this is how the caller knows that this code
        * has completed executing (whether it's cancelled or completed successfully
        * is another thing :) )
        */
        Semaphore *m_WaitSem;
};

#endif

