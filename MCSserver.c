#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include "ChatServer.h"
#include "chatlib.h"


struct member
{
	char nick [8];
	char addr [INET6_ADDRSTRLEN];
	int sock;
	State stat;
	life_cycle life;
	memberptr next;
};

struct thread_args
{
	int sock;
	int thread_number;
	struct member *head;
};

struct packet_info
{
	int size;
	unsigned char type;
};

struct thread_collector
{
	int collect;
	thread_collectorptr next;
};

int member_quest = 0;
int thread_count = 0;
pthread_t thread [BACKLOG];
struct thread_collector head_collector;
pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t member_lock = PTHREAD_MUTEX_INITIALIZER;


memberptr create_member (char *nick, char *addr, int sock)
{
	memberptr ptr;
	
	ptr = malloc(sizeof (struct member));
	if (ptr) {
		ptr->sock = sock;
		strcpy(ptr->addr, addr);
		strcpy(ptr->nick, nick);
		ptr->stat = offline;
		ptr->life = dead;
		ptr->next = NULL;
	} else {
		printf("error at alocating member\n");
		return NULL;
	}
	
	return ptr;
}

memberptr create_head_member ()
{
	memberptr ptr;
	
	ptr = malloc(sizeof (struct member));
	if (ptr) {
		memset(ptr, 0, sizeof (struct member));
		ptr->next = NULL;
	} else {
		printf("error at alocating head of member\n");
		return NULL;
	}
	
	return ptr;
}

thread_argsptr create_thread_args (memberptr member_head, int sock)
{
	thread_argsptr ptr;
	
	ptr = malloc(sizeof (struct thread_args));
	if (ptr) {
		ptr->head = member_head;
		ptr->sock = sock;
		ptr->thread_number = thread_count;
	} else {
		printf("error at alocating thread_args\n");
		return NULL;
	}
	
	return ptr;
}

packet_infoptr create_packet_info (unsigned char type)
{
	packet_infoptr ptr;
	
	ptr = malloc(sizeof (struct packet_info));
	if (ptr) {
		ptr->type = type;
	} else {
		printf("error at alocating packet_info\n");
		return NULL;
	}
	
	
	return ptr;
}

thread_collectorptr create_thread_collector (int collect)
{
	thread_collectorptr current;
	
	current = &head_collector;
	while (current->next != NULL) 
		current = current->next;
	current->next = malloc(sizeof (struct thread_collector));
	current->next->collect = collect;
	current->next->next = NULL;

	return current->next;
}

void destroy_member (memberptr previous, memberptr target)
{
	previous->next = target->next;
	free(target);
}

void destroy_head_member (memberptr target)
{
	free(target);
}

void destroy_thread_args (thread_argsptr target)
{
	free(target);
}

void destroy_packet_info (packet_infoptr target)
{
	free(target);
}

void destroy_thread_collector (thread_collectorptr target)
{
	free(target);
}

int use_thread_collector ()
{
	int using;
	thread_collectorptr sucessor;
	
	if (head_collector.next->next != NULL) {
		sucessor = head_collector.next->next;
		using = head_collector.next->collect;
		destroy_thread_collector(head_collector.next);
		head_collector.next = sucessor;
	} else {
		using = head_collector.next->collect;
		destroy_thread_collector(head_collector.next);
		head_collector.next = NULL;
	}

	return using;
}
//add member to linkedlist with known nick
memberptr add_nick (int sock, memberptr head, char *nick)
{
	memberptr current;
	struct sockaddr_storage addr;
	socklen_t addr_len = sizeof(addr);
	char ipstr [INET6_ADDRSTRLEN];
	char name [8];
	int guest = 0;
	
	current = head;
	while (current->next != NULL) {
		current = current->next;
		if (strcmp(current->nick, nick) == 0) {
			sprintf(name, "guest%d", ++member_quest);
			guest = 1;
		}
	}
	if (!guest)
		strcpy(name, nick);
	getpeername(sock, (struct sockaddr*) &addr, &addr_len);
	if (addr.ss_family == AF_INET) {
		struct sockaddr_in *s = (struct sockaddr_in *)&addr;
		inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
	} else {
	struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
    inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
	}
	pthread_mutex_lock(&member_lock);
	current->next = create_member(name, ipstr, sock);
	pthread_mutex_unlock(&member_lock);
		
	return current->next;
}

