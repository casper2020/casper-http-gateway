/**
 * @file deferred.h
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
#ifndef CASPER_PROXY_WORKER_DEFERRED_H_
#define CASPER_PROXY_WORKER_DEFERRED_H_

#include "casper/job/deferrable/deferred.h"

#include "casper/proxy/worker/arguments.h"

#include "cc/easy/http.h"

namespace casper
{

    namespace proxy
    {
    
        namespace worker
        {

            class Deferred final : public ::casper::job::deferrable::Deferred<casper::proxy::worker::Arguments>
            {

            protected: // Const Data
                
                const ev::Loggable::Data& loggable_data_; //!<
                
            protected: // Helper(s)
                
                ::cc::easy::HTTPClient    http_; //!< HTTP Client - NO OAuth2 support!

            private: // Data
                
                std::string               method_str_;

            public: // Constructor(s) / Destructor
                
                Deferred (const ::casper::job::deferrable::Tracking& a_tracking, const ev::Loggable::Data& a_loggable_data
                          CC_IF_DEBUG_CONSTRUCT_APPEND_VAR(const cc::debug::Threading::ThreadID, a_thread_id));
                virtual ~Deferred ();

            public: // Inherited Method(s) / Function(s) - from deferrable::Deferred<A>
                
                virtual void Run (const casper::proxy::worker::Arguments& a_args, Callbacks a_callbacks);
                                
            public: // Method(s) / Function(s) - HTTP Request Callbacks

                void OnCompleted (const ::cc::easy::HTTPClient::RawValue& a_value);
                void OnFailure   (const ::cc::Exception& a_exception);

            }; // end of class 'Deferred'
        
            inline std::string MakeID (const ::casper::job::deferrable::Tracking& a_tracking)
            {
                return a_tracking.rcid_;
            }
        
        } // end of namespace 'worker'
    
    } // end of namespace 'worker'

} // end of namespace 'casper'

#endif // CASPER_PROXY_WORKER_DEFERRED_H_
