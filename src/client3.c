#include <gtk/gtk.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <errno.h> 
#include <string.h> 
#include <netdb.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/poll.h>


#define PORT "7778"    
#define MAXDATASIZE 100 
#define nlen 21
#define msglen 64
#define MAX_NAME_LENGTH 20
#define MAX_NUMBERS_OF_NAME 10

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

int NO_SERVERS = 144;
int FATAL_SEND_ERROR = 228;

int NON_FLAG = 444;

struct my_rows{
	int id;
	char name[MAX_NAME_LENGTH];
};

struct myClient{
	int id;
	int idGame;
} client;

typedef struct
{
     GtkWidget *entry, *textview;
} Widgets;

struct my_rows names[MAX_NUMBERS_OF_NAME];

int sockfd, numbytes, course, number_course, per1 = 0;
char buf[MAXDATASIZE], cmd[3], message_send[msglen], message_recv[msglen];
const gchar matrx[] = "000102101112202122";

GtkWidget *contain1, *contain11;
GtkWidget *pl_gr1[3][3];
GtkWidget *pl_gr2[3][3];
GtkWidget *pl_gr_px[3][3];
GtkWidget *pl_gr_str[3];
GtkWidget *window;
GtkWidget *entry;
GtkWidget *contain;
GtkWidget *contain2;
GtkWidget *button;
Widgets *chat;
fd_set master, read_fds;

struct listOFServers {
	char address[15];
	char port[4];
	int sock;
	int chk;
	struct listOFServers *next;
};

struct listOFServers *addNewServer(struct listOFServers *root, char *address, char *port) {
	struct listOFServers *newServer = (struct listOFServers *) malloc(sizeof(struct listOFServers));
	
	strcpy(newServer->address, address);
	strcpy(newServer->port, port);
	newServer->sock = -1;
	newServer->chk = 0;
	
	newServer->next = root;
	return newServer;
}


struct listOFServers *root;

static void insertEntryText (GtkButton*, Widgets*);
void callback_pressButton(GtkWidget * widget, gpointer data);
GtkWidget *xpm_label_box(GtkWidget *parent, gchar *xpm_filename);
void callback_joinGame (GtkWidget * widget, gpointer data);
void createPlayground();
void callback_nonActiveButton (GtkWidget * widget, gpointer data);
gint delete_event (GtkWidget * widget, GdkEvent * event, gpointer data);
void createMainMenu();
void myConnect();
void reconnectSend(int id, char code, int data, char* data2, int send_soc, int reconnectCode);
void nonReconnectSend(int id, char code, int data, char* data2, int send_soc);
void nonReconnectRecv(int recv_soc);
void createEndMenu(int command);
void connectToServers();
int connectToServer(struct listOFServers *ptr);

/* *
 * добавить картинку
 * */
GtkWidget *xpm_label_box(
	GtkWidget *parent,
	gchar *xpm_filename)
{
	GtkWidget *box1;
	GtkWidget *pixmapwid;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkStyle *style;

	box1 = gtk_hbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (box1), 2);

	style = gtk_widget_get_style(parent);

	pixmap = gdk_pixmap_create_from_xpm(
		parent->window, &mask,
		&style->bg[GTK_STATE_NORMAL],
		xpm_filename);
	pixmapwid = gtk_pixmap_new (pixmap, mask);

	gtk_box_pack_start (GTK_BOX (box1),
		pixmapwid, FALSE, FALSE, 3);

	gtk_widget_show(pixmapwid);

	return(box1);
}

struct fullMessage {
	int id;
	int code;
	int data;
	char data2[200 - (sizeof(int)*2 + 1)];
} myFullSendMessage, myFullRecvMessage;

struct fullMessage myFullRecvMessage;
struct fullMessage myFullSendMessage;

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
		} else {
			total += n;
		}
	}
	return total;
}

int myRecv(int recv_soc) {
	unsigned int total = 0, n = 0;
 	
 	while(total < sizeof(struct fullMessage)) {
		n = recv(recv_soc,((char*) &myFullRecvMessage) + total, sizeof(struct fullMessage) - total, 0);
		if (n <= 0) {
			return n;
		}
		
		total += n;
	}
	return total;
}

void nonReconnectRecv(int recv_soc) {
	if (myRecv(recv_soc) <= 0)
	{
		close(recv_soc);
		createEndMenu(FATAL_SEND_ERROR);
	}
}

