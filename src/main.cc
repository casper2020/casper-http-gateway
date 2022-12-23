/**
* @file main.cc
*
* Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
*
* This file is part of casper-proxy-worker.
*
* casper-proxy-worker is free software: you can redistribute it and/or modify
* it under the terms of the GNU Affero General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* casper-proxy-worker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with casper-proxy-worker. If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>

#include "version.h"

#include "casper/job/handler.h"
#include "casper/proxy/worker/http/client.h"
#include "casper/proxy/worker/http/oauth2/client.h"

/**
 * @brief Main.
 *
 * param argc
 * param argv
 *
 * return
 */
int main(int argc, char** argv)
{
    const char* short_info = strrchr(CASPER_PROXY_WORKER_INFO, '-');
    if ( nullptr == short_info ) {
        short_info = CASPER_PROXY_WORKER_INFO;
    } else {
        short_info++;
    }

    //
    // LOG FILTERING:
    //
    // tail -f /usr/local/var/log/casper-proxy-worker/oauth2-http-client.1.log
    // tail -f /usr/local/var/log/casper-proxy-worker/http-client.1.log
    //
    // ... run ...
    return ::casper::job::Handler::GetInstance().Start(
        /* a_arguments */
        {
            /* abbr_           */ CASPER_PROXY_WORKER_ABBR,
            /* name_           */ CASPER_PROXY_WORKER_NAME,
            /* version_        */ CASPER_PROXY_WORKER_VERSION,
            /* rel_date_       */ CASPER_PROXY_WORKER_REL_DATE,
            /* rel_branch_     */ CASPER_PROXY_WORKER_REL_BRANCH,
            /* rel_hash_       */ CASPER_PROXY_WORKER_REL_HASH,
            /* rel_target_     */ CASPER_PROXY_WORKER_REL_TARGET,
            /* info_           */ short_info, // short version of CASPER_PROXY_WORKER_INFO
            /* banner_         */ CASPER_PROXY_WORKER_BANNER,
            /* argc_           */ argc,
            /* argv_           */ const_cast<const char** const >(argv),
        },
        /* a_factories */
        {
            {
                ::casper::proxy::worker::http::Client::sk_tube_, [] (const ev::Loggable::Data& a_loggable_data, const cc::easy::job::Job::Config& a_config) -> cc::easy::job::Job* {
                    return new ::casper::proxy::worker::http::Client(a_loggable_data, a_config);
                }
            },
            {
                ::casper::proxy::worker::http::oauth2::Client::sk_tube_, [] (const ev::Loggable::Data& a_loggable_data, const cc::easy::job::Job::Config& a_config) -> cc::easy::job::Job* {
                    return new ::casper::proxy::worker::http::oauth2::Client(a_loggable_data, a_config);
                }
            }
        },
        /* a_polling_timeout */ 20.0 /* milliseconds */
    );
}
