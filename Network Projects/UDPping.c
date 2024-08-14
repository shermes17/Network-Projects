/**
 * Sean Hermes
 * CPSC 3600
 * Assignment 2
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

// MACROS
#define MAXSTRINGLENGTH 10000
#define BUFSIZE 512

/** 
 * This program contains a client server UDP ping communication, 
 * if the user uses the server flag, the server runs where the server receives a
 * packet and imediatly sends it back to the sends. The client sends packet based
 * on an interval and a certain size. The client records all the rrt and delay 
 * information as well as packet information. Since this is UDP and it is not
 * reliable, packet loss may happen.
*/

/***************************** GLOBALS *****************************/
typedef struct{
    struct sockaddr_storage clntAddr;
    socklen_t clntAddrLen;
    int numBytesRcvd;
    int sock;
    char buffer[]; 
  }serverData_t;

typedef struct{
  struct addrinfo *servAddr;
  struct sockaddr_storage fromAddr;
  socklen_t fromAddrLen;
  char* port_number;
  char* ip_address;
  int sock;
  int ping_packet_count;
  int size;
  int count;
  double interval;
  char buffer[MAXSTRINGLENGTH];
  bool n;
}clientData_t;

// Communicaiton Statistics
double total_time_passed = 0;
double min_time = 10000;
double avg_time = 0;
double max_time = 0;
int recv_count = 0;
int send_count = 0;

pthread_cond_t dataReady = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
bool dataIsReady = true;

/***************************** FUNCTIONS PROTOTYPES *****************************/
// signals 
void* sigThread(void*);
void sig_handler(int signo);
void printStats();

// server
void runServer(char* portnumber);
int setupServer(char* portnumber);
void* server_send_threadfunc(void* i);
void* server_recv_threadfunc(void* i);

// client
void runClient(int ping_packet_count,  double ping_interval,char* port_number, 
int size_in_bytes, bool n, char* ip_address);
int setupClient(char* ipaddr, char* portnumber, clientData_t* data);
void* client_send_threadfunc(void* i);
void* client_recv_threadfunc(void* i);

// From Donahoo's DieWithMessage.c
void DieWithUserMessage(const char *msg, const char *detail);
void DieWithSystemMessage(const char *msg);
// Functions from Donahoo's addressUtility.c
void PrintSocketAddress(const struct sockaddr *address, FILE *stream);
bool SockAddrsEqual(const struct sockaddr *addr1, const struct sockaddr *addr2);



/***************************** MAIN *****************************/
int main(int argc, char *argv[]) {
  // initialize variables to default values
  int opt = 0;
  int ping_packet_count = 0x7fffffff;
  double ping_interval = 1.0;
  char* port_number = "33333";
  int size_in_bytes = 12;
  bool n = true; 
  bool server = false;
  char* ip_address;

  // parse command line arguments
  while ((opt = getopt(argc, argv, "c:i:p:s:n:S")) != -1){
    switch(opt){
      case 'c': // ping-packet-count 
        ping_packet_count = atoi(optarg);
        break;
      case 'i': // ping-interval
        ping_interval = strtod(optarg, NULL);
        break;
      case 'p': // port number
        port_number = optarg;
        break;
      case 's': // size in bytes
        size_in_bytes = atoi(optarg);
        break;
      case 'n': // no_print
        optind--;
        n = false;
        break;
      case 'S': // Server
        server = true;
        break;
      default: // unknown flag
        DieWithSystemMessage("Invalid Command Line Argument");
        break;
 
    } //switch
  }// while - parse arguments


  // if extra non flag argument --> IP address, used by client
 if (!server){
   ip_address = argv[argc-1];
 }

  if(server){ // server flag used, run server
    runServer(port_number);
  }
  else{ // no server flag, run client
    runClient(ping_packet_count,  ping_interval, port_number,  size_in_bytes, n, ip_address);
  }
  
} // main


/***************************** SIGNALS *****************************/
void* sigThread(void* i){
  if (signal(SIGINT, sig_handler) == SIG_ERR)
    printf("\ncan't catch SIGINT\n");
  // A long long wait so that we can easily issue a signal to this process
  while(1) 
    sleep(1);
}

void sig_handler(int signo){
  if (signo == SIGINT){
    printStats();
    exit(0);
  }
}

