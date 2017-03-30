#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/mman.h>
#include <time.h>

#define SERV_PORT 4000 // port
#define LISTENQ 10 // maximum number of client connections
#define ROOM_COUNT 8 // maximum number of client connections

#define MAX(A,B) (((A)>(B))?(A):(B))
#define MIN(A,B) (((A)<(B))?(A):(B))

#pragma pack(1)   // Using pragma pack to pack the struct for symmetry across network
struct request {
  char secretary_name[100];
  uint32_t log_timestamp;
  uint32_t start_time;
  char hostname[1024];
  uint32_t port;
};

struct request_queue {
  char secretary_name[100];
  uint32_t log_timestamp;
  uint32_t start_time;
  char hostname[1024];
  uint32_t port;
};

struct reservation_queue {
  struct request requests[50];
};
#pragma pack(0)   // End of packaging

// Lamport's logical clock is defined
static uint32_t *log_clock = 0;

// Request flag
static uint32_t *request_flag = 0;

// Reserve Flag
static uint32_t *reserve_flag = 0;

uint32_t room_reserve_flag[ROOM_COUNT] = { 0, 0, 0, 0, 0, 0, 0, 0 };

static const struct request empty_request;

// Optional arguments are client IP addresses
int main (int argc, char **argv)
{
 int decNumber1, decNumber2, listenfd, connfd, n;
 long long result;
 pid_t child1pid, child2pid;
 socklen_t clilen;
 struct request current_request;
 struct reservation_queue room_reservations[ROOM_COUNT];
 struct sockaddr_in cliaddr, servaddr;
 FILE *outputFile;

 // Mapping these variables to shared space to use across all children of this program.
 log_clock = mmap(NULL, sizeof *log_clock, PROT_READ | PROT_WRITE, 
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

 request_flag = mmap(NULL, sizeof *request_flag, PROT_READ | PROT_WRITE, 
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

 reserve_flag = mmap(NULL, sizeof *reserve_flag, PROT_READ | PROT_WRITE, 
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

 //Creating the socket for server - If condition to handle error
 if ((listenfd = socket (AF_INET, SOCK_STREAM, 0)) <0) {
  perror("Problem in creating the socket");
  exit(2);
 }

 //Initiating the socket
 servaddr.sin_family = AF_INET;
 servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
 servaddr.sin_port = htons(SERV_PORT);

 //binding the socket
 bind (listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

 //Listen to socket - Waiting for clients
 listen (listenfd, LISTENQ);

 printf("%s\n","Server has started... waiting for clients.");

 //Child2 takes care of reserving the rooms based on the requests in the queue.
 if ( (child2pid = fork ()) == 0 ) { 
    while(1) {
      //Child2 runs once in every 10 seconds.
      sleep(10);
      *reserve_flag = 1;

      char * line = NULL;
      size_t len = 0;
      ssize_t read;
      outputFile = fopen("serveroutput", "r");

      if (outputFile == NULL)
        exit(EXIT_FAILURE);

      while ((read = getline(&line, &len, outputFile)) != -1) {
        printf("%s", line);
        sscanf(line, "%s %d %d %s %d", current_request.secretary_name, &current_request.log_timestamp, &current_request.start_time, current_request.hostname, &current_request.port );

        int assign_flag = 0;

        // Loop through all 8 rooms to see if there's vacant space for the request to reserve
        for(int rq = 0; rq < ROOM_COUNT; rq++) {
          int num_requests = room_reserve_flag[rq];
          printf("%d\n", num_requests);
          room_reservations[rq].requests[num_requests] = current_request;
          assign_flag = 1;
          room_reserve_flag[rq]++;
          // Assign flag confirms that the room is assigned to a request
          for(int req_it = num_requests; req_it > 0; req_it--) {
            // If condition to check if there are any other reservations which start an hour before or after the current start time
            if(room_reservations[rq].requests[req_it - 1].start_time >= current_request.start_time + 3600 || room_reservations[rq].requests[req_it - 1].start_time <= current_request.start_time - 3600 ) {
              continue;
            } else if(room_reservations[rq].requests[req_it - 1].log_timestamp < current_request.log_timestamp) {
                // If there exists an overlap, reservation will be deleted based on the Lamport's timestamp of the request.
                room_reservations[rq].requests[num_requests] = empty_request;
                assign_flag = 0;
                room_reserve_flag[rq]--;
                break;
            }
          }
          if(assign_flag == 1)
            break;
        }

      }
      time_t print_time;
      fclose(outputFile);
      printf("\n********** current reservation schedule ***********\n\n");

      for(int i1 = 0; i1 < ROOM_COUNT; i1++) {
        if(room_reserve_flag[i1] != 0) {
          printf("%d reservations made for Room %d\n\n", room_reserve_flag[i1], i1);
          for(int i2 = 0; i2 < room_reserve_flag[i1]; i2++){
            printf("%s ", room_reservations[i1].requests[i2].secretary_name);
            print_time = room_reservations[i1].requests[i2].start_time;
            printf("%s ", ctime(&print_time));
            printf("%d ", room_reservations[i1].requests[i2].start_time);
            printf("Room : %d\n", i1);
          }
        }
      }
      printf("\n********** current reservation schedule ***********\n\n");

      *reserve_flag = 0;
    }
    exit(0);
  }

 for ( ; ; ) {

  clilen = sizeof(cliaddr);
  //Accepting a new connection
  connfd = accept (listenfd, (struct sockaddr *) &cliaddr, &clilen);

  printf("%s\n","Request received");

  if ( (child1pid = fork ()) == 0 ) { //if it’s 0, it’s child process

  printf ("%s\n","Child created to handle client's request");

  //close the listening socket in child process
  close (listenfd);

  while ( (n = recv(connfd, &current_request, sizeof(struct request),0)) > 0)  {

    while(*reserve_flag == 1);

    outputFile = fopen("serveroutput", "a");

    *log_clock = MAX(*log_clock, current_request.log_timestamp) + 1;
    current_request.log_timestamp = *log_clock;

    fprintf(outputFile, "%s %d %d %s %d\n", current_request.secretary_name, current_request.log_timestamp, current_request.start_time, current_request.hostname, current_request.port);

    fclose(outputFile);

    *request_flag = *request_flag + 1;

    result = 1;
    send(connfd, &result, sizeof(result), 0);
  }

  if (n < 0)
   printf("%s\n", "Read error");
  exit(0);
 }
//close the socket in the server
close(connfd);
}
}

