//文件名: client/app_client.c
//
//描述: 这是客户端应用程序代码. 客户端首先通过在客户端和服务器之间创建TCP连接,启动重叠网络层. 然后它调用stcp_client_init()初始化STCP客户端. 
//它通过两次调用stcp_client_sock()和stcp_client_connect()创建两个套接字并连接到服务器. 经过一段时间后, 客户端调用stcp_client_disconnect()断开到服务器的连接. 
//最后,客户端调用stcp_client_close()关闭套接字. 重叠网络层通过调用son_stop()停止.

//输入: 无

//输出: STCP客户端状态

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common/constants.h"
#include "stcp_client.h"
#include "unistd.h"

//创建两个连接, 一个使用客户端端口号87和服务器端口号88. 另一个使用客户端端口号89和服务器端口号90
#define CLIENTPORT1 87
#define SERVERPORT1 88
#define CLIENTPORT2 89
#define SERVERPORT2 90
//在连接创建后, 等待5秒, 然后关闭连接
#define WAITTIME 5

//这个函数通过在客户和服务器之间创建TCP连接来启动重叠网络层. 它返回TCP套接字描述符, STCP将使用该描述符发送段. 如果TCP连接失败, 返回-1. 
int son_start() {
	int sockfd;
	struct sockaddr_in serveraddr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(4321);
	serveraddr.sin_addr.s_addr = htons(INADDR_ANY);
	int connect_rt = connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (connect_rt < 0){
		return -1;
	}
	else return sockfd;
}

//这个函数通过关闭客户和服务器之间的TCP连接来停止重叠网络层
void son_stop(int son_conn) {
	close(son_conn);
}

int main() {
	//用于丢包率的随机数种子
	srand(time(NULL));

	//启动重叠网络层并获取重叠网络层TCP套接字描述符	
	int son_conn = son_start();
	if(son_conn<0) {
		printf("fail to start overlay network\n");
		exit(1);
	}


	//初始化stcp客户端, 传递TCP套接字描述符给STCP层, STCP将该套接字作为一个全局变量维护
	stcp_client_init(son_conn);

	//在端口87上创建STCP客户端套接字, 并连接到STCP服务器端口88
	int sockfd = stcp_client_sock(CLIENTPORT1);
	if(sockfd<0) {
		printf("fail to create stcp client sock");
		exit(1);
	}
	if(stcp_client_connect(sockfd,SERVERPORT1)<0) {
		printf("fail to connect to stcp server\n");
		exit(1);
	}
	printf("client connect to server, client port:%d, server port %d\n",CLIENTPORT1,SERVERPORT1);
	
	//在端口89上创建STCP客户端套接字, 并连接到STCP服务器端口90
	int sockfd2 = stcp_client_sock(CLIENTPORT2);
	if(sockfd2<0) {
		printf("fail to create stcp client sock");
		exit(1);
	}
	if(stcp_client_connect(sockfd2,SERVERPORT2)<0) {
		printf("fail to connect to stcp server\n");
		exit(1);
	}
	printf("client connect to server, client port:%d, server port %d\n",CLIENTPORT2, SERVERPORT2);

	//等待一段时间, 然后关闭连接
	sleep(WAITTIME);

	if(stcp_client_disconnect(sockfd)<0) {
		printf("fail to disconnect from stcp server\n");
		exit(1);
	}
	if(stcp_client_close(sockfd)<0) {
		printf("failt to close stcp client\n");
		exit(1);
	}
	
	if(stcp_client_disconnect(sockfd2)<0) {
		printf("fail to disconnect from stcp server\n");
		exit(1);
	}
	if(stcp_client_close(sockfd2)<0) {
		printf("failt to close stcp client\n");
		exit(1);
	}

	//停止重叠网络层
	son_stop(son_conn);
}
