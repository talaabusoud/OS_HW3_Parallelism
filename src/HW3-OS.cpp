//OS - HW3 
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <iostream>
#include <thread>
#include <chrono>

#define N 12027735 // (12027754 + 12027716)/2
#define M 10
#define EP 1e-9  // Epsilon value

using namespace std;

int main() {
	srand(time(0));
    double *packet1 = (double*)malloc(N * sizeof(double));
    double *packet2 = (double*)malloc(N * sizeof(double));
    double *result_packet1 = (double*)malloc(N * sizeof(double));
	
    // random packets
    for (int i=0; i<N; i++) {
        packet1[i] = (double)rand() / (double)(RAND_MAX / N);
        packet2[i] = (double)rand() / (double)(RAND_MAX / 10);
    }  

    // serial calculations
	clock_t beginSerial = clock();
    for (int i=0; i<N; i++) {
        result_packet1[i] = pow(packet1[i], packet2[i]);
    }
    clock_t endSerial = clock();
    double serial_RunTime = ((double)(endSerial - beginSerial)) / CLOCKS_PER_SEC;
    printf(" -- Time taken for serial processing: %f seconds\n", serial_RunTime);

    //**PART 2**//
    // parallel calculations (shared memory)	
	const char *name = "/OS2_shared_memory";
	int sharedMemFd = shm_open(name, O_CREAT | O_RDWR, 0666);
	ftruncate(sharedMemFd, N * sizeof(double));
    
	double *result_packet2 = (double *)mmap(0, N * sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemFd, 0);
	
	int problemsPerProcess = N/M;
    pid_t processId_1;
    clock_t beginParallel = clock();
	
	for (int i=0; i<M; i++) {
        processId_1 = fork();

        if (processId_1 == 0) {
            int startIndex = i * problemsPerProcess;
            int endIndex = (i == M - 1) ? N : startIndex + problemsPerProcess;

            for (int k=startIndex; k<endIndex; k++) {
                result_packet2[k] = pow(packet1[k], packet2[k]);
            }
            munmap(result_packet2, N * sizeof(double));
            exit(0);
        }
    }

    for (int i=0; i<M; i++) {
        wait(NULL);
    }
    clock_t endParallel = clock();
    double parallel_RunTime = ((double)(endParallel - beginParallel)) / CLOCKS_PER_SEC;
    printf(" -- Time taken for parallel processing: %f seconds\n", parallel_RunTime);

    // flag to check matching 
	bool isIdentical_1 = true;
    for (int i=0; i<N; i++) {
        // compare results (elements)
        if (fabs(result_packet1[i] - result_packet2[i]) > EP) {
            isIdentical_1 = false;
            break;
        }
    }
    if (isIdentical_1) {
        printf("** Result One matches Result Two.\n");
    }
    // unmap shared memo
    munmap(result_packet2, N * sizeof(double));
    // unlink shared memo obj 
    shm_unlink(name);

    //**PART 3**//
	// message passing calculations
	double *result_packet3 = (double*)malloc(N * sizeof(double));
	const char *fifoPath = "/tmp/fifoPath"; // fifo (named pipe) path
    // read/write permissions
    mkfifo(fifoPath, 0666);
	
	clock_t beginMessage = clock();
	
    for (int i=0; i<M; i++) {
        // fork new process
        pid_t processId_2 = fork();

        // child p
        if (processId_2 == 0) {
            int startIndex = i * problemsPerProcess;
            int endIndex = (i == M - 1) ? N : startIndex + problemsPerProcess;
            // open fifo in write only mode (to send data)
            int fifoFD = open(fifoPath, O_WRONLY);

            // compare results + write data to fifo
            for (int k=startIndex; k<endIndex; k++) {
                double R = pow(packet1[k], packet2[k]);
                write(fifoFD, &R, sizeof(double));
            }

            // close fifo
            close(fifoFD);
            exit(0);
        } 
        
        // parent p
        else if (processId_2 > 0) {
            // open fifo in read only mode (to receive data)
            int fifoFD = open(fifoPath, O_RDONLY);

            // read from fifo + store in RP3
            for (int k = i * problemsPerProcess; k < ((i == M - 1) ? N : (i + 1) * problemsPerProcess); k++) {
                read(fifoFD, &result_packet3[k], sizeof(double));
            }

            // close fifo
            close(fifoFD);
            // wait child p to complete
            waitpid(processId_2, NULL, 0); 
        } 
        
        else {
            perror("Error: unable to create child process");
            exit(1);
        }
    }
	
    clock_t endMessage = clock();
    double message_passing_RunTime = ((double)(endMessage - beginMessage)) / CLOCKS_PER_SEC;
    printf(" -- Time taken for message passing: %f seconds\n", message_passing_RunTime);

    // flag to check matching 
    bool isIdentical_2 = true;
    for (int i=0; i<N; i++) {
        // compare results (elements)
        if (fabs(result_packet1[i] - result_packet3[i]) > EP) {
            isIdentical_2 = false;
            break;
        }
    }
    if (isIdentical_2) {
        printf("** Result One matches Result Three.\n");
    } 
        
    //**PART 4**//
    // Parallel using threads
    double *result_packet4 = (double*)malloc(N * sizeof(double));

    clock_t beginThreads = clock();

    // create threads --> M number
    thread *threads = new thread[M];
    int itemsPerThread = N / M;
    int remainder = N % M;

    // parallel thread calculations
    for (int i = 0; i < M; ++i) {
        int start = i * itemsPerThread;
        int end = (i == M - 1) ? (start + itemsPerThread + remainder) : (start + itemsPerThread);

        threads[i] = thread([=, &result_packet4]() {
            for (int j = start; j < end; ++j) {
                result_packet4[j] = pow(packet1[j], packet2[j]);
            }
        });
    }

    // waiting all threads to complete
    for (int i = 0; i < M; ++i) {
        threads[i].join();
    }

    clock_t endThreads = clock();
    double parallel_threads_RunTime = ((double)(endThreads - beginThreads)) / CLOCKS_PER_SEC;
    printf(" -- Time taken for parallel threads: %f seconds\n", parallel_threads_RunTime);

    // flag to check matching 
    bool isIdentical_3 = true;
    for (int i=0; i<N; i++) {
        // compare results (elements)
        if (fabs(result_packet1[i] - result_packet4[i]) > EP) {
            isIdentical_3 = false;
            break;
        }
    }
    if (isIdentical_3) {
        printf("** Result One matches Result Four.\n");
    } 
    
    // cleaning up 
    delete[] threads;
    unlink(fifoPath);
    // free memory
    free(packet1);
    free(packet2);
    free(result_packet1);
    free(result_packet3);
    free(result_packet4);
    
    return 0;
}

