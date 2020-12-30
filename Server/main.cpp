#include <utility>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <string>
#include <thread>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
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

void handle_client_request(int server_socket, int client_fd, sockaddr_in client_addr, char rec_buffer [] , int bufferSize);
long checkFileExistence(string fileName);
vector<string> readDataFromFile(string fileName);
void sendTheDataPackets (int client_fd, struct sockaddr_in client_addr , vector<string> data);
packet create_packet_data(string packetString, int seqNum);
bool send_packet(int client_fd, struct sockaddr_in client_addr , string temp_packet_string, int seqNum);

 enum state {slow_start, congestion_avoidance, fast_recovery};

int main()
{

    int server_socket, client_socket;
    int portNom = 8000;

    struct sockaddr_in server_address{};
    struct sockaddr_in client_address{};
    int server_addrlen = sizeof(server_address);


    if ((server_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        perror("cannot create socket");
        exit(1);
    }

    memset(&server_address, 0, sizeof(server_address));
    memset(&client_address, 0, sizeof(client_address));
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
        socklen_t client_addrlen = sizeof(struct sockaddr);
        cout << "Waiting For A New Connection ... " << endl << flush;
        char rec_buffer[MSS];
        ssize_t Received_bytes = recvfrom(server_socket, rec_buffer, MSS, 0, (struct sockaddr*)&client_address, &client_addrlen);
        if (Received_bytes < 0){
             perror("error in receiving bytes");
             exit(1);
        }
        if (Received_bytes == 0){
             perror("Client closed connection or error in receiving bytes");
             exit(1);
        }
         auto* data_packet = (struct packet*) rec_buffer;
        //string str = string(rec_buffer);
        //cout << str.size();

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
             handle_client_request(server_socket,client_socket, client_address, rec_buffer , MSS);
             exit(0);
         }



    }




    return 0;
}

void handle_client_request(int server_socket, int client_fd, struct sockaddr_in client_addr, char rec_buffer [] , int bufferSize) {

     auto* data_packet = (struct packet*) rec_buffer;
     string fileName = string(data_packet->data);
     cout << "requested file name from client  : " << fileName << " , Lenght : " << fileName.size() << endl << flush;
     int fileSize = checkFileExistence(fileName);
     if (fileSize == -1){
        return;
     }
     int numberOfPackets = ceil(fileSize * 1.0 / MSS);
     cout << "File Size : " << fileSize << " Bytes , Num. of chuncks : " << numberOfPackets << endl << flush;

     /** send ack to file name **/
     struct ack_packet ack;
     ack.cksum = 0;
     ack.len = numberOfPackets;
     ack.ackno = 0;
     char* buf = new char[MSS];
     memset(buf, 0, MSS);
     memcpy(buf, &ack, sizeof(ack));
     ssize_t bytesSent = sendto(client_fd, buf, MSS, 0, (struct sockaddr *)&client_addr, sizeof(struct sockaddr));
     if (bytesSent == -1) {
        perror("couldn't send the ack");
        exit(1);
     } else {
        cout << "Ack of file name is sent successfully" << endl << flush;
     }

     /** read data from file **/
     vector<string> DataPackets = readDataFromFile(fileName);
     if (DataPackets.size() == numberOfPackets){
        cout << "Data is read successfully " << endl << flush;
     }


     /** start sending data and handling congestion control using the SM **/
     sendTheDataPackets(client_fd, client_addr, DataPackets);


}





