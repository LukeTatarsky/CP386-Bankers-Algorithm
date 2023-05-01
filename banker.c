#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/*
FIXME
1. make the customer number dynamic
    - works. tested up to 50 customers. encountered problems over 100 customers. Array indexing issues.

*/

// max line input length from user. probably not going to request 9999 of each resource
#define MAX_IN_LEN 25

int RESOURCES;
int CUSTOMERS;
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

// threads will require a struct
typedef struct Data
{
    int size;
    int *max_arr;
    int *aloc_arr;
    int *need_arr;
    int *available;
    int *safe_sequence;
    int *request_res;
    int *cust_number;

} Data;

int read_banker_file(Data myData);
int read_customer_num();
int user_input(char line[MAX_IN_LEN], Data myData);
int resource_request(Data myData);
int resource_release(Data myData);
int resource_release_run(Data myData);
void display_status(Data myData);
int calc_need(Data myData);
// calc available for all customers. used once at the beginning
void calc_available_all(Data myData);
// calc available for one customer after a set of resources is granted. used after granting of resources
void calc_available_one(int(*request_res),
                        int(*available));
// Handles excecution of threads
int run_sequence(Data myData);
// The code run by each thread
void *run_thread(void *arg);
int find_sequence(Data myData);

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        printf("error, incorrect number of arguments passed. exiting program.\n");
        return -1;
    }

    RESOURCES = argc - 1;
    CUSTOMERS = read_customer_num();

    Data *myData;
    myData = (Data *)malloc(sizeof(Data));

    //   having issues with free() when increasing customer size, adding to size helped
    myData->size = (CUSTOMERS * RESOURCES) + (15 * (CUSTOMERS * RESOURCES));
    myData->max_arr = (int *)malloc(myData->size * sizeof(int));
    myData->aloc_arr = (int *)calloc(myData->size, sizeof(int));
    myData->need_arr = (int *)malloc(myData->size * sizeof(int));
    myData->available = (int *)calloc(RESOURCES + 1, sizeof(int));
    myData->safe_sequence = (int *)calloc(CUSTOMERS + 1, sizeof(int));
    myData->request_res = (int *)calloc(RESOURCES + 1, sizeof(int));
    myData->cust_number = (int *)calloc(2, sizeof(int));

    if (myData->max_arr == NULL || myData->aloc_arr == NULL)
    {
        printf("failed array allocation\n");
        return -1;
    }

    /*  get available resources from program input   */
    for (int r = 0; r < RESOURCES; r++)
    {
        myData->available[r] = atoi(argv[r + 1]); // available = argsv
    }

    /* setup the maximum array  */
    read_banker_file(*myData);

    /* Print the arrays */
    printf("Number of Customers: %d\n", CUSTOMERS);
    printf("Currently Available resources:");
    for (int i = 0; i < RESOURCES; i++)
    {
        printf(" %d", myData->available[i]);
    }
    printf("\n");

    printf("Maximum resources from file:\n");
    for (int c = 0; c < CUSTOMERS; c++)
    {
        for (int r = 0; r < RESOURCES; r++)
        {
            printf("%d ", myData->max_arr[c * CUSTOMERS + r]);
        }
        printf("\n");
    }

    /* calc available - only run once at the beginning*/
    calc_available_all(*myData);

    /*
        MAIN MENU
    */
    int condition = -1, granted = 0;
    do
    {
        printf("Enter Command: ");
        char line[MAX_IN_LEN];
        fgets(line, MAX_IN_LEN, stdin);

        /* stop empty user inputs from causing issues*/
        if (strcmp(line, "\n") == 0)
            strcpy(line, "empty line");

        /* replace the newline char that fgets reads with a space for the tokens to work*/
        for (int i = 0; i < MAX_IN_LEN; i++)
        {
            if (line[i] == 10)
            {
                line[i] = 32;
                break;
            }
        }
        // printf("line: %s\n",line);

        condition = user_input(line, *myData);

        switch (condition)
        {
        case 1:
            // printf("Request\n");
            granted = resource_request(*myData);
            if (granted)
            {
                // once request is granted we can recalculate available
                calc_available_one(myData->request_res, myData->available);
                printf("State is safe, and request is satisfied\n");
            }
            else
                printf("State is not safe, and request is not satisfied\n");
            break;

        case 2:
            granted = resource_release(*myData);
            if (granted)
            {
                printf("The resources have been released successfully\n");
                // calc_available_one(myData->request_res, myData->available);
                // find_sequence(*myData);
            }
            else
                printf("The resources were unable to be released\n");
            break;

        case 3:
            // printf("Status\n");
            display_status(*myData);
            break;

        case 4:
            // printf("Run\n");
            // run the find sequence then excecute threads
            run_sequence(*myData);
            break;

        case 0:
            // printf("Exit\n");
            free(myData->max_arr);
            free(myData->aloc_arr);
            free(myData->need_arr);
            free(myData->safe_sequence);
            free(myData->request_res);
            free(myData->cust_number);
            free(myData->available);
            free(myData);
            return 0;

        default:
            // printf("Invalid input, invalid values\n");
            continue;
        }

        // printf("(TEST) Sequence: ");
        // for (int i = 0 ; i < CUSTOMERS ; i++){
        //     printf("%d ", myData->safe_sequence[i]);
        // }
        // printf("\n");

        // printf("(TEST) Available: ");
        // for (int i = 0 ; i < RESOURCES ; i++){
        //     printf("%d ", myData->available[i]);
        // }
        // printf("\n");

        // printf("\n");

    } while (condition != 0);

    return 0;
}

