//
//  main.c
//  DVA218 - lab3
//
//  Created by Kanaljen on 2017-05-01.
//  Copyright © 2017 Kanaljen. All rights reserved.
//

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include "main.h"
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <sys/select.h>
#include <sys/fcntl.h>

struct sockaddr_in localhost, remotehost[FD_SETSIZE];
int sock;
socklen_t slen;
fd_set readFdSet, fullFdSet;
int mode = 0;
struct serie *head = NULL;

int main(int argc, const char * argv[]) {
    int signal = NOSIG, i, freeSpaces, k=0;
    int stateDatabase[FD_SETSIZE];
    int waitTimes[FD_SETSIZE];
    char buffer[512];
    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    int clientSock;
    struct pkt *packet;
    int ln;
    int wnd[FULLWINDOW];
    int recStart=0, recEnd=0, sendStart=0, sendEnd=0;

    recEnd = recStart + WNDSIZE;

    slen=sizeof(struct sockaddr_in);

    sock = makeSocket();

    printf("Select Mode: 1 for server, 2 for client: ");

    FD_ZERO(&fullFdSet);

    /* Select process-mode */
    //system("clear");

    FD_SET(STDIN_FILENO,&fullFdSet); // Add stdin to active set

    while((mode != SERVER)&&(mode != CLIENT)){
      //  select(FD_SETSIZE, &fullFdSet, NULL, NULL, NULL);
        fgets(buffer, sizeof(buffer), stdin);
        size_t ln = strlen(buffer)-1;
        if (buffer[ln] == '\n') buffer[ln] = '\0';
        mode = atoi(buffer);
        if(mode != SERVER && mode != CLIENT)printf("> invalid mode\n");

    }

    FD_CLR(STDIN_FILENO,&fullFdSet); // Remove stdin from active set

    /* Mode specific startup */

    if(mode == SERVER){

        printf("Mode [%d]: SEVER\n\n",mode);
        bindSocket(sock);
        stateDatabase[sock] = WAITING + SYN;

    }

    else if(mode == CLIENT){

        printf("Mode [%d]: CLIENT\n\n",mode);
        connectTo(SRV_IP);
        printf("[%d] Sending SYN!\n",sock);
        sendPacket(sock,SYN,0);
        stateDatabase[sock] = WAITING + SYN + ACK;

    }

    while(1){ /* Start: Main process-loop */

        FD_ZERO(&readFdSet);

        packet = calloc(1, sizeof(struct pkt));

        readFdSet = fullFdSet;

        if(select(FD_SETSIZE, &readFdSet, NULL, NULL, &tv) < 0) printf("Select failed\n");


        // Loop ALL sockets
        for(i = sock;i < FD_SETSIZE;i++){ /* Start: ALL sockets if-statement */

            if(FD_ISSET(i,&fullFdSet)){ /* Start: ACTIVE sockets if-statement */



                switch (stateDatabase[i]) { /* Start: State-machine switch */

                    case WAITING + SYN: // Main server state

                        signal = readPacket(i,packet);

                        if (signal == SYN){

                            clientSock = newClient();

                            printf("[%d] Sending SYNACK!\n",clientSock);
                            sendPacket(clientSock,SYN + ACK, 0);

                            waitTimes[clientSock] = 0;
                            stateDatabase[clientSock] = WAITING + ACK;

                        }

                        else {

                            //printf("[%d] waiting\n",i);


                        }

                        break;

                    case WAITING + SYN + ACK:

                        signal = readPacket(i,packet);

                        if (signal == SYN + ACK){
                            printf("[%d] Sending ACK!\n",i);
                            sendPacket(i, ACK, 0);
                            stateDatabase[i] = ESTABLISHED;
                            printf("[%d] ESTABLISHED\n",i);
                            sleep(1);
                          //  FD_SET(STDIN_FILENO,&fullFdSet);

                        }

                        else{
                            printf("connection timedout...\n");
                            printf("[%d]Sending SYN!\n",i);
                            sendPacket(i, SYN, 0);

                        }

                        break;

                    case WAITING + ACK:

                        waitTimes[i]++;

                        signal = readPacket(i,packet);

                        if(waitTimes[i] >= NWAITS){

                            printf("[%d] connection from client lost\n",i);
                            close(i);
                            FD_CLR(i,&fullFdSet);

                        }
                        else if (signal == ACK){

                            printf("[%d] ESTABLISHED\n",i);
                            stateDatabase[i] = ESTABLISHED;

                        }
                        else{

                            printf("[%d] Sending new SYNACK!\n",i);
                            sendPacket(i, SYN + ACK, 0);

                        }

                        break;

                    case ESTABLISHED:

                      /*  for(k=0; k<WNDSIZE; k++){
                            printf("WND[k] = %d\n", wnd[k]);
                        }*/

                        signal = readPacket(i,packet);

                      //  printf("Signal = %d\n", signal);

                        switch (signal) { /* Start: established state-machine */

                            case SYN + ACK: // If the client gets an extra SYNACK after established
                                sendPacket(i, ACK, 0);
                                break;

                            case NOSIG:

                                break;

                            case DATA + SYN:

                                if((freeSpaces = freeWndSlots(wnd, recStart, recEnd)) == 0){
                                    printf("NO WNDSLOT FREE\n");
                                    break;
                                }
                                else
                                    printf("There are %d free places in window\n", freeSpaces);

                                if(mode == SERVER){
                                    head = createSerie(buffer, packet->serie);
                                }

                                head->data[0] = packet->data;
                                head->current = 1;

                                printf("DATA+SYN inkommet: %c \n",packet->data);
                                printf("På serienummer: %lf\n", head->serie);

                                sendAckPacket(i, packet->seq, head);


                                break;

                            case DATA:

                            if((freeSpaces = freeWndSlots(wnd, recStart, recEnd)) == 0){
                                printf("NO WNDSLOT FREE\n");
                                break;
                            }
                            else
                                printf("There are %d free places in window\n", freeSpaces);

                                head->data[head->current] = packet->data;
                                head->current ++;

                                printf("DATA INKOMMET ");
                                printf("Data = %c\n", packet->data);
                                printf("Index = %d\n", packet->seq);

                                sendAckPacket(i, packet->seq, head);
                                printf("SendAck Sent\n");

                                break;

                            case DATA + FIN:

                                head->data[head->current] = packet->data;

                                printf("Sista data inkommet: %c\n",packet->data);

                                printf("Hela strängen = %s\n", head->data);

                                sendAckPacket(i, packet->seq, head);

                                reQueueSerie(head);

                                break;

                            case DATA + FIN + SYN:

                                printf("Enda data inkommet: %c\n",packet->data);

                                break;

                            case DATA + ACK:

                                printf("ACK på seq: %d, bokstav: %c\n", packet->seq, head->data[packet->seq]);
                                printf("I can send max %d packets\n", packet->wnd);

                                if(packet->seq == (strlen(head->data) - 1)){
                                    printf("Köar om...\n");
                                    reQueueSerie(head);
                                }
                                else{
                                  sendSerie(i, head);
                                  printf("%c SENT! with\n", head->data[head->current]);
                                  head->current ++;
                                }


                                break;

                            case WNDUPDATE + ACK:

                              /*  wnd[packet->wnd] = 0;

                                if(wnd[0] == FREE)
                                    wndUpdate(wnd);*/

                                break;

                            default:

                                break;

                        } /* End: established state-machine */

                      if(mode == CLIENT && k == 0){
                            fgets(buffer, sizeof(buffer), stdin);
                            ln = strlen(buffer)-1;
                            if (buffer[ln] == '\n') buffer[ln] = '\0';
                            if(strncmp(buffer,"quit\n",ln) != 0)
                            {
                              if(ln>0)queueSerie(createSerie(buffer, timestamp()), &head);
                              printf("Skickar en serie med nummer: %lf\n", head->serie);

                                sendSerie(i, head);
                                sendEnd = head->current;
                                head->current ++;

                            }
                            else {
                              close(sock);
                              exit(EXIT_SUCCESS);
                            }
                            k++;
                        };
                    default:

                      //  printf("[%d] NO STATE: %d\n",i,stateDatabase[i]);

                        break;


                } /* End: state-machine switch */

            } /* End: ACTIVE sockets if-statement */

        }/* End: ALL sockets if-statement */

        free(packet);
    } /* End: Main process-loop */

    return 0;
}