/* *
 * получить адрес
 * */
 void *get_in_addr(struct sockaddr *sa) 
{ 
    if (sa->sa_family == AF_INET) { 
        return &(((struct sockaddr_in*)sa)->sin_addr); 
	}

    return &(((struct sockaddr_in6*)sa)->sin6_addr); 
}

/* *
 * ф-ия отображения полученного сообщения
 * */
void insertRecvText(gchar *text) {
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	char fulltext[70];
	
	sprintf(fulltext, "opponent: %s", text);
	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->textview));

	mark = gtk_text_buffer_get_insert (buffer);
	gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

	if (gtk_text_buffer_get_char_count(buffer))
	gtk_text_buffer_insert (buffer, &iter, "\n", 1);

	gtk_text_buffer_insert (buffer, &iter, fulltext, -1);
}

/* *
 * ф-ия пересылки и отображения набранного сообщения
 * */
static void insertEntryText(GtkButton *button, Widgets *w)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	const gchar *text;
	char fullText[75];
	char sText[70];
	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (w->textview));
	text = gtk_entry_get_text (GTK_ENTRY (w->entry));
	
	memset(sText, 0, msglen);
	strcpy(sText, text);
	
	sprintf(fullText, "you: %s", sText);
	reconnectSend(client.idGame, CHAT, 0, sText, sockfd, I_MOVE);

	mark = gtk_text_buffer_get_insert (buffer);
	gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

	if (gtk_text_buffer_get_char_count(buffer))
	gtk_text_buffer_insert (buffer, &iter, "\n", 1);

	gtk_text_buffer_insert (buffer, &iter, fullText, -1);
	gtk_editable_delete_text(GTK_EDITABLE(w->entry), 0, -1);
}

/* *
 * Изменение книпки при собственном ходе или ходе противника
 * */
void changeButton(int i, int j, int num)
{
	GtkWidget *button;
	
	gtk_widget_destroy (pl_gr2[i][j]);
		
	pl_gr2[i][j] = gtk_hbox_new(0, 0);
	pl_gr_px[i][j] = gtk_hbox_new(0, 0);
	button = gtk_button_new();
			
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
	GTK_SIGNAL_FUNC (callback_nonActiveButton), (gpointer) (gpointer) (matrx + ((3*i + j) * 2)));
				
	
	if (num == 1) {
		pl_gr_px[i][j] = xpm_label_box(window, "resources/x.png");
	} else {
		pl_gr_px[i][j] = xpm_label_box(window, "resources/o.png");
	}
	gtk_widget_show(pl_gr_px[i][j]);
	gtk_container_add (GTK_CONTAINER (button), pl_gr_px[i][j]);
				
	gtk_box_pack_start(GTK_BOX(pl_gr2[i][j]), button, TRUE, 10, 0);
	gtk_widget_show (button);
	gtk_box_pack_start(GTK_BOX(pl_gr1[i][j]), pl_gr2[i][j], TRUE, 10, 0);
	gtk_widget_show(pl_gr2[i][j]);
}

/* *
 * Функця проверки наличия сообщения при ожидании новой игры
 * */
gboolean waitOpponent(gpointer data)
{
	read_fds = master;
	if (select(sockfd + 1, &read_fds, NULL, NULL, 0) == -1) {
		perror("select");
		return FALSE;
	}
	
	if (FD_ISSET(sockfd, &read_fds)) {
		if (myRecv(sockfd) > 0) {
			if (myFullRecvMessage.code == NORM) {
				client.idGame = myFullRecvMessage.id;
				
				gtk_widget_destroy (contain);
				createPlayground();
				return FALSE;
			} else {
				return TRUE;
			}
		} else {
			myConnect();
			mySend(0, I_WAIT, client.id, "", sockfd);
			return TRUE;
		}
	} else {
		return TRUE;
	}
}