void printStats(){
  puts("");
  double loss = ((double)(send_count - recv_count) / send_count) * 100.0;
  double rtt = total_time_passed * 1000;
  printf("%d packets transmitted, %d received, %.2lf%% packet loss, time %.3lf ms\nrtt min/avg/max = %.3lf/%.3lf/%.3lf msec\n",
  send_count, recv_count, loss, rtt, min_time, avg_time,max_time);
  exit(0);

}

/***************************** SERVER *****************************/
void runServer(char* port_number){
  int socket = setupServer(port_number);
  serverData_t server_data;
  server_data.sock = socket;
  server_data.numBytesRcvd = 0;
  
  // set up send and recieve threads
  // Thread ids
  pthread_t server_send;
  pthread_t server_recv;

  // Create a threads, that will run thread funcs
  int err1 = pthread_create(&server_send, NULL, &server_send_threadfunc, &server_data);
  int err2 = pthread_create(&server_recv, NULL, &server_recv_threadfunc, &server_data);
    
  // Check if threads are created sucessfuly
  if(err1)
    DieWithSystemMessage("Thread creation failed\n");
  if(err2)
    DieWithSystemMessage("Thread creation failed\n");

  // join threads
  err1 = pthread_join(server_send, NULL);
  err2 = pthread_join(server_recv, NULL);

}

int setupServer(char* service){
  // Construct the server address structure
  struct addrinfo addrCriteria;                   // Criteria for address
  memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
  addrCriteria.ai_family = AF_UNSPEC;             // Any address family
  addrCriteria.ai_flags = AI_PASSIVE;             // Accept on any address/port
  addrCriteria.ai_socktype = SOCK_DGRAM;          // Only datagram socket
  addrCriteria.ai_protocol = IPPROTO_UDP;         // Only UDP socket

  struct addrinfo *servAddr; // List of server addresses
  int rtnVal = getaddrinfo(NULL, service, &addrCriteria, &servAddr);
  if (rtnVal != 0)
    DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));
    // Create socket for incoming connections
  int sock = socket(servAddr->ai_family, servAddr->ai_socktype,
      servAddr->ai_protocol);
  if (sock < 0)
    DieWithSystemMessage("socket() failed");

  // Bind to the local address
  if (bind(sock, servAddr->ai_addr, servAddr->ai_addrlen) < 0)
    DieWithSystemMessage("bind() failed");

  // Free address list allocated by getaddrinfo()
  freeaddrinfo(servAddr);
  return sock;
}// setupServer()

void *server_send_threadfunc(void *i) {
  serverData_t *data = (serverData_t *)i;

  // Continuously send data
  while (1) {
    if (data->numBytesRcvd > 0) {
      // Send received datagram back to the client
      ssize_t numBytesSent = sendto(data->sock, data->buffer, data->numBytesRcvd, 0,
      (struct sockaddr *)&data->clntAddr, data->clntAddrLen);
      if (numBytesSent < 0)
        DieWithSystemMessage("sendto() failed)");
      else if (numBytesSent != data->numBytesRcvd)
        DieWithUserMessage("sendto()", "sent unexpected number of bytes");
      data->numBytesRcvd = 0;
      }//if 
    }// while 
    return NULL;
}

void *server_recv_threadfunc(void *i) {
  serverData_t *data = (serverData_t *)i;
  int sock = data->sock;

  // Continuously receive data
  while (1) {
    struct sockaddr_storage clntAddr;
    socklen_t clntAddrLen = sizeof(clntAddr);
    char buffer[MAXSTRINGLENGTH];
    
    ssize_t numBytesRcvd = recvfrom(sock, buffer, MAXSTRINGLENGTH, 0, (struct sockaddr *)&clntAddr, &clntAddrLen);
    char newBuf[numBytesRcvd];

    if (numBytesRcvd < 0)
      DieWithSystemMessage("recvfrom() failed");

    // update struct
    data->clntAddr = clntAddr;
    data->clntAddrLen = clntAddrLen;
    data->numBytesRcvd = numBytesRcvd;
    memcpy((data->buffer), newBuf, numBytesRcvd);
  }
  return NULL;
}



