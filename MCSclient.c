#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include "chatlib.h"

#define WINLOGX 200
#define WINLOGY 100
#define POPY 800
#define POPX 1000
#define MAX_NICK 8

#define CHAT_PACKET 1 //type of packet that server will send to alive member
#define MEMQUIT_PACKET 2
#define MEMJOIN_PACKET 3

int state_mode = 0;
pthread_t loop_rec; 
pthread_attr_t attrib; // thread attribute

enum
{
	LIST_ITEM = 0,
	N_COLUMNS
};

struct data_send
{
	GtkWidget *wid;
	GtkEntryBuffer *nick;
	GtkEntryBuffer *addr;
	GtkEntryBuffer *port;
};

struct chat_send
{
	int sock;
	GtkWidget *ChatTextWid;
};

struct wait_recv
{
	int sock;
	GtkWidget *view;
	GtkWidget *list;
	char *nick;
};

struct packet_info
{
	int size;
	unsigned char type;
};

int establish (char *addr, char *port)
{
	struct addrinfo res, *iter;
	int error, sock, con;

	if ((error = getaddrinfo(addr, port, &res, &iter)) != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(error));
		return -1;
	}
	if ((sock = socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol)) < 0) {
		fprintf(stderr, "socket error: %s\n", gai_strerror(sock));
		return -1;
	}
	if ((con = connect(sock, iter->ai_addr, iter->ai_addrlen)) < 0) {
		fprintf(stderr, "try to connect error: %s\n", gai_strerror(con));
		return -1;
	}
	return sock;
}

void nick_from_packet (char *packet, char *nick_container, char *chat_container)
{
	int len = strcspn(packet, "\n");
	strncpy(nick_container, packet, len);
	*(nick_container + len) = '\0';
	strcpy(chat_container, packet + len + 1);
}

static void append_chat (GtkWidget *textview, gchar *text)
{
	GtkTextBuffer *tbuffer;
	GtkTextIter ei;

	tbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_get_end_iter(tbuffer, &ei);
	gtk_text_buffer_insert(tbuffer, &ei, text, -1);
}

void extest ()
{
	exit(0);
}

static void remove_from_list (GtkWidget *list, char *name)
{
	GtkTreeModel *model;
	GtkListStore *store;
	gchar* citer;
	GtkTreeIter iter;
	
	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(list));
	gtk_tree_model_get_iter_first(model, &iter);
	
	do {
		gtk_tree_model_get(model, &iter, 0, &citer, -1);
		if (!strcmp((char*) citer, name)) {
			gtk_list_store_remove(store, &iter);
			break;
		}
	}
	while (gtk_tree_model_iter_next(model, &iter));
}

void send_message (GtkWidget *widget, struct chat_send *ChatData)
{
	GtkEntryBuffer *ChatBuffer;
	const gchar *ChatText;
	int len;
	
	ChatBuffer = gtk_entry_get_buffer (GTK_ENTRY (ChatData->ChatTextWid));
	ChatText = gtk_entry_buffer_get_text(ChatBuffer);
	len = strlen(ChatText);
	kirim(ChatData->sock, &len, sizeof (int));
	kirim(ChatData->sock, (char*) ChatText, sizeof(char [len]));
	gtk_entry_set_text (GTK_ENTRY (ChatData->ChatTextWid), "");
}

static void add_to_list (GtkWidget *list, const gchar *str)
{
	GtkListStore *store;
	GtkTreeIter iter;
	GtkTreeIter itest;
	GtkTreePath *testpath;
	int i = 0;

	store = GTK_LIST_STORE(gtk_tree_view_get_model
		(GTK_TREE_VIEW (list)));

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, LIST_ITEM, str, -1);
}