void createEndMenu(int command)
{
	GtkWidget *label;
	GtkWidget *button;
	
	close(sockfd);
	
	if (command == SMB_WIN1 || command == SMB_WIN2) {
		if (course == 1 && command == SMB_WIN1) {
			label = gtk_label_new("Вы выиграли!");
		} else if (course == 2 && command == SMB_WIN2) {
			label = gtk_label_new("Вы проиграли!");
		}
	}
	if (command == DEAD_HEAT) {
		label = gtk_label_new("Ничья!");
	}
	if (command == 255) {
		label = gtk_label_new("Нет подходящих игр,\n попробуйте позже.");
	}
	if (command == NO_SERVERS)
	{
		label = gtk_label_new("Не удается связаться с сервером,\n попробуйте позже.");
	}
	if (command == FATAL_SEND_ERROR)
	{
		label = gtk_label_new("Произошла ужасная ошибка при передаче данных.");
	}
	if (command == DISCONECT_CLIENT)
	{
		label = gtk_label_new("Ваш оппонент отключился.");
	}
	
	
	gtk_widget_destroy (GTK_WIDGET(contain));
 	
	contain = gtk_vbox_new(0, 0);
	contain1 = gtk_vbox_new(0, 0);
	contain2 = gtk_vbox_new(0, 0);
	
	gtk_box_pack_start(GTK_BOX(contain1), label, TRUE, 10, 0);
	gtk_widget_show(label);
				
	gtk_container_add (GTK_CONTAINER(contain), contain1);
	
	button = gtk_button_new_with_label ("Main menu");

	gtk_signal_connect (GTK_OBJECT (button), "clicked",
		GTK_SIGNAL_FUNC (createMainMenu), GTK_OBJECT (window));
		
	gtk_box_pack_start(GTK_BOX(contain2), button, TRUE, 10, 10);
	gtk_widget_show (button);
	
	button = gtk_button_new_with_label ("Quit");

	gtk_signal_connect (GTK_OBJECT (button), "clicked",
		GTK_SIGNAL_FUNC (delete_event), GTK_OBJECT (window));
		
	gtk_box_pack_start(GTK_BOX(contain2), button, TRUE, 10, 10);
	gtk_widget_show (button);
	
	
	
	gtk_container_add (GTK_CONTAINER(contain), contain2);

	gtk_container_add (GTK_CONTAINER(window), contain);
	gtk_widget_show(contain);
	gtk_widget_show(contain1);
	gtk_widget_show(contain2);
}

/* *
 * ф-ия реагирования на событие ход противника
 * */
gboolean waitMessage(gpointer data)
{
	int ij, i, j;
	struct timeval tv;
	
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	
	read_fds = master;
	
	if (select(sockfd + 1, &read_fds, NULL, NULL, &tv) == -1) {
		//perror("select");
		return FALSE;
	}
	
	if (FD_ISSET(sockfd, &read_fds)) {
		if(myRecv(sockfd) <= 0) {
			myConnect();
			mySend(client.idGame, I_PLAY, client.id, "", sockfd);
			sleep(3);
			return TRUE;
		} else {
			if (myFullRecvMessage.code == MOVE && course == 2) {
				//принимаем i и j и изменяем кнопку и numpl
				ij = myFullRecvMessage.data;
				i = ij / 10;
				j = ij % 10;
					
				changeButton(i, j, 2);
				
				nonReconnectRecv(sockfd);
				
				if (myFullRecvMessage.code == GO_ON)
				{
					course = 1;
				} else {
					createEndMenu(myFullRecvMessage.code);
					return FALSE;
				}
				return TRUE;
			} else if (myFullRecvMessage.code == CHAT) {
				insertRecvText(myFullRecvMessage.data2);
				return TRUE;
			} else if (myFullRecvMessage.code == DISCONECT_CLIENT){
				createEndMenu(DISCONECT_CLIENT);
				return FALSE;
			} else {
				return TRUE;
			}
		}
	} else {
		return TRUE;
	}
}

/* *
 * пустая функция обратного вызова, для неактивных кнопок
 * */
void callback_nonActiveButton (GtkWidget * widget, gpointer data) {

}

/* *
 * ф-ия реагирования на свой ход
 * */
void callback_pressButton (GtkWidget * widget, gpointer data) {
	
	/*******отправка выбранного значения, если норм, то изменить, если нет, то ничего не делать*/
	gchar *name1;
	name1 = (gchar *) data;
	int ij;
	int i = name1[0] - '0';
	int j = name1[1] - '0';
	
	//проверить numpl
	//
	//отправить сообщение о i и j 
	//получить подтверждение
	//изменить кнопку
	//запустить таймер ожидания изменения кнопки противника
	
	if (course == 1) {
		ij = i * 10 + j;
		
		
		reconnectSend(client.idGame, MOVE, ij, "", sockfd, I_MOVE);
		nonReconnectRecv(sockfd);
		changeButton(i, j, 1);
		if (myFullRecvMessage.code == GO_ON)
		{
			course = 2;
		} else {
			createEndMenu(myFullRecvMessage.code);
		}
	}
}

/* *
 * создание игрового поля
 * */
