#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string>

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

struct packet {
    uint16_t cksum;
    uint16_t len;
    uint32_t seqno;
    char data [500];
};


struct ack_packet {
    uint16_t cksum;
    uint16_t len;
    uint32_t ackno;
};

packet create_packet_data(string file_name);
void saveFile (string fileName, string content);
void send_acknowledgement_packet(int client_socket, struct sockaddr_in server_address ,  int seqNum);
vector<string> readArgsFile();
uint16_t get_data_checksum (string content, uint16_t len , uint32_t seqno);
uint16_t get_ack_checksum (uint16_t len , uint32_t ackno);

int main()
{

    /** read args file info content **/
    vector<string> the_args = readArgsFile();
    string IP_Address = the_args[0];
    string theFile = the_args[2];
    int port = stoi(the_args[1]);

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
        perror("Error sending the file name ! ");
        exit(1);
    } else {
       cout << "Client Sent The file Name ." << endl << flush;
    }



    char rec_buffer[MSS];
    socklen_t addrlen = sizeof(server_address);
    ssize_t Received_bytes = recvfrom(client_socket, rec_buffer, MSS, 0, (struct sockaddr*)&server_address, &addrlen);
    if (Received_bytes < 0){
        perror("error in receiving file name ack .");
        exit(1);
    }
    auto* ackPacket = (struct ack_packet*) rec_buffer;
    cout << "Number of packets " << ackPacket->len << endl;
    long numberOfPackets = ackPacket->len;
    string fileContents [numberOfPackets];
    bool recieved[numberOfPackets] = {false};

    int i = 1;
    int expectedSeqNum = 0;
    while (true && i <= numberOfPackets){
        memset(rec_buffer, 0, MSS);
        ssize_t bytesReceived = recvfrom(client_socket, rec_buffer, MSS, 0, (struct sockaddr*)&server_address, &addrlen);
        if (bytesReceived == -1){
            perror("Error receiving data packet.");
            break;
        }
        auto* data_packet = (struct packet*) rec_buffer;
        cout <<"packet "<<i<<" received" <<endl<<flush;
        cout << "Sequence Number : " << data_packet->seqno << endl<<flush;
        int len = data_packet->len;
        for (int j = 0 ; j < len ; j++){
            fileContents[data_packet->seqno] += data_packet->data[j];
        }
        if (get_data_checksum(fileContents[data_packet->seqno], data_packet->len, data_packet->seqno) != data_packet->cksum){
            cout << "corrupted data packet !" << endl << flush;
        }

        //fileContents[data_packet->seqno] = string(data_packet->data);
        //fileContents[data_packet->seqno] = string(&data_packet->data[0], &data_packet->data[499]);
        //cout << data_packet->data[0]<<data_packet->data[499] << endl;



        /*
        if (recieved[data_packet->seqno] == false){
            i++;
        }
        recieved[data_packet->seqno] = true;
        if (expectedSeqNum == data_packet->seqno){
            // send ack
            send_acknowledgement_packet(client_socket, server_address , data_packet->seqno);
            for (int z = expectedSeqNum ; z < len ; z++){
                if (recieved[z] == true){
                    expectedSeqNum++;
                } else {
                    break;
                }
            }
        } else {
           send_acknowledgement_packet(client_socket, server_address , expectedSeqNum);
        }*/

        send_acknowledgement_packet(client_socket, server_address , data_packet->seqno);
        i++;

    }

    /** reordering the packets and write them into file **/
    string content = "";
    for (int i = 0; i < numberOfPackets ; i++){
        content += fileContents[i];
    }
    saveFile(fileName, content);
    cout << "File is saved successfully . " << endl << flush;



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

void send_acknowledgement_packet(int client_socket, struct sockaddr_in server_address ,  int seqNum){

    /** create ack packet **/
    struct ack_packet ack;
    ack.ackno = seqNum;
    ack.len = sizeof(ack);
    ack.cksum = get_ack_checksum(ack.len, ack.ackno);
    //ack.cksum = 0;

    /** convert packet to buffer **/
    char* ack_buf = new char[MSS];
    memset(ack_buf, 0, MSS);
    memcpy(ack_buf, &ack, sizeof(ack));

    /** send ack to server **/
    ssize_t bytesSent = sendto(client_socket, ack_buf, MSS, 0, (struct sockaddr *)&server_address, sizeof(struct sockaddr));
    if (bytesSent == -1) {
        perror("error sending the ack ! ");
        exit(1);
    } else {
        cout << "Ack for packet seq. Num " << seqNum << " is sent." << endl << flush;
    }

}



void saveFile (string fileName, string content){
    ofstream f_stream(fileName.c_str());
    f_stream.write(content.c_str(), content.length());
}



vector<string> readArgsFile(){
    string fileName = "info.txt";
    vector<string> commands;
    string line;
    string content = "";
    ifstream myfile;
    myfile.open(fileName);
    while(getline(myfile, line))
    {
        commands.push_back(line);
    }
    return commands;
}



uint16_t get_data_checksum (string content, uint16_t len , uint32_t seqno){

    uint32_t sum = 0;
    sum += len;
    sum += seqno;
    int n = content.length();
    char arr[n+1];
    strcpy(arr, content.c_str());
    for (int i = 0; i < n; i++){
        sum += arr[i];
    }
    while (sum >> 16){
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    uint16_t OCSum = (uint16_t) (~sum);
    return OCSum;
}

uint16_t get_ack_checksum (uint16_t len , uint32_t ackno){

    uint32_t sum = 0;
    sum += len;
    sum += ackno;
    while (sum >> 16){
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    uint16_t OCSum = (uint16_t) (~sum);
    return OCSum;
}

