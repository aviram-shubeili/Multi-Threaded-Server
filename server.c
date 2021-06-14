#include "segel.h"
#include "request.h"
#define SCHED_ALG_BLOCK 0
#define SCHED_ALG_DROP_TAIL 1
#define SCHED_ALG_DROP_HEAD 2
#define SCHED_ALG_RANDOM 3




Request* initRequest(int fd) {
    Request* r = malloc(sizeof(Request));
    r->fd = fd;
    gettimeofday(&r->stat_req_arrival, NULL);
    return r;
}
/*
 * ***********************************
 *          List implementation
 * ***********************************
 */
typedef struct Node{
    Request* data;
    struct Node* next;
    struct Node* prev;
}Node;

typedef struct List{
    Node* head;
    Node* tail;
} List;

List *initList();
Request *popHead(List* list);
Request *popTail(List* list); // TODO needed?
Request *popIndex(List* list, int index);
void addNodeToTail(List* list, Request* req);
void destroyList();

List *initList(){
    List* list = malloc(sizeof(List));
    list->head = NULL;
    list->tail = NULL;
    return list;
}

Request *popHead(List* list){ //TODO when do we kill the node?
    Node *node = list->head;
    // empty list
    if (!node) { return NULL; }
    // only one item
    else if (node == list->tail){
        Request* req = node->data;
        list->head = NULL;
        list->tail = NULL;
        free(node); //TODO needed here??
        return req;
    }
    else{
        Request* req = node->data;
        list->head = node->next;
        list->head->prev = NULL;
        free(node); //TODO needed here?
        return req;
    }
}

Request *popIndex(List* list, int index){
    // list is empty
    if(list->head == NULL) {
        return NULL;
    }
    Node* node = list->head;
    while (node){
        if (index == 0){
            Request* req = node->data;
            // taking care of prev (or head)
            if (node->prev) { node->prev->next = node->next; }
            else { list->head = node->next; }

            // taking care of next (or tail)
            if (node->next) { node->next->prev = node->prev; }
            else { list->tail = node->prev; }

            free (node); //TODO needed here?
            return req;
        }
        node = node->next;
        index--;
    }
    // should never get here!
    unix_error("Unexpected popIndex List Error");
    return NULL;
}

void addNodeToTail(List* list, Request* req){
    Node* node = malloc(sizeof(Node));
    node->data = req;

    // list is empty
    if (list->head == NULL){
        list->head = node;
        list->tail = node;
        node->prev = NULL;
        node->next = NULL;
    }
        // one item in list
    else if (list->head == list->tail){
        node->prev = list->head;
        list->head->next = node;
        node->next = NULL;
        list->tail = node;
    }
        // more than one in list
    else{
        list->tail->next = node;
        node->prev = list->tail;
        node->next = NULL;
        list->tail = node;
    }
}



/*
 * ***********************************
 *          Queue implementation
 * ***********************************
 */
typedef struct Queue {
    int capacity;
    int size;
    int head;    // TODO delete me
    int tail;    // TODO delete me
    List* element_list;
    Request** elements; // array of ptrs    // TODO delete me
    pthread_mutex_t* m;
    pthread_cond_t* empty;
    pthread_cond_t* full;
    int overload_policy;
} Queue;

Queue *initQueue(int capacity, pthread_cond_t *empty, pthread_cond_t *full, pthread_mutex_t *m, int overload_policy);
int isFull(Queue* q);
int isEmpty(Queue* q);
Request* dequeue(Queue* q);
void enqueue(Queue* q, Request* precentage);
void destroyQueue(Queue* q);
void randomCuts(Queue* q, int precentage);
void stupidCopyQueue(Queue *dest, Queue *src);
void randomRemove(Queue *q);



// some global variables
pthread_cond_t empty_g, full_g;
pthread_mutex_t global_lock;
Queue* requests_queue;
int active_requests = 0;

// TODO this is for debugging
int request_index = 0;



int isFull(Queue* q) {
    return q->capacity == q->size;
}
int isEmpty(Queue* q) {
    return q->size == 0;
}