/***************************** CLIENT *****************************/
void runClient(int ping_packet_count,  double ping_interval,char* port_number, 
 int size_in_bytes, bool n, char* ip_address){
  clientData_t data;
  int sock = setupClient(ip_address, port_number, &data);

  dataIsReady = true;
  data.sock = sock;
  data.ping_packet_count = ping_packet_count;
  data.interval = ping_interval;
  data.n = n;
  data.size = size_in_bytes;
  data.port_number = port_number;
  data.ip_address = ip_address;
  data.count = 1;


  printf("Count\t\t\t%6d\n", ping_packet_count);   
  printf("Size\t\t\t%6d\n", size_in_bytes);       
  printf("Interval\t\t%6.2lf\n", ping_interval);  
  printf("Port\t\t\t%6s\n", port_number);       
  printf("Server_ip\t%14s\n", ip_address);
  
  // create thread to catch signal
  pthread_t sig;
  int s = pthread_create(&sig, NULL, &sigThread, NULL);
  if(s)DieWithSystemMessage("Thread creation failed\n");

  // Thread ids
  pthread_t client_send;
  pthread_t client_recv;

  // Create a threads, that will run thread funcs
  int err1 = pthread_create(&client_send, NULL, &client_send_threadfunc, &data);
  int err2 = pthread_create(&client_recv, NULL, &client_recv_threadfunc, &data);
    
  // Check if threads are created sucessfuly
  if(err1)
    DieWithSystemMessage("Thread creation failed\n");
  if(err2)
    DieWithSystemMessage("Thread creation failed\n");

  
  // join thread
  err1 = pthread_join(client_send, NULL);
  err2 = pthread_join(client_recv, NULL);
  s = pthread_join(sig, NULL);

  // print stats if no signal
  printStats();
  exit(1);
    
}

int setupClient(char* server, char* servPort, clientData_t* data){
  // Tell the system what kind(s) of address info we want
  struct addrinfo addrCriteria;                   // Criteria for address match
  memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
  addrCriteria.ai_family = AF_UNSPEC;             // Any address family
  // For the following fields, a zero value means "don't care"
  addrCriteria.ai_socktype = SOCK_DGRAM;          // Only datagram sockets
  addrCriteria.ai_protocol = IPPROTO_UDP;         // Only UDP protocol
  struct addrinfo *servAddr; // List of server addresses
  int rtnVal = getaddrinfo(server, servPort, &addrCriteria, &servAddr);
  if (rtnVal != 0)
      DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));


  // Create a datagram/UDP socket
  int sock = socket(servAddr->ai_family, servAddr->ai_socktype,
      servAddr->ai_protocol); // Socket descriptor for client

  if (sock < 0)
      DieWithSystemMessage("socket() failed"); 
  data->servAddr = servAddr;
  return sock;

} //setupClient()

void* client_send_threadfunc(void* i) {
  clientData_t* data = (clientData_t*)i;
  struct timespec nextPingTime;
  struct timespec remainingTime;
  struct timespec sleepTime;

  // Calculate the initial nextPingTime
  clock_gettime(CLOCK_REALTIME, &nextPingTime);

  while (data->count <= data->ping_packet_count) {
    // Calculate the time left until the next ping
    clock_gettime(CLOCK_REALTIME, &nextPingTime);

    // Send the ping
    char buffer[data->size];
    ssize_t numBytes = sendto(data->sock, buffer, data->size, 0, data->servAddr->ai_addr, data->servAddr->ai_addrlen);
    send_count++;

    if (numBytes < 0)
        DieWithSystemMessage("sendto() failed");
    else if (numBytes != data->size)
        DieWithUserMessage("sendto() error", "sent an unexpected number of bytes");

    // Calculate the next ping time based on the interval
    nextPingTime.tv_sec += (time_t)data->interval;
    double fractionalPart = data->interval - (double)(time_t)data->interval;
    nextPingTime.tv_nsec += (long)(fractionalPart * 1e9);

    // Calculate the sleep time based on the time until the next ping
    struct timespec currentTime;
    clock_gettime(CLOCK_REALTIME, &currentTime);
    long timeLeft = (nextPingTime.tv_sec - currentTime.tv_sec) * 1000000000 +
                    (nextPingTime.tv_nsec - currentTime.tv_nsec);

  if (timeLeft > 0 || pthread_cond_timedwait(&dataReady, &mutex, &nextPingTime) != ETIMEDOUT ) {
        sleepTime.tv_sec = timeLeft / 1000000000;
        sleepTime.tv_nsec = timeLeft % 1000000000;
        nanosleep(&sleepTime, &remainingTime);
  }
  // reset flag
    dataIsReady = false;
  }

  return NULL;
}