int total_member (memberptr head)
{
	int counter = 0;
	
	memberptr current = head;
	while (current->next != NULL) {
		current = current->next;
		counter++;
	}
	return counter;
}

void delete_nick (memberptr head, char *nick)
{
	memberptr tmp;
	
	memberptr current = head;
	while (current->next != NULL) {
		tmp = current;
		current = current->next;
		if (strcmp(current->nick, nick) == 0) {
			if (!strncmp(nick, "guest", 5))
				member_quest--;
			pthread_mutex_lock(&member_lock);
			destroy_member(tmp, current);
			pthread_mutex_unlock(&member_lock);
			break;
		}
	}
}

void send_member_info (int amount, memberptr head, int sock)
{
	int i;
	memberptr current;

	current = head;
	for (i = 0; i < amount; i++) {
		current = current->next;
		kirim(sock, current->nick, sizeof (char [MAX_NICK]));
	}
	printf("send member end\n");
}

void notif_join_user (memberptr head, packet_infoptr pack, char *nick)
{
	memberptr current;

	current = head;
	while (current->next) {
		current = current->next;
		if (current->stat == online) {
			kirim(current->sock, pack, sizeof (struct packet_info));
			kirim(current->sock, nick, sizeof (char [MAX_NICK]));
		}
	}
}

void notif_quit_user (memberptr head, char *nick)
{
	memberptr current;
	struct packet_info pack;
	
	current = head;
	pack.type = MEMQUIT_PACKET;
	pack.size = MAX_NICK;
	while (current->next) {
		current = current->next;
		if (current->stat == online) {
			kirim(current->sock, &pack, sizeof (struct packet_info));
			kirim(current->sock, nick, sizeof (char [MAX_NICK]));
		}
	}
}

void main_member_cycle (int sock, memberptr head, memberptr this_member)
{
	int amount;
	unsigned char chk_rlmsg;
	unsigned char chk_pkt;
	int nick_len;
	char *real_msg;
	char *packet_send;
	struct packet_info pack;
	memberptr current;
//first, we will send announce for joining the chat
	pack.type = MEMJOIN_PACKET;
	pack.size = sizeof (char [MAX_NICK]);
	notif_join_user(head, &pack, this_member->nick);
	pack.type = CHAT_PACKET;
	this_member->stat = online;
	this_member->life= alive;
//infinite loop that will receive all chat message from socket
	while (1) {
		amount = 0;
		if (this_member->life == dead) {
			this_member->stat = offline;
			printf("member is dead\n");
			break;
		}
		chk_rlmsg = 0;
		chk_pkt = 0;
		if (terima(sock, &amount, sizeof (int)) == EPIPE) {
			this_member->life = dead;
			continue;
		}
		if (amount > 120)
			amount = 120;
		real_msg = malloc(sizeof (char [amount]));
		chk_rlmsg = 1;
		if (terima(sock, real_msg, sizeof (char [amount])) == EPIPE) {
			this_member->life = dead;
			continue;
		}
		*(real_msg + amount) = '\0';
		nick_len = strlen(this_member->nick);
		pack.size = nick_len + amount + 1;
		packet_send = malloc(sizeof (char [pack.size]));
		chk_pkt = 1;
		sprintf(packet_send, "%s\n%s", this_member->nick, real_msg);
//setup packet with '/n' as a pad
		current = head;
		while (current->next != NULL) {
			current = current->next;
			if (*real_msg == 0) { //to avoid endless infinite loop, reason still dont know
				this_member->life = dead;
				break;
			}
			if (current->stat == online) {
				kirim(current->sock, &pack, sizeof (struct packet_info));
				kirim(current->sock, packet_send, sizeof (char [pack.size]));
			}
		}
		if (chk_rlmsg)
			free(real_msg);
		if (chk_pkt)
			free(packet_send);
	}
}