Queue *initQueue(int capacity, pthread_cond_t *empty, pthread_cond_t *full, pthread_mutex_t *m, int overload_policy) {

    Queue* q = malloc(sizeof(struct Queue));
    if(q == NULL) {
        unix_error("Malloc error");
    }

    q->element_list = initList();
    if(q->element_list == NULL) {
        unix_error("Malloc error");
    }

    q->capacity = capacity;
    q->size = 0;
    q->head = 0;    // this field will be updated cyclic in dequeue.    // TODO delete me
    q->tail = capacity-1; // this field will be updated cyclic in enqueue.    // TODO delete me

    // allocating the pointers array // TODO delete me
    q->elements = malloc(capacity * sizeof(Request*));
    if(q->elements == NULL) {
        unix_error("Malloc error");
    }

    q->m = m;
    q->empty = empty;
    q->full = full;
    q->overload_policy = overload_policy;
    return q;
}
Request* dequeue(Queue* q) {

    pthread_mutex_lock(q->m);
    while(isEmpty(q)) {
        pthread_cond_wait(q->empty, q->m);
    }

    // removing from head of queue
    // Request* element = q->elements[q->head];     // TODO delete me
    Request* element = popHead(q->element_list);
/*
    q->elements[q->head] = NULL;
    q->head = (q->head + 1) % q->capacity;
    */     // TODO delete me
    q->size--;
    pthread_cond_signal(q->full);
    pthread_mutex_unlock(q->m);
    return element;
}

void enqueue(Queue* q, Request* element) {
    pthread_mutex_lock(q->m);

    while(isFull(q) ||
          ((q->size + active_requests) == q->capacity) ) {

        if(q->overload_policy == SCHED_ALG_BLOCK) {
            pthread_cond_wait(q->full, q->m);
        }
        else if (q->overload_policy == SCHED_ALG_DROP_TAIL) {
            Close(element->fd);
            free(element); // TODO: should i?
            pthread_mutex_unlock(q->m);
            return;
        }
        else if(q->overload_policy == SCHED_ALG_DROP_HEAD) {
            if(isEmpty(q)) { // all requests are active --> drop new request (piazza @450)
                Close(element->fd);
                free(element); // TODO: should i?
                pthread_mutex_unlock(q->m);
                return;
            }
            else { // drop head and continue with current request
//                Request* oldest_request = q->elements[q->head];     // TODO delete me
                /*
                q->elements[q->head] = NULL;
                q->head = (q->head + 1) % q->capacity;
                 */     // TODO delete me
                Request* oldest_request = popHead(q->element_list);
                q->size--;
                Close(oldest_request->fd);
                free(oldest_request); // TODO should i?
            }
        }
        else if(q->overload_policy == SCHED_ALG_RANDOM) {
            if(isEmpty(q)) { // all requests are active --> drop new request (piazza @398)
                Close(element->fd);
                free(element); // TODO: should i?
                pthread_mutex_unlock(q->m);
                return;
            }
            else {
                randomCuts(q,25);
            }
        }
        else {
            // should never get here!
            unix_error("Unexpected enqueue Error");
        }
    }

    // Adding to tail of the queue
    /*
    q->tail = (q->tail + 1) % q->capacity;
    q->elements[q->tail] = element;
     */
    addNodeToTail(q->element_list,element);
    q->size++;
    pthread_cond_signal(q->empty);
    pthread_mutex_unlock(q->m);
}


// this is never used TODO: Maybe add aa SIGINT handler ?
void destroyQueue(Queue* q) {
//    free(q->elements); TODO delete me and maybe free element_list?
    free(q);
}


void randomCuts(Queue* q, int precentage) {

// ******** Figuring out how many Requests to cut ********

    int num_of_cuts = (q->size * precentage) / 100;
    // rounding up
    if((q->size * precentage) % 100 != 0) {
        ++num_of_cuts;
    }

    // ******** Removing Requests randomly ********
    for (int i = 0; i < num_of_cuts; ++i) {
        randomRemove(q);
    }
/*
// ******** Initializing and copying to a temp queue ********

    // temp queue to the rescue -- used to override global mutex lock and condition variables
    pthread_cond_t temp_empty;
    pthread_cond_t temp_full;
    pthread_mutex_t temp_lock;
    pthread_cond_init( &temp_empty, NULL);
    pthread_cond_init( &temp_full, NULL);
    int mutex_res = pthread_mutex_init(&temp_lock, NULL);
    if(mutex_res != 0) {
        unix_error("Mutex error");
    }
    Queue* temp_q = initQueue(q->capacity, &temp_empty, &temp_full, &temp_lock, q->overload_policy);
    // backing this up so i can free it later
    Request** temp_elements = temp_q->elements;
    // This doesnt do anything smart
    stupidCopyQueue(temp_q, q);


// ******** Removing Requests ********

    for (int i = 0; i < num_of_cuts; ++i) {
        randomRemove(temp_q);
    }

    // copy back to the original queue
    stupidCopyQueue(q, temp_q);

// ******** destroy temp queue ********

    temp_q->elements = temp_elements;
    destroyQueue(temp_q);

    // destroyed the queue so no one is stuck on those
    pthread_mutex_destroy(&temp_lock);
    pthread_cond_destroy(&temp_empty);
    pthread_cond_destroy(&temp_full);
*/     // TODO delete me
}

