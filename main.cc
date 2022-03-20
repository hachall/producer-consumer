/******************************************************************
 * The Main program with the two functions. A simple
 * example of creating and using a thread is provided.
 ******************************************************************/

#include "helper.h"

void *producer (void *id);
void *consumer (void *id);

int main (int argc, char **argv)
{
  srand(time(NULL));

  /*Input check*/
  if (argc < 5) {
    cerr << "ERROR: too little input parameters provided - required: <size of queue> <jobs per producer> <number of producers> <number of consumers>" << endl;
    return -1;
  }

  /*Set up variables and data stuctures*/
  int queue_length;
  int p_num_jobs;
  int num_producers;
  int num_consumers;
  if ((queue_length = check_arg(argv[1])) == -1) {cerr << "Input Error" << endl; return -1 ;};
  if ((p_num_jobs = check_arg(argv[2])) == -1) {cerr << "Input Error" << endl; return -1 ;};
  if ((num_producers = check_arg(argv[3])) == -1) {cerr << "Input Error" << endl; return -1 ;};
  if ((num_consumers = check_arg(argv[4])) == -1) {cerr << "Input Error" << endl; return -1 ;};

  pthread_t threads [num_producers + num_consumers];

  job** queue;
  queue = new job*[queue_length + 1] {}; //queue is accessed 1-n, and index 0 ignored. Simplifies aligning job id with job position
  int queue_head = 1;
  int queue_tail = 1;

  var_obj* var_access = new var_obj;
  var_access->job_queue = queue;
  var_access->queue_length = queue_length;
  var_access->num_jobs = p_num_jobs;
  var_access->queue_head = &queue_head;
  var_access->queue_tail = &queue_tail;

  /*Set up and initialise semaphores*/
  int sem_set_id = sem_create(SEM_KEY, 3);
  if (sem_set_id == -1) {
    cerr << "Error creating semaphores" << endl;
    return -1;
  }
  if (sem_init(sem_set_id, Mutex, 1) == -1) {cerr << "Couldnt initalize Mutex" ; return -1;}; //mutex
  if (sem_init(sem_set_id, Space, queue_length) == -1) {cerr << "Couldnt initalize Space" ; return -1;}; //space
  if (sem_init(sem_set_id, Items, 0) == -1) {cerr << "Couldnt initalize Items" ; return -1;}; //items
  var_access->sem_id = sem_set_id;

  /*Custom data objects for threads*/
  data_packet* data_array [num_producers + num_consumers] {};
  for (int i = 0; i < num_producers + num_consumers ; i++) {
    data_packet* data = new data_packet;
    data->var_access = var_access;
    if (i < num_producers) {
      data->id = i + 1;
    } else {
      data->id = (i - num_producers) + 1;
    }
    data_array[i] = data;
  }

  //Create producer and consumer threads
  for (int j = 0 ; j < num_producers + num_consumers ; j ++ ) {
    if (j < num_producers) {
      pthread_create (&threads[j], NULL, producer, (void*) data_array[j]);
    } else {
      pthread_create (&threads[j], NULL, consumer, (void*) data_array[j]);
    }
  }

  /*Join terminating threads to ensure program doesn't exit prematurely*/
  for (int k = 0 ; k < num_producers + num_consumers ; k++) {
    pthread_join(threads[k], NULL);
  }

  cout << "All threads terminated" << endl;

  /*Clean up*/
  for (int l = 0 ; l < num_producers + num_consumers ; l++) {
    delete data_array[l];
  }
  delete var_access;
  delete queue;
  sem_close(sem_set_id);
  return 0;
}

void *producer (void *parameter)
{
  int id;

  /*Unpack data packet*/
  data_packet* data = (data_packet*) parameter;
  var_obj *param = data->var_access;
  id = data->id;

  for (int i = 0 ; i < param->num_jobs ; i++) {
    sleep(rand() % 5 + 1);
    job* new_job = new job;
    new_job->duration = rand() % 10 + 1;

    if(sem_timed_wait(param->sem_id, Space, 20) == -1) {
      printf("Producer(%d): quit! waited too long in queue\n", id);
      pthread_exit(0);
    }; //space lock
    sem_wait(param->sem_id, Mutex); //mutual exclusion lock

    //Add job to queue, and increment queue head
    new_job->id = *param->queue_head;
    param->job_queue[*param->queue_head] = new_job;
    printf("Producer(%d): job id %d duration %d\n", id, *param->queue_head, new_job->duration);
    *param->queue_head = (*param->queue_head % param->queue_length) + 1;

    sem_signal(param->sem_id, Mutex);
    sem_signal(param->sem_id, Items);

  }

  printf("Producer(%d): no more jobs\n", id);
  pthread_exit(0);
}

void *consumer (void *parameter)
{ 
  int id;
  job* job_ptr;

  /*Unpack data packet*/
  data_packet* data = (data_packet*) parameter;
  var_obj* param = data->var_access;
  id = data->id;

  while ((sem_timed_wait(param->sem_id, Items, 20) != -1)) {
    sem_wait(param->sem_id, Mutex);

    //Remove job from queue, and increment queue tail
    job_ptr = param->job_queue[*param->queue_tail];
    param->job_queue[*param->queue_tail] = NULL;
    *param->queue_tail = (*param->queue_tail % param->queue_length) + 1;

    sem_signal(param->sem_id, Mutex);
    sem_signal(param->sem_id, Space);

    printf("Consumer(%d): job id %d executing sleep duration %d\n", id, job_ptr->id, job_ptr->duration);
    sleep(job_ptr->duration);
    printf("Consumer(%d): job id %d completed\n", id, job_ptr->id);

    delete job_ptr;

  };

  printf("Consumer(%d): no more jobs left\n", id);
  pthread_exit (0);
}
