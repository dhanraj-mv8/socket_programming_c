#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <pthread.h>
#include <time.h>

#define DATA 3
#define ACK 4
#define ERR 5

#define max_file_size 512
#define timeout_limit 10

//OPCODE for request type
#define RRQ 1
#define WRQ 2

//OPCODE for mode 
#define NETASCII 1
#define OCTET 2

typedef struct tftp_req tftp_req_t;
typedef struct tftp_ack tftp_ack_t;
typedef struct tftp_dat tftp_dat_t;
typedef struct tftp_err tftp_err_t;

struct tftp_req {
    uint16_t oc;
    uint8_t filename_mode[max_file_size];
};

struct tftp_ack {
    uint16_t oc;
    uint16_t blk_num;
};

struct tftp_dat {
    uint16_t oc;
    uint16_t blk_num;
    uint8_t data[max_file_size];
};

struct tftp_err {
    uint16_t oc;
    uint16_t err_cd;
    uint8_t err_dat[max_file_size];
};

//strcuture for TFTP packet
typedef union tftp_pkt tftp_pkt_t;

union tftp_pkt {
    uint16_t oc;
    tftp_req_t request;
    tftp_ack_t ack1;
    tftp_dat_t data;
    tftp_err_t error;
};

uint16_t val;
ssize_t mg;
int cn;
ssize_t snd_msg(int wrt1, tftp_pkt_t * message, size_t size, struct sockaddr_in *socket_addr, socklen_t socklen1)
{
	ssize_t data_sent = sendto(wrt1, message, size, 0, (struct sockaddr *) socket_addr, socklen1);
    // Check if data was sent successfully
	if(!(data_sent >= 0))
	{
		perror("Server: Error Sending Data");
	}
	return data_sent;
}

ssize_t rcv_msg(int rcv1, tftp_pkt_t * rcv_data, struct sockaddr_in * address, socklen_t * socklen1){
	ssize_t rcv_size = recvfrom(rcv1, rcv_data, sizeof(*rcv_data), 0, (struct sockaddr *) address, socklen1);
	if(rcv_size < 0)
	{
        //Check if not EAGAIN
		if(errno !=EAGAIN)
		{
			perror("Server: Message receive failed! ");
		}
	}
	return rcv_size;
}


ssize_t tftp_send(int action, int wrt1, int mode, uint16_t ack_exp, uint8_t *data, ssize_t msg_len, char * err_dat, struct sockaddr_in * address, socklen_t socklen1)
{
tftp_pkt_t msg2send;
switch (action) {
	case 1:
		{
            //send DATA
			char msg_content[max_file_size];
			int add_char = 0;
			int max_len1 = 0;
			
			memcpy(msg_content, data, msg_len);
			max_len1 = add_char+msg_len;
			
			msg2send.data.blk_num = htons(ack_exp);
			msg2send.oc = htons(DATA);
			
			memcpy(msg2send.data.data, msg_content, max_len1);
			return snd_msg(wrt1, &msg2send, max_len1+4, address, socklen1);
		}
		break;
	case 2:	
		{
            //send ACK
			msg2send.ack1.blk_num = htons(ack_exp);
			msg2send.oc = htons(ACK);
			
			return snd_msg(wrt1, &msg2send, sizeof(msg2send.ack1), address, socklen1);
		}
	break;
	case 3:
		{
            //send ERR
			if(!(max_file_size > strlen(err_dat))){
				return 0;
			}
			
			msg2send.error.err_cd = htons(mode);
			msg2send.oc = htons(ERR);
			
			strcpy((char *)msg2send.error.err_dat, err_dat);
			return snd_msg(wrt1, &msg2send, strlen(err_dat) + 5, address, socklen1);
		}
		break;
	default:
		break;
	}
	return -1;
}


void sigchild(int s)
{
	pid_t p1;
	int stat;
	
    //terminating the child process
	while((p1 = waitpid(-1,&stat, WNOHANG)) >0)
	printf("Server: Child with Process ID = %d is terminated\n", p1);
	return;
}

