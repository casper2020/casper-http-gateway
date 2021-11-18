/**
 * @file version.h
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
 * along with casper-http-gateway. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef CASPER_HTTP_GATEWAY_VERSION_H_
#define CASPER_HTTP_GATEWAY_VERSION_H_

#ifndef CASPER_HTTP_GATEWAY_ABBR
#define CASPER_HTTP_GATEWAY_ABBR "chg"
#endif

#ifndef CASPER_HTTP_GATEWAY_NAME
#define CASPER_HTTP_GATEWAY_NAME "casper-http-gateway@b.n.s@"
#endif

#ifndef CASPER_HTTP_GATEWAY_VERSION
#define CASPER_HTTP_GATEWAY_VERSION "x.x.x"
#endif

#ifndef CASPER_HTTP_GATEWAY_REL_DATE
#define CASPER_HTTP_GATEWAY_REL_DATE "r.r.d"
#endif

#ifndef CASPER_HTTP_GATEWAY_REL_BRANCH
#define CASPER_HTTP_GATEWAY_REL_BRANCH "r.r.b"
#endif

#ifndef CASPER_HTTP_GATEWAY_REL_HASH
#define CASPER_HTTP_GATEWAY_REL_HASH "r.r.h"
#endif

#ifndef CASPER_HTTP_GATEWAY_INFO
#define CASPER_HTTP_GATEWAY_INFO CASPER_HTTP_GATEWAY_NAME " v" CASPER_HTTP_GATEWAY_VERSION
#endif

#define CASPER_HTTP_GATEWAY_BANNER \
"  ____    _    ____  ____  _____ ____       _   _ _____ _____ ____        ____    _  _____ _______        ___ __   __\n" \
" / ___|  / \\  / ___||  _ \\| ____|  _ \\     | | | |_   _|_   _|  _ \\      / ___|  / \\|_   _| ____\\ \\      / / \\\\ \\ / /\n" \
"| |     / _ \\ \\___ \\| |_) |  _| | |_) |____| |_| | | |   | | | |_) |____| |  _  / _ \\ | | |  _|  \\ \\ /\\ / / _ \\\\ V / \n" \
"| |___ / ___ \\ ___) |  __/| |___|  _ <_____|  _  | | |   | | |  __/_____| |_| |/ ___ \\| | | |___  \\ V  V / ___ \\| |  \n" \
" \\____/_/   \\_\\____/|_|   |_____|_| \\_\\    |_| |_| |_|   |_| |_|         \\____/_/   \\_\\_| |_____|  \\_/\\_/_/   \\_\\_|\n"

#endif // CASPER_HTTP_GATEWAY_VERSION_H_
