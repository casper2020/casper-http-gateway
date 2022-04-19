/**
 * @file types.h
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
#ifndef CASPER_PROXY_WORKER_HTTP_TYPES_H_
#define CASPER_PROXY_WORKER_HTTP_TYPES_H_

#include "casper/job/deferrable/arguments.h"

#include "cc/non-movable.h"

#include "cc/easy/http/client.h"

#include "json/json.h"

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
            
                class Parameters final : public ::cc::NonMovable
                {
                    
                public: // Data Type(s)

                    typedef struct {
                        ::cc::easy::http::Client::Method      method_;
                        std::string                           url_;
                        std::string                           body_;
                        ::cc::easy::http::Client::Headers     headers_;
                        ::cc::easy::http::Client::Timeouts    timeouts_;
                        bool                                  follow_location_;
                    } HTTPRequest;
                    
                    typedef struct {
                        std::string         uri_;         //!< local file URI.
                        std::string         url_;         //!< URL to access file
                        bool                base64_;      //!< if true will convert do base64
                        int64_t             validity_;    //!< local file validity
                    } HTTPResponse;
                    
                public: // Const Data
                    
                    const Json::Value&                       data_;
                    const bool                               primitive_;
                    const int                                log_level_;
                    const bool                               log_redact_;
                    
                private: // Data
                    
                    HTTPRequest*                             http_req_;      //!< Evaluated HTTP request data.
                    HTTPResponse*                            http_resp_;     //!< Evaluated HTTP response config.
                    
                public: // Constructor(s) / Destructor
                    
                    Parameters () = delete;
                    
                    /**
                     * @brief Default constructor.
                     *
                     * @param a_data       JSON object.
                     * @param a_primitive  True when response should be done in 'primitive' mode.
                     * @param a_log_level  Log level.
                     * @param a_log_redact Log redact flag.
                     */
                    Parameters (const Json::Value& a_data, const bool a_primitive, const int a_log_level, const bool a_log_redact)
                     : data_(a_data), primitive_(a_primitive), log_level_(a_log_level), log_redact_(a_log_redact),
                       http_req_(nullptr), http_resp_(nullptr)
                    {
                        /* empty */
                    }
                    
                    /**
                     * @brief Copy constructor.
                     *
                     * @param a_parameters Object to copy.
                     */
                    Parameters (const Parameters& a_parameters)
                     : data_(a_parameters.data_), primitive_(a_parameters.primitive_), log_level_(a_parameters.log_level_), log_redact_(a_parameters.log_redact_),
                       http_req_(nullptr), http_resp_(nullptr)
                    {
                        if ( nullptr != a_parameters.http_req_ ) {
                            http_req_ = new HTTPRequest(*a_parameters.http_req_);
                        }
                        if ( nullptr != a_parameters.http_resp_ ) {
                            http_resp_ = new HTTPResponse(*a_parameters.http_resp_);
                        }
                    }
                    
                    /**
                     * @brief Default constructor.
                     */
                    virtual ~Parameters ()
                    {
                        if ( nullptr != http_req_ ) {
                            delete http_req_;
                        }
                        if ( nullptr != http_resp_ ) {
                            delete http_resp_;
                        }
                    }

                public: // Overloaded Operator(s)
                    
                    void operator = (Parameters const&)  = delete;  // assignment is not allowed

                public: // Inline Method(s) / Function(s)
                    
                    /**
                     * @return R/O access to HTTP request data.
                     */
                    inline const HTTPRequest& http_request () const
                    {
                        if ( nullptr != http_req_ ) {
                            return *http_req_;
                        }
                        throw cc::InternalServerError("Invalid call to %s!", __PRETTY_FUNCTION__);
                    }
                    
                    /**
                     * @brief Prepare request data, exception for better tracking of variables write acccess.
                     *
                     * @return R/O access to request data.
                     */
                    inline const HTTPRequest& http_request (const std::function<void(HTTPRequest&)>& a_callback)
                    {
                        // ... if doesn't exists yet ...
                        if ( nullptr == http_req_ ) {
                            http_req_ = new HTTPRequest({
                                /* method_          */ ::cc::easy::http::Client::Method::NotSet,
                                /* url_             */ "",
                                /* body_            */ "",
                                /* headers_         */ {},
                                /* timeouts_        */ { -1, -1 },
                                /* follow_location_ */ false
                            });
                        }
                        // ... callback ...
                        a_callback(*http_req_);
                        // ... done ...
                        return *http_req_;
                    }
                                        
                    /**
                     * @return R/O access to HTTP response config.
                     */
                    inline const HTTPResponse& http_response () const
                    {
                        if ( nullptr != http_resp_ ) {
                            return *http_resp_;
                        }
                        throw cc::InternalServerError("Invalid call to %s!", __PRETTY_FUNCTION__);
                    }
                    
                    /**
                     * @return True if a custom HTTP response was configured.
                     */
                    inline bool IsCustomHTTPResponseSet () const
                    {
                        return nullptr != http_resp_;
                    }
                                        
                    /**
                     * @brief Prepare response config, exception for better tracking of variables write acccess.
                     *
                     * @return R/O access to response config.
                     */
                    inline const HTTPResponse& http_response (const std::function<void(HTTPResponse&)>& a_callback)
                    {
                        // ... if doesn't exists yet ...
                        if ( nullptr == http_resp_ ) {
                            http_resp_ = new HTTPResponse({
                                /* uri_               */ "",
                                /* url_               */ "",
                                /* base64_            */ false,
                                /* validity_          */ -1
                            });
                        }
                        // ... callback ...
                        a_callback(*http_resp_);
                        // ... done ...
                        return *http_resp_;
                    }

                }; // end of class 'Parameters'
            
                // MARK: -

                class Arguments final : public ::casper::job::deferrable::Arguments<Parameters>
                {
                    
                public: // Constructor(s) / Destructor
                
                    Arguments() = delete;
                    
                    /**
                     * @brief Default constructor.
                     *
                     * @param a_parameters Parameters.
                     */
                    Arguments (const Parameters& a_parameters)
                     : ::casper::job::deferrable::Arguments<Parameters>(a_parameters)
                    {
                        /* empty */
                    }
                    
                    /**
                     * @brief Default constructor.
                     */
                    virtual ~Arguments ()
                    {
                        /* empty */
                    }
                    
                public: // Method(s) / Function(s)
                    
                    virtual bool Primitive () const { return parameters_.primitive_; }
                    
                }; // end of class 'Arguments'
        
            } // end of namespace 'http'

        } // end of namespace 'worker'
    
    } // end of namespace 'proxy'

} // end of namespace 'casper'

#endif // CASPER_PROXY_WORKER_HTTP_TYPES_H_
