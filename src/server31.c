/*
** selectserver.c -- сервер многопользовательского чата
*/

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <errno.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h> 
#include <arpa/inet.h> 
#include <sys/wait.h> 
#include <signal.h>
#include "llist.h"
#include <sys/select.h>

FILE *fileLog;
FILE *fileLogEr;
 
#define PORT "7781"   // порт, который мы слушаем

#define MAX_GAME 30
#define MAX_CLIENTS 40


int TEST = 111;
int RECV_SERVER = 222;

int WAIT = 1;
int CHOOSE = 2;

int MAKE_CHOICE = 4;
int TOO_LATE = 5;
int NORM = 6;
int NO_GAMES = 255;
int MOVE = 10;
int CHAT = 11;

int SMB_WIN1 = 30;
int SMB_WIN2 = 40;
int DEAD_HEAT = 50;
int GO_ON = 60;

int I_WAIT = 100;
int I_PLAY = 101;
int I_MOVE = 102;
int DISCONECT_CLIENT = 151;

struct list *ptrlist;
struct lroot *root;
int clientsCount = 0;

fd_set master;    // главный сет дескрипторов
int fdmax;        // макс. число дескрипторов

int gameCount = 0;
int clients[MAX_CLIENTS];
int servSock, recvServSock = -1;



struct listOFServers {
	char address[15];
	char port[4];
	int sock;
	int chk;
	struct listOFServers *next;
};

struct listOFServers *rootServers;

struct listOFServers *addNewServer(struct listOFServers *rootS, char *address, char *port) {
	struct listOFServers *newServer = (struct listOFServers *) malloc(sizeof(struct listOFServers));
	
	strcpy(newServer->address, address);
	strcpy(newServer->port, port);
	newServer->sock = -1;
	newServer->chk = 0;
	
	newServer->next = rootS;
	return newServer;
}

struct Game {
	int op1;
	int op2;
	int id1;
	int id2;
	int playGround[3][3];
};

struct Game *games[MAX_GAME];

struct fullMessage {
	int id;
	int code;
	int data;
	char data2[200 - (sizeof(int)*2 + 1)];
} myFullSendMessage, myFullRecvMessage;

struct fullMessage myFullRecvMessage;
struct fullMessage myFullSendMessage;

int connectToServer(struct listOFServers *ptr);
int connectToNextServer();

int mySend(int id, int code, int data, char* data2, int send_soc) {
	int total = 0, n = 0;
	
	//memset(fullMessage, 0, msglen);
 	myFullSendMessage.id = id;
	myFullSendMessage.code = code;
	myFullSendMessage.data = data;
 	strcpy(myFullSendMessage.data2, data2);
 	
 	while(total < sizeof(struct fullMessage)) {
		n = send(send_soc,((char*) &myFullSendMessage) + total, sizeof(struct fullMessage) - total, 0);
		if (n == -1) {
			return -1;
		}
		total += n;
	}
	return total;
}

int myRecv(int recv_soc) {
	unsigned int total = 0, n = 0;
 	
 	while(total < sizeof(struct fullMessage)) {
		n = recv(recv_soc,((char*) &myFullRecvMessage) + total, sizeof(struct fullMessage) - total, 0);
		if (n == 0) {
			//perror("recv");
			return 0;
		}
		if (n < 0)
		{
			return n;
		}
		
		total += n;
	}
	return total;
}

void newGame(int op1, int op2)
{
	struct Game *newGame = (struct Game*)malloc(sizeof(struct Game));
	newGame->op1 = op1;
	newGame->op2 = op2;
	newGame->id1 = -1;
	newGame->id2 = -1;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			(newGame->playGround)[i][j] = 0;
		}
	}
	games[gameCount] = newGame;
}

int check_gr(int play_gr[3][3])
{
	int res = 0, i, j;	
	
	if (
		((play_gr[0][0] == play_gr[1][0]) && (play_gr[0][0] == play_gr[2][0]) && (play_gr[0][0] != 0)) ||
		((play_gr[0][1] == play_gr[1][1]) && (play_gr[0][1] == play_gr[2][1]) && (play_gr[0][1] != 0)) ||
		((play_gr[0][2] == play_gr[1][2]) && (play_gr[0][2] == play_gr[2][2]) && (play_gr[0][2] != 0)) ||
		((play_gr[0][0] == play_gr[0][1]) && (play_gr[0][0] == play_gr[0][2]) && (play_gr[0][0] != 0)) ||
		((play_gr[1][0] == play_gr[1][1]) && (play_gr[1][0] == play_gr[1][2]) && (play_gr[1][0] != 0)) ||
		((play_gr[2][0] == play_gr[2][1]) && (play_gr[2][0] == play_gr[2][2]) && (play_gr[2][0] != 0)) ||
		((play_gr[0][0] == play_gr[1][1]) && (play_gr[0][0] == play_gr[2][2]) && (play_gr[0][0] != 0)) ||
		((play_gr[2][0] == play_gr[1][1]) && (play_gr[2][0] == play_gr[0][2]) && (play_gr[2][0] != 0)) 
	) {
		res = 1;
	}
	
	if (res != 1) {
		int fl = 0;
		for (i = 0; i < 3; i++)
		{
			for (j = 0; j < 3; j++)
			{
				if (play_gr[i][j] == 0)
				{
					fl = 1;
				}
				
			}
		}
		if (fl == 0)
		{
			res = 2;
		}
	}
	
	return res;
}