void randomRemove(Queue *q) {
    int random_value = rand() % q->size;
    Request* req = popIndex(q->element_list,random_value);
    q->size--;
    Close(req->fd);
    free(req);

    /*
    int old_size = q->size;
    int j = 0;
    while(j < random_value) {
        enqueue(q, dequeue(q));
        j++;
    }
    j++;
    while(j < old_size) {
        enqueue(q, dequeue(q));
        j++;
    }
     */    // TODO delete me
}

void stupidCopyQueue(Queue *dest, Queue *src) {
    dest->capacity = src->capacity;
    dest->size = src->size;
    dest->elements = src->elements;
    dest->head = src->head;
    dest->tail = src->tail;
}     // TODO delete me



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
    else {
        fprintf(stderr, "[schedalg] %s is unrecognized. please use: block, dt, dh or random\n", argv[4]);
        exit(1);
    }
}

// TODO delete _Noreturn
_Noreturn void* handleRequests(void* thread_id /* NOTE: this will be used mainly for section C when we want to gather thread statistics */) {

    // this is to make sure another thread isnt touching my thread_id value
    int my_thread_id = *(int*)thread_id;
    free(thread_id);
    // NOTE: this is not busy waiting!!
    //      there is usage in condition variables in dequeue
    while(1) {
        Request* req = dequeue(requests_queue);
        thread_current_request[my_thread_id] = req;
        // updating dispatch interval time (secs and usecs)
        struct timeval current_time;
        gettimeofday(&current_time,NULL);
        req->stat_req_dispatch_interval.tv_sec = current_time.tv_sec - req->stat_req_arrival.tv_sec;
        if( current_time.tv_usec < req->stat_req_arrival.tv_usec) {
            req->stat_req_dispatch_interval.tv_usec = 1e6 + current_time.tv_usec - req->stat_req_arrival.tv_usec;
            req->stat_req_dispatch_interval.tv_sec--;
        }
        else {
        req->stat_req_dispatch_interval.tv_usec = current_time.tv_usec - req->stat_req_arrival.tv_usec;
        }

        // incrementing active_request in a synchronized way
        pthread_mutex_lock(&global_lock);
        active_requests++;
        pthread_mutex_unlock(&global_lock);

        requestHandle(req->fd, my_thread_id);

        Close(req->fd);
        free(req); // TODO Should i?
        thread_current_request[my_thread_id] = NULL;

        // decrementing active_request in a synchronized way
        pthread_mutex_lock(&global_lock);
        active_requests--;
        pthread_mutex_unlock(&global_lock);
    }

}
int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen, num_of_threads, queue_size, overload_policy;
    struct sockaddr_in clientaddr;

    getargs(argc, argv, &port, &num_of_threads, &queue_size, &overload_policy);

    //initializing condition variables --> this will allways succeed
    pthread_cond_init( &empty_g, NULL);
    pthread_cond_init( &full_g, NULL);
    int mutex_res = pthread_mutex_init(&global_lock, NULL);
    if(mutex_res != 0) {
        unix_error("Mutex error");
    }
    requests_queue = initQueue(queue_size, &empty_g, &full_g, &global_lock, overload_policy);

    //
    // HW3: Create some threads...
    //
//    pthread_t worker_thread[num_of_threads];
    pthread_t* worker_thread = malloc(num_of_threads * sizeof(pthread_t));
    thread_statistics = malloc(num_of_threads * sizeof(ThreadStats));
    // allocating the PTR array
    thread_current_request = malloc(num_of_threads * sizeof(Request*));
    if(worker_thread == NULL || thread_statistics == NULL) {
        unix_error("Malloc error");
    }

    for (int i = 0; i < num_of_threads; ++i) {

        // this is to make sure another thread isnt touching my thread_id value
        int* i1 = malloc(sizeof(int)); // handleRequests will copy and free this
        *i1 = i;

        pthread_create(&worker_thread[i],NULL,handleRequests , i1 );
        thread_statistics[i].stat_thread_id = i;
        thread_statistics[i].stat_thread_count = 0;
        thread_statistics[i].stat_thread_dynamic = 0;
        thread_statistics[i].stat_thread_static = 0;
    }
    // Part 3 -- Array of Statistics Struct (Cell per thread)


    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

//        // TODO this is for debugging
//        request_index++;

        //
        // HW3: In general, don't handle the request in the main thread.
        // Save the relevant info in a buffer and have one of the worker threads
        // do the work.
        //
        Request* req = initRequest(connfd);
        enqueue(requests_queue, req);

    }
}






