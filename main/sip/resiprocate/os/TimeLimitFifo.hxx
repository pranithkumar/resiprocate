#if !defined(RESIP_TimeLimitFifo_hxx)
#define RESIP_TimeLimitFifo_hxx 

static const char* const resipTimeLimitFifo_h_Version =
   "$Id: TimeLimitFifo.hxx,v 1.1 2003/09/24 18:04:09 davidb Exp $";

#include "resiprocate/os/AbstractFifo.hxx"
#include <iostream>
#if defined( WIN32 )
#include <time.h>
#endif

// efficiency note: use a circular buffer to avoid list node allocation

// what happens to timers that can't be queued?

namespace resip
{

template <class Msg>
class TimeLimitFifo : public AbstractFifo
{
   private:
      class Timestamped
      {
         public:
            Timestamped(Msg* msg, time_t n)
               : mMsg(msg),
                 mTime(n)
            {}

            Msg* mMsg;
            time_t mTime;
      };

   public:
      typedef enum {EnforceTimeDepth, IgnoreTimeDepth, InternalElement} DepthUsage;

      TimeLimitFifo(unsigned int maxDurationSecs,
                    unsigned int maxSize);

      virtual ~TimeLimitFifo();
      
      // Add a message to the fifo.
      // return true iff succeeds
      // second arg:
      //    EnforceTimeDepth -- external (non ACK) requests
      //    IgnoreTimeDepth -- external reponse and ACK
      //    InternalElement -- internal messages (timers, application postbacks..); used reseved queue space
      bool add(Msg* msg, DepthUsage usage);

      /** Returns the first message available. It will wait if no
       *  messages are available. If a signal interrupts the wait,
       *  it will retry the wait. Signals can therefore not be caught
       *  via getNext. If you need to detect a signal, use block
       *  prior to calling getNext.
       */
      Msg* getNext();

      /** Return the time depth of the queue. Zero if no depth. */
      time_t timeDepth() const;

      // would an add called now work?
      bool wouldAccept(DepthUsage usage) const;

   private:
      time_t timeDepthInternal() const;
      inline bool wouldAcceptInteral(DepthUsage usage) const;
      time_t mMaxDurationSecs;
      unsigned int mUnreservedMaxSize;
};

template <class Msg>
TimeLimitFifo<Msg>::TimeLimitFifo(unsigned int maxDurationSecs,
                                  unsigned int maxSize)
   : AbstractFifo(maxSize),
     mMaxDurationSecs(maxDurationSecs),
     mUnreservedMaxSize(int(maxSize * 0.8)) // !dlb! random guess
{}

template <class Msg>
TimeLimitFifo<Msg>::~TimeLimitFifo()
{
   Lock lock(mMutex); (void)lock;
   while (!mFifo.empty())
   {
      Timestamped* ts = static_cast<Timestamped*>(mFifo.front());
      delete ts->mMsg;
      delete ts;
      mFifo.pop_front();
   }
}

template <class Msg>
bool
TimeLimitFifo<Msg>::add(Msg* msg,
                        DepthUsage usage)
{
   Lock lock(mMutex); (void)lock;

   if (wouldAcceptInteral(usage))
   {
      time_t n = time(0);
      mFifo.push_back(new Timestamped(msg, n));
      mSize++;
      mCondition.signal();
      return true;
   }
   else
   {
      return false;
   }
}

template <class Msg>
bool
TimeLimitFifo<Msg>::wouldAccept(DepthUsage usage) const
{
   Lock lock(mMutex); (void)lock;

   return wouldAcceptInteral(usage);
}

template <class Msg>
Msg*
TimeLimitFifo<Msg>::getNext()
{
   std::auto_ptr<Timestamped> tm(static_cast<Timestamped*>(AbstractFifo::getNext()));
   return tm->mMsg;
}

template <class Msg>
time_t
TimeLimitFifo<Msg>::timeDepthInternal() const
{
   assert(!empty());

   Timestamped* tm = static_cast<Timestamped*>(mFifo.front());
   return time(0) - tm->mTime;
}   

template <class Msg>
bool
TimeLimitFifo<Msg>::wouldAcceptInteral(DepthUsage usage) const
{
   if ((mMaxSize != 0 &&
        mSize >= mMaxSize))
   {
      return false;
   }

   if (usage == InternalElement)
   {
      return true;
   }

   if (mUnreservedMaxSize != 0 &&
       mSize >= mUnreservedMaxSize)
   {
      return false;
   }

   if (usage == IgnoreTimeDepth)
   {
      return true;
   }

   assert(usage == EnforceTimeDepth);

   if (mSize == 0 ||
       mMaxDurationSecs == 0 ||
       timeDepthInternal() < mMaxDurationSecs)
   {
      return true;
   }

   return false;
}

template <class Msg>
time_t
TimeLimitFifo<Msg>::timeDepth() const
{
   Lock lock(mMutex); (void)lock;

   if (empty())
   {
      return 0;
   }

   Timestamped* tm = static_cast<Timestamped*>(mFifo.front());
   return time(0) - tm->mTime;
}   

} // namespace resip

#endif

/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * 3. The names "VOCAL", "Vovida Open Communication Application Library",
 *    and "Vovida Open Communication Application Library (VOCAL)" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact vocal@vovida.org.
 *
 * 4. Products derived from this software may not be called "VOCAL", nor
 *    may "VOCAL" appear in their name, without prior written
 *    permission of Vovida Networks, Inc.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
 * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES
 * IN EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * 
 * ====================================================================
 * 
 * This software consists of voluntary contributions made by Vovida
 * Networks, Inc. and many individuals on behalf of Vovida Networks,
 * Inc.  For more information on Vovida Networks, Inc., please see
 * <http://www.vovida.org/>.
 *
 */