void sendNext(int id, int code, int data, char *data2) {
	int num = -1, x = 0, n = -1;
	
	while(x == 0) {
		n = mySend(id, code, data, data2, servSock);
		x = 1;
	}
}

void sendClient(int id, int code, int data, char *data2, int sockClient) {
	if (mySend(id, code, data, data2, sockClient) == -1) {
		int j;
		for (j = 0; j < MAX_CLIENTS; j++)
		{
			if (clients[j] == sockClient)
			{
				break;
			}
		}
		fprintf(fileLog, "time Client id:%d not responding\n", j); 
		fflush(fileLog);
	}
}
int connectToNextServer()
{
	struct listOFServers *ptr;
	int mSock = -1;
	
	if (servSock != -1)
	{
		FD_CLR(servSock, &master);
	}
	
	ptr = rootServers;
	while (ptr != NULL)
	{
		
		mSock = connectToServer(ptr);
		if (mSock != -1)
		{
			break;
		} else {
			ptr = ptr->next;
			rootServers = ptr;
		}
	}
	if (mSock == -1)
	{
		return mSock;
	} else {
		FD_SET(mSock, &master);
		if (mSock > fdmax) {
			fdmax = mSock;
		}
		mySend(0, RECV_SERVER, 0, "", mSock);
		return mSock;
	}
}

// получаем sockaddr, IPv4 или IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int connectToServer(struct listOFServers *ptr)
{
    struct addrinfo hints, *servinfo, *p; 
    int rv; 
    char s[INET6_ADDRSTRLEN];
	int mySocket, flag = 0;

    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM; 
    if ((rv = getaddrinfo(ptr->address, ptr->port, &hints, &servinfo)) != 0) { 
        //fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
        //exit(1); 
        flag = 1;
	}
	
    for(p = servinfo; p != NULL; p = p->ai_next) { 
        if ((mySocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) { 
			//perror("client: socket"); 
			continue; 
		}
        if (connect(mySocket, p->ai_addr, p->ai_addrlen) == -1) { 
            close(ptr->sock); 
            //perror("client: connect"); 
			continue; 
		}
		break;
	}

    if (p == NULL || flag == 1) { 
        return -1;
	}

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s); 

	//FD_SET(sockfd, &master);
	
    freeaddrinfo(servinfo);
    
    return mySocket;
}

void serverRecv(int i) {
	int nbytes;
	nbytes = myRecv(i);
	if (nbytes <= 0) {
     // получена ошибка или соединение закрыто клиентом
		if (nbytes == 0) {
        // соединение закрыто
			fprintf(fileLog, "Sever disconnected\n");  
			fflush(fileLog);
        } else {
			fprintf(fileLog, "Server error\n"); 
			fflush(fileLog);
        }
        close(i); // bye!
        FD_CLR(i, &master); // удаляем из мастер-сета
        recvServSock = -1;
	} else {
		int rcvCode = myFullRecvMessage.code;
		if (rcvCode == WAIT)
		{
			addelem(root, -1, clientsCount, myFullRecvMessage.data2);
			sendNext(0, WAIT, 0, myFullRecvMessage.data2);
			clients[clientsCount] = i;		
			clientsCount++;
		} else if (rcvCode == MAKE_CHOICE) {
			ptrlist = listfind(root, myFullRecvMessage.id);
							
			newGame(-1, -1);
			games[gameCount]->id1 = myFullRecvMessage.id;
			games[gameCount]->id2 = myFullRecvMessage.data;
			
				
			sendNext(games[gameCount]->id1, MAKE_CHOICE, games[gameCount]->id2, "");
			clientsCount++;
			gameCount++;
			deletelem(ptrlist, root);
		} else if (rcvCode == MOVE) {
				//отправить i и j op2
				int ij, gameId = -1;
				ij = myFullRecvMessage.data;
				
				int a = ij / 10;
				int b = ij % 10;
	
				gameId = myFullRecvMessage.id;
				sendNext(gameId, MOVE, ij, "");
							
				myRecv(i);
				if (myFullRecvMessage.data == games[gameId]->id1)
				{
					games[gameId]->playGround[a][b] = 1;
					sendNext(gameId, MOVE, games[gameId]->id1, "");		
				} else {
					games[gameId]->playGround[a][b] = 2;
					sendNext(gameId, MOVE, games[gameId]->id2, "");
				}
			}
		}
	
}