int get_mode (char *track_mode)
{
    int trtype = 0;
    //getting whether mode is ascii or binary
    if (strcasecmp(track_mode, "netascii") == 0)
        trtype = NETASCII;

    if (strcasecmp(track_mode, "octet") == 0)
        trtype = OCTET;

    return trtype;
}

void tftp_request(tftp_pkt_t *message, ssize_t msglen, struct sockaddr_in *client1, socklen_t socklen1gth)
{
    tftp_pkt_t client_msg1;
    int            server1;
    ssize_t        len_data, gen_size;
    uint16_t       ack_exp = 0;
    FILE          *fd, *temp_fd;
    int            trtype = 0;
    uint16_t       oc1;
    uint8_t        data[512];
    int            attempts;
    int            flag1 = 0;
    char           tmp_fname[50];


    //new socket for handling request.

   server1 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server1 < 0)
    {
        perror("Socket creation error");
        exit(-1);
    }
    printf("Server: Socket creation succeeded!\n");
    
    char          *fname, *track_mode, *eof_check;
    //timeval structure for setting timeout
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    //passing the tv to set the socket options
    if (setsockopt(server1, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror("setsockopt() fail");
        exit(-1);
    }

    printf("Server: Client joined, Details: %s:%u!\n", inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
    
	fname = (char *)message->request.filename_mode;
    eof_check = &fname[msglen - 3];
	
    if (!(*eof_check == '\0'))
    {
        printf("Server:Invalid file name/mode requested %s':%u!\n", inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
        tftp_send(3, server1, 0, val, NULL, mg, "Invalid filename or mode", client1, socklen1gth);
        exit(-1);
    }
	
    track_mode = strchr(fname, '\0') + 1;

    if (track_mode > eof_check)
    {
        printf("Server: Please specify the mode of transfer - %s:%u\n",inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
        tftp_send(3, server1, 0, val, NULL, mg, "transfer mode not given", client1, socklen1gth);
        exit(-1);
    }


    oc1 = ntohs(message->oc);
    if(oc1 == RRQ)
    	fd = fopen(fname, "r");
    else if(oc1 == WRQ)
    	fd = fopen(fname, "w");
    
    if (fd == NULL)
    {
        printf("Server: File not found\n");
        tftp_send(3, server1, 1, val, NULL, mg, "File not found", client1, socklen1gth);
        exit(-1);
    }

    //retrieving the mode of transfer
   trtype = get_mode(track_mode);
   if (trtype == 0)
   {
       printf("Server: Please set the correct transfer mode - %s:%u\n", inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
       tftp_send(3, server1, 0, val, NULL, mg, "Invalid mode", client1, socklen1gth);
       exit(-1);
   }

   printf("Server: File %s in mode %s will be %s by %s:%u! \n", fname, track_mode, ntohs(message->oc) == RRQ ? "read" : "written", inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
	
	//BONUS: BELOW METHOD TO HANDLE WRQ REQUEST
	switch(oc1)
	{
		case (WRQ):
		{
			flag1 = 0;
			gen_size = 0;
			ack_exp = 0;
			tftp_pkt_t client_msg1;

			if (tftp_send(2, server1, cn, ack_exp, NULL, mg, NULL, client1, socklen1gth) < 0)
			{
				printf("Server: Unable to send, session terminated by %s:%u!\n", inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
				exit(-1);
			}

			while (!flag1)
			{
                //setting timeout limit
				attempts = timeout_limit;
				while(attempts)
				{
					gen_size = rcv_msg(server1, &client_msg1, client1, &socklen1gth);

					if(gen_size >= 0)
					{
						if(gen_size < 4)
						{
							printf("Server: Request of invalid size received from %s:%u\n", inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
							tftp_send(3, server1, 0, val, NULL, mg, "Invalid req size", client1, socklen1gth);
							exit(-1);
						}
					}

					if (gen_size >= 4)
						break;
					
					if (errno != EAGAIN)
					{
						printf("Server: session terminated for %s:%u!\n", inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
                        //Terminating client session
						exit(-1);
					}

					gen_size = tftp_send(2,server1, cn, ack_exp, NULL, mg, NULL, client1, socklen1gth);

					if (gen_size < 0)
					{
                        //Terminating client session
						printf("Server: session terminated for %s:%u!\n", inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
						exit(-1);
					}
					attempts--;
				}

				if (attempts == 0)
				{
                    //displaying timeouts
					printf("Server: Timer count = %d. Client not responding - %s:%u!\n", attempts, inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
					exit(-1);
				}

				ack_exp++;

				if (gen_size < sizeof(client_msg1.data))
					flag1 = 1;
				
				if (ntohs(client_msg1.ack1.blk_num) != ack_exp)
				{
					printf("Server: Invalid Block received from %s:%u!\n", inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
					tftp_send(3, server1, 0, val, NULL, mg, "Invalid Block number", client1, socklen1gth);
					exit(-1);
				}

				if (ntohs(client_msg1.oc) != DATA)
				{
					printf("Server: unexpected data received from %s:%u!\n", inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
					tftp_send(3, server1, 0, val, NULL, mg, "unexpected data received", client1, socklen1gth);
					exit(-1);
				}

				if (ntohs(client_msg1.oc) == ERR)
				{
					printf("Server: Error message %u:%s received for %s:%u\n", ntohs(client_msg1.error.err_cd),
							client_msg1.error.err_dat, inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
					exit(-1);
				}


				gen_size = fwrite(client_msg1.data.data, 1, gen_size - 4, fd);

				if (gen_size < 0)
				{
					perror("write()");
					exit(-1);
				}

				gen_size = tftp_send(2, server1, cn, ack_exp, NULL, mg, NULL,client1, socklen1gth);

				if (gen_size < 0)
				{
					printf("Server: session terminated for %s:%u!\n", inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
					exit(-1);
				}

			}
		}
		break;
	//METHOD TO HANDLE RRQ REQUEST
    case (RRQ):
    {
        if (trtype == NETASCII)
        {
            //dealing with netascii for CR
            sprintf(tmp_fname, "tmp_fname_xyz.txt");
			FILE * temp_fd = fopen(tmp_fname, "w");
			int x1;
			int char_nxt = -1;
			while(1){
				x1 = getc(fd);
				if(char_nxt >= 0){
					fputc(char_nxt, temp_fd);
					char_nxt = -1;
				}
				if(x1 == EOF){
					if(ferror(temp_fd)){
						perror("Error writing to Temp File");
					}
					break;
				}
				else if(x1 == '\n'){
					x1 = '\r';
					char_nxt = '\n';
				}
				else if(x1 == '\r'){
					char_nxt = '\0';
				}
				else{
					char_nxt = -1;
				}
				fputc(x1,temp_fd);
			}
			fclose(temp_fd);
			temp_fd =  fopen(tmp_fname, "r");
            fd = temp_fd;
        }
        
        while (!flag1)
        {
            len_data = fread(data, 1, sizeof(data), fd);
            ack_exp++;

            if (len_data < 512)
                flag1 = 1;
            
	    
            attempts = timeout_limit;
            while (attempts)
            {
                if (tftp_send(1, server1, trtype, ack_exp, data, len_data,  NULL,client1, socklen1gth) < 0)
                {
                    printf("Server: session terminated for %s:%u!\n", inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
                    exit(-1);
                }

                gen_size = rcv_msg(server1, &client_msg1, client1, &socklen1gth);

                if (gen_size >= 0)
				{
					if(gen_size < 4)
					{
						printf("Server: Invalid req size received from %s:%u\n",inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
						tftp_send(3, server1, 0, val, NULL, mg, "Invalid req size", client1, socklen1gth);
						exit(-1);
					}
				}
                if (gen_size >= 4)
                break;
                

                if (errno != EAGAIN)
                {
                    printf("Server: session terminated for %s:%u!\n", inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
                    exit(-1);
                }

                printf("Server: Timer count = %d. Client not responding - %s:%u!\n", attempts, inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
                attempts--;
            }

            if (attempts == 0)
            {
                printf("Server: Transfer session has failed\n");
                exit(-1);
            }

            if (ntohs(client_msg1.oc) == ERR)
            {
                printf("Server: Error message %u:%s received for %s:%u\n", ntohs(client_msg1.error.err_cd),
                        client_msg1.error.err_dat, inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
                exit(-1);
            }

            if (ntohs(client_msg1.oc) != ACK)//acknowledgement
            {
                printf("Server: unknown message received from %s:%u!\n",	inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
                tftp_send(3, server1, 0, val, NULL, mg, "unexpected message received", client1, socklen1gth);
                exit(-1);
            }

            if (ntohs(client_msg1.ack1.blk_num) != ack_exp) // size of Ack Block Number is too high
            {
                printf("Server: Invalid ack1 received from %s:%u!\n", inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
                tftp_send(3, server1, 0, val, NULL, mg, "Invalid ack1 number", client1, socklen1gth);
                exit(-1);
            }
        }
    }
	break;
	default:
		break;

	}
    printf("Server: Data transfer done successfully for %s:%u!\n", inet_ntoa(client1->sin_addr), ntohs(client1->sin_port));
    fclose(fd);
    if(tmp_fname != NULL){
        remove(tmp_fname);
    }
    close(server1);
    exit(0);
}



int main(int argc, char *argv[])
{
    struct sockaddr_in server1, client1;
    socklen_t socklen1gth = sizeof(server1);
    ssize_t msg_len;
    int create1;
    uint16_t server_port , opc;
    tftp_pkt_t  data1;

    if (argc < 2)
    {
        printf("Server: Check the input arguments. USAGE: .server <server_ip> <server_port>\n");
        exit(-1);
    }

    server_port = htons(atoi(argv[2]));
    create1 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (create1 == -1)
    {
        perror("Server: Socket creation error:");
        exit(-1);
    }
    else
    {
        printf("Server: Socket Creation successful!\n");
    }
	
    server1.sin_family = AF_INET;
    server1.sin_addr.s_addr = htonl(INADDR_ANY);
    server1.sin_port = server_port;
	//server binding
    if (bind(create1, (struct sockaddr *) &server1, sizeof(server1)) == -1)
    {
        perror("Server: Error in binding");
        close(create1);
        exit(-1);
    }
    else
    {
        printf("Server: Socket is bound!\n");
    }
	
	
	//HANDLE ZOMBIE PROCESS
    signal(SIGCHLD, sigchild);

    printf("Server: Server is listening on port %d\n", ntohs(server_port));

    while (1)
    {
        msg_len = rcv_msg(create1, &data1, &client1, &socklen1gth);
        if (msg_len < 0)
        {
            continue;
        }

        if (msg_len < 4)
        {
            printf("Server: Requested filename from %s:%d is not allowed.\n", inet_ntoa(client1.sin_addr), ntohs(client1.sin_port));
            tftp_send(3, create1, 0, val, NULL, mg, "Invalid size", &client1, socklen1gth);
            continue;
        }

        opc = ntohs(data1.oc);

        if (opc == RRQ || opc == WRQ)
        {
            if (fork() == 0)
            {
				//only called in child process
                tftp_request(&data1, msg_len, &client1, socklen1gth);
                exit(-1);
            }

        }
        else
        {
            printf("Server: Type request not valid from %s:%d. Please check!\n", inet_ntoa(client1.sin_addr), ntohs(client1.sin_port));
            tftp_send(3, create1, 0, val, NULL, mg, "Invalid opcode", &client1, socklen1gth);
        }
    }
    close(create1);
    printf("Server: Closing server!\n");
    return 0;
}

