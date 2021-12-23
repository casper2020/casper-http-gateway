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
#ifndef CASPER_PROXY_WORKER_HTTP_OAUTH2_DEFERRED_H_
#define CASPER_PROXY_WORKER_HTTP_OAUTH2_DEFERRED_H_

#include "casper/job/deferrable/deferred.h"

#include "casper/proxy/worker/http/oauth2/types.h"

#include "cc/easy/http.h"
#include "cc/bitwise_enum.h"

#include <vector>

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

                    class Deferred final : public ::casper::job::deferrable::Deferred<casper::proxy::worker::http::oauth2::Arguments>
                    {
                        
                    protected: // Alias
                        
                        using DeferredBaseClasss = ::casper::job::deferrable::Deferred<casper::proxy::worker::http::oauth2::Arguments>;

                    private: // Data Type(s)

                        enum class Operation : uint8_t {
                            NotSet = 0x00,
                            LoadTokens = 0x01,
                            RestartOAuth2,
                            PerformRequest,
                            SaveTokens
                        };
                        
                        enum class HTTPOptions : uint8_t {
                            NotSet    = 0,
                            Log       = 1 << 0,
                            Trace     = 1 << 1,
                            Redact    = 1 << 2,
                            OAuth2    = 1 << 3,
                            NonOAuth2 = 1 << 4
                        };
                        
                        typedef struct {
                            const uint16_t    code_; //!< != 0 it's a response
                            const std::string data_; //!< if code is NOT 0 it's request data otherwise it's response.
                        } HTTPTrace;

                    private: // Const Data

                        const ev::Loggable::Data&                       loggable_data_;

                    private: // Helper(s)

                        ::cc::easy::HTTPClient*                         http_;
                        ::cc::easy::OAuth2HTTPClient*                   http_oauth2_;
                        HTTPOptions                                     http_options_;
                        std::vector<HTTPTrace>                          http_trace_;

                    private: // Data

                        Operation                                       current_;               //!< Current operation.
                        std::vector<Operation>                          operations_;            //!< Chained operations.
                        std::string                                     operation_str_;         //!< Current operation, string representation.
                        std::map<Operation, job::deferrable::Response>  responses_;             //!< Operations responses.
                        bool                                            allow_oauth2_restart_;  //!< Mainly for grant_type 'client_credentials' or 'authorization_code-auto'.

                    public: // Constructor(s) / Destructor

                        Deferred (const ::casper::job::deferrable::Tracking& a_tracking, const ev::Loggable::Data& a_loggable_data
                                  CC_IF_DEBUG_CONSTRUCT_APPEND_VAR(const cc::debug::Threading::ThreadID, a_thread_id));
                        virtual ~Deferred ();

                    public: // Inherited Method(s) / Function(s) - from deferrable::Deferred<A>

                        virtual void Run (const casper::proxy::worker::http::oauth2::Arguments& a_args, Callbacks a_callbacks);

                    private: // Method(s) / Function(s)

                        void ScheduleLoadTokens     (const bool a_track, const char* const a_origin, const size_t a_delay);
                        void ScheduleSaveTokens     (const bool a_track, const char* const a_origin, const size_t a_delay);
                        void ScheduleAuthorization  (const bool a_track, const char* const a_origin, const size_t a_delay);
                        void SchedulePerformRequest (const bool a_track, const char* const a_origin, const size_t a_delay);
                        void Finalize               (const std::string& a_tag);

                    private: // Method(s) / Function(s) - HTTP && OAuth2 HTTP Client Request(s) Callbacks

                        void OnOAuth2TokensChanged  ();
                        void OnHTTPRequestCompleted (const ::cc::easy::HTTPClient::RawValue& a_value);
                        void OnHTTPRequestError     (const ::cc::easy::HTTPClient::RawError& a_error);
                        void OnHTTPRequestFailure   (const ::cc::Exception& a_exception);
                        
                    private: // Method(s) / Function(s) - HTTP && OAuth2 HTTP Client Logging Callbacks

                        void OnLogHTTPRequest (const ::ev::curl::Request&, const std::string&);
                        void OnLogHTTPValue   (const ::ev::curl::Value&, const std::string&);

                        void LogHTTPOAuth2ClientRequest  (const ::ev::curl::Request&, const std::string&);
                        void LogHTTPOAuth2ClientValue    (const ::ev::curl::Value&, const std::string&);
                        
                    private: // Method(s) / Function(s) - HTTP && OAuth2 HTTP Client Debug Callbacks
                        
                        CC_IF_DEBUG(
                            void LogHTTPOAuth2ClientDebug    (const ::ev::curl::Request&, const std::string&);
                            void LogHTTPOAuth2ClientProgress (const ::ev::curl::Request&, const uint8_t, const bool);
                        )

                    private: //  Method(s) / Function(s) - HTTP && OAuth2 HTTP Clients Callback(s)
                        
                        void OnHTTPRequestWillRunLogIt (const ::ev::curl::Request&, const std::string&, const HTTPOptions);
                        void OnHTTPRequestSteppedLogIt (const ::ev::curl::Value&, const std::string&, const HTTPOptions);

                    }; // end of class 'Deferred'

                    inline std::string MakeID (const ::casper::job::deferrable::Tracking& a_tracking)
                    {
                        return a_tracking.rcid_;
                    }
                
                    DEFINE_ENUM_WITH_BITWISE_OPERATORS(Deferred::HTTPOptions);
                    
                } // end of namespace 'oauth2'
    
            } // end of namespace 'http'

        } // end of namespace 'worker'
    
    } // end of namespace 'proxy'

} // end of namespace 'casper'

#endif // CASPER_PROXY_WORKER_HTTP_OAUTH2_DEFERRED_H_