int main(void)
{   
    /*
     * 
     * 
     * 
     * изменить имя лога в соответсвии с именем каждого сервера
     * 
     * 
     * */
    fileLog = fopen("logServ1", "a");
    fileLogEr = fopen("logSendRecv", "a");
    
    fd_set read_fds;  // временный сет дескрипторов для select()

    int listener;     // дескриптор слушающего сокета
    int newfd;        // дескриптор для новых соединений после accept()
    struct sockaddr_storage remoteaddr; // адрес клиента
    socklen_t addrlen;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // для setsockopt() SO_REUSEADDR, ниже
    int i, j, a, b, ij, rv;

    struct addrinfo hints, *ai, *p;

	root = init();
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		clients[i] = -1;
	}
	rootServers = NULL;
	
	rootServers = addNewServer(rootServers, "127.0.0.1", "7784");
	rootServers = addNewServer(rootServers, "127.0.0.1", "7783");
	rootServers = addNewServer(rootServers, "127.0.0.1", "7782");
	servSock = -1;
	servSock = connectToNextServer();
    
    for (i = 0; i < MAX_GAME; i++)
	{
		games[i] = NULL;
	}
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		clients[i] = -1;
	}
	
	
    FD_ZERO(&master);    // очищаем оба сета
    FD_ZERO(&read_fds);

    // получаем сокет и биндим его
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %sn", gai_strerror(rv));
        exit(1);
    }
   
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }
       
        // избегаем ошибки "address already in use"
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // если мы попали сюда, значит мы не смогли забиндить сокет
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bindn");
        exit(2);
    }

    freeaddrinfo(ai); // с этим мы всё сделали

    // слушаем
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // добавляем слушающий сокет в мастер-сет
    FD_SET(listener, &master);

    // следим за самым большим номером дескриптора
    fdmax = listener; // на данный момент это этот

	if (servSock != -1)
	{
		FD_SET(servSock, &master);
		if (servSock > fdmax)
		{
			fdmax = servSock;
		}
	}
	
    // главный цикл
    for(;;) {
        read_fds = master; // копируем его
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // проходим через существующие соединения, ищем данные для чтения
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // есть!
                if (i == listener) {
                    // обрабатываем новые соединения
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // добавляем в мастер-сет
                        if (newfd > fdmax) {    // продолжаем отслеживать самый большой номер дескиптора
                            fdmax = newfd;
                        }
                        printf("selectserver: new connection from %s on "
                            "socket %dn",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                    }
                } else if (i == recvServSock) {
					serverRecv(i);
				} else if (i == servSock) {
					int nbytes;
					if ((nbytes = myRecv(i)) <= 0) {
                        // получена ошибка или соединение закрыто клиентом
                        if (nbytes == 0) {
                            // соединение закрыто
                            fprintf(fileLog, "Server disconnected\n"); 
							fflush(fileLog);
                        } else {
							fprintf(fileLog, "Server error\n"); 
							fflush(fileLog);
                        }
                        servSock = -1;
                        close(i); // bye!
                        FD_CLR(i, &master); // удаляем из мастер-сета
					}
				} else {
					//обрабатываем сообщение от клиента
					int nbytes;
					
					if ((nbytes = myRecv(i)) <= 0) {
                        // получена ошибка или соединение закрыто клиентом
                        for (j = MAX_CLIENTS; j >= 0; j--)
						{
								if (clients[j] == i)
								{
									break;
								}
						}
						
                        if (nbytes == 0) {
                            // соединение закрыто
                            fprintf(fileLog, "Client id:%d disconnected\n", j); 
							fflush(fileLog);
                        } else {
							fprintf(fileLog, "Client id:%d error\n", j); 
							fflush(fileLog);
                        }
                        if (j != -1)
						{
							int k;
							for (k = MAX_GAME; k >= 0; k--)
							{
								if (games[k] != NULL)
								{
									if (games[k]->op1 == i)
									{
										sendClient(0, DISCONECT_CLIENT, 0, "", games[k]->op2);
									}
									if (games[k]->op2 == i)
									{
										sendClient(0, DISCONECT_CLIENT, 0, "", games[k]->op1);
									}
								}
							}
						}
						
						
                        close(i); // bye!
                        FD_CLR(i, &master); // удаляем из мастер-сета
					} else {
						int rcvCode = myFullRecvMessage.code;
						if (rcvCode == WAIT)
						{
							addelem(root, i, clientsCount, myFullRecvMessage.data2);
							sendClient(clientsCount, WAIT, 0, "", i);
							
							clients[clientsCount] = i;
							
							sendNext(0, WAIT, 0, myFullRecvMessage.data2);
							clientsCount++;
						} else if (rcvCode == CHOOSE) {
							ptrlist = root->first_node;
							sendClient( 0, CHOOSE, root->count, "", i);
							for (int j = 0; j < root->count; j++)
							{
								sendClient(ptrlist->id, CHOOSE, 0, ptrlist->name, i);
								ptrlist = ptrlist->ptr;
								myRecv(i);
							}
						} else if (rcvCode == MAKE_CHOICE) {
							//message[numbytes] = '\0';
							ptrlist = listfind(root, myFullRecvMessage.data);
							if (ptrlist == NULL)
							{
								sendClient(0, TOO_LATE, 0, "", i);
							} else {
								newGame(ptrlist->fd, i);
								games[gameCount]->id1 = ptrlist->id;
								games[gameCount]->id2 = clientsCount;
								
								clients[clientsCount] = i;
								sendNext(ptrlist->id, MAKE_CHOICE, clientsCount, "");
								
								sendClient(gameCount, NORM, 0, "", games[gameCount]->op1);
								sendClient(gameCount, NORM, clientsCount, "", games[gameCount]->op2);
	
								clientsCount++;
								gameCount++;
								deletelem(ptrlist, root);
							}
						} else if (rcvCode == MOVE) {
							ij = myFullRecvMessage.data;
							a = ij / 10;
							b = ij % 10;
										
							//НУЖНО СДЕЛАТЬ ПРОВЕРКУ НА КОРРЕКТНОСТЬ
										
							
							sendNext(myFullRecvMessage.id, MOVE, myFullRecvMessage.data, "");
							
							//отправить i и j op2
							int myOp2;
							if (i == games[myFullRecvMessage.id]->op1)
							{
								myOp2 = games[myFullRecvMessage.id]->op2;
								games[myFullRecvMessage.id]->playGround[a][b] = 1;
								
								sendNext(myFullRecvMessage.id, MOVE, games[myFullRecvMessage.id]->id1, "");
							} else {
								myOp2 = games[myFullRecvMessage.id]->op1;
								games[myFullRecvMessage.id]->playGround[a][b] = 2;
								
								sendNext(myFullRecvMessage.id, MOVE, games[myFullRecvMessage.id]->id2, "");
							}
							sendClient(myFullRecvMessage.id, MOVE, ij, "", myOp2);
							
							//проверка на выигрыш
							int chk = check_gr(games[myFullRecvMessage.id]->playGround);
							if (chk == 1) {
								//игрок выиграл
								sendClient(myFullRecvMessage.id, SMB_WIN1, 0, "", i);
								sendClient(myFullRecvMessage.id, SMB_WIN2, 0, "", myOp2);
								
							} else {
								if (chk == 2)
								{
									//ничья
									sendClient(myFullRecvMessage.id, DEAD_HEAT, 0, "", games[myFullRecvMessage.id]->op1);
									sendClient(myFullRecvMessage.id, DEAD_HEAT, 0, "", games[myFullRecvMessage.id]->op2);
									
								} else {
									sendClient(myFullRecvMessage.id, GO_ON, 0, "", games[myFullRecvMessage.id]->op1);
									sendClient(myFullRecvMessage.id, GO_ON, 0, "", games[myFullRecvMessage.id]->op2);
								}
							}
						} else if (rcvCode == CHAT) {
							int myOp2;
							if (i == games[myFullRecvMessage.id]->op1)
							{
								myOp2 = games[myFullRecvMessage.id]->op2;
							} else {
								myOp2 = games[myFullRecvMessage.id]->op1;
							}
							sendClient(myFullRecvMessage.id, CHAT, 0, myFullRecvMessage.data2, myOp2);
						} else if (rcvCode == RECV_SERVER) {
							recvServSock = i;
						} else if (rcvCode == I_WAIT) {
							ptrlist = listfind(root, myFullRecvMessage.data);
							ptrlist->fd = i;
							clients[ptrlist->id] = i;
						} else if (rcvCode == I_PLAY) {
							if (myFullRecvMessage.data == games[myFullRecvMessage.id]->id1)
							{
								games[myFullRecvMessage.id]->op1 = i;
								clients[games[myFullRecvMessage.id]->id1] = i;
							} else if (myFullRecvMessage.data == games[myFullRecvMessage.id]->id2) {
								games[myFullRecvMessage.id]->op2 = i;
								clients[games[myFullRecvMessage.id]->id2] = i;
							} else {
							}
						} else if (rcvCode == TEST) {
							mySend(123, TEST, 12, "test", i);
						}

					}
                }
                 // Закончили обрабатывать данные от клиента
				
            } // Закончили обрабатывать новое входящее соединение
        } // Закончили цикл по дескрипторам
    } // Закончили for(;;)
   
    return 0;
}
