/**
 * @file version.h
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
#ifndef CASPER_PROXY_WORKER_VERSION_H_
#define CASPER_PROXY_WORKER_VERSION_H_

#ifndef CASPER_PROXY_WORKER_ABBR
#define CASPER_PROXY_WORKER_ABBR "chg"
#endif

#ifndef CASPER_PROXY_WORKER_NAME
#define CASPER_PROXY_WORKER_NAME "casper-proxy-worker@b.n.s@"
#endif

#ifndef CASPER_PROXY_WORKER_VERSION
#define CASPER_PROXY_WORKER_VERSION "x.x.x"
#endif

#ifndef CASPER_PROXY_WORKER_REL_DATE
#define CASPER_PROXY_WORKER_REL_DATE "r.r.d"
#endif

#ifndef CASPER_PROXY_WORKER_REL_BRANCH
#define CASPER_PROXY_WORKER_REL_BRANCH "r.r.b"
#endif

#ifndef CASPER_PROXY_WORKER_REL_HASH
#define CASPER_PROXY_WORKER_REL_HASH "r.r.h"
#endif

#ifndef CASPER_PROXY_WORKER_REL_TARGET
#define CASPER_PROXY_WORKER_REL_TARGET "r.r.t"
#endif

#ifndef CASPER_PROXY_WORKER_INFO
#define CASPER_PROXY_WORKER_INFO CASPER_PROXY_WORKER_NAME " v" CASPER_PROXY_WORKER_VERSION
#endif

#define CASPER_PROXY_WORKER_BANNER \
"  ____    _    ____  ____  _____ ____       ____  ____   _____  ____   __ __        _____  ____  _  _______ ____  \n" \
" / ___|  / \\  / ___||  _ \\| ____|  _ \\     |  _ \\|  _ \\ / _ \\ \\/ /\\ \\ / / \\ \\      / / _ \\|  _ \\| |/ / ____|  _ \\ \n" \
"| |     / _ \\ \\___ \\| |_) |  _| | |_) |____| |_) | |_) | | | \\  /  \\ V /___\\ \\ /\\ / / | | | |_) | ' /|  _| | |_) |\n" \
"| |___ / ___ \\ ___) |  __/| |___|  _ <_____|  __/|  _ <| |_| /  \\   | |_____\\ V  V /| |_| |  _ <| . \\| |___|  _ < \n" \
" \\____/_/   \\_\\____/|_|   |_____|_| \\_\\    |_|   |_| \\_\\\\___/_/\\_\\  |_|      \\_/\\_/  \\___/|_| \\_\\_|\\_\\_____|_| \\_\\"

#endif // CASPER_PROXY_WORKER_VERSION_H_
