#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace std;

int main(int a1, char *a2[])
{

	if (a1 != 4)
	{
		printf("Client: Incorrect format. ./server <IP> <Port no> <URL>\n"); // checking if argument is in proper format or not
		return 1;
	}
	struct addrinfo server1;
	struct addrinfo *serveraddr1, *tmp;

	memset(&server1, 0, sizeof(server1));
	server1.ai_family = AF_INET;	   // setting the family
	server1.ai_socktype = SOCK_STREAM; // setting TCP

	int temp1, socket1, recv1;

	temp1 = getaddrinfo(a2[1], a2[2], &server1, &serveraddr1); // resolving address and port from command line and stroing

	if (temp1 != 0)
	{
		printf("Client: Error at getaddrinfo\n"); // failure in above step
		exit(1);
	}
	// client socket creation
	for (tmp = serveraddr1; tmp != NULL; tmp = tmp->ai_next)
	{
		socket1 = socket(tmp->ai_family, tmp->ai_socktype, tmp->ai_protocol); // creating socket for client
		if (socket1 == -1)
		{
			printf("Client: Socket creation failed\n");
			continue;
		}
		break;
	}
	// client socket failure detection
	if (tmp == NULL)
	{
		printf("Client: Connection attempt failure\n");
		return 2; // exited if connection attempt fails
	}

	// variable declaration
	char *recv_text = (char *)malloc((2049) * sizeof(char));
	char *header1 = (char *)" HTTP/1.0\r\n";
	char *host1 = (char *)"Host: ";
	char *endofheader1 = (char *)"\r\n";
	char *filename = (char *)malloc(512 * sizeof(char));
	char *default_fname = (char *)"default.html\0";
	char *url = a2[3];
	unsigned short int tmp_len1;
	int header_cnt1 = 0, header_cnt2 = 4, name_ptr1 = 0, looper = 0;

	bool flag1 = true, flag2 = true;
	stringstream tmp_fname;
	ofstream out_file;
	// connection to server socket
	if ((temp1 = connect(socket1, tmp->ai_addr, tmp->ai_addrlen)) == -1)
		printf("Client: Client connection attempt failure\n"); // return if failure in connection to server

	freeaddrinfo(serveraddr1);

	int header_len1 = strlen(url);
	char *req_data = (char *)malloc((header_len1 + 50) * sizeof(char));

	strcpy(req_data, "GET ");
	// initaite the http request string with GET statement
	if (strlen(url) >= 6)
	{
		if (strncmp(url, "http://", 7) == 0) // identify the hostname position
			header_cnt1 = 7;
	}

	int temp = header_cnt1;

	while (url[temp] != '/' && header_len1 > header_cnt1)
		temp++; // identify the first / in the request

	strcat(req_data, "/");
	header_cnt2++;
	temp++;

	while (header_len1 > temp)
		req_data[header_cnt2++] = url[temp++];

	strcpy(req_data + header_cnt2, header1); // adding hostname to get request
	header_cnt2 = header_cnt2 + strlen(header1);
	strcpy(req_data + header_cnt2, host1); // adding the hostname in new line
	header_cnt2 = header_cnt2 + strlen(host1);
	while (url[header_cnt1] != '/' && header_len1 > header_cnt1)
		req_data[header_cnt2++] = url[header_cnt1++];

	strcpy(req_data + header_cnt2, endofheader1);
	header_cnt2 = header_cnt2 + strlen(endofheader1);
	// formulating the GET request
	tmp_len1 = strlen(req_data);
	cout << req_data << endl;
	printf("Client: Requested URL -  %s\n", a2[3]);
	// send the get request through the server socket to the proxy server
	temp1 = send(socket1, req_data, tmp_len1, 0);
	if (temp1 == -1)
		printf("Client: Send to server failed\n"); // returns if fails to send get request to server

	for (looper = header_len1 - 1; looper > -1; looper--)
	{
		if (url[looper] == '/') // to find the  last / value
		{
			name_ptr1 = looper;
			break;
		}
	}

	if (name_ptr1 == header_len1 - 1)
		filename = (char *)"default.html\0"; // if there is no filename mentioned after the last / then use this name
	else if (name_ptr1 != 0)
	{
		strcpy(filename, &url[name_ptr1 + 1]);
		filename[header_len1 - name_ptr1] = '\0'; // if filename is mentioned by user in the URL then extract that
	}

	tmp_fname << filename;

	out_file.open(tmp_fname.str().c_str(), ios::binary | ios::out); // open a new file using the filename calculated from above

	if (out_file.is_open() == false)
	{
		printf("Client: Cant open the file\n"); // failure in opening a new file with given name
		exit(0);
	}

	for (;;)
	{
		recv1 = recv(socket1, recv_text, 2048, 0); // receiving the data from server through the socket and writing to recv_text variable
		if (recv1 == -1)
			printf("Client: Receive failure");
		if (flag2)
		{
			stringstream tmp_str1;
			tmp_str1 << recv_text; // storing the received trxt from socket to the string stream
			string recv_tmp_str2;
			getline(tmp_str1, recv_tmp_str2);
			if (recv_tmp_str2.find("404") != string::npos)
				printf("Client: Error 404! Page not found.\n"); // checking if page is not present
			flag2 = false;
		}
		looper = 0;
		if (flag1)
		{
			if (recv1 >= 4) // since last sequence is of length 4 checking the size
			{
				looper = 3;
				while (recv1 > looper)
				{ // looping through to identify the end of sequence
					if (recv_text[looper] == '\n' && recv_text[looper - 1] == '\r' && recv_text[looper - 2] == '\n' && recv_text[looper - 3] == '\r')
					{
						flag1 = false;
						looper++; // looping until we find the end of the sequence mentioned in above condition
						break;
					}
					looper++;
				}
			}
		}

		if (flag1 == false)
			out_file.write(recv_text + looper, recv1 - looper); // writing to the file all the received data between the start of the new block and the end of file sequence

		if (recv1 == 0)
		{
			close(socket1);
			printf("Client: File name - %s\n", filename);
			out_file.flush(); // flushing the file
			out_file.close(); // closing the file as it is temporary
			break;
		}
		out_file.flush();
	}
	out_file.close(); // closing temp file and cleaning up
	return 0;
}
