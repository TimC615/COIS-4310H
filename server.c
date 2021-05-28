//Assignment 4
//Talks to 2 instances of 'client.c'
//
//Name: client.c
//
//Written by: Timothy Chaundy - April 2021
//Base code borrowed from Beej's website
//
//Purpose: Used to be a central controller for multiple instances of 'client.c' and perform the role of routing messages
//to the correct destination.
//
//Usage: ./server
//
//Example: ./server

//libraries to be used within the below code
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#define MYPORT 8080    /* the port users will be connecting to */
#define BACKLOG 2      /* how many pending connections queue will hold */
#define MAX_DATA 256   /* defines how long the message string is allowed to be */
#define CODE_VERSION 4 /* contains the current code version number ((1 for assignment 1, 2 for #2...) */
#define ROT_VALUE 13   /* defines the amount of rotation in the message encryption */

//structure that contains all of the packet information that will be sent through the socket
struct FULL_PACKET{
    int ack_status;
    int packet_num;
    int code_version;
    char source_name[10];
    char dest_name[10];
    char verb[5];
    int client_num;
    int checksum;
    char message[MAX_DATA];
};

//function to send message to desired client through provided socket ID
void sendMsg(int sockfd, struct FULL_PACKET packet){
    int numbytes;

    printf("Sending %s to sockfd %d\n", packet.message, sockfd);

    //will infinitely loop until an acknowledgement message is retreive from client
    while(1){
        //sends desired message to specified client
        send(sockfd, &packet, sizeof(packet), 0);

        //checks for error when retreiving message from desired socket
        if ((numbytes=recv(sockfd, &packet, sizeof(packet), 0)) == -1) {
            perror("recv");
            continue;
        }

        //checks if acknowledgement message has been sent by client
        //if true, terminate function
        if(packet.ack_status == 1){
            printf("ACK received\n");
            packet.ack_status = 0;
            return;
        }
        sleep(1);
    }
}

//function to retreive message to desired client through provided socket ID
struct FULL_PACKET receiveMsg(int sockfd, struct FULL_PACKET packet){
    int numbytes;
    int checksum_confirm = 0;
    int x;

    //empties the message field of packet
    memset(packet.message,0,sizeof(packet.message));

    //checks for error when retreiving message from desired socket
    if ((numbytes=recv(sockfd, &packet, sizeof(packet), 0)) == -1) {
        perror("recv");
        exit(1);
    }

    //adds the ASCII values of all characters currently in 'packet.message'
    for(x = 0; x < MAX_DATA; x++){
        checksum_confirm = checksum_confirm + packet.message[x];
    }

    //checks if there was an error with the checksum received vs the checksum locally calculated
    if(checksum_confirm != packet.checksum){

        printf("Error when receiving message from sockfd %d\n", sockfd);

        //returns a "NAK" message to client (signifies error found in checksum)
        packet.ack_status = 2;
        strncpy(packet.source_name, "server", 10);
        strncpy(packet.dest_name, "client", 10);

        printf("Requesting sockfd %d to send corrected message\n", sockfd);

        send(sockfd, &packet, sizeof(packet), 0);

        //checks for error when retreiving message from desired socket
        if ((numbytes=recv(sockfd, &packet, sizeof(packet), 0)) == -1) {
            perror("recv");
            exit(1);
        }
    }
    
    printf("Received %s from sockfd %d\n", packet.message, sockfd);

    //sends acknowledgement back to client, signifying that the message has been successfully received
    packet.ack_status = 1;
    strncpy(packet.source_name, "server", 10);
    strncpy(packet.dest_name, "client", 10);

    send(sockfd, &packet, sizeof(packet), 0);

    return packet;
}

