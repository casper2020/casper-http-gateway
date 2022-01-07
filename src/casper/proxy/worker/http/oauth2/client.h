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
#ifndef CASPER_PROXY_WORKER_HTTP_OAUTH2_CLIENT_H_
#define CASPER_PROXY_WORKER_HTTP_OAUTH2_CLIENT_H_

#include "casper/job/deferrable/base.h"

#include "casper/proxy/worker/http/oauth2/types.h"
#include "casper/proxy/worker/http/oauth2/deferred.h"

#include "casper/proxy/worker/v8/script.h"

#include "cc/crypto/rsa.h"

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
            
                namespace oauth2
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
                        
                    private: // Data Type(s)
                        
                        struct RejectedHeadersComparator {
                            bool operator () (const std::string& a_lhs, const std::string& a_rhs)
                            {
                                return strcasecmp(a_lhs.c_str(), a_rhs.c_str()) < 0;
                            }
                        };
                        typedef std::set<std::string, RejectedHeadersComparator> RejectedHeadersSet;

                    public: // Static Const Data
                        
                        static           const char* const        sk_tube_;
                        static           const Json::Value        sk_behaviour_;
                        static           const RejectedHeadersSet sk_rejected_headers_;
                        constexpr static const long               sk_storage_connection_timeout_ = 30;
                        constexpr static const long               sk_storage_operation_timeout_  = 60;
                    private: // Data
                        
                        std::map<std::string, proxy::worker::http::oauth2::Config*> providers_;
                        
                    public: // Constructor(s) / Destructor
                        
                        Client () = delete;
                        Client (const ev::Loggable::Data& a_loggable_data, const cc::easy::job::Job::Config& a_config);
                        virtual ~Client ();

                    protected: // Inherited Virtual Method(s) / Function(s) - ::casper::job::deferrable::Base<ClientStep, ClientStep::Done>
                        
                        virtual void InnerSetup ();
                        virtual void InnerRun   (const int64_t& a_id, const Json::Value& a_payload, cc::easy::job::Job::Response& o_response);

                    private: // Method(s) / Function(s) - deferrable::Dispatcher Callbacks
                        
                        uint16_t OnDeferredRequestCompleted (const ::casper::job::deferrable::Deferred<casper::proxy::worker::http::oauth2::Arguments>* a_deferred, Json::Value& o_payload);
                        uint16_t OnDeferredRequestFailed    (const ::casper::job::deferrable::Deferred<casper::proxy::worker::http::oauth2::Arguments>* a_deferred, Json::Value& o_payload);
                                                
                    private: // Method(s) / Function(s) - Schedule Helper(s)

                        ::cc::easy::http::oauth2::Client::GrantType TranslatedGrantType        (const std::string& a_name);
                        ::cc::crypto::RSA::SignOutputFormat         TranslatedSignOutputFormat (const std::string& a_name);

                        void SetupGrantRequest (const ::casper::job::deferrable::Tracking& a_tracking,
                                                const casper::proxy::worker::http::oauth2::Config& a_provider, casper::proxy::worker::http::oauth2::Arguments& a_arguments, casper::proxy::worker::http::oauth2::Parameters::GrantAuthCodeRequest& a_auth_code,
                                                Json::Value& o_v8_data);
                        void SetupHTTPRequest  (const ::casper::job::deferrable::Tracking& a_tracking,
                                                const casper::proxy::worker::http::oauth2::Config& a_provider, casper::proxy::worker::http::oauth2::Arguments& a_arguments, casper::proxy::worker::http::oauth2::Parameters::HTTPRequest& a_request,
                                                casper::proxy::worker::v8::Script& a_script,
                                                Json::Value& o_v8_data);

                    private: // Method(s) / Function(s) - V8 Helper(s)
                        
                        void Evaluate (const uint64_t& a_id   , const std::string& a_expression, const Json::Value& a_data, std::string& o_value, casper::proxy::worker::v8::Script& a_script) const;
                        void Evaluate (const std::string& a_id, const std::string& a_expression, const Json::Value& a_data, std::string& o_value, casper::proxy::worker::v8::Script& a_script) const;
                        
                        void ValidateScopes (const std::string& a_requested, const std::string& a_allowed) const;

                    }; // end of class 'Client'
                
                } // end of namespace 'oauth2'
            
            } // end of namespace 'http'
                
        } // end of namespace 'worker'
    
    } // end of namespace 'proxy'

} // end of namespace 'casper'

#endif // CASPER_PROXY_WORKER_HTTP_OAUTH2_CLIENT_H_
