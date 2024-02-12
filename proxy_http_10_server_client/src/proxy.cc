#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <bits/stdc++.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <sstream>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <string>
#include "utils.h"
#include <map>
#include <set>

std::map<int,struct req_detail> req_details; 
std::map<int,struct client_request> new_specs;
std::map<std::string,int> req_cache_id;

struct LRU_entry LRU[CACHE_CAPACITY];

std::set<int> unique_numbers;


int main(int a1, char *a2[])
{
	int server1, client1, error;
	int recv_data_count;
	char temp_char_buff[BUFFER_SIZE];
	char time_stamp[256];
	char temp_ch1[512];
	
	if(a1 != 3)
	{
		printf("Proxy: Incorrect format Usage: ./proxy <IP Address> <Port number>\n");
		return 1;
	}
	
	struct addrinfo addr;
	struct addrinfo *addr_info, *p;
	
	//HTTP operates over TCP
	addr.ai_family = AF_INET;
	addr.ai_socktype = SOCK_STREAM;
	
	
	struct sockaddr_in serveraddr1;
	const std::ios_base::openmode file_mode1 = std::ios::in | std::ios::binary;
	const std::ios_base::openmode file_mode2 = std::ios::out | std::ios::binary;
	const std::ios_base::openmode file_mode3 = std::ios::out | std::ios::binary | std::ios::app;
	
	char *ip = a2[1];
	char *port = a2[2];

	server1 = socket(AF_INET, SOCK_STREAM, 0);
	if(server1 == -1){
		printf("Proxy: Socket creation error!\n");
		exit(1);
	}
	
	bzero(&serveraddr1,sizeof(serveraddr1));
	serveraddr1.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr1.sin_family = AF_INET;
	serveraddr1.sin_port = htons(atoi(a2[2]));
	int bind1 = bind(server1, (struct sockaddr *)&serveraddr1, sizeof(serveraddr1));
	if(bind1 == -1){
		printf("Proxy: Server socket bind error\n");
		exit(1);
	}
	int listen1 = listen(server1, MAX_CLIENTS);												
	if( listen1 == -1){
		printf("Proxy: Failed to listen to clients\n");
		exit(1);
	}
	
	
	fd_set master_fds, read_fds, write_fds, master_wrt_fds;
	//clearing the File descriptors
	FD_ZERO(&master_fds);
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	FD_ZERO(&master_wrt_fds);
	//adding server1 fd to the masters fds
	FD_SET(server1,&master_fds);
	
	int max_fd_count = server1;
	
	int k=0;
	while(CACHE_CAPACITY > k){
		LRU[k].lu=k+1;
		LRU[k].val=0;
		k++;
	}
	
	
	struct tm time_val,*utc_format;
	int tm_size = sizeof(struct tm);
	time_t extracted_time;
	printf("The proxy is ready and listening for client requests at %s and port %s\n", ip, port );
	for(;;)
	{
		read_fds =  master_fds;
      	write_fds = master_wrt_fds;
	   
	    //Multiple clients are handled through select similar to MP2
	    int select1 = select(max_fd_count + 1, &read_fds, &write_fds, NULL, NULL);
			if( select1 ==-1)
			{
				printf("Proxy: Error at select execution.\n");
				exit(4);
			}	   
			int i = 0;
				while(i <= max_fd_count)
				{
					//looping through all the file descriptors
					if(FD_ISSET(i,&read_fds))
					{
						//client connections getting connected to the proxy
						
						if(i==server1)
						{
							client1 = accept(server1, (struct sockaddr *)NULL, NULL);
							if( client1 == -1)
							{
								printf("Proxy: Error accepting client\n");
							}
							else
							{
								//new client added to the master_fds set
								FD_SET(client1,&master_fds);

								new_specs[client1].mode=false;

								if(client1 > max_fd_count){
									max_fd_count = client1;
								}
								printf("Client connection established\n");
							}
						}
						else
						{
								recv_data_count = recv(i,temp_char_buff,sizeof(temp_char_buff),0);
							 	if(recv_data_count <= 0)
								{
									if(recv_data_count != 0)
										printf("Proxy: Error in receiving data\n");

									close(i);
									FD_CLR(i, &master_fds);
									
									if(new_specs[i].mode)
									{
											
											FD_SET(new_specs[i].client_request_id,&master_wrt_fds);
											int cid = new_specs[i].client_request_id;
											int cache_ix = req_details[cid].cache_id;
											if(cache_ix!=-1)
											{
												//Since cache_ix is not 1, we will now serve from cache block
												std::ifstream file2;
												//printf(req_details[cid].filename);
												file2.open(req_details[cid].filename,file_mode1);
												
												std::string line;
												bool notModified = false;
												
												if(file2.is_open())
												{
													getline(file2,line);
													if(line.find("304")!=-1)		
														notModified = true;
													bool containsExpHeader = false;
													
													while(file2.eof() == false)
													{
														getline(file2,line);
														
														if(line=="\r")
															break;
														int exp_index = line.find("Expires:");
														//printf("exp index %d\n",exp_index);
														if(exp_index == 0)
														{
															containsExpHeader = true;
															int ptr;
															if(line[8]==' ')
																ptr = 9;
															else
																ptr= 8;
															
															std::stringstream s2;
															s2 << line.substr(ptr);
															std::getline(s2,line);
															LRU[cache_ix].expire_date = line.substr(ptr);
															bzero(&time_val, tm_size);
															strptime(line.data(), "%a, %d %b %Y %H:%M:%S ", &time_val);
															LRU[cache_ix].exp_tsEpoch = mktime(&time_val);
															break;
														}
													}

													file2.close();
													
													
													if(containsExpHeader == false)
													{
														
														time(&extracted_time);
														utc_format = gmtime(&extracted_time);
														
														strftime(time_stamp, sizeof(time_stamp), "%a, %d %b %Y %H:%M:%S ",utc_format);	
														LRU[cache_ix].exp_tsEpoch = mktime(utc_format);
														LRU[cache_ix].expire_date = std::string(time_stamp)+ std::string("GMT");; 
													}
												}
												
												time(&extracted_time);
												utc_format = gmtime(&extracted_time);
												strftime(time_stamp, sizeof(time_stamp), "%a, %d %b %Y %H:%M:%S ",utc_format);
				
												printf("The request was performed at %s\n", time_stamp);
												printf("The expiry time is at %s\n", LRU[cache_ix].expire_date.data());
												if(notModified == true)
												{
													//Request served from the cache block
													printf("The request was served from the cache block\n");
													
													if(req_details[cid].isExpired)
													{
														remove(req_details[cid].filename);
														std::stringstream s4;
														s4 << cache_ix;
														strcpy(temp_ch1,s4.str().data());
														strcpy(req_details[cid].filename,temp_ch1);									
														req_details[cid].temp_flag = false;
													}
												}

												if(notModified == false)
												{
													
													if(LRU[cache_ix].val!=1)
													{
														if(LRU[cache_ix].val>1)
														{
															
															LRU[cache_ix].val--;
															int cb_new_idx;
															//getting the least recently used
															cb_new_idx = getLRU(LRU);
															
															if(cb_new_idx!=-1)
															{
																req_details[cid].cache_id = cb_new_idx;
																LRU[cb_new_idx].exp_tsEpoch = LRU[cache_ix].exp_tsEpoch;
																LRU[cb_new_idx].expire_date = LRU[cache_ix].expire_date;
																cache_ix = cb_new_idx;
															}
															else
															{
																req_details[cid].temp_flag = true;
																req_details[cid].cache_id = cb_new_idx;
																cache_ix = cb_new_idx;
															}
															
														}
													}
													else
													{
														LRU[cache_ix].val = CURR_NEG_VALUE;
													}
												}
												
			
												if(cache_ix != -1 && LRU[cache_ix].val == CURR_NEG_VALUE)
												{
													std::stringstream s3;
													s3 << cache_ix;
													strcpy(temp_ch1,s3.str().data());
													std::ofstream ostream;
													ostream.open(temp_ch1, file_mode2);
													ostream.close();
													remove(temp_ch1);
													rename(req_details[cid].filename,temp_ch1);
													strcpy(req_details[cid].filename,temp_ch1);
													LRU[cache_ix].url = std::string(new_specs[new_specs[i].client_request_id].URL);
													req_cache_id[LRU[cache_ix].url]=cache_ix;
													req_details[cid].temp_flag = false;
													LRU[cache_ix].val = 1;
												}
											}
										
									}
										
							}
							
							else if(new_specs[i].mode)
							{
								std::ofstream temp_ch1_file;
								temp_ch1_file.open(req_details[new_specs[i].client_request_id].filename,file_mode3);
								
								if(temp_ch1_file.is_open() == false)
								{ 
									printf("An error occured while opening the file\n");
								}
								else
								{
									temp_ch1_file.write(temp_char_buff,recv_data_count);
									temp_ch1_file.close();
								}
							}
							
							else
							{
								struct Http_request* http_req = retrieve_http_request(temp_char_buff);
								char* hname = http_req->hostname;
								char* fname = http_req->filename;
								new_specs[i].URL = std::string(hname)+std::string(fname);
								printf("Client made a request to %s\n", new_specs[i].URL.data() );
								
								int cache_index = ifCached(LRU, req_cache_id, new_specs[i].URL);
								bool isExpired = false;
								req_details[i].isExpired = false;
								
								if(cache_index != -1)
								{
									if(LRU[cache_index].val>=0)
									{
										time_t ext_time;
										time(&ext_time);
										utc_format = gmtime(&ext_time);
										ext_time = mktime(utc_format);
										if(LRU[cache_index].exp_tsEpoch-ext_time<=0)
											isExpired = true;
										
										if(isExpired)
										{
											req_details[i].isExpired = true;
										}
										else
										{
											FD_SET(i,&master_wrt_fds);
											req_details[i].cache_id = cache_index;		
											req_details[i].number = 0;
											req_details[i].temp_flag = false;
											LRU[cache_index].val++;
											std::stringstream sstream;
											sstream << cache_index;
											strcpy(req_details[i].filename,sstream.str().data());
											
											printf("Present in cache and cache block is hit at index %d \n",CACHE_CAPACITY - cache_index );
										}
										
									}
								}
								if(isExpired==true ||cache_index==-1 )
								{
									char* hname = http_req->hostname;
									char* fname = http_req->filename;
									std::string temp_ch1_str = "GET "+ std::string(fname) + " HTTP/1.0\r\nHost: "+std::string(hname)+"\r\n\r\n";
									recv_data_count = temp_ch1_str.length();
									strcpy(temp_char_buff,temp_ch1_str.data());
									
									if(!isExpired)
									{
										cache_index = getLRU(LRU);
										printf("The requested entry is not in cache and currently caching to block index %d\n", CACHE_CAPACITY - cache_index);
									}
									else
									{
										LRU[cache_index].val++;
										temp_char_buff[recv_data_count-2]='\0';
										std::string req = std::string(temp_char_buff);
										//Conditional GET request  
										// printf("hereeeeeeee %s\n",LRU[cache_index].expire_date.data());
										std::string new_req = req + "If-Modified-Since: "+ LRU[cache_index].expire_date + "\r\n\r\n\0";
										recv_data_count = new_req.length();
										strcpy(temp_char_buff,new_req.data());
										printf("Present in cache and cache block is hit at index %d \n",CACHE_CAPACITY - cache_index);
										printf("Checking If-Modified-Since: %s\n", LRU[cache_index].expire_date.data());
									}
									req_details[i].cache_id = cache_index;
									
									int sockfd_new;
									bzero(&addr,sizeof(addr));		
									addr.ai_family = AF_INET;
									addr.ai_socktype = SOCK_STREAM;
									
									int getaddr_status = getaddrinfo(http_req->hostname, "80", &addr, &addr_info);
									if( getaddr_status != 0)
									{	
										printf("Error in getting the address info\n");
										exit(1);
									}
									p = addr_info;
									while(p!=NULL)
									{
										sockfd_new = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
										if(sockfd_new == -1)
										{	
											printf("Proxy: Socket creation error\n");
											p=p->ai_next;
											continue;
										}
										break;
									}
									int s_connect_status = connect(sockfd_new,p->ai_addr,p->ai_addrlen);
									if(s_connect_status == -1)
									{
										printf("Error connecting to the server\n");
									}	
									freeaddrinfo(addr_info);
			
									new_specs[sockfd_new].mode=true;
									new_specs[sockfd_new].client_request_id = i;
									
									req_details[i].temp_flag = true;				
									req_details[i].number = 0;
									std::stringstream sstream;
									srand(time(NULL));
									int temp_ch1_filenum = rand();
									while(unique_numbers.count(temp_ch1_filenum)>0)
									temp_ch1_filenum = (temp_ch1_filenum+1)%1000;
									unique_numbers.insert(temp_ch1_filenum);
									sstream << "tempfile_"<<temp_ch1_filenum;
									strcpy(req_details[i].filename,sstream.str().data());
									std::ofstream op1;
									op1.open(req_details[i].filename, file_mode2);
									op1.close();
									FD_SET(sockfd_new,&master_fds);
									max_fd_count = std::max(sockfd_new, max_fd_count);
									printf("\n");
									int send_status = send(sockfd_new,temp_char_buff,recv_data_count,0);
									if( send_status == -1)
									printf("Proxy: Error sending to the server\n");
									
								}

							free(http_req);
							}
							
						}
						
						
						
					}
					else if(FD_ISSET(i,&write_fds))
					{			   
						if(FD_ISSET(i,&master_fds)==false)		       
						{
							FD_CLR(i,&master_fds);
							FD_CLR(i,&master_wrt_fds);
							//closing the connection
							close(i);
							if(req_details[i].cache_id !=-1)
							{
								updateLRUCache(LRU, req_details[i].cache_id);
								LRU[req_details[i].cache_id].val--;
								
							}	
							else
							{
								if(req_details[i].temp_flag==true)
									remove(req_details[i].filename);	
								
							}
							continue;
						}
						std::ifstream file1;
						file1.open(req_details[i].filename,file_mode1);
						
						if(file1.is_open())
						{
							file1.seekg((req_details[i].number)*BUFFER_SIZE, std::ios::beg);		
							req_details[i].number++;
							file1.read(temp_char_buff,BUFFER_SIZE);
				            recv_data_count = file1.gcount();
							if(recv_data_count!=0 && (error = send(i,temp_char_buff,recv_data_count,0)) == -1)
							printf("Error sending to the client\n");
							
							if(recv_data_count<BUFFER_SIZE)
							{
								//EOF reached as recv data size < buffer size						
								FD_CLR(i,&master_wrt_fds);
								FD_CLR(i,&master_fds);			
								close(i);
								if(req_details[i].cache_id!=-1)
								{
									updateLRUCache(LRU, req_details[i].cache_id);	
									LRU[req_details[i].cache_id].val--;	
										
								}
								else 
								{
									if(req_details[i].temp_flag==true)		
									remove(req_details[i].filename);
								}
								printf("============================================================= \n" );
							}	
							
						}
						else
						{
							printf("Unable to open the cached file %s\n", req_details[i].filename);
							FD_CLR(i,&master_wrt_fds);
						}
						
						file1.close();
					}
					//incrementing fd
					i++;
					
				}
				
				
			}	
			return 0;	
		}