/**
* @file main.cc
*
* Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
*
* This file is part of casper-http-gateway.
*
* casper-http-gateway is free software: you can redistribute it and/or modify
* it under the terms of the GNU Affero General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* casper-http-gateway is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with casper-http-gateway. If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>

#include "version.h"

#include "casper/job/handler.h"
#include "casper/http/gateway/oauth2-client.h"

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
    const char* short_info = strrchr(CASPER_HTTP_GATEWAY_INFO, '-');
    if ( nullptr == short_info ) {
        short_info = CASPER_HTTP_GATEWAY_INFO;
    } else {
        short_info++;
    }

    //
    // LOG FILTERING:
    //
    // tail -f /usr/local/var/log/casper-http-proxy/gw-oauth2-http-client.1.log
    //
    // ... run ...
    return ::casper::job::Handler::GetInstance().Start(
        /* a_arguments */
        {
            /* abbr_           */ CASPER_HTTP_GATEWAY_ABBR,
            /* name_           */ CASPER_HTTP_GATEWAY_NAME,
            /* version_        */ CASPER_HTTP_GATEWAY_VERSION,
            /* rel_date_       */ CASPER_HTTP_GATEWAY_REL_DATE,
            /* rel_branch_     */ CASPER_HTTP_GATEWAY_REL_BRANCH,
            /* rel_hash_       */ CASPER_HTTP_GATEWAY_REL_HASH,
            /* info_           */ short_info, // short version of CASPER_HTTP_GATEWAY_INFO
            /* banner_         */ CASPER_HTTP_GATEWAY_BANNER,
            /* argc_           */ argc,
            /* argv_           */ const_cast<const char** const >(argv),
        },
        /* a_factories */
        {
            {
                ::casper::http::gateway::OAuth2Client::sk_tube_, [] (const ev::Loggable::Data& a_loggable_data, const cc::easy::job::Job::Config& a_config) -> cc::easy::job::Job* {
                    return new ::casper::http::gateway::OAuth2Client(a_loggable_data, a_config);
                }
            },
        },
        /* a_polling_timeout */ 20.0 /* milliseconds */
    );
}
