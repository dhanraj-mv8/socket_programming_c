#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sbcp.h>


char* clientname;
char* serverIP;
int serverPort;

int client_create_socket(int *listen_fd);

int main(int argc,char *argv[])
{
    int listen_fd = 0;
    int sockfd = 0;
    fd_set readfds;
    fd_set writefds;
    fd_set excepfds;
    char line[MAX_STR_LEN];
    int num_bytes;
    if (argc != 4){
    printf ("Incorrect arguments FORMAT: ./client <Username> <IP_Address> <Port_Number>");
    return 0;
    }
    //assigning clientname, server IP and port from command line
    clientname = argv[1];
    serverIP = argv[2];
    serverPort = atoi(argv[3]);

    if(client_create_socket(&listen_fd) != 0) {
        exit(0);
    }
    sockfd = listen_fd;

    struct SBCP_Message *client_message;
    struct SBCP_Message *server_message;

    //allocating memory on the heap for client_message
    client_message = malloc(sizeof(struct SBCP_Message));
    //assigning the attribute type = 2 for username with length 16 bytes and storing the client name in username
    client_message->attribute.Type = 2;                    
    client_message->attribute.Length = 16;                 
    strcpy(client_message->attribute.Payload, clientname);  

    //JOIN request sent to server by setting the message type = 2
    client_message->Version = 3;                                 
    client_message->Type = 2;                                    
    client_message->Length = 24;                                   
    //client_message = SBCP_Message(3,2,24, SBCP_Attribute_Format(2,20, clientname));

    printf ("CLIENT: Attempting to JOIN\n");
    if (write(sockfd, client_message, sizeof(struct SBCP_Message)) == -1) {
        printf("ERROR: JOIN Error");
    }
    int linesize = 0;
    int ret_val;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    struct timeval timeout;

    while(1){
        //setting the timeout to 10 sec and passing the timeout value to the select()
        timeout.tv_sec = 10;
        ret_val = select(sockfd+1, &readfds, NULL, NULL, &timeout);
        if (ret_val == -1) {
            //error occured in select()
            printf("ERROR: Select Error");
            exit(0);
        }

        //timeout has occured, sending to Server indicating that the Client is IDLE
        if(ret_val == 0){
            //allocating memory on the heap for client_message
            client_message = malloc(sizeof(struct SBCP_Message));
            //assigning the attribute type = 2 for username with length 16 bytes                                 
            //client_message->attribute = NULL;
            //assigning the message type to 9 to indicate IDLE
            client_message->Version = 3;                                
            client_message->Type = 9;                                    
            client_message->Length = 24;    
            if (write(sockfd, client_message, sizeof(struct SBCP_Message)) == -1) 
                printf("ERROR: Write Error");
        }

        //reading from the client input and writing to server
        if(FD_ISSET(0, &readfds)){
            //clearing the buffer
            bzero(line, MAX_STR_LEN);
            //reading from the standard input
            fgets(line,MAX_STR_LEN,stdin);
            linesize = strlen(line);
            //if client exits updating the message
            if(line[linesize-1] == '\n'){ 
                line[linesize-1] = '\0';
            }
            //SEND to the server

            //allocating memory on the heap for client_message                
            client_message = malloc(sizeof(struct SBCP_Message));
            
            //assigning message type = 4 for SEND request
            client_message->Version = 3;                                 
            client_message->Type = 4;                                    
            client_message->Length = 520;
            //assigning the attribute type = 4 for message with length 512 bytes                                
            client_message->attribute.Type = 4;                    
            client_message->attribute.Length = 512;               
            strcpy(client_message->attribute.Payload, line);
            if (write(sockfd, client_message, sizeof(struct SBCP_Message)) == -1) 
                printf("ERROR: Write Error");
                
                    
        }

        //reading data from the server and displaying to client
        if(FD_ISSET(sockfd, &readfds)){    
            //allocating memory on the heap for client_message                                                
            server_message = malloc(sizeof(struct SBCP_Message));
        
            num_bytes = read(sockfd, server_message, sizeof(struct SBCP_Message));

            if(server_message->Type == 7){                                                      //ACK
                printf ("%s \n", server_message->attribute.Payload);  
            }
            else if(server_message->Type == 5 && server_message->attribute.Type == 1)      // NAK 
            {
                printf ("%s\n", server_message->attribute.Payload);  
                exit(0);           
            }

            else if(server_message->Type == 8 && server_message->attribute.Type == 2)      // ONLINE 
            {
                printf ("%s is ONLINE \n", server_message->attribute.Payload);             
            }

           else if(server_message->Type == 6 && server_message->attribute.Type == 2)      // OFFLINE
            {
                printf ("%s is OFFLINE, user has left.\n", server_message->attribute.Payload);           
            }

            else if(server_message->Type == 9 && server_message->attribute.Type == 2)      // IDLE message from Client to close connection
            {
                printf ("%s is IDLE \n", server_message->attribute.Payload);      
            }
            else{
            printf ("%s \n", server_message->attribute.Payload); //FWD
            }
            free(server_message);
            
             
        }
        FD_SET (0, &readfds);
        FD_SET (sockfd, &readfds);
        
    }
    close(sockfd);  
    //
    return 0;
}

//client socket creation for IPv4
int client_create_socket(int *listen_fd) {
    
    struct sockaddr_in server_addr;

    if((*listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("ERROR : Socket creation failed");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(serverIP);
    server_addr.sin_port = htons(serverPort); 

    
    if(connect(*listen_fd,(struct sockaddr *)&server_addr,sizeof(struct sockaddr))!=0) {
        printf("ERROR : Connection to server failed");
        return -1;
    }
    return 0;

}

//For IPv6, use the below client_create_socket method.

// int client_create_socket(int *listen_fd) {
//     struct sockaddr_in6 server_addr;

//     if ((*listen_fd = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {
//         printf("ERROR : Socket creation failed");
//         return -1;
//     }

//     memset(&server_addr, 0, sizeof(server_addr));
//     server_addr.sin6_family = AF_INET6;
//     if (inet_pton(AF_INET6, serverIP, &server_addr.sin6_addr) != 1) {
//         printf("ERROR : Invalid server IP address");
//         close(*listen_fd);
//         return -1;
//     }
//     server_addr.sin6_port = htons(serverPort);

//     if (connect(*listen_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in6)) != 0) {
//         printf("ERROR : Connection to server failed");
//         close(*listen_fd);
//         return -1;
//     }

//     return 0;
// }





