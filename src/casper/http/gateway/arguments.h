/**
 * @file arguments.h
 *
 * Copyright (c) 2011-2021 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-http-gateway.
 *
 * casper-http-gateway is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-http-gateway  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-http-gateway.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef CASPER_HTTP_GATEWAY_ARGUMENTS_H_
#define CASPER_HTTP_GATEWAY_ARGUMENTS_H_

#include "casper/job/deferrable/arguments.h"

#include "ev/curl/object.h" // EV_CURL_HEADERS_MAP

namespace casper
{

    namespace http
    {
    
        namespace gateway
        {

            class Parameters
            {
                
            public: // Const Data
                
                const std::string         method_;
                const std::string         url_;
                const EV_CURL_HEADERS_MAP headers_;
                const std::string         body_;
                
            public: // Constructor(s) / Destructor
                
                Parameters () = delete;
                
                /**
                 * @brief Default constructor.
                 *
                 * @param a_method  HTTP method name.
                 * @param a_url     HTTP URL.
                 * @param a_headers HTTP headers.
                 */
                Parameters (const std::string& a_method, const std::string& a_url, const EV_CURL_HEADERS_MAP& a_headers)
                 : method_(a_method), url_(a_url), headers_(a_headers), body_("")
                {
                    /* empty */
                }
                
                /**
                 * @brief POST constructor.
                 *
                 * @param a_method  HTTP method name.
                 * @param a_url     HTTP URL.
                 * @param a_headers HTTP headers.
                 */
                Parameters (const std::string& a_method, const std::string& a_url, const EV_CURL_HEADERS_MAP& a_headers, const std::string& a_body)
                 : method_(a_method), url_(a_url), headers_(a_headers), body_(a_body)
                {
                    /* empty */
                }
                
                /**
                 * @brief Default constructor.
                 */
                virtual ~Parameters ()
                {
                    /* empty */
                }
                
            public: // Overloaded Operator(s)
                
                void operator = (Parameters const&)  = delete;  // assignment is not allowed
                
            }; // end of class 'Parameters'
        
            typedef ::casper::job::deferrable::Arguments<Parameters> Arguments;

        } // end of namespace 'gateway'
    
    } // end of namespace 'http'

} // end of namespace 'casper'

#endif // CASPER_HTTP_GATEWAY_ARGUMENTS_H_
