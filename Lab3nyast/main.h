//
//  main.h
//  DVA218 - lab3
//
//  Created by Kanaljen on 2017-05-01.
//  Copyright © 2017 Kanaljen. All rights reserved.
//

#ifndef main_h
#define main_h

#define PORT 8888
#define SRV_IP "127.0.0.1"

/* Define process mode */
#define SERVER 1
#define CLIENT 2

/* Define machine signals */

#define NOSIG     0
#define DATA      1
#define ACK       2
#define SYN       4
#define FIN       8
#define WNDUPDATE 16

/* Define mashine states */

#define NOSTATE         0
#define WAITING         100
#define ESTABLISHED     200

#define NWAITS      3
#define TIMEOUT     10
#define SENDDELAY   0
#define BUFFSIZE    512
#define WNDSIZE     1
#define FULLWINDOW  3

#define FREE        0
#define TAKEN       1

struct pkt{
    int flg;
    double serie;
    int seq;
    int wnd;
    char data;
    int chksum;
};

struct serie{
    double serie;
    char data[BUFFSIZE];
    int current;
    struct serie *next;
};

void sendAckPacket(int client, int seq, struct serie *head);
int freeWndSlots(int wnd[FULLWINDOW], int recStart, int recEnd);
void wndUpdate(int wnd[WNDSIZE]);
struct pkt createPacket(int signal, double serie, int seq, int wnd, char data);
double timestamp(void);
void sendSerie(int client, struct serie *head);
struct serie *createSerie(char* input, double timestamp);
void queueSerie(struct serie *newSerie,struct serie **serieHead);
void reQueueSerie(struct serie *head);
int makeSocket(void);
int newClient();
int connectTo(char* server);
int bindSocket(int sock);
int readPacket(int client,struct pkt *packet);
int sendPacket(int client, int signal, char msg);
int waitFor(int signal);
int signalRecived(int signal);

int readSocket();

#endif /* main_h */
