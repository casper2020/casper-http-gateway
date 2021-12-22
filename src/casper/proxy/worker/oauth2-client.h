/**
 * @file oauth2-client.h
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
#ifndef CASPER_PROXY_WORKER_OAUTH2_CLIENT_H_
#define CASPER_PROXY_WORKER_OAUTH2_CLIENT_H_

#include "casper/job/deferrable/base.h"
#include "casper/proxy/worker/types.h"
#include "casper/proxy/worker/deferred.h"

#include "casper/proxy/worker/v8/script.h"

#include <string>
#include <map>

namespace casper
{

    namespace proxy
    {
    
        namespace worker
        {

            // MARK: -

            enum class OAuth2ClientStep : uint8_t {
                Fetching = 5,
                DoingIt  = 95,
                Done     = 100
            };

            // MARK: -

            class OAuth2Client final : public ::casper::job::deferrable::Base<Arguments, OAuth2ClientStep, OAuth2ClientStep::Done>
            {

            public: // Static Const Data
                
	        static           const char* const sk_tube_;
                constexpr static const long        sk_storage_connection_timeout_ = 30;
                constexpr static const long        sk_storage_operation_timeout_  = 60;
                static           const Json::Value sk_behaviour_;

            private: // Data
                
                std::map<std::string, proxy::worker::Config*> providers_;
                casper::proxy::worker::v8::Script*            script_;
                
            public: // Constructor(s) / Destructor
                
                OAuth2Client () = delete;
                OAuth2Client (const ev::Loggable::Data& a_loggable_data, const cc::easy::job::Job::Config& a_config);
                virtual ~OAuth2Client ();

            protected: // Inherited Virtual Method(s) / Function(s) - ::casper::job::deferrable::Base<OAuth2ClientStep, OAuth2ClientStep::Done>
                
                virtual void InnerSetup ();
                virtual void InnerRun   (const int64_t& a_id, const Json::Value& a_payload, cc::easy::job::Job::Response& o_response);

            private: // Method(s) / Function(s) - deferrable::Dispatcher Callbacks
                
                uint16_t OnDeferredRequestCompleted (const ::casper::job::deferrable::Deferred<casper::proxy::worker::Arguments>* a_deferred, Json::Value& o_payload);
                uint16_t OnDeferredRequestFailed    (const ::casper::job::deferrable::Deferred<casper::proxy::worker::Arguments>* a_deferred, Json::Value& o_payload);
                
                
            private: // Method(s) / Function(s) - Schedule Helper(s)

                ::cc::easy::OAuth2HTTPClient::GrantType TranslatedGrantType (const std::string& a_name);

                void SetupGrantRequest (const ::casper::job::deferrable::Tracking& a_tracking,
                                        const casper::proxy::worker::Config& a_provider, casper::proxy::worker::Arguments& a_arguments, casper::proxy::worker::Parameters::GrantAuthCodeRequest& a_auth_code,
                                        Json::Value& o_v8_data);
                void SetupHTTPRequest  (const ::casper::job::deferrable::Tracking& a_tracking,
                                        const casper::proxy::worker::Config& a_provider, casper::proxy::worker::Arguments& a_arguments, casper::proxy::worker::Parameters::HTTPRequest& a_request,
                                        Json::Value& o_v8_data);

            private: // Method(s) / Function(s) - V8 Helper(s)
                
                void Evaluate (const uint64_t& a_id   , const std::string& a_expression, const Json::Value& a_data, std::string& o_value) const;
                void Evaluate (const std::string& a_id, const std::string& a_expression, const Json::Value& a_data, std::string& o_value) const;

            }; // end of class 'OAuth2Client'
        
        } // end of namespace 'worker'
    
    } // end of namespace 'proxy'

} // end of namespace 'casper'

#endif // CASPER_PROXY_WORKER_OAUTH2_CLIENT_H_
