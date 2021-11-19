/**
 * @file arguments.h
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
#ifndef CASPER_PROXY_WORKER_ARGUMENTS_H_
#define CASPER_PROXY_WORKER_ARGUMENTS_H_

#include "casper/job/deferrable/arguments.h"

#include "json/json.h"


namespace casper
{

    namespace proxy
    {
    
        namespace worker
        {

            class Parameters
            {
                
            public: // Const Data
                
                const Json::Value& request_;
                
            public: // Constructor(s) / Destructor
                
                Parameters () = delete;
                
                /**
                 * @brief Default constructor.
                 *
                 * @param a_request JSON object.
                 */
                Parameters (const Json::Value& a_request)
                 : request_(a_request)
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

        } // end of namespace 'worker'
    
    } // end of namespace 'worker'

} // end of namespace 'casper'

#endif // CASPER_PROXY_WORKER_ARGUMENTS_H_
