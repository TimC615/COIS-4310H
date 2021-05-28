//Assignment 4
//Two instances of 'client.c' are used to talk to 'server.c'
//
//Name: client.c
//
//Written by: Timothy Chaundy - April 2021
//
//Purpose: Used to act as a client interface to be able to send messages back and forth between two instances
//of the client program
//
//Usage: ./client [IP address]
//
//Example: ./client 127.0.0.1

//libraries to be used within the code below
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8080    /* the port client will be connecting to */
#define MAX_DATA 256 /* defines how long the message string is allowed to be */
#define ROT_VALUE 13 /* defines the amount of rotation in the message encryption */

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

//function to send a given message through the desired socket ID
void sendMsg(int sockfd, struct FULL_PACKET packet){
    int numbytes;
    int x;
    time_t t;   //variable used for random number creation
    double random; //determines of the checksum will contain an error or not (>0.900 = ERROR)

    //makes it so that random numbers can be created later in the function
    srand((unsigned) time(&t));

    //resets the checksum value of the message that will be sent
    packet.checksum = 0;

    //calculates the checksum of the packet by adding the ASCII values of all characters in the message together
    for(x = 0; x < MAX_DATA; x++){
        packet.checksum = packet.checksum + packet.message[x];
    }

    //creates a random number between 0.0 and 1.0
    random = rand() / ((double) RAND_MAX);

    //checks if error will be created or not
    if(random > 0.900){
        packet.checksum = packet.checksum + 5;
    }

    /**********************************************/
    /*                                            */
    /*         Start of Assignment 4 Work         */
    /*                                            */
    //*********************************************/

    //creates a string to hold the encrypted message before being sent to the packet
    char temp_encrypt[MAX_DATA] = {'\0'};
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

    /**********************************************/
    /*                                            */
    /*          End of Assignment 4 Work          */
    /*                                            */
    //*********************************************/

    //infinite loop to operate until client receives an "ACK" message signifying that the server has 
    //received the message successfully
    while(1){

        //sends message to server
        send(sockfd, &packet, sizeof(packet), 0);

        //error checking for the receive function
        if ((numbytes=recv(sockfd, &packet, sizeof(packet), 0)) == -1) {
            perror("recv");
            continue;
        }

        //checks for "ACK" message and terminates while loop if found
        if(packet.ack_status == 1){
            packet.ack_status = 0;
            return;
        }

        //checks for "NAK" message and re-sends the message to server
        if(packet.ack_status == 2){
            //prepares various packet header variables for the message to be sent
            packet.ack_status == 0;
            strncpy(packet.source_name, "client", 10);
            strncpy(packet.dest_name, "server", 10);

            //resets the checksum value of the message that will be sent
            packet.checksum = 0;

            //calculates the checksum of the packet by adding the ASCII values of all characters in the message together
            for(x = 0; x <= sizeof(packet.message); x++){
                packet.checksum = packet.checksum + packet.message[x];
            }

            //sends message to server
            send(sockfd, &packet, sizeof(packet), 0);

            //checks for an error when receiving the packet from server
            if ((numbytes=recv(sockfd, &packet, sizeof(packet), 0)) == -1) {
                perror("recv");
                continue;
            }

            //checks for an "ACK" response from server
            if(packet.ack_status == 1){
                packet.ack_status = 0;
                return;
                //return packet;
            }
        }

        sleep(1);
    }
}

//function to retreive message from server through desired socket ID
struct FULL_PACKET receiveMsg(int sockfd, struct FULL_PACKET packet){
    int numbytes;

    //empties the message field of packet
    memset(packet.message,0,sizeof(packet.message));

    //error checking for the receive function
    if ((numbytes=recv(sockfd, &packet, sizeof(packet), 0)) == -1) {
        perror("recv");
        exit(1);
    }