void* recv_message (void *args)
{
	struct wait_recv *arg = (struct wait_recv*) args;
	struct packet_info pack;
	unsigned char size;
	char packet_rec [128];
	char packet [134];
	char nick [8];
	char ChatText [120];
	
	while (1) {
		memset(packet_rec, 0, sizeof packet_rec);
		memset(packet, 0, sizeof packet);
		memset(nick, 0, sizeof nick);
		terima(arg->sock, &pack, sizeof (struct packet_info));
		if (pack.type == CHAT_PACKET) {
			terima(arg->sock, packet_rec, pack.size);
			nick_from_packet(packet_rec, nick, ChatText);
			memset(packet, 0, sizeof packet);
			sprintf(packet, "<%s> : %s\n", nick, ChatText);
			append_chat(arg->view, packet);
		} else if (pack.type == MEMQUIT_PACKET) {
			terima(arg->sock, nick, sizeof nick);
			remove_from_list(arg->list, nick);
			sprintf(packet, "the %s has been quited, reason : reset by peer\n", nick);
			append_chat(arg->view, packet);
		} else if (pack.type == MEMJOIN_PACKET) {
			terima(arg->sock, nick, pack.size);
			if (strcmp(arg->nick, nick)) {
				add_to_list(arg->list, nick);
				sprintf(packet, "%s joined the chat\n", nick);
				append_chat(arg->view, packet);
			}
		}
	}
}

static void init_list (GtkWidget *list)
{

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkListStore *store;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("List Items",
          renderer, "text", LIST_ITEM, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW (list), column);
	store = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW (list), GTK_TREE_MODEL (store));
	g_object_unref(store);
}



static void winset (GtkWidget *window, int sizex, int sizey,
	const char *title)
{
	gtk_window_set_title (GTK_WINDOW (window), title);
	gtk_window_set_default_size (GTK_WINDOW (window), sizex, sizey);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

}

GtkWidget* popup (GtkWidget *tree, int sock)
{
	struct chat_send *chat = malloc(sizeof (struct chat_send));
	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget *view = gtk_text_view_new();
	GtkWidget *Scrollview = gtk_scrolled_window_new(NULL, NULL);
	GtkWidget *Scrolltree = gtk_scrolled_window_new(NULL, NULL);
	GtkWidget *ChatText = gtk_entry_new();
	GtkWidget *grid = gtk_grid_new();
	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	GtkWidget *send = gtk_button_new_with_label("send");
	GtkWidget *logout = gtk_button_new_with_label("log out");

	chat->sock = sock;
	chat->ChatTextWid = ChatText;
	g_signal_connect(logout, "clicked", G_CALLBACK (extest), NULL);
	g_signal_connect(send, "clicked", G_CALLBACK (send_message), chat);
	g_signal_connect(ChatText, "activate", G_CALLBACK(send_message), chat);
	winset(window, POPX, POPY, "chat");

	//setting GtkTextView
	gtk_widget_set_size_request(view, 400,400);
	gtk_container_add(GTK_CONTAINER (Scrollview), view);
	gtk_container_add(GTK_CONTAINER (Scrolltree), tree);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);

	g_object_set(Scrollview, "width-request", 500, NULL);
	g_object_set(Scrolltree, "width-request", 300, NULL);
	g_object_set(Scrollview, "height-request", 400, NULL);
	g_object_set(Scrollview, "margin", 12, NULL);
	g_object_set(ChatText, "margin", 12, NULL);
	g_object_set(tree, "margin", 12, NULL);
	gtk_grid_set_row_spacing(GTK_GRID (grid), 10);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW (view), GTK_WRAP_CHAR);

	gtk_grid_attach(GTK_GRID (grid), Scrollview, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID (grid), ChatText, 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID (grid), Scrolltree, 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID (grid), send, 1, 1, 1, 1);
	gtk_grid_attach(GTK_GRID (grid), logout, 0, 2, 2, 1);

	gtk_container_add(GTK_CONTAINER (window), grid);
	gtk_widget_show_all(window);
	return view;
}

