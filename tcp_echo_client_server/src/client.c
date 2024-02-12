#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define MAX_SIZE 10

// Function declarations
int writen(int clientSocket, char *data, int dataSize);
int readChar(int clientSocket, char *charBuffer);
int readLine(int clientSocket, char *lineBuffer, int maxLineSize);

int main(int argc, char *argv[]) {
    // Checking for the correct number of command-line arguments
    if (argc != 3) {
        printf("Usage: %s <Server IP Address> <Server Port Number>\n", argv[0]);
        return 1;
    }

    char *serverIP = argv[1];
    int serverPort = atoi(argv[2]);

    // Creating a socket for the client
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        perror("Client socket creation error");
        return 1;
    }

    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr(serverIP);
    serverAddress.sin_port = htons(serverPort);

    // Connect to the server
    if (connect(clientSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
        perror("Connection not established");
        return 1;
    }

    printf("Connected to the server.\n");
    printf("Please input your messages to send:\n");

    char message[MAX_SIZE];
    char receivedMessage[MAX_SIZE];

    while (1) {
    	//Clearing the buffer
        bzero(message, MAX_SIZE);

        // Reading user input
        if (fgets(message, MAX_SIZE, stdin) == NULL || feof(stdin)) {
            printf("Detected EOF (Ctrl+D).\n");
            printf("Disconnecting from the server.\n");
            close(clientSocket);
            return 1;
        }

        // Sending the user's message to the server
        if (writen(clientSocket, message, strlen(message)) == -1) {
            perror("Write error");
            return 1;
        }
	printf("Client sent = %s\n",message);
        // Receive and display the echoed message from the server
        if (readLine(clientSocket, receivedMessage, MAX_SIZE) == -1) {
            perror("Error reading echoed message");
            return 1;
        }

        printf("Client received = %s\n", receivedMessage);
    }

    // Close the socket and exit
    close(clientSocket);
    return 0;
}

// Writes all data over the socket
int writen(int clientSocket, char *data, int dataSize) {
    int sentBytes, remainingBytes;
    remainingBytes = dataSize;

    while (remainingBytes > 0) {
        sentBytes = write(clientSocket, data, remainingBytes);
        if(sentBytes <=0){
		if (sentBytes < 0 && errno == EINTR) 
		    sentBytes = 0;
		 else
		    return -1;
        }

        data += sentBytes;
        remainingBytes -= sentBytes;
    }

   return dataSize;
}

// Reads a character from the socket
int readChar(int clientSocket, char *charBuffer) {
    static int bufferCount;
    static char *bufferPtr;
    static char dataBuffer[MAX_SIZE];

    if (bufferCount <= 0) {
        int bytesRead;
        do {
            bytesRead = read(clientSocket, dataBuffer, sizeof(dataBuffer));
        } while (bytesRead < 0 && errno == EINTR);

        if (bytesRead < 0) {
            return -1;
        } else if (bytesRead == 0) {
            return 0;
        }

        bufferPtr = dataBuffer;
        bufferCount = bytesRead;
    }

    bufferCount--;
    *charBuffer = *bufferPtr;
    bufferPtr++;

    return 1;
}

// Reads a line of text from the socket
int readLine(int clientSocket, char *lineBuffer, int maxLineSize) {
    int bytesRead, i;
    char character, *ptr;

    ptr = lineBuffer;

    for (i = 1; i < maxLineSize; i++) {
        bytesRead = readChar(clientSocket, &character);

        if (bytesRead == 1) {
            *ptr++ = character;
            if (character == '\n'){
                break;
            }
        } 
        else if (bytesRead == 0) {
            *ptr = 0;
            return (i - 1);
        } 
        else {
            return -1;
        }
    }

    *ptr = 0;
    return i;
}