void createPlayground()
{
	int i, j;
	GtkWidget *button;
	GtkWidget *table;
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	GtkWidget *scrolled_win;
	char txt[] = "Chat:\n";
	
	chat = g_slice_new (Widgets);
	contain11 = gtk_hbox_new(0, 0);
	contain = gtk_hbox_new(0, 0);
	table = gtk_table_new (5, 8, TRUE);

	
	for(i = 0; i < 3; i++) {
		pl_gr_str[i]= gtk_vbox_new(0, 0);
		gtk_box_pack_start(GTK_BOX(contain11), pl_gr_str[i], TRUE, 10, 0);
		gtk_widget_show(pl_gr_str[i]);
	}
	
	for(i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			pl_gr1[i][j] = gtk_hbox_new(0, 0);
			gtk_box_pack_start(GTK_BOX(pl_gr_str[i]), pl_gr1[i][j], TRUE, 10, 0);
			gtk_widget_show(pl_gr1[i][j]);
		}
	}
	for(i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			pl_gr2[i][j] = gtk_hbox_new(0, 0);

			button = gtk_button_new();
			gtk_signal_connect (GTK_OBJECT (button), "clicked",
				GTK_SIGNAL_FUNC (callback_pressButton), (gpointer) (matrx + ((3*i + j) * 2)));
			
			pl_gr_px[i][j] = xpm_label_box(window, "resources/zero.png");
			gtk_widget_show(pl_gr_px[i][j]);
			gtk_container_add (GTK_CONTAINER (button), pl_gr_px[i][j]);
			
			gtk_box_pack_start(GTK_BOX(pl_gr2[i][j]), button, TRUE, 10, 0);
			gtk_widget_show (button);
			gtk_box_pack_start(GTK_BOX(pl_gr1[i][j]), pl_gr2[i][j], TRUE, 10, 0);
			gtk_widget_show(pl_gr2[i][j]);
		}
	}
	gtk_table_attach_defaults (GTK_TABLE (table), contain11, 0, 3, 0, 3);
	gtk_widget_show(contain11);
	gtk_widget_show(contain);
	
	button = gtk_button_new_with_label ("Send");
		
	g_signal_connect (G_OBJECT (button), "clicked",
          G_CALLBACK (insertEntryText), (gpointer) chat);
    g_signal_connect (G_OBJECT (button), "activate",
          G_CALLBACK (insertEntryText), (gpointer) chat);
          
	gtk_table_attach_defaults (GTK_TABLE (table), button, 6, 8, 4, 5);
	gtk_widget_show(button);
	
	{
	
	chat->textview = gtk_text_view_new ();
    chat->entry = gtk_entry_new ();
	
	scrolled_win = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scrolled_win), chat->textview);
	
	gtk_table_attach_defaults (GTK_TABLE (table), scrolled_win, 4, 8, 0, 3);
	
	gtk_table_attach_defaults (GTK_TABLE (table), chat->entry, 0, 5, 4, 5);
	gtk_widget_show(chat->entry);
	gtk_widget_show(scrolled_win);
	gtk_widget_show(chat->textview);
	}

	gtk_container_add (GTK_CONTAINER (contain), table);
	gtk_container_add (GTK_CONTAINER (window), contain);
	gtk_widget_show(table);
	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->textview));
	mark = gtk_text_buffer_get_insert (buffer);
	gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

	gtk_text_buffer_insert (buffer, &iter, txt, -1);
	
	gtk_widget_set_state(chat->textview, GTK_STATE_INSENSITIVE);
	
	g_timeout_add_seconds(1, waitMessage, NULL);
	
}

/* *
 * отправить свое имя и присоедениться
 * */

void callback_sendName (GtkWidget * widget, gpointer data)
{
	GtkWidget *label;
 	contain11 = gtk_vbox_new(0, 0);
 	const gchar *name;
 	char normName[MAX_NAME_LENGTH];
 	
 	myConnect();
 	
 	name = gtk_entry_get_text(GTK_ENTRY(entry));
 	memset(normName, 0, MAX_NAME_LENGTH);
 	strcpy(normName, name);

	reconnectSend(0, WAIT, 0, normName, sockfd, NON_FLAG);
	nonReconnectRecv(sockfd);
	
	client.id = myFullRecvMessage.data;
	
	course = 1;
 	number_course = 1;
 	
	gtk_widget_destroy (GTK_WIDGET(data));
	
	label = gtk_label_new("Opponent waiting...");
	gtk_box_pack_start(GTK_BOX(contain11), label, TRUE, 10, 0);
	gtk_widget_show(label);
	
	gtk_box_pack_start(GTK_BOX(contain1), contain11, TRUE, 10, 10);
	gtk_widget_show(contain11);
	
	g_timeout_add_seconds(1, waitOpponent, NULL);
}

