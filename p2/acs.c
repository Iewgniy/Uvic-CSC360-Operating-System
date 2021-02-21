#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct customer_info{ /// use this struct to record the customer information read from customers.txt
    int user_id;
	int class_type;
	int arrival_time;
	int service_time;
    double start_time;
    double end_time;
} customer_info;

//use to store the clerk id
typedef struct clerk {
    int id;
    int servingID; // use to record the clerk current serving customer ID.
} clerk;

//node type use to create queue
typedef struct node_t {
    customer_info* customer;
    struct node_t* next;
} node_t;

//Queue type include the head and tail of the queue list.
typedef struct Queue {
    struct node_t* head;
    struct node_t* tail;
    int size;
} Queue;

//defualt the customer node
node_t* constructNode(customer_info* currCustomer) {
    node_t* node = (node_t*) malloc(sizeof (node_t));
    if(node == NULL) {
        perror("Error: failed on malloc Node");
        return NULL;
    }
    node->customer = currCustomer;
    node->next = NULL;
    return node;
}

//defualt the Queue list
Queue* constructQueue() {
    Queue* queue = (Queue*) malloc(sizeof (Queue));
    if(queue == NULL) {
        perror("Error: failed on malloc Queue\n");
        return NULL;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    return queue;
}

//enQueue function use to add node into the list
void enQueue(Queue* equeue, node_t* newNode) {
    if(equeue->size == 0) {
        equeue->head = newNode;
        equeue->tail = newNode;
    }else {
        equeue->tail->next = newNode;
        equeue->tail = newNode;
    }
    equeue->size++;
}

//deQueue function use to remove the peek of the queue list
void deQueue(Queue* dqueue) {
    if(dqueue->size == 0) {
        exit(-1);
    }else if(dqueue->size == 1) {
        dqueue->head = NULL;
        dqueue->tail = NULL;
        dqueue->size--;
    }else {
        dqueue->head = (dqueue->head)->next;
        dqueue->size--;
    }
}

/* global variables */

#define MAX_SIZE 256
struct timeval init_time; // use this variable to record the simulation start time.
int total_bClass = 0; //The count for counting the number of business customers.
double overall_waiting_time_bClass; //A global variable to add up the overall waiting time for all customers from bussiness class.
double overall_waiting_time_eClass; //A global variable to add up the overall waiting time for all customers from economy class.
Queue* Queue_bClass = NULL;
Queue* Queue_eClass = NULL;
customer_info customerArray[MAX_SIZE]; // Array to store all customer information.
clerk clerkArray[4]; // Array to store all clerks information.
int clerk_record = 0; // use to record the current clerk ID.

//initial the pthread and mutex and condition varibal.
pthread_t customerThreads[MAX_SIZE];
pthread_t clerkThreads[4]; // 4 clerk pthreads

//use for clerk lock and unlock and convar wait and signal
pthread_mutex_t C1_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t C1_convar = PTHREAD_COND_INITIALIZER;
pthread_mutex_t C2_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t C2_convar = PTHREAD_COND_INITIALIZER;
pthread_mutex_t C3_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t C3_convar = PTHREAD_COND_INITIALIZER;
pthread_mutex_t C4_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t C4_convar = PTHREAD_COND_INITIALIZER;

//use for two queues lock and unlock and convar wait and signal
pthread_mutex_t bQ_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t bQ_convar = PTHREAD_COND_INITIALIZER;
pthread_mutex_t eQ_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t eQ_convar = PTHREAD_COND_INITIALIZER;

//use for access queue lock and unlock and convar wait and signal
pthread_mutex_t access_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t access_queue_convar = PTHREAD_COND_INITIALIZER;

//use for time lock and unlock and convar wait and signal
pthread_mutex_t time_mutex = PTHREAD_MUTEX_INITIALIZER;


//use to get the time of the curr time
double getCurrentSimulationTime(){
	
	struct timeval cur_time;
	double cur_secs, init_secs;
	
	//pthread_mutex_lock(&start_time_mtex); you may need a lock here
	init_secs = (init_time.tv_sec + (double) init_time.tv_usec / 1000000);
	//pthread_mutex_unlock(&start_time_mtex);
	
	gettimeofday(&cur_time, NULL);
	cur_secs = (cur_time.tv_sec + (double) cur_time.tv_usec / 1000000);
	
	return cur_secs - init_secs;
}

//set the equeue serving id
void set_eQueue_serving(int clerk_ID) {
    if(Queue_eClass->head != NULL) {
        if(clerk_ID == 1) {
            clerkArray[0].servingID = Queue_eClass->head->customer->user_id;
        }else if(clerk_ID == 2) {
            clerkArray[1].servingID = Queue_eClass->head->customer->user_id;
        }else if(clerk_ID == 3) {
            clerkArray[2].servingID = Queue_eClass->head->customer->user_id;
        }else {
            clerkArray[3].servingID = Queue_eClass->head->customer->user_id;
        }
    }
}

//set the bQueue serving id
void set_bQueue_serving(int clerk_ID) {
  if(Queue_bClass->head != NULL) {
      if(clerk_ID == 1) {
          clerkArray[0].servingID = Queue_bClass->head->customer->user_id;
      }else if(clerk_ID == 2) {
          clerkArray[1].servingID = Queue_bClass->head->customer->user_id;
      }else if(clerk_ID == 3) {
          clerkArray[2].servingID = Queue_bClass->head->customer->user_id;
      }else {
          clerkArray[3].servingID = Queue_bClass->head->customer->user_id;
      }
  }
}

//set and return the current serving id
int set_clerk_serving(int clerk_ID) {
    if(clerk_ID == 1) {
        return clerkArray[0].servingID;
    }else if(clerk_ID == 2) {
        return clerkArray[1].servingID;
    }else if(clerk_ID == 3) {
        return clerkArray[2].servingID;
    }else {
        return clerkArray[3].servingID;
    }
}

//Read in the file data
void readFileIn(char* file, char fileInformation[MAX_SIZE][MAX_SIZE]) {
    FILE *fp = fopen(file, "r");
    if(fp != NULL) {
        int count = 0;
        while(1) {
            fgets(fileInformation[count++], MAX_SIZE, fp);
            if(feof(fp)) break;
        }
    }else {
        perror("Error: failed on read file.\n");
    }
    fclose(fp);
}

//token the information and cast it to integer
void tokenInformation(char fileInformation[MAX_SIZE][MAX_SIZE], int totalCustomer) {
    int i = 1;
    for(; i<=totalCustomer; i++) {
        int j = 0;
        while(fileInformation[i][j] != '\0') {
            if(fileInformation[i][j] == ':' || fileInformation[i][j] == ',') {
                fileInformation[i][j] = ' ';
            }
            j++;
        }
        customer_info c = {
            atoi(&fileInformation[i][0]), //user_id
            atoi(&fileInformation[i][2]), //class_type
            atoi(&fileInformation[i][4]), //arrival_time
            atoi(&fileInformation[i][6]), //service_time
            0.0,
            0.0
        };
        if(atoi(&fileInformation[i][2]) != 0 && atoi(&fileInformation[i][2]) != 1){
            printf("%s","The input file has illegal values.");
            exit(0);
        }
        // printf("%i seconds. \n",atoi(&fileInformation[i][0]));
        // printf("%i seconds. \n",atoi(&fileInformation[i][2]));
        // printf("%i seconds. \n",atoi(&fileInformation[i][4]));
        // printf("%i seconds. \n",atoi(&fileInformation[i][6]));
        customerArray[i-1] = c;
    }
}

// function entry for clerk threads
void* clerk_entry(void* currClerk) {
    clerk* clerk_infor = (clerk*) currClerk;
    int clerk_ID = clerk_infor->id;

    while(1) {
        //lock the condition only one clerk thread can access the condition.
        pthread_mutex_lock(&access_queue_mutex);
        while (Queue_bClass->size == 0 && Queue_eClass->size == 0) {
            pthread_cond_wait(&access_queue_convar, &access_queue_mutex);
        }
        pthread_mutex_unlock(&access_queue_mutex);

        //if the business class is not empty and it has high priority than economy class.
        if(Queue_bClass->size != 0) {
            pthread_mutex_lock(&bQ_mutex);

            clerk_record = clerk_ID;

            set_bQueue_serving(clerk_ID);

            //send the signal to business queue
            if(pthread_cond_signal(&bQ_convar)) {
                perror("Error: failed on condition signal.\n");
                exit(0);
            }

            //unlock the queue
            pthread_mutex_unlock(&bQ_mutex);

            if(clerk_ID == 1 ) {
                pthread_mutex_lock(&C1_mutex);
                pthread_cond_wait(&C1_convar, &C1_mutex);
                pthread_mutex_unlock(&C1_mutex);
            }else if(clerk_ID == 2 ) {
                pthread_mutex_lock(&C2_mutex);
                pthread_cond_wait(&C2_convar, &C2_mutex);
                pthread_mutex_unlock(&C2_mutex);
            }else if(clerk_ID == 3 ) {
                pthread_mutex_lock(&C3_mutex);
                pthread_cond_wait(&C3_convar, &C3_mutex);
                pthread_mutex_unlock(&C3_mutex);
            }else if(clerk_ID == 4 ) {
                pthread_mutex_lock(&C4_mutex);
                pthread_cond_wait(&C4_convar, &C4_mutex);
                pthread_mutex_unlock(&C4_mutex);
            }

        }else if(Queue_eClass->size != 0 ) { //if the business queue size is zero and the economy class is not empty.
            pthread_mutex_lock(&eQ_mutex);

            clerk_record = clerk_ID;

            set_eQueue_serving(clerk_ID);

            if(pthread_cond_signal(&eQ_convar)) {
                perror("Error: failed on condition signal.\n");
                exit(0);
            }

            pthread_mutex_unlock(&eQ_mutex);

            if(clerk_ID == 1 ) {
                pthread_mutex_lock(&C1_mutex);
                pthread_cond_wait(&C1_convar, &C1_mutex);
                pthread_mutex_unlock(&C1_mutex);
            }else if(clerk_ID == 2 ) {
                pthread_mutex_lock(&C2_mutex);
                pthread_cond_wait(&C2_convar, &C2_mutex);
                pthread_mutex_unlock(&C2_mutex);
            }else if(clerk_ID == 3 ) {
                pthread_mutex_lock(&C3_mutex);
                pthread_cond_wait(&C3_convar, &C3_mutex);
                pthread_mutex_unlock(&C3_mutex);
            }else if(clerk_ID == 4 ) {
                pthread_mutex_lock(&C4_mutex);
                pthread_cond_wait(&C4_convar, &C4_mutex);
                pthread_mutex_unlock(&C4_mutex);
            }

        }

    }
    return NULL;
}

// function entry for customer threads
void * customer_entry(void * cus_info){
	
	customer_info* cur_customer = (customer_info*) cus_info;
	usleep(cur_customer->arrival_time * 100000);
	
	fprintf(stdout, "A customer arrives: customer ID %2d. \n", cur_customer->user_id);
	
    // 0 = economy class, 1 = business class.
	//case 1 if class_type == 0 economy class
    if(cur_customer->class_type == 0) { 

        if(pthread_mutex_lock(&eQ_mutex)) {
            perror("Error: failed on mutex lock.\n");
        }
        enQueue(Queue_eClass, constructNode(cur_customer)); //add the node into the queue

        pthread_cond_signal(&access_queue_convar); //send signal to clerk the customer arrived

        printf("A customer enters a queue: the queue ID %1d, and length of the queue %2d. \n", 0, Queue_eClass->size);

        cur_customer->start_time = getCurrentSimulationTime();

        //wait the clerk signal when the clerk is ready.
        if(pthread_cond_wait(&eQ_convar, &eQ_mutex)) {
            perror("Error: failed on condition wait.\n");
        }

        //record the current clerk id from the global.
        int clerk_curr = clerk_record;

        while(cur_customer->user_id != set_clerk_serving(clerk_curr)) {
            pthread_cond_wait(&eQ_convar, &eQ_mutex);
        }

        deQueue(Queue_eClass); //remove the node from queue

        if(pthread_mutex_unlock(&eQ_mutex)) {
            perror("Error: failed on mutex unlock.\n");
        };

        cur_customer->end_time = getCurrentSimulationTime();

        // get the wait time.
        pthread_mutex_lock(&time_mutex);
        overall_waiting_time_eClass += cur_customer->end_time - cur_customer->start_time;
        pthread_mutex_unlock(&time_mutex);

        printf("A clerk starts serving a customer: start time %.2f, the customer ID %2d, the clerk ID %1d. \n", cur_customer->end_time, cur_customer->user_id, clerk_curr);

        //usleep customer proccessing
        usleep(cur_customer->service_time * 100000);

        double end_serving_time = getCurrentSimulationTime();

        printf("A clerk finishes serving a customer: end time %.2f, the customer ID %2d, the clerk ID %1d. \n", end_serving_time, cur_customer->user_id, clerk_curr);

        //send back a signal to clerk cotinue pick customer
        if(clerk_curr == 1) {
            pthread_mutex_lock(&C1_mutex);
            pthread_cond_signal(&C1_convar);
            pthread_mutex_unlock(&C1_mutex);
        }else if(clerk_curr == 2) {
            pthread_mutex_lock(&C2_mutex);
            pthread_cond_signal(&C2_convar);
            pthread_mutex_unlock(&C2_mutex);
        }else if(clerk_curr == 3) {
            pthread_mutex_lock(&C3_mutex);
            pthread_cond_signal(&C3_convar);
            pthread_mutex_unlock(&C3_mutex);
        }else if(clerk_curr == 4){
            pthread_mutex_lock(&C4_mutex);
            pthread_cond_signal(&C4_convar);
            pthread_mutex_unlock(&C4_mutex);
        }

    }else { //case 2 if class_type == 1 economy class
        total_bClass++;
        if(pthread_mutex_lock(&bQ_mutex)) {
            perror("Error: failed on mutex lock.\n");
        }
        enQueue(Queue_bClass, constructNode(cur_customer));

        pthread_cond_signal(&access_queue_convar);

        printf("A customer enters a queue: the queue ID %1d, and length of the queue %2d. \n", 1, Queue_bClass->size);

        cur_customer->start_time = getCurrentSimulationTime();

        if(pthread_cond_wait(&bQ_convar, &bQ_mutex)) {
            perror("Error: failed on condition wait.\n");
        }

        int clerk_curr = clerk_record;

        while(cur_customer->user_id != set_clerk_serving(clerk_curr)) {
            pthread_cond_wait(&bQ_convar, &bQ_mutex);
        }

        deQueue(Queue_bClass);

        if(pthread_mutex_unlock(&bQ_mutex)) {
            perror("Error: failed on mutex unlock.\n");
        }

        cur_customer->end_time = getCurrentSimulationTime();

        pthread_mutex_lock(&time_mutex);
        overall_waiting_time_bClass += cur_customer->end_time - cur_customer->start_time;
        pthread_mutex_unlock(&time_mutex);

        printf("A clerk starts serving a customer: start time %.2f, the customer ID %2d, the clerk ID %1d. \n", cur_customer->end_time, cur_customer->user_id, clerk_curr);

        usleep(cur_customer->service_time * 100000);

        double end_serving_time = getCurrentSimulationTime();

        printf("A clerk finishes serving a customer: end time %.2f, the customer ID %2d, the clerk ID %1d. \n", end_serving_time, cur_customer->user_id, clerk_curr);

        if(clerk_curr == 1) {
            pthread_mutex_lock(&C1_mutex);
            pthread_cond_signal(&C1_convar);
            pthread_mutex_unlock(&C1_mutex);
        }else if(clerk_curr == 2) {
            pthread_mutex_lock(&C2_mutex);
            pthread_cond_signal(&C2_convar);
            pthread_mutex_unlock(&C2_mutex);
        }else if(clerk_curr == 3) {
            pthread_mutex_lock(&C3_mutex);
            pthread_cond_signal(&C3_convar);
            pthread_mutex_unlock(&C3_mutex);
        }else if(clerk_curr == 4){
            pthread_mutex_lock(&C4_mutex);
            pthread_cond_signal(&C4_convar);
            pthread_mutex_unlock(&C4_mutex);
        }

    }
    pthread_exit(NULL);
    return NULL;
}

int main(int argc, char* argv[]) {

	char fileInformation[MAX_SIZE][MAX_SIZE];
    readFileIn(argv[1], fileInformation);
    int totalCustomer = atoi(fileInformation[0]);
    tokenInformation(fileInformation, totalCustomer);
    Queue_bClass = constructQueue();
    Queue_eClass = constructQueue();

    //store the clerk id to clerkAray.
    for(int f = 1; f <= 4; f++){
        clerk c = {f};
        clerkArray[f-1] = c;
    }

    //create the clerk threads
    for(int k = 0; k < 4; k++) {
        if(pthread_create(&clerkThreads[k], NULL, clerk_entry, &clerkArray[k])) {
            perror("Error: failed on thread create.\n");
            exit(0);
        }
    }

    //initial the time
    gettimeofday(&init_time, NULL);

    //create the customer threads
    for(int i = 0; i < totalCustomer; i++) {
        if(pthread_create(&customerThreads[i], NULL, customer_entry, &customerArray[i])) {
            perror("Error: failed on thread creation.\n");
            exit(0);
        }
    }

    //join the customer threads back to main thread or main function
    for(int j = 0; j < totalCustomer; j++) {
        if(pthread_join(customerThreads[j], NULL)){
            perror("Error: failed on join thread.\n");
            exit(0);
        }
    }

    // printf("%.6f seconds. \n",overall_waiting_time_bClass);
    // printf("%.6f seconds. \n",overall_waiting_time_eClass);
    // printf("%i. \n",total_bClass);
    printf("The average waiting time for all customers in the system is: %.2f seconds. \n", (overall_waiting_time_bClass+overall_waiting_time_eClass)/totalCustomer);
    printf("The average waiting time for all business-class customers is: %.2f seconds. \n", overall_waiting_time_bClass/total_bClass);
    printf("The average waiting time for all economy-class customers is: %.2f seconds. \n", overall_waiting_time_eClass/(totalCustomer - total_bClass));

    return 0;
}


