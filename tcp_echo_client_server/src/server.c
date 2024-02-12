/*
TCP Multi-Client Echo Server

Authors:
Dhanraj Murali / Swetha Reddy

Course:
ECEN 602 - Machine Problem 1

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>


int writen(int client_socket, const void *ptr, int n)
{
	int s_bytes, w_bytes;
	char *sent = (char *) ptr;
	for(w_bytes = n; w_bytes >0;w_bytes-= s_bytes)	
	{
	   s_bytes = write(client_socket,ptr,w_bytes);
	   if(s_bytes<=0)
	   {
		 if(errno == EINTR) 
		   s_bytes = 0; //initiate to write again
		 else  
		   return 1;         
	   }
	   ptr+= s_bytes;
	}
   	printf("Server: Echoed = %s\n",sent);
    	return n;
}


void sigchild(int s)
{
	pid_t p1;
	int stat;
	
	while((p1 = waitpid(-1,&stat, WNOHANG)) >0)
	printf("Child with Process ID = %d is terminated\n", p1);
	return;
}


int main(int a1, char *a2[]) 
{
	if (a1!=2) 
	{
		printf("Server: Please check input arg\n" );
       		return 0;
 	}

	int server1,client1;  
	struct sockaddr_in servaddr1,clientaddr1; 
	ssize_t s_bytes,r_bytes;
	int max=10;
	char temp[max];
	pid_t fork1;

	//server definition
	//servaddr1.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr1.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr1.sin_family = AF_INET;          
	servaddr1.sin_port = htons(atoi(a2[1]));     
	 
	//server connection creation using socket api
	server1 = socket(AF_INET,SOCK_STREAM,0);
	int bind1 = bind(server1,(struct sockaddr*)&servaddr1,sizeof(servaddr1));
	if(bind1 == -1)
	{
	perror("Server: Unable to bind socket to port/ip: %s");
	//err_sys("bind error");
	return 1;
	}
	printf("Server: Socket creation/bind successful.\n");
	
	//server to start listening, max no of concurrent clients allowed is defined as 10
 	int listen1 = listen(server1, 10);
 	if(listen1==0)
	printf("Server: Listening at Port = %d, Use this to initiate client!\n",atoi(a2[1]));
	else
	return -1;
	signal(SIGCHLD, sigchild);
	socklen_t cli_addr_len;
	char ipaddr[max];
	for(;;)
	{

		cli_addr_len = sizeof(clientaddr1);
		client1 =accept(server1, (struct sockaddr*)&clientaddr1, &cli_addr_len);
		if(client1==-1)  
		{	
			if(errno==EINTR)
			continue;
			else
 			printf("Server: Exiting due to accept error: %s\n", strerror(errno));
  			return 1;
		}
  		printf("Server: Server-Client: Connection Successfully Established!\n");
		
		//client ip address display			
  		inet_ntop(AF_INET, &clientaddr1.sin_addr, ipaddr, 16);
  		//inet_ntoa(&clientaddr1.sin_addr);
  		printf("Server: Connection etablished with %s\n",ipaddr);
		
		//forking server to handle concurrent clients
		fork1 = fork();
		//if value returned by fork() is 0, then it is a child process. following block will get executed only in child.
		if(fork1 == 0){
		for(;;)
		{	
		//read the input from the remote client socket connection and store it in a char array of max size defined 100 
			close(listen1);
			//child process does not need to listen as parent will be listening
			r_bytes = read(client1,temp,max-1);
			if(r_bytes <= 0)
			{
				printf("Server: Reached EOF from client. Disconnecting client.\n");			
				close(client1);
				return 1;
				
			}
			printf("Server: Received = %s\n",temp);
			
			//echo the received bytes back to the client socket via the server socket
			s_bytes=writen(client1,temp,strlen(temp));
			//flushing the char array to continue reading from the client until eof/termination reached
			bzero(temp,max);
			//close(client1);
			}
			
		}
		
	}
	close(client1);
	printf("Exiting Server.");
	close(server1);
  	return 0;
}