main()
{
    //declaration of client struct to be used to store various information about each client instance
    struct client{
        int sockfd;
        struct sockaddr_in their_addr;
        int sin_size;
        int new_fd;
        int client_index;
    };

    struct FULL_PACKET packet;

    //used to store the new_fd
    struct client client_list[2];

    packet.code_version = CODE_VERSION;

    //variable initialization
    int sockfd, new_fd;  /* listen on sock_fd, new connection on new_fd */
    struct sockaddr_in my_addr;    /* my address information */
    struct sockaddr_in their_addr; /* connector's address information */
    int sin_size;
    int client_count = 0;

    //creation of new socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    //socket address initialization
    my_addr.sin_family = AF_INET;         /* host byte order */
    my_addr.sin_port = htons(MYPORT);     /* short, network byte order */
    my_addr.sin_addr.s_addr = INADDR_ANY; /* automatically fill with my IP */
    bzero(&(my_addr.sin_zero), 8);        /* zero the rest of the struct */

    //binds above socket information to newly created socket
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        exit(1);
    }

    //checks for error when listening for client to connect to socket
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sin_size = sizeof(struct sockaddr_in);

    //checks for error when accepting socket connection from client
    if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
        perror("accept");
        //continue;
        exit(1);
    }
    printf("server: got connection from %s\n",inet_ntoa(their_addr.sin_addr));

    //stores socket information for 1st client instance
    client_list[client_count].new_fd = new_fd;
    client_list[client_count].sin_size = sin_size;
    client_list[client_count].sockfd = sockfd;
    client_list[client_count].client_index = client_count;

    //sets up a welcome packet to be sent to client
    strcpy(packet.message, "Welcome to the chatroom! You are the 1st user here so you can send a message right now.");
    packet.ack_status = 0;
    packet.client_num = 1;
    strncpy(packet.verb, "send", 5);
    strncpy(packet.source_name, "server", 10);
    strncpy(packet.dest_name, "client", 10);

    char temp_encrypt[MAX_DATA] = {'\0'};
    //zeros out the string to hold the encrypted message
    bzero(temp_encrypt, MAX_DATA);

    //iterates through the original message
    int x;
    //iterates through the original message
    for(x = 0; x < strlen(packet.message); x++){
        //retreives the ascii value of the current character, adds ROT_VALUE (set to 13) to "encrypt" the character and
        //applies a modulus of 94 to ensure the encrypted character had a value stays in the range of 0 - 94
        int key = (((packet.message[x] - ' ') + ROT_VALUE) % 94);

        //converts the ascii value into an actual character to later be added to the output string
        char encrypted_char = ' ' + key;

        //appends the current character to the end of the encrypted message
        temp_encrypt[x] = encrypted_char;
    }

    //sets 'packet.message' to the encrypted message, making it ready to be sent to client
    strncpy(packet.message, temp_encrypt, MAX_DATA);

    //sends welcome message to 1st client instance
    sendMsg(client_list[client_count].new_fd, packet);

    strncpy(packet.message, "", sizeof(packet.message));

    client_count++;

    sin_size = sizeof(struct sockaddr_in);

    //checks for error when accepting socket connection from client
    if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
        perror("accept");
        //continue;
        exit(1);
    }
    printf("server: got connection from %s\n",inet_ntoa(their_addr.sin_addr));

    //stores socket information for 2nd client instance
    client_list[client_count].new_fd = new_fd;
    client_list[client_count].sin_size = sin_size;
    client_list[client_count].sockfd = sockfd;
    client_list[client_count].client_index = client_count;

    //sets up a welcome packet to be sent to client
    strcpy(packet.message, "Welcome to the chatroom! You are the 2nd user here so you must first wait for the other client to send you a message.");
    packet.ack_status = 0;
    packet.client_num = 2;
    strncpy(packet.verb, "send", 5);
    strncpy(packet.source_name, "server", 10);
    strncpy(packet.dest_name, "client", 10);

    //zeros out the string to hold the encrypted message
    bzero(temp_encrypt, MAX_DATA);

    //iterates through the original message
    for(x = 0; x < strlen(packet.message); x++){
        //retreives the ascii value of the current character, adds ROT_VALUE (set to 13) to "encrypt" the character and
        //applies a modulus of 94 to ensure the encrypted character had a value stays in the range of 0 - 94
        int key = (((packet.message[x] - ' ') + ROT_VALUE) % 94);

        //converts the ascii value into an actual character to later be added to the output string
        char encrypted_char = ' ' + key;

        //appends the current character to the end of the encrypted message
        temp_encrypt[x] = encrypted_char;
    }

    //sets 'packet.message' to the encrypted message, making it ready to be sent to client
    strncpy(packet.message, temp_encrypt, MAX_DATA);

    //sends welcome message to 2nd client instance
    sendMsg(client_list[client_count].new_fd, packet);

    strncpy(packet.message, "", sizeof(packet.message));

    //infinitely loops until 1 or both clients disconnect
    while(1) {
        //retreive message from 1st client and send to 2nd client
        packet = receiveMsg(client_list[0].new_fd, packet);

        //alteration of packet information in preparation of sending to other client
        packet.ack_status = 0;
        strncpy(packet.source_name, "server", 10);
        strncpy(packet.dest_name, "client", 10);

        //sends message to other client
        sendMsg(client_list[1].new_fd, packet);

        strncpy(packet.message, "", sizeof(packet.message));

        //retreive message from 2nd client and send to 1st client
        packet = receiveMsg(client_list[1].new_fd, packet);

        //alteration of packet information in preparation of sending to other client
        packet.ack_status = 0;
        strncpy(packet.source_name, "server", 10);
        strncpy(packet.dest_name, "client", 10);

        //sends message to other client
        sendMsg(client_list[0].new_fd, packet);

        strncpy(packet.message, "", sizeof(packet.message));
    }
}