#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>

#define SERV_PORT 4000 // port

#pragma pack(1)   // Using pragma pack to pack the struct fro symmetry across network
struct request {
  char secretary_name[100];
  uint32_t log_timestamp;
  uint32_t start_time;
  char hostname[1024];
  uint32_t port;
};
#pragma pack(0)   // Packing done!

// Lamport's logical clock is defined
static uint32_t log_clock = 0;

// Provides a random value to define Unix time within the next three days.
long random_at_most(long max) {
  unsigned long
    // max <= RAND_MAX < ULONG_MAX, so this is okay.
    num_bins = (unsigned long) max + 1,
    num_rand = (unsigned long) RAND_MAX + 1,
    bin_size = num_rand / num_bins,
    defect   = num_rand % num_bins;

  long x;
  do { 
   srandom(time( NULL ));
   x = random();
  }
  // This is carefully written not to overflow
  while (num_rand - defect <= (unsigned long)x);

  // Truncated division is intentional
  return x/bin_size;
}


int main(int argc, char **argv)
{
 int sockfd;
 struct sockaddr_in servaddr;
 struct request current_request;
 long long result;
 FILE *outputFile;

 //basic check of the arguments
 //additional checks can be inserted
 if (argc != 4) {
  perror("<Secretary's firstname> or <IP address of the server> or <Client Port> is missing. Note: this program receives 2 arguments only.");
  exit(1);
 }

 //Creating the socket for Client - If condition to handle error
 if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) <0) {
  perror("Error in connection...");
  exit(2);
 }

 //Setting the socket
 memset(&servaddr, 0, sizeof(servaddr));
 servaddr.sin_family = AF_INET;
 servaddr.sin_addr.s_addr= inet_addr(argv[2]);
 servaddr.sin_port =  htons(SERV_PORT); //convert to big-endian order

 //Connecting client to the socket
 if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))<0) {
  perror("Error in connection...");
  exit(3);
 }

 outputFile = fopen(strcat(argv[1], "output"), "w+");

 for (int i = 0; i < 6 ; i++) {
    log_clock = log_clock + 1;
    strncpy(current_request.secretary_name, argv[1], 100);
    current_request.log_timestamp = log_clock;
    current_request.start_time = 1471957200 + random_at_most(205200);
    current_request.hostname[1023] = '\0';
    gethostname(current_request.hostname, 1023);
    sscanf (argv[3],"%d",&current_request.port);

    send(sockfd, &current_request, sizeof(struct request), 0);

    if (recv(sockfd, &result, sizeof(result), 0) == 0){
     perror("Unexpected server termination occurred");
     exit(4);
    }

    fprintf(outputFile, "%lld \n", result);
    sleep(3);
 }

 fclose(outputFile);
 
 printf("Output file is generated and client is being terminated...\n");
 exit(0);
}