/**
 * @file client.h
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
#ifndef CASPER_PROXY_WORKER_HTTP_CLIENT_H_
#define CASPER_PROXY_WORKER_HTTP_CLIENT_H_

#include "casper/job/deferrable/base.h"

#include "casper/proxy/worker/http/types.h"
#include "casper/proxy/worker/http/deferred.h"

#include <string>
#include <map>

namespace casper
{

    namespace proxy
    {
    
        namespace worker
        {

            namespace http
            {
        
                // MARK: -

                enum class ClientStep : uint8_t {
                    Fetching = 5,
                    DoingIt  = 95,
                    Done     = 100
                };

                // MARK: -

                class Client final : public ::casper::job::deferrable::Base<Arguments, ClientStep, ClientStep::Done>
                {
                    
                    using ClientBaseClass = ::casper::job::deferrable::Base<Arguments, ClientStep, ClientStep::Done>;

                public: // Static Const Data
                    
                    static const char* const             sk_tube_;
                    static             const Json::Value sk_behaviour_;

                public: // Constructor(s) / Destructor
                    
                    Client () = delete;
                    Client (const ev::Loggable::Data& a_loggable_data, const cc::easy::job::Job::Config& a_config);
                    virtual ~Client ();

                protected: // Inherited Virtual Method(s) / Function(s) - ::casper::job::deferrable::Base<ClientStep, ClientStep::Done>
                    
                    virtual void InnerSetup ();
                    virtual void InnerRun   (const uint64_t& a_id, const Json::Value& a_payload, cc::easy::job::Job::Response& o_response);
                
                private: // Method(s) / Function(s) - deferrable::Dispatcher Callbacks
                    
                    uint16_t OnDeferredRequestCompleted (const ::casper::job::deferrable::Deferred<worker::http::Arguments>* a_deferred, Json::Value& o_payload);
                    uint16_t OnDeferredRequestFailed    (const ::casper::job::deferrable::Deferred<worker::http::Arguments>* a_deferred, Json::Value& o_payload);

                }; // end of class 'Client'
                        
            } // end of namespace 'http'
                
        } // end of namespace 'worker'
    
    } // end of namespace 'proxy'

} // end of namespace 'casper'

#endif // CASPER_PROXY_WORKER_HTTP_CLIENT_H_