void* client_recv_threadfunc(void* i) {
  clientData_t* data = (clientData_t*)i;
  
  while (data->count <= data->ping_packet_count) {
      char buffer[MAXSTRINGLENGTH];
    
      // time ping is sent
      struct timespec startTime;
      clock_gettime(CLOCK_REALTIME, &startTime);
      
      ssize_t numBytesRcvd = recvfrom(data->sock, buffer, MAXSTRINGLENGTH, 0, NULL, 0);

      if (numBytesRcvd < 0) {
          DieWithSystemMessage("recvfrom() failed");
      }
      
      // get time after ping received
      struct timespec endTime;
      clock_gettime(CLOCK_REALTIME, &endTime);
      // calculate rrt
      double elapsedTime = difftime(endTime.tv_sec, startTime.tv_sec) +
                          (endTime.tv_nsec - startTime.tv_nsec) / 1.0e9;

      if (data->n) {
          printf("\t\t%d\t%zd\t%.3lf\n", data->count, numBytesRcvd, elapsedTime);
      } else {
          printf("*");
      }
      fflush(stdout);

      // update globals
      total_time_passed += elapsedTime; // in seconds
      if (elapsedTime >= max_time)
          max_time = elapsedTime;
      if (elapsedTime <= min_time)
          min_time = elapsedTime;
      avg_time = total_time_passed / data->count;
      recv_count++;
      data->count++;

      //set flag
      dataIsReady = true;
      pthread_mutex_lock(&mutex);
      pthread_cond_signal(&dataReady);
      pthread_mutex_unlock(&mutex);
  }

  printStats();
  return NULL;
}





/***************************** EXTRA FUNCTIONS *****************************/
// From Donahoo's DieWithMessage.c
void DieWithUserMessage(const char *msg, const char *detail) {
  fputs(msg, stderr);
  fputs(": ", stderr);
  fputs(detail, stderr);
  fputc('\n', stderr);
  exit(1);
}
void DieWithSystemMessage(const char *msg) {
  perror(msg);
  exit(1);
}
// Functions from Donahoo's addressUtility.c
void PrintSocketAddress(const struct sockaddr *address, FILE *stream) {
  // Test for address and stream
  if (address == NULL || stream == NULL)
    return;

  void *numericAddress; // Pointer to binary address
  // Buffer to contain result (IPv6 sufficient to hold IPv4)
  char addrBuffer[INET6_ADDRSTRLEN];
  in_port_t port; // Port to print
  // Set pointer to address based on address family
  switch (address->sa_family) {
  case AF_INET:
    numericAddress = &((struct sockaddr_in *) address)->sin_addr;
    port = ntohs(((struct sockaddr_in *) address)->sin_port);
    break;
  case AF_INET6:
    numericAddress = &((struct sockaddr_in6 *) address)->sin6_addr;
    port = ntohs(((struct sockaddr_in6 *) address)->sin6_port);
    break;
  default:
    fputs("[unknown type]", stream);    // Unhandled type
    return;
  }
  // Convert binary to printable address
  if (inet_ntop(address->sa_family, numericAddress, addrBuffer,
      sizeof(addrBuffer)) == NULL)
    fputs("[invalid address]", stream); // Unable to convert
  else {
    fprintf(stream, "%s", addrBuffer);
    if (port != 0)                // Zero not valid in any socket addr
      fprintf(stream, "-%u", port);
  }
}
bool SockAddrsEqual(const struct sockaddr *addr1, const struct sockaddr *addr2) {
  if (addr1 == NULL || addr2 == NULL)
    return addr1 == addr2;
  else if (addr1->sa_family != addr2->sa_family)
    return false;
  else if (addr1->sa_family == AF_INET) {
    struct sockaddr_in *ipv4Addr1 = (struct sockaddr_in *) addr1;
    struct sockaddr_in *ipv4Addr2 = (struct sockaddr_in *) addr2;
    return ipv4Addr1->sin_addr.s_addr == ipv4Addr2->sin_addr.s_addr
        && ipv4Addr1->sin_port == ipv4Addr2->sin_port;
  } else if (addr1->sa_family == AF_INET6) {
    struct sockaddr_in6 *ipv6Addr1 = (struct sockaddr_in6 *) addr1;
    struct sockaddr_in6 *ipv6Addr2 = (struct sockaddr_in6 *) addr2;
    return memcmp(&ipv6Addr1->sin6_addr, &ipv6Addr2->sin6_addr,
        sizeof(struct in6_addr)) == 0 && ipv6Addr1->sin6_port
        == ipv6Addr2->sin6_port;
  } else
    return false;
}