void* member_cycle (void *args)
{
	int total_req;
	char nick [MAX_NICK];
	memberptr this_member;
	thread_argsptr arg;

	arg = (thread_argsptr) args;
	terima(arg->sock, nick, sizeof nick); //accept nick request
	this_member = add_nick(arg->sock, arg->head, nick); //adding nick to linkedlist
	total_req = total_member(arg->head); //count total member in linkedlist without head
	kirim(arg->sock, &total_req, sizeof total_req); //send total count to client
	send_member_info(total_req, arg->head, arg->sock); //send member from linkedlist as much as requested
	kirim(arg->sock, this_member->nick, sizeof nick);
	main_member_cycle(arg->sock, arg->head, this_member); //real action is here
	notif_quit_user(arg->head, this_member->nick); //annouce we will qit from the chat
	delete_nick(arg->head, this_member->nick);
	close(arg->sock);
	//collecting thread number
	if (arg->thread_number < thread_count - 1)
		create_thread_collector(arg->thread_number);
	else {
		pthread_mutex_lock(&counter_lock);
		thread_count -= 1;
		pthread_mutex_unlock(&counter_lock);
	}
	destroy_thread_args(arg);
}


int main () 
{
	void *result;
	memberptr head;
	thread_argsptr tmp;
	struct addrinfo res, *iter, *p;
	struct sockaddr_storage addr_accept;
	socklen_t addr_accept_len;
	int error, sock, sockaccept, reuse = 1;
	int collect = 0;

	head_collector.next = NULL;
	head = create_head_member();
	memset(&res, 0, sizeof res);
//ordinary session for tcp socket
	res.ai_family = AF_INET;
	res.ai_socktype = SOCK_STREAM; // TCP stream sockets
	res.ai_flags = AI_PASSIVE;
	if ((error = getaddrinfo(NULL, "5555", &res, &iter)) != 0) {
    		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(error));
    		return error;
	}
	if ((sock = socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol)) < 0) {
		fprintf(stderr, "socket error: %s\n", gai_strerror(error));
		return sock;
	}
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(int)) == -1)
		fprintf(stderr, "cannot reuse socket error: %s\n", gai_strerror(sockaccept));
	if ((error = bind(sock, iter->ai_addr, iter->ai_addrlen)) < 0) {
		fprintf(stderr, "bind error: %s\n", gai_strerror(error));
    		return error;
	}
	if (listen(sock, BACKLOG) == -1)
		return -1;
//end
	while (1) {
		sockaccept = accept(sock, (struct sockaddr *) &addr_accept, &addr_accept_len);
		if (sockaccept < 0) {
			fprintf(stderr, "accepting connection error: %s\n", gai_strerror(sockaccept));
    			return sockaccept;
		}
		if (thread_count > 100) {
			printf("thread full\n");
			return -1;
		}
		tmp = create_thread_args(head, sockaccept);
		if (head_collector.next != NULL) {
			collect = use_thread_collector();
			if (pthread_create(&(thread [collect]), NULL, member_cycle, tmp) == -1) {
				printf("thread collector error\n");
				return -1;
			}
		} else {
			if (pthread_create(&(thread [thread_count]), NULL, member_cycle, tmp) == -1) {
				printf("error at creating thread, try again");
				if (pthread_create(&(thread [thread_count + 1]), NULL, member_cycle, tmp) == -1) {
					printf("error at creating thread, exit");
					return -1;
				}
			}
			pthread_mutex_lock(&counter_lock);
			thread_count += 1;
			pthread_mutex_unlock(&counter_lock);
		}
	}
	return 0;
}