void sendTheDataPackets (int client_fd, struct sockaddr_in client_addr , vector<string> data){

    int cwnd_base = 0;
    int cwnd = 1;
    int base_packet_number = 0;
    long sentBytes = 0;
    int sst = 128;
    bool flag = true;
    int seqNum = 0;
    long sentPacketsNotAcked = 0;
    state st = slow_start;
    long numberOfDupAcks = 0;
    int lastAckedSeqNum = -1;
    bool stillExistAcks = true;
    char rec_buf[MSS];
    socklen_t client_addrlen = sizeof(struct sockaddr);
    while (flag){

        if (sentPacketsNotAcked > 0){

            while (stillExistAcks){

                ssize_t Received_bytes = recvfrom(client_fd, rec_buf, ACK_PACKET_SIZE, 0, (struct sockaddr*)&client_addr, &client_addrlen);
                if (Received_bytes < 0){
                     perror("error in receiving bytes");
                     exit(1);
                }
                else if (Received_bytes != ACK_PACKET_SIZE){
                     cout << "Expecting Ack Got Something Else" << endl << flush;
                     exit(1);
                }
                else {

                    auto ack = (ack_packet*) malloc(sizeof(ack_packet));
                    memcpy(ack, rec_buf, ACK_PACKET_SIZE);
                    int ack_seqno = ack->ackno;
                    if (lastAckedSeqNum == ack_seqno){

                        numberOfDupAcks++;
                        if (st == fast_recovery){
                            cwnd++;
                        } else if (numberOfDupAcks == 3){
                            sst = cwnd / 2;
                            cwnd = sst + 3;
                            st = fast_recovery;

                            /** rettransmit the lost packet **/

                        }


                    } else {

                        numberOfDupAcks = 0;
                        lastAckedSeqNum = ack_seqno;
                        int advance = lastAckedSeqNum - base_packet_number;
                        cwnd_base = cwnd_base - advance;
                        base_packet_number = lastAckedSeqNum;
                        if (st = slow_start){
                           cwnd++;
                           if (cwnd >= sst){
                                st = congestion_avoidance;
                           }
                        } else if (st == congestion_avoidance){
                            cwnd += (1 / floor(cwnd));
                        } else if (st == fast_recovery){
                            st = congestion_avoidance;
                            cwnd = sst;
                        }

                    }

                }

            }


        }



        /**
        this part will run first to send first datagram as stated in pdf.
        **/
        while(cwnd_base < cwnd){
            seqNum = base_packet_number + cwnd_base;
            string temp_packet_string = data[seqNum];
            /**
                in case error simulated won't send the packet so the seqnumber will not correct at the receiver so will send duplicate ack.
            **/
            bool isSent = send_packet(client_fd, client_addr, temp_packet_string,seqNum);
            if (isSent = false) {
                perror("couldn't send the ack");
                exit(1);
            } else {
                sentPacketsNotAcked++;
            }
            cwnd_base++;
        }


    }


}

bool send_packet(int client_fd, struct sockaddr_in client_addr , string temp_packet_string, int seqNum){
     char sendBuffer [MSS];
     struct packet data_packet = create_packet_data(temp_packet_string, seqNum);
     memset(sendBuffer, 0, MSS);
     memcpy(sendBuffer, &data_packet, sizeof(data_packet));
     ssize_t bytesSent = sendto(client_fd, sendBuffer, MSS, 0, (struct sockaddr *)&client_addr, sizeof(struct sockaddr));
     if (bytesSent == -1) {
            return false;
     } else {
            return true;
     }
}

packet create_packet_data(string packetString, int seqNum) {

    struct packet p{};
    strcpy(p.data, packetString.c_str());
    p.seqno = seqNum;
    p.cksum = 0;
    p.len = packetString.size();
    return p;

}





long checkFileExistence(string fileName){
     ifstream file(fileName.c_str(), ifstream::ate | ifstream::binary);
     if (!file.is_open()) {
        cout << "file doesn't exist or Can't open file\n";
        return -1;
     }
     cout << "File opened successfully\n";
     long len = file.tellg();
     file.close();
     return len;
}

vector<string> readDataFromFile(string fileName){

    vector<string> DataPackets;
    string temp = "";
    ifstream fin;
    fin.open(fileName);
    if (fin){
        char c;
        int char_counter = 0;
        while(fin.get(c)){
            if(char_counter < 500) {
                temp += c;
            }else{
                DataPackets.push_back(temp);
                temp.clear();
                temp += c;
                char_counter = 0;
                continue;
            }
            char_counter++;
        }
        if (char_counter > 0){
            DataPackets.push_back(temp);
        }
    }
    fin.close();
    return DataPackets;

}