/*
parse the input and figure out what to do
will update  request_res and cust_number
return 1 for request
        2 for release
        3 for status
        4 for run
        0 for exit
        -1 for error
*/
int user_input(char line[MAX_IN_LEN], Data myData)
{
    char *token;
    char *command;
    char copyOfString[MAX_IN_LEN];
    int count = 0;

    /*  Do a length check on the input. Should not allow more than 4 resources or less than 4   */
    strcpy(copyOfString, line);
    token = strtok(copyOfString, " ");
    while (token != NULL)
    {
        count++;
        token = strtok(NULL, " ");
    }

    /*  If Elif Else for input commands  */
    command = strtok(line, " ");

    if (count == (RESOURCES + 2) && strcmp(command, "RQ") == 0)
    {
        myData.cust_number[0] = atoi(strtok(NULL, " "));

        /* validate customer number */
        if (myData.cust_number[0] < 0 || myData.cust_number[0] > CUSTOMERS)
        {
            return -1;
        }
        for (int r = 0; r < RESOURCES; r++)
        {
            token = strtok(NULL, " ");
            myData.request_res[r] = atoi(token);
            if (myData.request_res[r] < 0)
            {
                return -1;
            }
        }
        return 1;
    }
    else if (count == (RESOURCES + 2) && strcmp(command, "RL") == 0)
    {
        myData.cust_number[0] = atoi(strtok(NULL, " "));

        /* validate customer number */
        if (myData.cust_number[0] < 0 || myData.cust_number[0] > CUSTOMERS)
        {
            return -1;
        }
        for (int r = 0; r < RESOURCES; r++)
        {
            token = strtok(NULL, " ");
            myData.request_res[r] = atoi(token);
            if (myData.request_res[r] < 0)
            {
                return -1;
            }
        }
        return 2;
    }
    else if (count == 1 && strcmp(command, "Status") == 0)
    {
        return 3;
    }
    else if (count == 1 && strcmp(command, "Run") == 0)
    {
        return 4;
    }
    else if (count == 1 && strcmp(command, "Exit") == 0)
    {
        return 0;
    }
    else
    {
        if (count == 1 || count == 6)
            printf("Invalid input, use one of RQ, RL, Status, Run, Exit\n");
        else
            printf("Invalid input, use one of RQ, RL, Status, Run, Exit\n");
            // printf("Invalid input, expecting either 1 or %d input attributes\n", RESOURCES + 2);
    }
    return -1;
}

