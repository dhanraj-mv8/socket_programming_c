#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <sbcp.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>

char current_users[100][16] = {0};

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
	if (a1 != 3)
	{
		printf("Please check the input arguments. Format <port_no> <max clients allowed>\n");
		return -1;
	}


	int server1, client1, bind1, listen1, select1, write1, read1;
	int cli_limit = atoi(a2[2]);
	int cli_count = 0;
	int recent_sock = 0;
	int main_loop = 0;	
	int fd1;
	char generic_msg[532] = {0};
	char received_msg_array[532] = {0};
	char reason[32] = {0};
	char idle_user[16] = {0};
	
	fd_set Master; 
	fd_set readfds; 

	struct sockaddr_in servaddr1; 
	struct SBCP_Message *received_msg;
	struct SBCP_Message *sent_msg;

	FD_ZERO(&Master); 
	FD_ZERO(&readfds);

	server1 = socket(AF_INET, SOCK_STREAM, 0);

	if (server1 == -1)
	printf("Server: Error at socket creation.\n");

	bzero( &servaddr1, sizeof(servaddr1));
	
	servaddr1.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr1.sin_family = AF_INET; /// Bonus - IPv6 - Modify this to enable IPv6 addressing to AF_INET6
	servaddr1.sin_port = htons(atoi(a2[1]));\
	
	bind1 = bind(server1, (struct sockaddr *) &servaddr1, sizeof(servaddr1));
	if (bind1 == -1)
	printf("Server: Error binding server to local address.\n");
	
	listen1 = listen(server1, cli_limit);
	if (listen1 == -1)
		printf("Server: Error listening to port\n");
	
	printf("Server:Listening at port - %d\n",(atoi(a2[1])));
	
	signal(SIGCHLD, sigchild);
	
	//Adding server socket to master set.
	FD_SET(server1, &Master);
	//To track the socket file descriptor with highest value
	recent_sock = server1;
	
	for(;;)
	{
		readfds = Master;//cloning master set
		select1 = select(recent_sock+1, &readfds, NULL, NULL, NULL);
		if (select1 == -1)
		{
			printf("Server: Error at select execution.");
			return -1;
		}
		
		main_loop = 0;
		while(main_loop <= recent_sock)
		{
			if (FD_ISSET(main_loop, &readfds))//to check if any client is ready to send
			{ 
				if (main_loop != server1)
				{
					//allocating memory
					received_msg = malloc(sizeof(struct SBCP_Message));
					sent_msg = malloc(sizeof(struct SBCP_Message));
					//reading from client socket
					read1 = read(main_loop, received_msg, sizeof(struct SBCP_Message));
					//handle problem when reading from socket
					if (read1 <= 0)
					{
						if(read1 == -1)
							printf("Server: Error reading from client\n");
						if(read1 == 0)
						{ 
							//Bonus - Offline - Notify everyone
							printf ("Server: Client closed connection. Username - %s\n", current_users[main_loop]);
							for(fd1 = 0; fd1 <= recent_sock; fd1++)
							{
								if (FD_ISSET(fd1, &Master))
								{
									if (fd1 != main_loop && fd1 != server1)
									{
										//Message forwarded all clients except server and the exited client.
										sent_msg->attribute.Type = 2;
										sent_msg->Type = 6; 
										//sending username as payload in the offline message
										strcpy(sent_msg->attribute.Payload, current_users[main_loop]);
										write1 = write(fd1, sent_msg, sizeof (struct SBCP_Message));
										if (write1 == -1)
											printf("Server: Error writing to multiple clients");
									}
								}
							}
							//removing the username from the username list so that it can be reused in the same session
							current_users[main_loop][0]='\0';
						}
						//clearing all client related info when a client goes offline
						close(main_loop);
						cli_count--;
						FD_CLR(main_loop, &Master);
					}
					
					else
					{	
						//TYPE - 4 - SEND - FWD
						if (received_msg->Type == 4 && received_msg->attribute.Type == 4)
						{
							//Normal Forward to all clients
							sprintf(received_msg_array,"%s: %s", current_users[main_loop], received_msg->attribute.Payload);
							for (fd1=0; fd1<=recent_sock; fd1++)
							{
								if (FD_ISSET(fd1, &Master))
								{
									if (fd1 != main_loop && fd1 != server1) 
									{
										sent_msg->attribute.Type = 4; 
										sent_msg->Type = 3;
										//Forwarding the received payload with user identifier to every other client.
										strcpy(sent_msg->attribute.Payload, received_msg_array);
										write(fd1, sent_msg, sizeof(struct SBCP_Message));
									}
								}
							}
						}
						
						//TYPE - 9 - IDLE
						if (received_msg->Type == 9)
						{
							printf("Server: Client idle for long. Username - %s\n", current_users[main_loop]);
							for (fd1=0; fd1<=recent_sock; fd1++)
							{
								if (FD_ISSET(fd1, &Master))
								{
									if (fd1 != main_loop && fd1 != server1) 
									{
										sent_msg->attribute.Type = 2; 
										sent_msg->Type = 9;
										//Send username as payload
										strcpy(sent_msg->attribute.Payload, current_users[main_loop]);
										write(fd1, sent_msg, sizeof(struct SBCP_Message));
									}
								}
							}
						}
						
						
						//TYPE 2 - JOIN
						if (received_msg->Type == 2 && received_msg->attribute.Type == 2)
						{ 
							// Bonus - JOIN
							cli_count+=1;//updating total clients
							//printf("hi");
							if (cli_count > (cli_limit))
							{
								//TYPE - 5 - NAK
								// Maximum number of clients allowed is reached, rejecting with NAK message.
								printf ("Server: NAK sent for connect request. Reason - Chat room at capacity.\n");
								sent_msg->Type = 5; // Bonus - NAK 
								sent_msg->attribute.Type = 1;
								sprintf(reason, "Chat room is at capacity.");
								//NAK - Reason - 32 bytes length
								strcpy(sent_msg->attribute.Payload, reason); 
								//payload is reason for NAK from server to client
								write(main_loop, sent_msg, sizeof(struct SBCP_Message));
								//clearing client related information
								close(main_loop);
								cli_count--;
								FD_CLR(main_loop, &Master);
							}
							else
							{
								bool unique_user = true;
								for(fd1=0; fd1<=recent_sock; fd1++)
								{
									if(strcmp(received_msg->attribute.Payload,current_users[fd1])==0)
									{
										//TYPE - 5 - NAK
										//Username is not unique
										sent_msg->attribute.Type = 1;
										sent_msg->Type = 5; // Bonus - NAK
										sprintf(reason, "Username already exists.");//NAK - Reason - 32 bytes length
										strcpy(sent_msg->attribute.Payload, reason ); //NAK Reason - 32 Bytes length 
										write(main_loop, sent_msg, sizeof(struct SBCP_Message));
										cli_count--;
										close(main_loop);
										FD_CLR(main_loop, &Master);	
										unique_user=false;
										break;
									}
								}
								if (unique_user)
								{ 
									//TYPE 7 - ACK
									//Username is valid
									sprintf(current_users[main_loop],"%s",received_msg->attribute.Payload);
									sent_msg->Type = 7; // Bonus - ACK
									//Client count is exactly 1 then client should wait until new user joins before sending.
									if (cli_count == 1)
										strcpy (generic_msg,"Total Clients = 1, wait for users to join before sending!");
									else
									{
										sprintf(generic_msg, "Total Clients = %d. ", cli_count);
										strcat (generic_msg,"Users currently online: ");
										if(cli_count != 1)
										{
											for(fd1=4; fd1<=recent_sock; fd1++)
											{
													if(fd1 != server1 && fd1 != main_loop)
													{
														//concatenating all the usernames into a char array
														strcat (generic_msg, current_users[fd1]);
														strcat (generic_msg," ");
													}
											}
										}
									}
									strcpy(sent_msg->attribute.Payload, generic_msg);
									//Bonus - ACK - With total clients count and usernames of all clients involved.
									printf ("Server: ACK message sent to client: %s\n", received_msg->attribute.Payload);
									write(main_loop, sent_msg, sizeof(struct SBCP_Message));
									
									//Bonus - Online - Notify all clients when a new client joins
									printf("Server: %s joined chat room\n", current_users[main_loop]);
									for(fd1 = 0; fd1 <= recent_sock; fd1++)
									{
										if (FD_ISSET(fd1, &Master))
										{
											if (fd1 != server1 && fd1 != main_loop)
											{
												//Message forwarded all clients except server and the new client.
												sent_msg->attribute.Type = 2;
												sent_msg->Type = 8; 
												//sending username as payload for online message.
												strcpy(sent_msg->attribute.Payload, current_users[main_loop]);
												write1 = write(fd1, sent_msg, sizeof (struct SBCP_Message));
												if (write1 == -1)
													printf("Server: Error writing to multiple clients");
											}
										}
									}
								}
							}
						}
						//releasing the memory earlier received
						free(sent_msg);
						free(received_msg);
					}
				}
				else
				{
					//When a new client tries to connect
					if (cli_count <= cli_limit)
					{
						//accepting new client connection request.
						client1 = accept(server1, (struct sockaddr*) NULL, NULL);
						if(client1 == -1)
							printf("Server: Error accepting client");
						else
						{
							//new client added to master set
							FD_SET(client1, &Master);
							if (client1 > recent_sock)
								recent_sock = client1;
							
						}
					}
				}
			}
			main_loop++;
		}
	}
	//close both client and server.
	close(client1);
	close(server1);
}

