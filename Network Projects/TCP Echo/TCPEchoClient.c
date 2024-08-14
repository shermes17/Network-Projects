/**
 * Sean Hermes
 * CPSC 3600
 * Assignment 1
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Practical.h"


#include <stdbool.h>

int main(int argc, char *argv[]) {

  
  //CHANGE TO HAVE CORRECT PARAMETERS
  if (argc < 2|| argc > 3){ // Test for correct number of arguments
    DieWithUserMessage("Parameter(s)",
        "<Server Address> [<Server Port>]");
  }
  
 
  char *servIP = argv[1];     // First arg: server IP address (dotted quad)
 
  
  // Third arg (optional): server port (numeric).  7 is well-known echo port
  in_port_t servPort = (argc == 3) ? atoi(argv[2]) : 7;

  // Create a reliable, stream socket using TCP
  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0)
    DieWithSystemMessage("socket() failed");

  // Construct the server address structure
  struct sockaddr_in servAddr;            // Server address
  memset(&servAddr, 0, sizeof(servAddr)); // Zero out structure
  servAddr.sin_family = AF_INET;          // IPv4 address family


  // Convert address
  int rtnVal = inet_pton(AF_INET, servIP, &servAddr.sin_addr.s_addr);

  if (rtnVal == 0)
    DieWithUserMessage("inet_pton() failed", "invalid address string");
  else if (rtnVal < 0)
    DieWithSystemMessage("inet_pton() failed");
  servAddr.sin_port = htons(servPort);    // Server port
  

  
  // Establish the connection to the echo server
  if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0){
    DieWithSystemMessage("connect() failed");
  }

  // FILE EXCHANGE
  char* msg;
  char buffer[BUFSIZE];
  char filePick[2];
  char* fileName;

  
  printf("Requesting a list of files from the server %s\n", servIP);
  
  // send server request for file command
  // start file request
  msg = "REQ";
  int msg_size = send(sock, msg, sizeof(msg), 0);
  if (msg_size < 0)
    DieWithSystemMessage("send() failed");

  // receive response from server saying request was received
  int buffsize = recv(sock, buffer, sizeof(buffer),0);
  if (buffsize < 0)
    DieWithSystemMessage("recv() failed");
  
  // confirm request was received
  if(strcmp(buffer,"LRC") == 0)
      puts("List recieved.");

  // receive list of files from server
  buffsize = recv(sock, buffer, 1024,0);
  if (buffsize < 0)
    DieWithSystemMessage("recv() failed");

  printf("%s", buffer); 

  // get input for which file is chosen
  int x;
  do{
  scanf("%s", filePick);
  x = atoi(filePick);
  if (x < 1 || x > 3){
    puts("Invalid option, please pick again!");
  }
  }while(x < 1 || x > 3);

  // send choice to the server 
  msg_size = send(sock, filePick, sizeof(filePick), 0);
  if (msg_size < 0)
    DieWithSystemMessage("send() failed");

  msg_size = recv(sock, buffer, sizeof(buffer), 0);
  if (msg_size < 0)
    DieWithSystemMessage("recv() failed");
  printf("Requesting file \"%s\"\n", buffer);


  // get size of file form the server, and allocate that amount of memory
  long int fileSize;
  msg_size = recv(sock, &fileSize, sizeof(fileSize), 0);
  if (msg_size < 0)
    DieWithSystemMessage("recv() failed");
  char file[fileSize];


  // make sure file received is not corrupted
  bool fileValid = false;
  do{
    // recieve file
    msg_size = recv(sock, &file, sizeof(file), 0);
    if (msg_size < 0)
      DieWithSystemMessage("recv() failed");

    // mae sure received file is expected size
    if(msg_size == fileSize){
      fileValid = true;
    }
    else{
      // tell server file was bad, loop back to receive new file from server
      strcpy(buffer,"BADFILE");
      msg_size = send(sock,buffer, buffsize, 0);
      if (msg_size < 0)
        DieWithSystemMessage("send() failed");
    }
  } while(!fileValid);

  // tell server file was good
  strcpy(buffer,"GOODFILE");
  msg_size = send(sock,buffer, buffsize, 0);
  if (msg_size < 0)
        DieWithSystemMessage("send() failed");

   // receivd chosen file
  puts("File received:");

  //print file
  char c;
  for(int i = 0; i < fileSize; i++){
    printf("%c", file[i]);
  }


  puts("\nGoodbye!!!\n");
  close(sock);
  exit(0);
}