/*
    Allocate the requested resources and try to find a safe sequence.
    If no safe sequence is found then reverse the allocation.
    returns 1 on success
        and 0 otherwise
*/
int resource_request(Data myData)
{

    for (int r = 0; r < RESOURCES; r++)
    {
        myData.aloc_arr[myData.cust_number[0] * CUSTOMERS + r] += myData.request_res[r];
        // aloc_arr[cust_number[0] * CUSTOMERS + r] += request_arr[r];
    }

    /*  Find Sequence   */
    int exists = find_sequence(myData);
    if (exists)
    {
        // printf("(request_arr) State is safe\n");
    }
    else
    {
        // printf(" (request_arr) Unsafe state\n");
        for (int r = 0; r < RESOURCES; r++)
        {
            myData.aloc_arr[myData.cust_number[0] * CUSTOMERS + r] -= myData.request_res[r];
        }
    }
    return exists;
}

/*
    Release the specified resources from customer allocation.

    not sure if this should check that the requested numbers can be released or simply release as much as possible.
    return 1 on success and 0 on failure.
*/
int resource_release(Data myData)
{
    // int allow = 0;
    int count = 0;

    for (int r = 0; r < RESOURCES; r++)
    {
        if ((myData.aloc_arr[myData.cust_number[0] * CUSTOMERS + r] - myData.request_res[r]) >= 0)
            count += 1;
    }

    if (count == RESOURCES)
    {
        for (int r = 0; r < RESOURCES; r++)
        {
            myData.aloc_arr[myData.cust_number[0] * CUSTOMERS + r] -= myData.request_res[r];
            myData.need_arr[myData.cust_number[0] * CUSTOMERS + r] += myData.request_res[r];
            myData.available[r] += myData.request_res[r];
            myData.request_res[r] = 0;
        }
        find_sequence(myData); // update the safe sequence
    }
    else
    {
        return 0;
    }
    return 1;
}

int resource_release_run(Data myData)
{
    /*same as resource_release but doesnt update the safe sequence*/
    // int allow = 0;
    int count = 0;

    for (int r = 0; r < RESOURCES; r++)
    {
        if ((myData.aloc_arr[myData.cust_number[0] * CUSTOMERS + r] - myData.request_res[r]) >= 0)
            count += 1;
    }

    if (count == RESOURCES)
    {
        for (int r = 0; r < RESOURCES; r++)
        {
            myData.aloc_arr[myData.cust_number[0] * CUSTOMERS + r] -= myData.request_res[r];
            myData.need_arr[myData.cust_number[0] * CUSTOMERS + r] += myData.request_res[r];
            myData.available[r] += myData.request_res[r];
            myData.request_res[r] = 0;
        }
    }
    else
    {
        return 0;
    }
    return 1;
}

/* Status Display  */
void display_status(Data myData)
{
    printf("Available Resources:\n");
    for (int i = 0; i < RESOURCES; i++)
    {
        printf("%d ", myData.available[i]);
    }
    printf("\n");

    printf("Maximum Resources:\n");
    for (int c = 0; c < CUSTOMERS; c++)
    {
        for (int r = 0; r < RESOURCES; r++)
        {
            printf("%d ", myData.max_arr[(c * CUSTOMERS) + r]);
        }
        printf("\n");
    }
    printf("Allocated Resources:\n");
    for (int c = 0; c < CUSTOMERS; c++)
    {
        for (int r = 0; r < RESOURCES; r++)
        {
            printf("%d ", myData.aloc_arr[(c * CUSTOMERS) + r]);
        }
        printf("\n");
    }
    calc_need(myData);
    printf("Need Resources:\n");
    for (int c = 0; c < CUSTOMERS; c++)
    {
        for (int r = 0; r < RESOURCES; r++)
        {
            printf("%d ", myData.need_arr[(c * CUSTOMERS) + r]);
        }
        printf("\n");
    }
    return;
}

/*
calc need array
returns 1 if need is 0 or greater
        0 if need is negative
*/
int calc_need(Data myData)
{
    int i;
    for (int c = 0; c < CUSTOMERS; c++)
    {
        for (int r = 0; r < RESOURCES; r++)
        {
            i = c * CUSTOMERS + r;
            myData.need_arr[i] = myData.max_arr[i] - myData.aloc_arr[i];

            if (myData.need_arr[i] < 0)
            {
                return 0; // need is negative ( you have allocated too many resources)
            }
        }
    }
    return 1;
}