static void connect_server (GtkWidget *widget, struct data_send *dat)
{
	struct wait_recv *arg = malloc(sizeof (struct wait_recv));
	int sock;
	int total;
	int i;
	char *name;
	GtkWidget *tree = gtk_tree_view_new();
	GtkWidget *view;
	const gchar *nick;
	const gchar *addr;
	const gchar *port;
	char welcome_text [20];
	
	gtk_widget_set_sensitive(dat->wid, FALSE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW (tree), FALSE);
	init_list(tree);
	nick = gtk_entry_buffer_get_text(dat->nick);
	addr = gtk_entry_buffer_get_text(dat->addr);
	port = gtk_entry_buffer_get_text(dat->port);
	name = malloc(sizeof (char [MAX_NICK]));
	sock = establish((char*) addr, (char*) port);
	if (sock == -1) {
		g_print("socket error\n");
		return;
	}
	kirim(sock, (char*) nick, sizeof (char [MAX_NICK]));
	terima(sock, &total, sizeof (int));
	for (i = 0; i < total; i++) {
		memset(name, 0, sizeof (char [MAX_NICK]));
		terima(sock, name, sizeof (char [MAX_NICK]));
		*(char*) (name + MAX_NICK) = '\0';
		add_to_list(tree, name);
	}
	memset(name, 0, sizeof (char [MAX_NICK]));
	terima(sock, name, sizeof (char [MAX_NICK]));
	view = popup(tree, sock);
	sprintf(welcome_text, "you are a %s now\n", name);
	append_chat(view, welcome_text);
	arg->sock = sock;
	arg->view = view;
	arg->list = tree;
	arg->nick = name;
	//creating detached thread
	pthread_attr_init(&attrib);
	pthread_attr_setdetachstate(&attrib, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&loop_rec, &attrib, recv_message, arg) == -1) {
		printf("error at creating thread, try again");
	}

}

static void activate (GtkApplication *app, gpointer user_data)
{
	struct data_send *send;
	GtkWidget *window = gtk_application_window_new(app);
	GtkWidget *grid = gtk_grid_new();
	GtkWidget *label = gtk_label_new("nick");
	GtkWidget *TextNick = gtk_entry_new();
	GtkEntryBuffer *TextNick_buffer;
	GtkWidget *AddrLog = gtk_entry_new();
	GtkEntryBuffer *AddrLog_buffer;
	GtkWidget *PortLog = gtk_entry_new();
	GtkEntryBuffer *PortLog_buffer;
	GtkWidget *loginButton = gtk_button_new_with_label("login");

	send = malloc(sizeof (struct data_send));
	gtk_entry_set_text(GTK_ENTRY (TextNick), "nick"); //set for gtk_entry
	TextNick_buffer = gtk_entry_get_buffer (GTK_ENTRY (TextNick));
	gtk_entry_set_text(GTK_ENTRY (AddrLog), "localhost");
	AddrLog_buffer = gtk_entry_get_buffer (GTK_ENTRY (AddrLog));
	gtk_entry_set_text(GTK_ENTRY (PortLog), "5555");
	PortLog_buffer = gtk_entry_get_buffer (GTK_ENTRY (PortLog));

	send->addr = AddrLog_buffer;
	send->nick = TextNick_buffer;
	send->port = PortLog_buffer;
	send->wid = window;
	g_signal_connect(loginButton, "clicked", G_CALLBACK (connect_server), send);

	//grid setting
	gtk_grid_set_row_spacing(GTK_GRID (grid), 5);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 10);

	//window setting
	winset(window, WINLOGX, WINLOGY, "login first");

	gtk_container_add (GTK_CONTAINER (window), grid);
	gtk_grid_attach(GTK_GRID (grid), label, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID (grid), TextNick, 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID (grid), AddrLog, 0, 2, 1, 1);
	gtk_grid_attach(GTK_GRID (grid), PortLog, 1, 2, 1, 1);
	gtk_grid_attach(GTK_GRID (grid), loginButton, 1, 1, 1, 1);

	gtk_widget_show_all (window);

}

int main (int argc, char **argv)
{
	GtkApplication *app;
	int status;

	app = gtk_application_new("ryan.project", G_APPLICATION_FLAGS_NONE);

	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
	status = g_application_run (G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	return status;
}
