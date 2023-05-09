#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "stcp_client.h"
#include "string.h"
#include "unistd.h"

/*面向应用层的接口*/


client_tcb_t* tcb[MAX_TRANSPORT_CONNECTIONS];

int real_tcp_sockfd;

//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// stcp客户端初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

client_tcb_t* gettcb(unsigned int client_port){
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    if (tcb[i] != NULL && tcb[i]->client_portNum == client_port){
      return tcb[i];
    }
  }
  return NULL;
}



void stcp_client_init(int conn) {
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    tcb[i] = NULL;
  }
  real_tcp_sockfd = conn;
  pthread_t seghandler_thread;
  pthread_create(&seghandler_thread, NULL, seghandler, NULL); // conn is the sockfd of TCP
}

// 创建一个客户端TCB条目, 返回套接字描述符
//
// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_sock(unsigned int client_port) {
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    if (tcb[i] != NULL && tcb[i]->client_portNum == client_port){
      return -1;
    }
  }
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    if (tcb[i] == NULL){
      tcb[i] = (client_tcb_t*)malloc(sizeof(client_tcb_t));
      tcb[i]->client_portNum = client_port;
      tcb[i]->state = CLOSED;
      return i;
    }
  }
  return -1;
}

// 连接STCP服务器
//
// 这个函数用于连接服务器. 它以套接字ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_connect(int sockfd, unsigned int server_port) {
  client_tcb_t* tb = tcb[sockfd];
  if (tb == NULL) return -1;
  if (tb->state == CLOSED){
    tb->server_portNum = server_port;
    tb->state = SYNSENT;
    seg_t synseg;
    bzero(&synseg, sizeof(synseg));
    synseg.header.src_port = tb->client_portNum;
    synseg.header.dest_port = server_port;
    synseg.header.type = SYN;
    int syn_rt = sip_sendseg(real_tcp_sockfd, &synseg);
    printf("\033[32m[INFO]\033[0m Client: SYN sent!\n");
    printf("\033[34m[STATE]\033[0m Client: SYNSENT\n");
    int retry = SYN_MAX_RETRY;
    while (retry > 0){
      select(real_tcp_sockfd+1,0,0,0, &(struct timeval){.tv_usec = SYN_TIMEOUT/1000});
      if (tb->state == CONNECTED){
        //printf("stcp-client-connect: SYN ACK!\n");
        return 1;
      }
      else {
        int resend_rt = sip_sendseg(real_tcp_sockfd, &synseg);
        printf("\033[32m[INFO]\033[0m Client: SYN resent!\n");
        retry--;
      }
    }
    //retry times exceed
    //printf("stcp-client-connect: RETRY EXCEED!\n");
    printf("\033[31m[ERROR]\033[0m Client: SYN Retry exceed!\n");
    tb->state = CLOSED;
    printf("\033[34m[STATE]\033[0m Client: CLOSED\n");
    return -1;
  }
  return -1;
}

// 发送数据给STCP服务器
//
// 这个函数发送数据给STCP服务器. 你不需要在本实验中实现它。
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_send(int sockfd, void* data, unsigned int length) {
	return 1;
}

// 断开到STCP服务器的连接
//
// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN segment给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_disconnect(int sockfd) {
  client_tcb_t* tb = tcb[sockfd];
  if (tb == NULL) return -1;
  if (tb->state == CONNECTED){
    seg_t finseg;
    bzero(&finseg, sizeof(finseg));
    finseg.header.src_port = tb->client_portNum;
    finseg.header.dest_port = tb->server_portNum;
    finseg.header.type = FIN;
    int fin_rt = sip_sendseg(real_tcp_sockfd, &finseg);
    printf("\033[32m[INFO]\033[0m Client: FIN sent!\n");
    tb->state = FINWAIT;
    printf("\033[34m[STATE]\033[0m Client: FINWAIT\n");
    int retry = FIN_MAX_RETRY;
    while (retry > 0){
      select(real_tcp_sockfd+1,0,0,0, &(struct timeval){.tv_usec = FIN_TIMEOUT/1000});
      if (tb->state == CLOSED){
        return 1;
      }
      else {
        int resend_rt = sip_sendseg(real_tcp_sockfd, &finseg);
        retry--;
        printf("\033[32m[INFO]\033[0m Client: FIN resent!\n");
      }
    }
    //retry times exceed
    printf("\033[31m[ERROR]\033[0m Client: FIN retry exceed!\n");
    tb->state = CLOSED;
    printf("\033[34m[STATE]\033[0m Client: CLOSED\n");
    return -1;
  }
  return -1;
}

// 关闭STCP客户
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_close(int sockfd) {
	client_tcb_t* tb = tcb[sockfd];
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
// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明重叠网络连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void *seghandler(void* arg) {
  while (1){
    seg_t recvseg;
    bzero(&recvseg, sizeof(recvseg));
    //select(real_tcp_sockfd+1,0,0,0,NULL);
    int recv_rt = sip_recvseg(real_tcp_sockfd, &recvseg);
    if (recv_rt > 0){
      client_tcb_t* tb = gettcb(recvseg.header.dest_port);
      if (tb != NULL){
        if (tb->state == SYNSENT && recvseg.header.type == SYNACK){
          tb->state = CONNECTED;
          printf("\033[32m[INFO]\033[0m Client: receive SYNACK!\n");
          printf("\033[34m[STATE]\033[0m Client: CONNECTED\n");
        }
        else if (tb->state == FINWAIT && recvseg.header.type == FINACK){
          tb->state = CLOSED;
          printf("\033[32m[INFO]\033[0m Client: receive FINACK!\n");
          printf("\033[34m[STATE]\033[0m Client: CLOSED\n");
        }
      }
    }
  }
  pthread_exit(NULL);
}



