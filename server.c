#include "segel.h"
#include "request.h"
#define SCHED_ALG_BLOCK 0
#define SCHED_ALG_DROP_TAIL 1
#define SCHED_ALG_DROP_HEAD 2
#define SCHED_ALG_RANDOM 3



/*
 * ***********************************
 *          Queue implementaion
 * ***********************************
 */
typedef struct Queue {
    int capacity;
    int size;
    int head;
    int tail;
    int* elements;
    pthread_mutex_t* m;
    pthread_cond_t* empty;
    pthread_cond_t* full;
} Queue;

int isFull(Queue* q) {
    return q->capacity == q->size;
}
int isEmpty(Queue* q) {
    return q->size == 0;
}
Queue* initQueue(int capacity, pthread_cond_t* empty, pthread_cond_t* full, pthread_mutex_t* m) {
    Queue* q = malloc(sizeof(struct Queue));
    q->capacity = capacity;
    q->size = 0;
    q->head = 0;    // this field will be updated cyclic in dequeue.
    q-> tail = capacity-1; // this field will be updated cyclic in enqueue.
    q->elements = malloc(capacity * sizeof(int));
    q->m = m;
    q->empty = empty;
    q->full = full;
}
int dequeue(Queue* q) {
    pthread_mutex_lock(q->m);
    while(isEmpty(q)) {
        pthread_cond_wait(q->empty, q->m);
    }
    // removing from head of queue
    int element = q->elements[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->size--;
    pthread_cond_signal(q->full);
    pthread_mutex_unlock(q->m);
    return element;
}
void enqueue(Queue* q, int element) {
    pthread_mutex_lock(q->m);
    while(isFull(q)) {
        pthread_cond_wait(q->full, q->m);
    }
    // Adding to tail of the queue
    q->tail = (q->tail + 1) % q->capacity;
    q->elements[q->tail] = element;
    q->size++;
    pthread_cond_signal(q->empty);
    pthread_mutex_unlock(q->m);
}

void destroyQueue(Queue* q) {
    free(q->elements);
    free(q);
}



// 
// server.c: A very, very simple web server
//
// To run:
//  ./server [portnum (above 2000)] [threads] [queue_size] [schedalg]
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// HW3: Parse the new arguments too
void getargs(int argc, char *argv[], int *port, int *num_of_threads, int *queue_size, int *schedalg)
{
    if (argc < 5) {
        fprintf(stderr, "Usage: %s [portnum] [threads] [queue_size] [schedalg]\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *num_of_threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);

    if(strcmp("block",argv[4]) == 0) {
        *schedalg = SCHED_ALG_BLOCK;
    }
    else if(strcmp("dt",argv[4]) == 0) {
        *schedalg = SCHED_ALG_DROP_TAIL;
    }
    else if(strcmp("dh",argv[4]) == 0) {
        *schedalg = SCHED_ALG_DROP_HEAD;
    }
    else if(strcmp("random",argv[4]) == 0) {
        *schedalg = SCHED_ALG_RANDOM;
    }
}
// some global variables
pthread_cond_t empty_g, full_g;
pthread_mutex_t global_lock;
Queue* requests_queue;

void* handleRequests(void* thread_id /* NOTE: this will be used mainly for section C when we want to gather thread statistics */) {

    // NOTE: this is not busy waiting!!
    //      there is usage in condition variables in dequeue
    while(1) {
        int fd = dequeue(requests_queue);
        // TODO: add a list (or maybe queue) for requests currently being processed (Page 6 Hint No.1 in manual)
        requestHandle(fd);
        Close(fd);
    }

}
int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen, num_of_threads, queue_size, schedalg;
    struct sockaddr_in clientaddr;

    getargs(argc, argv, &port, &num_of_threads, &queue_size, &schedalg);

    //initializing condition variables --> this will allways succeed
    pthread_cond_init( &empty_g, NULL);
    pthread_cond_init( &full_g, NULL);


    // 
    // HW3: Create some threads...
    //
    pthread_t worker_thread[num_of_threads];
    for (int i = 0; i < num_of_threads; ++i) {
        pthread_create(worker_thread[i],NULL,handleRequests , &i );
    }
    requests_queue = initQueue(queue_size, &empty_g, &full_g, &global_lock);

    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        //
        // HW3: In general, don't handle the request in the main thread.
        // Save the relevant info in a buffer and have one of the worker threads
        // do the work.
        //
        enqueue(requests_queue, connfd);

    }

}


    


 