/* *
 * ф-ия реагирования на событие "Новая игра"
 * */
void callback_newGame (GtkWidget * widget, gpointer data)
{
 	gtk_widget_destroy (GTK_WIDGET(data));
 	
 	GtkWidget *label;
	GtkWidget *button;
 	
 	contain11 = gtk_vbox_new(0, 0);
 	
 	button = gtk_button_new_with_mnemonic("_Connect");
 	entry = gtk_entry_new();
	
	
	label = gtk_label_new("Enter your name:");
	gtk_box_pack_start(GTK_BOX(contain11), label, TRUE, 10, 0);
	gtk_widget_show(label);
	
	g_signal_connect(button, "clicked", G_CALLBACK(callback_sendName), contain11);
	gtk_box_pack_start(GTK_BOX(contain11), entry, TRUE, 10, 0);
	gtk_box_pack_start(GTK_BOX(contain11), button, TRUE, 10, 0);
	gtk_widget_show(entry);
	gtk_widget_show(button);
	
	gtk_box_pack_start(GTK_BOX(contain1), contain11, TRUE, 10, 10);
	gtk_widget_show(contain11);
}

/* *
 * ф-ия реагирования на событие выбора аппонента из списка
 * */
void callback_chooseOpponent (GtkWidget * widget, gpointer data) {
	gtk_widget_destroy (GTK_WIDGET(contain));
	struct my_rows *chooseName;
	chooseName = (struct my_rows*) data;
	
	//printf("%d %s\n", chooseName->id, chooseName->name);
	//может быть переделать, в зависимости от реализации сервера
	reconnectSend(0, MAKE_CHOICE, chooseName->id, chooseName->name, sockfd, NON_FLAG);
	nonReconnectRecv(sockfd);
	if (myFullRecvMessage.code == TOO_LATE)
	{
		//ПО ВОЗМОЖНОСТИ ПЕРЕДЕЛАТЬ
		createEndMenu(NO_GAMES);
	} else if (myFullRecvMessage.code == NORM) {
		client.idGame = myFullRecvMessage.id;
		client.id = myFullRecvMessage.data;
		createPlayground();
	} else {
		//ОШИБКА
	}
}

/* *
 * ф-ия чтения списка игроков
 * */
void readList(int count)
{
	for(int i = 0; i < count; i++) {
		nonReconnectRecv(sockfd);
		names[i].id = myFullRecvMessage.id;
		strcpy(names[i].name, myFullRecvMessage.data2);
		//names[i].name[myFullRecvMessage.data];
		
		nonReconnectSend(0, CHOOSE, 0, "", sockfd);
	}
}
/* *
 * ф-ия реагирования на событие "Присоедениться к игре"
 * */
void callback_joinGame (GtkWidget * widget, gpointer data)
{
	myConnect();
	
 	gtk_widget_destroy (GTK_WIDGET(data));
 	
 	int i, count = 0;
 	GtkWidget *button;
 	GtkWidget *label;
 	contain11 = gtk_vbox_new(0, 0);
 	
 	
 	course = 2;
 	number_course = 2;
	
	reconnectSend(0, CHOOSE, 0, "", sockfd, NON_FLAG);
	
 	label = gtk_label_new("List of games:");
	gtk_box_pack_start(GTK_BOX(contain11), label, TRUE, 10, 0);
	gtk_widget_show(label);
	
	nonReconnectRecv(sockfd);
	
	if (myFullRecvMessage.code == CHOOSE)
	{
		count = myFullRecvMessage.data;
		readList(count);
		for(i = 0; i < count; i++) {
			button = gtk_button_new_with_label (names[i].name);

			gtk_signal_connect (GTK_OBJECT (button), "clicked",
				GTK_SIGNAL_FUNC (callback_chooseOpponent), (gpointer) &(names[i]));
			gtk_box_pack_start(GTK_BOX(contain11), button, TRUE, 10, 0);
			gtk_widget_show (button);
		}
		gtk_box_pack_start(GTK_BOX(contain1), contain11, TRUE, 10, 10);
		gtk_widget_show(contain11);
	} else {
		fprintf(stdout, "ssss %d", myFullRecvMessage.code);
		createEndMenu(NO_GAMES);
	}
}