/*
calc available for all customers
used once at the beginning
*/
void calc_available_all(Data myData)
{
    int i;
    for (int c = 0; c < CUSTOMERS; c++)
    {
        for (int r = 0; r < RESOURCES; r++)
        {
            i = c * CUSTOMERS + r;
            myData.available[r] -= myData.aloc_arr[i]; // available recalculation
        }
    }
    return;
}
/*
calc available for one customer after a set of resources is granted
used after a granting of resources
*/
void calc_available_one(int(*request_res),
                        int(*available))
{
    for (int r = 0; r < RESOURCES; r++)
    {
        available[r] -= request_res[r]; // available recalculation
    }
    return;
}

/*
Checks to see if safe sequence exists.
updates the safe sequence.
returns 1 if sequence exists, 0 otherwise.
*/
int find_sequence(Data myData)
{
    int sequence_exists = 1;
    int i;

    /* calc need with new allocated array */
    if (calc_need(myData) == 0)
    {
        // printf("need is negative!\n");    // cant allocate more than max
        return 0;
    }

    // create copies of arrays
    int size = myData.size;
    int *aloc_arr_copy = (int *)malloc(size * sizeof(int));
    int *need_arr_copy = (int *)malloc(size * sizeof(int));
    int available_copy[RESOURCES + 1];
    int safe_sequence_copy[CUSTOMERS + 1];
    int *run_check = (int *)calloc(CUSTOMERS, sizeof(int));
    int progress = 1, run_count = 0, resource_pass = 0;

    for (i = 0; i < CUSTOMERS; i++)
        safe_sequence_copy[i] = myData.safe_sequence[i];

    for (i = 0; i < RESOURCES; i++)
    {
        available_copy[i] = myData.available[i];
    }
    /* Recalculate available_copy with the newly allocated resources */
    calc_available_one(myData.request_res, available_copy);

    for (i = 0; i < size; i++)
    {
        aloc_arr_copy[i] = myData.aloc_arr[i];
        need_arr_copy[i] = myData.need_arr[i];
    }

    /*
    progress - during each while loop we must have at least 1 process run. otherwise no progress and no safe sequence
    run_count - keep track of how many have run
    c - customer index
    */
    while (run_count != CUSTOMERS && progress == 1)
    {
        progress = 0;

        for (int c = 0; c < CUSTOMERS; c++)
        {
            resource_pass = 0;
            /* checking each resource */
            if (run_check[c] == 0)
            {
                for (int r = 0; r < RESOURCES; r++)
                {
                    // printf("avail: %d   need: %d    aloc: %d \n",available_copy[r],need_arr_copy[c * CUSTOMERS + r], aloc_arr_copy[c * CUSTOMERS + r]);
                    if (available_copy[r] - need_arr_copy[c * CUSTOMERS + r] >= 0)
                        resource_pass += 1;
                }

                /* this process has passed its check,  run it*/
                if (resource_pass == RESOURCES)
                {
                    // printf("%d run\n", c);
                    safe_sequence_copy[run_count] = c;
                    run_count += 1;
                    progress = 1;
                    run_check[c] = 1;

                    /* release the process resources */
                    for (int i = 0; i < RESOURCES; i++)
                    {
                        available_copy[i] += aloc_arr_copy[c * CUSTOMERS + i];
                        aloc_arr_copy[c * CUSTOMERS + i] = 0;
                    }
                }
                // else
                //     printf("%d Skip\n", c);
                // printf("\n");
            }
        }
    }

    if (run_count == CUSTOMERS)
    {
        // printf("find_sequence: safe state!\n");
        for (int i = 0; i < CUSTOMERS; i++)
            myData.safe_sequence[i] = safe_sequence_copy[i];

        sequence_exists = 1;
    }
    else
    {
        // printf("find_sequence: Unsafe state\n");
        sequence_exists = 0;
    }

    free(aloc_arr_copy);
    free(need_arr_copy);
    free(run_check);

    return sequence_exists;
}