    //determines of the current verb is 'bye' or not, signifying that the other client has disconnected
    if(strcmp(packet.verb, "bye") == 0){
        printf("The other user has disconnected from the chatroom.\n");
    }
    else{
        if(strcmp(packet.verb, "new") == 0){
            printf("%s\n", packet.message);
        }
        else{
            /**********************************************/
            /*                                            */
            /*         Start of Assignment 4 Work         */
            /*                                            */
            //*********************************************/

            int x;
            //iterates through the encrypted message
            for(x = 0; x < strlen(packet.message); x++){
                //retreives the ascii value of the current character and removes the ROT_VALUE from it
                int key = (packet.message[x] - ' ') - ROT_VALUE;

                //ensures the ascii value falls within the range of 0 - 94
                if(key < 0){
                    key = key + 94;
                } 

                //reverts the ascii value into an actual character
                char encrypted_char = ' ' + key;

                //prints the current character to the user's terminal
                printf("%c", encrypted_char);
            }
            printf("\n");

            /**********************************************/
            /*                                            */
            /*          End of Assignment 4 Work          */
            /*                                            */
            //*********************************************/
        }
    }

    //sends acknowledgement back to server, signifying that the message has been successfully received
    strncpy(packet.source_name, "client", 10);
    strncpy(packet.dest_name, "server", 10);
    packet.ack_status = 1;

    send(sockfd, &packet, sizeof(packet), 0);

    return packet;
}

int main(int argc, char *argv[])
{
    //variables used for connecting to and interacting with socket
    int sockfd, numbytes;  
    struct hostent *he;
    struct sockaddr_in their_addr; /* connector's address information */

    struct FULL_PACKET packet;

    //checks if user has inputted an IP address or not when initializing the program
    if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    //checks for error with the IP address given
    if ((he=gethostbyname(argv[1])) == NULL) {  /* get the host info */
        perror("gethostbyname");
        exit(1);
    }

    //checks for error when creating the socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    //socket address initialization
    their_addr.sin_family = AF_INET;         /* host byte order */
    their_addr.sin_port = htons(PORT);     /* short, network byte order */
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    bzero(&(their_addr.sin_zero), 8);        /* zero the rest of the struct */

    //checks for error when connecting to the newly created socket
    if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1) {
        perror("connect");
        exit(1);
    }

    //retreives the initial welcome message from 'server.c'
    packet = receiveMsg(sockfd, packet);

    //checks which welcome message was retreived from 'server.c'
    if(packet.client_num == 1){

        //infinite while loop for 1st instance of client
        while(1){
            printf("Message to send: ");
            scanf(" %[^\n]s", packet.message);
            
            //prepares the packet with default information prior to sending
            strncpy(packet.source_name, "client", 10);
            strncpy(packet.dest_name, "server", 10);
            packet.ack_status = 0;
                
            //determines if the user wants to disconnect
            //if true, carry out disconnect procedure
            if(strcmp(packet.message, "bye") == 0){
                strncpy(packet.verb, "bye", 5);
                sendMsg(sockfd, packet);

                printf("Hope you enjoyed your time in the chatroom!\n");

                //closes client-server socket and terminates the program
                close(sockfd);
                return 0;
            }
            else{
                //default message sending state
                strncpy(packet.verb, "send", 5);
                sendMsg(sockfd, packet);
            }

            //receive message from server
            packet = receiveMsg(sockfd, packet);
            strncpy(packet.message, "", sizeof(packet.message));
        }
    }
    else{
        //infinite while loop for 2nd instance of client
        if(packet.client_num == 2){
            while(1){
                //receive message from server
                packet = receiveMsg(sockfd, packet);

                strncpy(packet.message, "", sizeof(packet.message));

                printf("Message to send: ");
                scanf(" %[^\n]s",packet.message);

                //prepares the packet with default information prior to sending
                strncpy(packet.source_name, "client", 10);
                strncpy(packet.dest_name, "server", 10);
                packet.ack_status = 0;

                //determines if the user wants to disconnect
                //if true, carry out disconnect procedure
                if(strcmp(packet.message, "bye") == 0){
                    strncpy(packet.verb, "bye", 5);
                    sendMsg(sockfd, packet);

                    printf("Hope you enjoyed your time in the chatroom!\n");

                    //closes client-server socket and terminates the program
                    close(sockfd);
                    return 0;
                }
                else{
                    //default message sending state
                    strncpy(packet.verb, "send", 5);
                    sendMsg(sockfd, packet);
                }
            }
        }
        else{
            //something went wrong when checking which client the current instance is
            printf("Unexpected error encountered when determining client number\n");
        }
    }
    //closes client-server socket and terminates the program
    close(sockfd);
    return 0;
}