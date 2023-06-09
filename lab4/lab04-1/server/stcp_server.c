#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "stcp_server.h"
#include "../common/constants.h"
#include "string.h"

/*面向应用层的接口*/

int real_tcp_sockfd;

server_tcb_t* tcb[MAX_TRANSPORT_CONNECTIONS];


server_tcb_t* gettcb(unsigned int server_port){
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    if (tcb[i] != NULL &&tcb[i]->server_portNum == server_port){
		return tcb[i];
    }
  }
  return NULL;
}

//
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//

// stcp服务器初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量,
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
//

void stcp_server_init(int conn) {
	for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    	tcb[i] = NULL;
  	}
  	real_tcp_sockfd = conn;
  	pthread_t seghandler_thread;
  	pthread_create(&seghandler_thread, NULL, seghandler, NULL); // conn is the sockfd of TCP
}

// 创建服务器套接字
//
// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.

int stcp_server_sock(unsigned int server_port) {
	for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    	if (tcb[i] != NULL && tcb[i]->server_portNum == server_port){
    	  	return -1;
    	}
  	}
	for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
		if (tcb[i] == NULL){
			tcb[i] = (server_tcb_t*)malloc(sizeof(server_tcb_t));
			tcb[i]->server_portNum = server_port;
			tcb[i]->state = CLOSED;
			return i;
		}
	}
	return -1;
}

// 接受来自STCP客户端的连接
//
// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后进入忙等待(busy wait)直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//

int stcp_server_accept(int sockfd) {
	server_tcb_t* tb = tcb[sockfd];
	if (tb == NULL) return -1;
	if (tb->state == CLOSED){
		tb->state = LISTENING;
		printf("\033[34m[STATE]\033[0m Server: Listening!\n");
		
		while (1){
			//select(real_tcp_sockfd+1,0,0,0,NULL);
			//sleep(1);
			if (tb->state == CONNECTED) return 1;
		}
	}
}

// 接收来自STCP客户端的数据
//
// 这个函数接收来自STCP客户端的数据. 你不需要在本实验中实现它.
//
int stcp_server_recv(int sockfd, void* buf, unsigned int length) {
	return 1;
}

// 关闭STCP服务器
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//

int stcp_server_close(int sockfd) {
	server_tcb_t* tb = tcb[sockfd];
	if (tb == NULL) return -1;
	if (tb->state == CLOSED){
		free(tb);
		tb = NULL;
		return 1;
	}
	return -1;
}

// 处理进入段的线程
//
// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明重叠网络连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
//

void *seghandler(void* arg) {
	while (1){
		seg_t recvseg;
		bzero(&recvseg, sizeof(recvseg));
		//select(real_tcp_sockfd+1, 0,0,0, NULL);
		int recv_rt = sip_recvseg(real_tcp_sockfd, &recvseg);
		if (recv_rt > 0){
			server_tcb_t* tb = gettcb(recvseg.header.dest_port);
			if (tb != NULL){
				switch (tb->state){
					case CLOSED:{
						break;
					}
					case LISTENING:{
						if (recvseg.header.type == SYN){
							tb->state = CONNECTED;
							tb->client_portNum = recvseg.header.src_port;
							printf("\033[32m[INFO]\033[0m Server: recvive SYN!\n");
							printf("\033[34m[STATE]\033[0m Server: CONNECTED\n");
							seg_t ackseg;
							bzero(&ackseg, sizeof(ackseg));
							ackseg.header.type = SYNACK;
							ackseg.header.src_port = tb->server_portNum;
							ackseg.header.dest_port = tb->client_portNum;
							int ack_rt = sip_sendseg(real_tcp_sockfd, &ackseg);
						}
						break;
					}
					case CONNECTED:{
						if (recvseg.header.type == SYN){
							tb->state = CONNECTED;
							printf("\033[32m[INFO]\033[0m Server: recvive SYN!\n");
							
							seg_t ackseg;
							bzero(&ackseg, sizeof(ackseg));
							ackseg.header.type = SYNACK;
							ackseg.header.src_port = tb->server_portNum;
							ackseg.header.dest_port = tb->client_portNum;
							int ack_rt = sip_sendseg(real_tcp_sockfd, &ackseg);
						}
						else if (recvseg.header.type == FIN){
							tb->state = CLOSEWAIT;
							printf("\033[32m[INFO]\033[0m Server: recvive FIN!\n");
							printf("\033[34m[STATE]\033[0m Server: CLOSEWAIT\n");
							seg_t ackseg;
							bzero(&ackseg, sizeof(ackseg));
							ackseg.header.type = FINACK;
							ackseg.header.src_port = tb->server_portNum;
							ackseg.header.dest_port = tb->client_portNum;
							int ack_rt = sip_sendseg(real_tcp_sockfd, &ackseg);
							
							//this's wrong?
							select(real_tcp_sockfd+1,0,0,0, &(struct timeval){.tv_usec = FIN_TIMEOUT/1000});
							tb->state = CLOSED;
							printf("\033[34m[STATE]\033[0m Server: CLOSED\n");
						}
						break;
					}
					case CLOSEWAIT:{
						if (recvseg.header.type == FIN){
							tb->state = CLOSEWAIT;
							printf("\033[32m[INFO]\033[0m Server: recvive FIN!\n");
							seg_t ackseg;
							bzero(&ackseg, sizeof(ackseg));
							ackseg.header.type = FINACK;
							ackseg.header.src_port = tb->server_portNum;
							ackseg.header.dest_port = tb->client_portNum;
							int ack_rt = sip_sendseg(real_tcp_sockfd, &ackseg);
							select(real_tcp_sockfd+1,0,0,0, &(struct timeval){.tv_usec = FIN_TIMEOUT/1000});
							tb->state = CLOSED;
							printf("\033[34m[STATE]\033[0m Server: CLOSED\n");
						}
						break;
					}
				}
			}
		}
	}
	pthread_exit(NULL);
}