void sendAckWnd(int wnd[WNDSIZE]){

}

int freeWndSlots(int wnd[FULLWINDOW], int recStart, int recEnd){
    int i, freeSlots = 0;

    for(i = recStart; i < recEnd; i++)
    {
        if(wnd[i] == FREE)
            freeSlots ++;
    }
    return freeSlots;
}

void wndUpdate(int wnd[WNDSIZE]){

    int i;

    if(wnd[0] == FREE)
    {
        for(i = 1; i < WNDSIZE; i++)
        {
          wnd[i-1] = wnd[i];
        }
    }
    if(wnd[0] == FREE)
        wndUpdate(wnd);

}

void reQueueSerie(struct serie *head){

    struct serie *serieToRemove;
    serieToRemove = head;

    if(head->next == NULL)
        free(head);
    else
    {
        head = head->next;
        free(serieToRemove);
    }

}

struct pkt createPacket(int signal, double serie, int seq, int wnd, char data){

    struct pkt packet;

    packet.flg = signal;
    packet.serie = serie;
    packet.seq = seq;
    packet.wnd = wnd;
    packet.data = data;

    return packet;

}

double timestamp(void){

    struct timespec tp;

    clock_gettime(CLOCK_MONOTONIC, &tp);

    double timestamp = tp.tv_sec + (tp.tv_nsec*0.000000001);

    return timestamp;

}

