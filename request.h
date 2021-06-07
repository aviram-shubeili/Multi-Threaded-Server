#ifndef __REQUEST_H__

/*
 * ***********************************
 *          ThreadStats struct
 * ***********************************
 */
typedef struct ThreadStats {
    int stat_thread_id;
    int stat_thread_count;
    int stat_thread_static;
    int stat_thread_dynamic;
} ThreadStats;

ThreadStats* thread_statistics;

/*
 * ***********************************
 *          Request struct
 * ***********************************
 */
typedef struct Request {
    int fd;
    struct timeval stat_req_arrival ;
    struct timeval stat_req_dispatch;
} Request;

Request** thread_current_request; // An array of ptrs


void requestHandle(int fd, int handling_thread_id);

#endif
