//this is interface file

#define BACKLOG 100
#define MAX_NICK 8

#define CHAT_PACKET 1 //type of packet that server will send to alive member and member in online state
#define MEMQUIT_PACKET 2
#define MEMJOIN_PACKET 3



typedef enum
{
   online,
   offline
} State;

typedef enum
{
	alive,
	dead
} life_cycle;

typedef struct member* memberptr;
typedef struct thread_args* thread_argsptr;
typedef struct packet_info* packet_infoptr;
typedef struct thread_collector* thread_collectorptr;

memberptr create_member (char *nick, char *addr, int sock);
memberptr create_head_member ();
thread_argsptr create_thread_args (memberptr member_head, int sock);
packet_infoptr create_packet_info (unsigned char type);
thread_collectorptr create_thread_collector (int collect);

void delete_thread_collector ();
void destroy_member (memberptr previous, memberptr target);
void destroy_head_member (memberptr target);
void destroy_thread_args (thread_argsptr target);
void destroy_packet_info (packet_infoptr target);
void destroy_thread_collector (thread_collectorptr target);

memberptr add_nick (int sock, memberptr head, char *nick);
int total_member (memberptr head);
void delete_nick (memberptr head, char *nick);
void send_member (int amount, memberptr head, int sock);
void notif_join_user (memberptr head, packet_infoptr pack, char *nick);
void notif_quit_user (memberptr head, char *nick);

void main_member_cycle (int sock, memberptr head, memberptr this_member);
void* member_cycle (void *args);