void sendAckPacket(int client, int seq, struct serie *head){

    int signal = DATA + ACK;

    struct pkt packet = createPacket(signal, head->serie, seq, 1, 0);

if (sendto(client, (void*)&packet, sizeof(struct pkt), 0, (struct sockaddr*)&remotehost[client], slen)==-1)exit(EXIT_FAILURE);

    printf("ACK sent with signal = %d to client : %d\n", packet.flg, client);

}

void sendSerie(int client, struct serie *head){

    int signal;

    if(mode == CLIENT){
        int len = strlen(head->data) - 1;

        if(head->current == 0) if(len == 0) signal = DATA + SYN + FIN; else signal = DATA + SYN;
        else if(head->current == len) signal = DATA + FIN;
        else signal = DATA;
    }

    struct pkt packet = createPacket(signal, head->serie, head->current, 0, head->data[head->current]);

    if(mode == SERVER){
        if (sendto(client, (void*)&packet, sizeof(struct pkt), 0, (struct sockaddr*)&remotehost[client], slen)==-1)exit(EXIT_FAILURE);
    }
    else{
        if (sendto(sock, (void*)&packet, sizeof(struct pkt), 0, (struct sockaddr*)&remotehost[sock], slen)==-1){
            printf("Nåt fel!!!!");
            exit(EXIT_FAILURE);
        }
    }

    printf("Packet sent with Signal = %d\n", signal);

}

struct serie *createSerie(char* input, double timestamp){

    struct serie *newSerie = (struct serie*)malloc(sizeof(struct serie));

    // Create serie-node
    newSerie->serie = timestamp;
    strcpy(newSerie->data, input);
    newSerie->next = NULL;
    newSerie->current = 0;

    return newSerie;
}

void queueSerie(struct serie *newSerie,struct serie **serieHead){

    if(*serieHead == NULL)*serieHead = newSerie;
    else queueSerie(newSerie,&((*serieHead)->next));


}

int makeSocket(void){

    int sock;

    // Create the UDP socket
    if ((sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)exit(EXIT_FAILURE);

    return sock;
}

int newClient(){

    int clientSock;

    clientSock = makeSocket();

    remotehost[clientSock].sin_family = remotehost[sock].sin_family;

    remotehost[clientSock].sin_port = remotehost[sock].sin_port;

    remotehost[clientSock].sin_addr = remotehost[sock].sin_addr;

    printf("[%d] new client on [%d]\n",sock,clientSock);

    FD_SET(clientSock,&fullFdSet);

    return clientSock;

}

int connectTo(char* server){

  struct hostent *hostInfo;

  memset((char *) &remotehost, 0, sizeof(struct sockaddr_in));

  remotehost[sock].sin_family = AF_INET;

  remotehost[sock].sin_port = htons(PORT);

/*  if (inet_aton(server, &remotehost[sock].sin_addr)==0) {
      fprintf(stderr, "inet_aton() failed\n");
      exit(EXIT_FAILURE);
  }*/
  hostInfo = gethostbyname(server);
  if(hostInfo == NULL) {
    fprintf(stderr, "initSocketAddress - Unknown host %s\n",server);
    exit(EXIT_FAILURE);
  }

  remotehost[sock].sin_addr = *(struct in_addr *)hostInfo->h_addr;

  FD_SET(sock,&fullFdSet);

  return 1;
}

int bindSocket(int socket){

    memset((char *) &localhost, 0, sizeof(localhost));

    localhost.sin_family = AF_INET;

    localhost.sin_port = htons(PORT);

    localhost.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socket, (struct sockaddr*)&localhost, sizeof(localhost))==-1)exit(EXIT_FAILURE);

    FD_SET(sock,&fullFdSet);

    return 1;
}

int sendPacket(int client, int signal, char msg){

    struct pkt packet;

    memset((char *) &packet, 0, sizeof(packet));

    packet.flg = signal;

    sleep(SENDDELAY);

    if(signal == DATA)
        packet.data = msg;

    if(mode == SERVER){
        if (sendto(client, (void*)&packet, sizeof(struct pkt), 0, (struct sockaddr*)&remotehost[client], slen)==-1)exit(EXIT_FAILURE);
    }
    else{
        if (sendto(sock, (void*)&packet, sizeof(struct pkt), 0, (struct sockaddr*)&remotehost[sock], slen)==-1){
            printf("Nåt fel!!!!");
            exit(EXIT_FAILURE);
        }
    }


    return 1;
}

int readPacket(int client,struct pkt *packet){

    memset((char *) packet, 0, sizeof(struct pkt));
  //  memset((char *) &(packet->data), 0, sizeof(struct pkt));

    if(FD_ISSET(client,&readFdSet)){

        recvfrom(client, (void*)packet, sizeof(struct pkt), 0, (struct sockaddr*)&remotehost[client],&slen);

        return packet->flg;

    }

    return NOSIG;


}
