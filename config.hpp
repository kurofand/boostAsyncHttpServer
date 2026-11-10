#pragma once

#ifndef HEADERS_SIZE_LIMIT
#define HEADERS_SIZE_LIMIT 1024*16
#endif

#ifndef CONTENT_LENGTH_LIMIT
#define CONTENT_LENGTH_LIMIT 1024*1024
#endif

#ifndef KEEP_ALIVE_TIMEOUT
#define KEEP_ALIVE_TIMEOUT 10
#endif

#ifndef HEADERS_TIMEOUT
#define HEADERS_TIMEOUT 3
#endif

#ifndef BODY_IDLE_TIMEOUT
#define BODY_IDLE_TIMEOUT 3
#endif

#ifndef MAX_CLIENTS
#define MAX_CLIENTS 64
#endif

#ifndef MAX_PENDING_CLIENTS
#define MAX_PENDING_CLIENTS 128
#endif

#ifndef MIN_LOG_LEVEL
#define MIN_LOG_LEVEL 1
#endif

#ifndef READ_CHUNK_SIZE
#define READ_CHUNK_SIZE 1024
#endif
