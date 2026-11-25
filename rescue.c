#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#define SHIFT_MINUTES 540
//realistic limit of total numbet of students
#define MAX_STUDENTS  1000

sem_t chairs;                  //number of chairs that students can simultainuously occupy
sem_t mutex;                   //protects the waiting studens
sem_t tutor_ready;             //semaphore that signals when the tutor is ready
sem_t student_ready[MAX_STUDENTS];   //an array of semaphors that indicates when the given student is ready
sem_t session_done[MAX_STUDENTS];    //posts on this array when the session is over

int queue[MAX_STUDENTS];       /* FIFO of waiting student ids */
int q_head = 0;
int q_count = 0;

//add students to the back of the queue
void enqueue(int id)
{
    //moves the new students behind the last waiting student
    queue[(q_head + q_count) % MAX_STUDENTS] = id;
    q_count++;
}

//remove the students from the front line while returning their id
int dequeue(void)
{
    //take the students back from the line and return their id
    int id = queue[q_head];
    q_head = (q_head + 1) % MAX_STUDENTS;
    q_count--;
    return id;
}

void *student(void *arg)
{
    int id = *(int *)arg; //read the student id (void is cast to an int)
    free(arg); //release the memory reserved for arg

    printf("Student %d says, \"Arriving at the student center.\"\n", id);

    //take a chair if one is available, otherwise the students leaves
    if (sem_trywait(&chairs) != 0) {
        printf("Student %d says, \"There are no seats, guess I fail.\"\n", id);
        return NULL;
    }
    printf("Student %d says, \"There is an empty seat, I will wait.\"\n", id);

    //add student to the waiting queue; only the given student can access the thread within the lock
    sem_wait(&mutex);
    enqueue(id);
    sem_post(&mutex);

    //the student waits on the tutor to be called
    sem_wait(&student_ready[id]);   
    sem_post(&chairs);              //there was a free chair
    sem_post(&tutor_ready);         //tutor was signaled that a student got to the table

    sem_wait(&session_done[id]);    
    printf("Student %d says, \"Time to go!\"\n", id);
    return NULL;
}

void *tutor(void *arg)
{
    int minutes = 0;
    (void)arg;

    printf("Tutor says, \"I'm beginning my workday.\"\n");

    while (minutes < SHIFT_MINUTES) {
        int a = -1, b = -1;

        /* Take two students under the mutex, but never wait while holding it. */
        sem_wait(&mutex);
        if (q_count >= 2) {
            a = dequeue();
            b = dequeue();
        }
        sem_post(&mutex);

        if (a < 0) {
            printf("Tutor says, \"Not enough students, time to doomscroll.\"\n");
            sleep(1);
            minutes++;
            continue;
        }

        printf("Tutor says, \"There are two students, tutoring them.\"\n");
        sem_post(&student_ready[a]);
        sem_post(&student_ready[b]);

        // first student seated 
        sem_wait(&tutor_ready);  
        //second student seated 
        sem_wait(&tutor_ready);     

        int duration = 5 + rand() % 26;   /* 5 to 30 minutes per session*/
        printf("Tutor says, \"Tutoring for %d minutes.\"\n", duration);
        for (int i = 0; i < duration; i++) {
            sleep(1);
            minutes++;
        }

        sem_post(&session_done[a]);
        sem_post(&session_done[b]);
        printf("Tutor says, \"Tutoring done!\"\n");
    }

    printf("Tutor says, \"That's enough for one day, good night!\"\n");
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("usage: %s N C\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    int c = atoi(argv[2]);

    srand((unsigned)time(NULL));

    sem_init(&chairs, 0, n);
    sem_init(&mutex, 0, 1);
    sem_init(&tutor_ready, 0, 0);

    //holds the threads identifier
    pthread_t tt;
    pthread_create(&tt, NULL, tutor, NULL);

    int next_id = 1;
    for (int minute = 1; minute <= SHIFT_MINUTES; minute++) {
        if (rand() % c == 0 && next_id < MAX_STUDENTS) {
            int id = next_id++;

            /* Initialize before the thread exists, so it can't wait on an uninitialized semaphore. */
            sem_init(&student_ready[id], 0, 0);
            sem_init(&session_done[id], 0, 0);

            int *p = malloc(sizeof *p);
            if (p == NULL) {
                return 1;
            }
            *p = id;

            pthread_t t;
            pthread_create(&t, NULL, student, p);
            pthread_detach(t);
        }
        sleep(1);
    }

    pthread_join(tt, NULL);
    return 0;
}