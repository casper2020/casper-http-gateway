/**
 * @file dispatcher.h
 *
 * Copyright (c) 2011-2021 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-proxy-worker.
 *
 * casper-proxy-worker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-proxy-worker  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-proxy-worker.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef CASPER_PROXY_WORKER_DISPATCHER_H_
#define CASPER_PROXY_WORKER_DISPATCHER_H_

#include "casper/job/deferrable/dispatcher.h"

#include "casper/proxy/worker/types.h"

namespace casper
{

    namespace proxy
    {
    
        namespace worker
        {
            
            class Dispatcher final : public ::casper::job::deferrable::Dispatcher<casper::proxy::worker::Arguments>
            {

            protected: // Const Data

                const ev::Loggable::Data& loggable_data_; //!< reference to loggable data.

            public: // Constructor(s) / Destructor
                
                CC_IF_DEBUG(Dispatcher () = delete;);
                Dispatcher (CC_IF_DEBUG_CONSTRUCT_DECLARE_VAR(const cc::debug::Threading::ThreadID, a_thread_id)) = delete;
                Dispatcher (const ev::Loggable::Data& a_loggable_data
                            CC_IF_DEBUG_CONSTRUCT_APPEND_VAR(const cc::debug::Threading::ThreadID, a_thread_id));
                virtual ~Dispatcher ();

            public: // Inherited Pure Virtual Method(s) / Function(s) - from deferrable::Dispatcher<A>
                
                virtual void Setup (const Json::Value& a_config);
                
            public: // Method(s) / Function(s)

                void Push (const ::casper::job::deferrable::Tracking& a_tracking, const casper::proxy::worker::Arguments& a_args);

            }; // end of class 'Dispatcher'

        } // end of namespace 'worker'
    
    } // end of namespace 'proxy'

} // end of namespace 'casper'

#endif // CASPER_PROXY_WORKER_DISPATCHER_H_