/*
Handles the excecution of threads
*/
int run_sequence(Data myData)
{
    // will generate the safe sequence.
    // Done automatically during a resource request but if nothing has been requested yet, this needs to run.
    find_sequence(myData);

    // need to prevent an unsafe sequence from running. The case is when you hit "Run" without having a possible safe sequence.
    // tried using find_sequence() but it did not work for some reason.
    // will add up the safe sequence and compare to # of customers
    int c1 = 0, c2 = 0;
    for (int i = 0; i < CUSTOMERS; i++)
    {
        c1 += myData.safe_sequence[i];
        c2 += i;
    }
    if (c1 != c2)
    {
        printf("Safe Sequence does not exist. Please Exit.\n");
    }
    else
    {
        printf("Safe Sequence is: ");
        for (int i = 0; i < CUSTOMERS; i++)
        {
            printf(" %d", myData.safe_sequence[i]);
        }
        printf("\n");

        // create thread handle array
        pthread_t thread_handles[CUSTOMERS];

        for (int i = 0; i < CUSTOMERS; i++)
        {
            // printf("sequence[%d]: %d,\n",i, myData.safe_sequence[i]);
            *myData.cust_number = myData.safe_sequence[i];
            printf("Customer/Thread %d\n", *myData.cust_number);

            printf("Allocated resources:");
            for (int r = 0; r < RESOURCES; r++)
            {
                printf(" %d", myData.aloc_arr[myData.cust_number[0] * CUSTOMERS + r]);
            }
            printf("\n");

            printf("Needed:");
            for (int r = 0; r < RESOURCES; r++)
            {
                printf(" %d", myData.need_arr[myData.cust_number[0] * CUSTOMERS + r]);
            }
            printf("\n");

            printf("Available:");
            for (int r = 0; r < RESOURCES; r++)
            {
                printf(" %d", myData.available[r]);
            }
            printf("\n");

            // run_thread(&myData); // for pthread
            pthread_create(&(thread_handles[i]), NULL, run_thread, &myData);

            pthread_join(thread_handles[i], NULL);

            printf("New Available:");
            for (int r = 0; r < RESOURCES; r++)
            {
                printf(" %d", myData.available[r]);
            }
            printf("\n");
        }
    }
    return 0;
}

/*
Thread excecution function
*/
void *run_thread(void *arg)
{
    // wait mutex
    pthread_mutex_lock(&mutex1);

    printf("Thread has started\n");
    // void arg(myData) so we have to tell the compiler what it is
    struct Data *myData = (struct Data *)arg;

    printf("Thread has finished\n");

    // Need to specify the resources to be released from RL in myData.request_res
    printf("Thread is releasing resources\n");
    for (int r = 0; r < RESOURCES; r++)
    {
        // printf("release resource for customer %d\n", myData->cust_number[0]);
        myData->request_res[r] = myData->aloc_arr[myData->cust_number[0] * CUSTOMERS + r];
    }

    resource_release_run(*myData);

    // post mutex
    pthread_mutex_unlock(&mutex1);
    return myData;
}

/*
Creates the 2-d list of ints from a file
*/
int read_banker_file(Data myData)
{
    FILE *bankerIn;
    bankerIn = fopen("sample_in_banker.txt", "r");

    if (bankerIn == NULL)
    {
        printf("(read_banker_file) Error opening file. \n");
        fclose(bankerIn);
        return -1;
    }

    int max_len = sizeof(int) * RESOURCES * 2;
    char line[max_len];
    char *token;

    for (int c = 0; c < CUSTOMERS; c++)
    {
        fgets(line, max_len, bankerIn);
        token = strtok(line, ",");
        myData.max_arr[(c * CUSTOMERS)] = atoi(token);

        for (int r = 1; r < RESOURCES; r++)
        {
            token = strtok(NULL, ",");
            myData.max_arr[(c * CUSTOMERS) + r] = atoi(token);
        }
    }
    fclose(bankerIn);
    return 0;
}

int read_customer_num()
{
    FILE *bankerIn;
    bankerIn = fopen("sample_in_banker.txt", "r");
    char line[MAX_IN_LEN];

    if (bankerIn == NULL)
    {
        printf("(read_banker_file) Error opening file. \n");
        fclose(bankerIn);
        return -1;
    }
    int count = 0;

    while (fgets(line, MAX_IN_LEN, bankerIn) != NULL)
    {
        count++;
    }
    fclose(bankerIn);
    return count;
}