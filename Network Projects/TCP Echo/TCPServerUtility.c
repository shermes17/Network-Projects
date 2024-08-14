/**
 * Sean Hermes
 * CPSC 3600
 * Assignment 1
*/

#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include "Practical.h"


#include <stdlib.h>

static const int MAXPENDING = 5; // Maximum outstanding connection requests

int SetupTCPServerSocket(const char *service) {
  // Construct the server address structure
  struct addrinfo addrCriteria;                   // Criteria for address match
  memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
  addrCriteria.ai_family = AF_UNSPEC;             // Any address family
  addrCriteria.ai_flags = AI_PASSIVE;             // Accept on any address/port
  addrCriteria.ai_socktype = SOCK_STREAM;         // Only stream sockets
  addrCriteria.ai_protocol = IPPROTO_TCP;         // Only TCP protocol

  struct addrinfo *servAddr; // List of server addresses
  int rtnVal = getaddrinfo(NULL, service, &addrCriteria, &servAddr);
  if (rtnVal != 0)
    DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));

  int servSock = -1;
  for (struct addrinfo *addr = servAddr; addr != NULL; addr = addr->ai_next) {
    // Create a TCP socket
    servSock = socket(addr->ai_family, addr->ai_socktype,
        addr->ai_protocol);
    if (servSock < 0)
      continue;       // Socket creation failed; try next address

    // Bind to the local address and set socket to listen
    if ((bind(servSock, addr->ai_addr, addr->ai_addrlen) == 0) &&
        (listen(servSock, MAXPENDING) == 0)) {
      // Print local address of socket
      struct sockaddr_storage localAddr;
      socklen_t addrSize = sizeof(localAddr);
      if (getsockname(servSock, (struct sockaddr *) &localAddr, &addrSize) < 0)
        DieWithSystemMessage("getsockname() failed");
      fputs("Binding to ", stdout);
      PrintSocketAddress((struct sockaddr *) &localAddr, stdout);
      fputc('\n', stdout);
      break;       // Bind and listen successful
    }

    close(servSock);  // Close and try again
    servSock = -1;
  }

  // Free address list allocated by getaddrinfo()
  freeaddrinfo(servAddr);

  return servSock;
}

int AcceptTCPConnection(int servSock) {
  struct sockaddr_storage clntAddr; // Client address
  // Set length of client address structure (in-out parameter)
  socklen_t clntAddrLen = sizeof(clntAddr);

  // Wait for a client to connect
  int clntSock = accept(servSock, (struct sockaddr *) &clntAddr, &clntAddrLen);
  if (clntSock < 0)
    DieWithSystemMessage("accept() failed");

  // clntSock is connected to a client!

  fputs("Handling client ", stdout);
  PrintSocketAddress((struct sockaddr *) &clntAddr, stdout);
  fputc('\n', stdout);

  return clntSock;
}

void HandleTCPClient(int clntSocket, char * client_IP) {

  char buffer[BUFSIZE]; // Buffer for echo string

  // Receive message from client
  ssize_t numBytesRcvd = recv(clntSocket, buffer, BUFSIZE, 0);
  if (numBytesRcvd < 0)
    DieWithSystemMessage("recv() failed");

// HANDLE INPUT WITH THE CLIENT HERE

    
    char choice[1];
    const char* file_list[3] = {"song.txt", "poem.txt", "quote.txt"};
    char msg[BUFSIZE];
    int numBytesSent;

    // initial request received
    if(strcmp(buffer, "REQ") == 0){
      printf("Received file list request from %s\n", client_IP);
      fflush(stdout);
        
      // send confirmation that list request was received
      strcpy(msg, "LRC");
      numBytesSent = send(clntSocket, msg, sizeof(msg), 0);
      if (numBytesSent < 0)
        DieWithSystemMessage("send() failed");
            
      puts("Sending list of files");
       
      // send list of files
      strcpy(msg, "User, please select a file:\n1. song.txt\n2. poem.txt\n3. quote.txt\n");
      numBytesSent = send(clntSocket, msg, sizeof(msg), 0);
      if (numBytesSent < 0)
        DieWithSystemMessage("send() failed");

      // receive clients file choice
      numBytesRcvd = recv(clntSocket, choice, 1, 0);
      if (numBytesRcvd < 0)
          DieWithSystemMessage("recv() failed");

      int index = atoi(choice);
      printf("Received request for file \"%s\"\n", file_list[index-1]);

      strcpy(msg,file_list[index-1]);
      numBytesSent = send(clntSocket, msg, sizeof(msg), 0);
      if (numBytesSent < 0)
        DieWithSystemMessage("send() failed");

      puts("Sending file to the client");

      // open file
      FILE* fp = fopen(file_list[index-1], "r");

      // get file size to allocate correct amount of memory
      fseek(fp,0,SEEK_END); // send fp to EOF
      long int fileSize = ftell(fp);  // return size
      rewind(fp); // set fp back to begining

      char file [fileSize]; // allocate file size amount of mem
      fread(&file, 1,fileSize,fp); // read in from file

      // tell client how much memory to expect
      numBytesSent = send(clntSocket, &fileSize, sizeof(long int), 0);
      if (numBytesSent < 0)
        DieWithSystemMessage("send() failed");
   
      // send file until client confirms file was received correctly
      do{

        // send file
        numBytesSent = send(clntSocket,file,sizeof(file),0);
        if (numBytesSent < 0)
          DieWithSystemMessage("send() failed");

        // receive response
        numBytesRcvd = recv(clntSocket,msg,sizeof(msg),0);
        if (numBytesRcvd < 0)
            DieWithSystemMessage("recv() failed");

      }while(!strcmp(msg, "GOODFILE")); // check response

      puts("File sent");
      fclose(fp);

      puts("Goodbye!!!\n");
    }
  
  close(clntSocket); // Close client socket
}