/* *
 * удаление
 * */
gint delete_event (GtkWidget * widget, GdkEvent * event, gpointer data)
{
	gtk_main_quit ();
	return (FALSE);
}

void myConnect()
{
	if(sockfd != -1) {
		FD_CLR(sockfd, &master);
		connectToServers();
		FD_SET(sockfd, &master);
	} else {
		connectToServers();
		FD_SET(sockfd, &master);
	}
}

void reconnectSend(int id, char code, int data, char* data2, int send_soc, int reconnectCode) {
	int sendSocket = send_soc;
	while (1) {
		if (mySend(id, code, data, data2, sendSocket) == -1) {
			myConnect();
			sendSocket = sockfd;
			if (reconnectSend == I_MOVE)
			{
				mySend(client.idGame, I_PLAY, client.id, "", sendSocket);
			}
			
			if (reconnectSend != NON_FLAG && reconnectSend != I_MOVE)
			{
				mySend(client.idGame, reconnectCode, client.id, "", sendSocket);
			}
		} else {
			break;
		}
	}
}

void nonReconnectSend(int id, char code, int data, char* data2, int send_soc) {
	if (mySend(id, code, data, data2, send_soc) == -1) {
		createEndMenu(FATAL_SEND_ERROR);
	}
}

void connectToServers()
{
	struct listOFServers *ptr;
	int i = -1;
	ptr = root;
	
	while (ptr != NULL)
	{
		i = connectToServer(ptr);
		if (i != -1) {
			root = ptr->next;
			sockfd = i;
			break;
		} else {
			ptr = ptr->next;
			root = ptr;
		}
	}
	if (i == -1)
	{
		createEndMenu(NO_SERVERS);
	}
}
/* *
 * ф-ия соединения с сервером
 * */
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

/* *
 * ф-ия создания главного меню
 * */
void createMainMenu()
{
	if (per1 == 1) {
		gtk_widget_destroy (contain);
	} else {
		per1 = 1;
	}
	
	contain = gtk_vbox_new(0, 0);
	contain1 = gtk_vbox_new(0, 0);
	contain11 = gtk_vbox_new(0, 0);
	contain2 = gtk_vbox_new(0, 0);
	
	//Новая игр	
	{
		button = gtk_button_new_with_label ("New game");

		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (callback_newGame), (gpointer) contain11);
		
		gtk_box_pack_start(GTK_BOX(contain11), button, FALSE, TRUE, 0);
		gtk_widget_show (button);
	}
	//Присоедениться к игре
	{
		button = gtk_button_new_with_label ("Join game");

		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (callback_joinGame), (gpointer) contain11);
		
		gtk_box_pack_start(GTK_BOX(contain11), button, FALSE, TRUE, 0);
		gtk_widget_show (button);
	}
	gtk_box_pack_start(GTK_BOX(contain1), contain11, FALSE, TRUE, 0);
	gtk_widget_show (contain11);
	//Выход
	{
		button = gtk_button_new_with_label ("Quit");

		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (delete_event), GTK_OBJECT (window));
		
		gtk_box_pack_start(GTK_BOX(contain2), button, FALSE, TRUE, 10);
		gtk_widget_show (button);
	}
	gtk_box_pack_start(GTK_BOX(contain), contain1, FALSE, TRUE, 10);
	gtk_box_pack_start(GTK_BOX(contain), contain2, FALSE, TRUE, 10);
	gtk_widget_show (contain1);
	gtk_widget_show (contain2);
	
	gtk_container_add (GTK_CONTAINER(window), contain);
	
	
	gtk_widget_show (contain);
	gtk_widget_show (window);
	
}

int main (int argc, char *argv[])
{
	srand(time(NULL));
	root = NULL;
	
	root = addNewServer(root, "127.0.0.1", "7784");
	root = addNewServer(root, "127.0.0.1", "7783");
	root = addNewServer(root, "127.0.0.1", "7782");
	root = addNewServer(root, "127.0.0.1", "7781");
	
	gtk_init (&argc, &argv);
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	
	gtk_window_set_title (GTK_WINDOW (window), "Krestiki noliki");
	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
		GTK_SIGNAL_FUNC (delete_event), NULL);
	gtk_container_set_border_width (GTK_CONTAINER (window), 20);
	gtk_window_set_default_size(GTK_WINDOW(window), 600, 450);
	
	createMainMenu();
	
	gtk_main ();
	return 0;
}
