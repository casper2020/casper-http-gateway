/**
 * @file script.h
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
 * along with casper-proxy-worker. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef CASPER_PROXY_WORKER_V8_SCRIPT_H_
#define CASPER_PROXY_WORKER_V8_SCRIPT_H_

#include "cc/v8/basic/evaluator.h"
#include "cc/v8/value.h"

#include "cc/macros.h"

#include "cc/non-movable.h"
#include "cc/non-copyable.h"

namespace casper
{
    
    namespace proxy
    {
        
        namespace worker
        {
            
            namespace v8
            {

                class Script final : public ::cc::v8::basic::Evaluator
                {

                public: // Constructor(s) / Destructor
                    
                    Script () = delete;
                    Script (const std::string& a_owner, const std::string& a_name, const std::string& a_uri,
                            const std::string& a_out_path);
                    virtual ~Script ();
                                        
                }; // end of class 'Script'
                
            } // end of namespace 'v8'
            
        } // end of namespace 'worker'
        
    } // end of namespace 'proxy'
    
} // end of namespace 'casper'

#endif // CASPER_PROXY_WORKER_V8_SCRIPT_H_
