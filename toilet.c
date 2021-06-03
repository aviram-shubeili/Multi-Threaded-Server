#include "segel.h"
Mutex m; // initially unlocked
pid_t lock_owner = -1; // -1 means unlocked
int sys_global_lock() {
    lock(m);
    // check if the lock is locked and im the one who locked it
    if(lock_owner == gettid()) {
        unlock(m);
        return EPERM;
    }
    while (global_lock != 0) {
        cond_wait(global_queue, m, (global_lock==0));
    }
    global_lock = 1;
    // set my pid as the lock_owner
    lock_owner = getpid();
    unlock(m);
}
int sys_global_unlock() {
    lock(m);

    if(not lock_owner == gettid()) { // if im not the lock owner (or the lock is unlocked (lock_owner == -1))
        unlock(m);
        return EPERM;
    }
    global_lock = 0; //Free lock
    lock_owner = -1;    // Set no owner (lock is free)
    cond_signal(global_queue);
//Wakes a single process waiting in global_queue
    unlock(m);
}