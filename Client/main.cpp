#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <string>
#include <thread>
#include <ctime>
#include <bits/stdc++.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>


using namespace std;

static const int MSS = 508;
/* Data-only packets */
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

packet create_packet_data(string file_name);

int main()
{
    struct sockaddr_in server_address;
    int client_socket;

    memset(&client_socket, '0', sizeof(client_socket));

    int portNum = 8000;

    if ((client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("failed to create the client socket");
        exit(1);
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(portNum);

    /** first send packet with the file name **/
    string fileName;
    cout << "Enter File Name : " << endl<<flush;
    cin >> fileName;
    cout << "File Name is : " << fileName << " The lenght of the Name : " << fileName.size() << endl << flush;
    struct packet fileName_packet = create_packet_data(fileName);
    char* buffer = new char[MSS];
    memset(buffer, 0, MSS);
    memcpy(buffer, &fileName_packet, sizeof(fileName_packet));
    ssize_t bytesSent = sendto(client_socket, buffer, MSS, 0, (struct sockaddr *)&server_address, sizeof(struct sockaddr));
    if (bytesSent == -1) {
        perror("couldn't send the packet");
    } else {
       cout << "Sent The file Name " << endl << flush;
    }


    char rec_buffer[MSS];
    socklen_t addrlen = sizeof(server_address);
    ssize_t Received_bytes = recvfrom(client_socket, rec_buffer, MSS, 0, (struct sockaddr*)&server_address, &addrlen);
    if (Received_bytes < 0){
        perror("error in receiving bytes");
        exit(1);
    }
    auto* ackPacket = (struct ack_packet*) rec_buffer;
    cout << "Number of packets " << ackPacket->len << endl;
    long numberOfPackets = ackPacket->len;


    int i = 1;
    while (true){
        memset(rec_buffer, 0, MSS);
        ssize_t bytesReceived = recvfrom(client_socket, rec_buffer, MSS, 0, (struct sockaddr*)&server_address, &addrlen);
        if (bytesReceived == -1){
            perror("Error in recv(). Quitting");
            break;
        }
        auto* data_packet = (struct packet*) rec_buffer;
        cout <<"packet "<<i<<" received" <<endl<<flush;
        cout << "Sequence Number : " << data_packet->seqno << endl<<flush;
        //out << data_packet->data[0]<<data_packet->data[499] << endl;
        /** send ack **/
        i++;
    }



    return 0;
}


packet create_packet_data(string file_name) {

    struct packet p{};
    strcpy(p.data, file_name.c_str());
    p.seqno = 0;
    p.cksum = 0;
    p.len = file_name.length() + sizeof(p.cksum) + sizeof(p.len) + sizeof(p.seqno);
    return p;
}
