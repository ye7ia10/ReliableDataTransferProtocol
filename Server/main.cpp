#include <utility>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>
#include <string>
#include <thread>
#include <chrono>
#include <bits/stdc++.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>

using namespace std;


static const int MSS = 508;
static const int ACK_PACKET_SIZE = 8;

struct packet {
    /* Header */
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t seqno;
    /* Data */
    char data [500]; /* Not always 500 bytes, can be less */
};

/* Ack-only packets are only 8 bytes */
struct ack_packet {
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t ackno;
};
void handle_client_request(int client_fd, sockaddr_in client_addr, char rec_buffer [] , int bufferSize);
int main()
{

    int server_socket, client_socket;
    int portNom = 8000;

    struct sockaddr_in server_address, client_address;
    int server_addrlen = sizeof(server_address);


    if ((server_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        perror("cannot create socket");
        exit(1);
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET; // host byte order
    server_address.sin_port = htons(portNom); // short, network byte order
    server_address.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
    memset(&(server_address.sin_zero), '\0', ACK_PACKET_SIZE); // zero the rest of the struct

    if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
    {
        perror("error in binding server");
        exit(1);
    }

    while (1){
        socklen_t client_addrlen = sizeof(client_address);
        cout << "Waiting For A New Connection ... " << endl << flush;
        char rec_buffer[MSS];
        ssize_t Received_bytes = recvfrom(server_socket, rec_buffer, MSS, 0, (struct sockaddr*)&client_socket, &client_addrlen);
        if (Received_bytes < 0){
             perror("error in receiving bytes");
             exit(1);
        }
        if (Received_bytes == 0){
             perror("Client closed connection or error in receiving bytes");
             exit(1);
        }
        cout << "requested from client  : " << rec_buffer << endl << flush;
        /** forking to handle request **/
         pid_t pid = fork();
         if (pid == -1){
             perror("error in forking a child process");
         } else if (pid == 0){

            /** child process for client **/
            /** create client socket **/
            if ((client_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
            {
                perror("cannot create client socket");
                exit(1);
            }
             memset(&client_address, 0, sizeof(client_address));
             handle_client_request(client_socket, client_address, rec_buffer , MSS);
             exit(0);
         }



    }




    return 0;
}

void handle_client_request(int client_fd, sockaddr_in client_addr, char rec_buffer [] , int bufferSize) {

     auto* data_packet = (struct packet*) rec_buffer;
     cout << "I am handling bruh" << endl << flush;


}
























