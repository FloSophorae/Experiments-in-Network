#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "stcp_client.h"
#include "string.h"


client_tcb_t* tcb[MAX_TRANSPORT_CONNECTIONS];

int real_tcp_sockfd;


client_tcb_t* gettcb(unsigned int client_port){
  for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
    if (tcb[i] != NULL && tcb[i]->client_portNum == client_port){
      return tcb[i];
    }
  }
  return NULL;
}


//
//  ?ょ?ょ?ょ?ょ?ょ??ㄑ?ょ–?ょ?ょ?ょ?ょ??窶?幻??ょ???断窶??, ?ょ?ょㄇ?ょ?ょ?窶, ?ょ?ょ?ょ??ょ???ょ??猭?ょ?ょ?ょ??.
//
//  猔?ょ: ?ょ??ょ?ょㄇ?ょ?ょ?, ?ょ?ょ璶?ょ?ょFSM?ょ?ょ??狽窶??, ?ょ?ょ???隈witch?ょ?ょ??窶??.
//
//  ヘ?ょ: ?ょ?ょ?ょ?ょ?ょ?ょ篔??窶?ょ?ょ??礥?ょ?ょ.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// ?ょ?ょ?ょ?ょ?????CB?ょ, ?ょ?ょ?ょ?ょヘ?ょ??疺ULL.  
// ?ょ?ょ?ょ?ょヂ?ょ?ょ??CP??ょ?ょ?ょ?ょ?ょconn?ょ﹍?ょ?ょSTCP?ょ????刁鍃?ょ, ??鍃?ょ?ょ?sip_sendseg?ょsip_recvseg?ょ?ょ?ょ?ょ??.
// ?ょ??, ?ょ?ょ?ょ?ょ?ょ?隈eghandler?很??ょ?ょ?ょ?ょ?ょ?诸TCP?ょ. ?幻ル?ょ?ょ?ょseghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void stcp_client_init(int conn)
{
	for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++){
	    tcb[i] = NULL;
  	}
  	real_tcp_sockfd = conn;
  	pthread_t seghandler_thread;
  	pthread_create(&seghandler_thread, NULL, seghandler, NULL); // conn is the sockfd of TCP
  	return;
}

// ?ょ?ょ?ょ?ょ?ょ??ょ??CB?ょ?ょ?处窶?ょ?ょNULL?ょヘ, 礛?ょㄏ?ょmalloc()??ょ?ょヘ?ょ?ょ?ょ??窶TCB?ょヘ.
// ?ょTCB?窶?ょ?ょ?刁琿??ょ?ょ﹍?ょ. ?ょ?ょ, TCB state?ょ?ょ?ょ?CLOSED?ょ?幻ル?断狠鍃?ょ?ょ??ょ?ょ?ょ????ょclient_port. 
// TCB?ょ?ょ?ょヘ?ょ?ょ?ょ?ょ??ょ??幻ル?断窶?ょ??ょ?ょID?ょ?ょ?ょ?ょ?ょ?ょ??, ?ょ?ょ??鍃??幻ル?断窶?ょ?ょ. 
// ?ょ??CB?ょ?ょ??ょ?ょヘ?ょ?ょ, ?ょ?ょ?ょ?ょ?ょ??-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_client_sock(unsigned int client_port){
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
			tcb[i]->next_seqNum = 0;
			tcb[i]->bufMutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
			pthread_mutex_init(tcb[i]->bufMutex, NULL);
			tcb[i]->sendBufHead = NULL;
			tcb[i]->sendBufunSent = NULL;
			tcb[i]->sendBufTail = NULL;
			tcb[i]->unAck_segNum = 0;
      		return i;
    	}
  	}
  	return -1;
}

// ?ょ?ょ?ょ?ょ?ょ?ょ?ょ羢?ょ?ょ??. ?ょ?ょ??ょ?ょID?幻??ょ?ょ??狠礥?ょ??ょ?ょ?ょ??. ??ょ?ょID?ょ?ょ?处窶TCB?ょヘ.  
// ?ょ?ょ?ょ?ょ?ょ??CB????ょ?ょ?断礥,  礛?ょㄏ?ょsip_sendseg()?ょ?ょ?ょSYN???ょ?ょ?ょ.  
// ????ょ?ょSYN?ょぇ?ょ, ?ょ?ょ??ょ?ょ?ょ?ょ. ?ょ?ょ?诸YNSEG_TIMEOUT??ょぇ?ょ??ょ?峙窶SYNACK, SYN ??ょ?ょ?篒?. 
// ?ょ?ょ??ょ??, ?幻??ょ1. ?ょ?ょ, ?ょ?ょ?诸YN????ょ?ょ?ょSYN_MAX_RETRY, ?幻ょstate??ょ?ょCLOSED, ?ょ?ょ?ょ-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_client_connect(int sockfd, unsigned int server_port){
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
		synseg.header.seq_num = 0;
    	int syn_rt = sip_sendseg(real_tcp_sockfd, &synseg);
    	printf("\033[32m[INFO]\033[0m Client SYN sent!\n");
    	int retry = SYN_MAX_RETRY;
    	while (retry > 0){
      		select(real_tcp_sockfd+1,0,0,0, &(struct timeval){.tv_usec = SYN_TIMEOUT/1000});
      		if (tb->state == CONNECTED){
        		//printf("stcp-client-connect: SYN ACK!\n");
        		return 1;
      		}
      		else {
        		int resend_rt = sip_sendseg(real_tcp_sockfd, &synseg);
        		printf("\033[32m[INFO]\033[0m Client: SYN Resent!\n");
        		retry--;
      		}
    	}
    	//retry times exceed
    	//printf("stcp-client-connect: RETRY EXCEED!\n");
    	printf("\033[31m[ERROR]\033[0m Client: SYN Retry exceed!\n");
    	tb->state = CLOSED;
    	return -1;
  	}
  	return -1;
}

// ?ょ?ょ?ょ?倍STCP?ょ?ょ?ょ. ?ょ?ょ?ょ???ょ?ょ?惨D?处窶TCB?ょ?窶?ょヘ. 
// 礛?ょ?ょㄏ?ょ??ㄑ?ょ?ょ?倍??ょsegBuf, ?ょ?ょ?ょ?钡窶?ょ?幻ル?ょ?ょ?ょ?ょ?ょ. 
// ?ょ?ょ?ょ?ょ?ょ?ょ??ょ?ょ?ょ?刁?玡??ょ, ?ょ?ょ?sendbuf_timer?ょ?很祘碞ル?ょ?ょ. 
// –?ょSENDBUF_ROLLING_INTERVAL??ょ?耽??ょ?幻ル?ょ?ょ???ょろ?ょ???鍃??ょ?ょ??.
// ?ょ?ょ?ょ?ょ???鍃?ょ??1?ょ?ょ?瑉ル-1. 
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_client_send(int sockfd, void* data, unsigned int length){
	//printf("stcp-client-send 1-1\n");
	client_tcb_t* tb = tcb[sockfd];
	if (tb == NULL) return -1;
	if (tb->state == CONNECTED){
		//printf("stcp-client-send 1-2\n");
		int segNum = length / MAX_SEG_LEN;
		if (length % MAX_SEG_LEN != 0){
			segNum++;
		}
		for (int i = 0; i < segNum; i++){
			//printf("stcp-client-send 1-3\n");
			segBuf_t* segbuf = (segBuf_t*)malloc(sizeof(segBuf_t));
			bzero(segbuf, sizeof(segBuf_t));
			segbuf->seg.header.type = DATA;
			segbuf->seg.header.src_port = tb->client_portNum;
			segbuf->seg.header.dest_port = tb->server_portNum;
			if (length % MAX_SEG_LEN != 0 && i == segNum-1){
				segbuf->seg.header.length = length % MAX_SEG_LEN;
			}
			else {
				segbuf->seg.header.length = MAX_SEG_LEN;
			}
			memcpy(segbuf->seg.data, ((char*)data)+i*MAX_SEG_LEN, segbuf->seg.header.length);
			pthread_mutex_lock(tb->bufMutex);
			segbuf->seg.header.seq_num = tb->next_seqNum;
			tb->next_seqNum += segbuf->seg.header.length;
			//printf("\033[31m[INFO]\033[0m Client: next_seqNum: %d\n", tb->next_seqNum);	
			if (tb->sendBufHead == NULL){
				tb->sendBufHead = segbuf;
				tb->sendBufunSent = segbuf;
				tb->sendBufTail = segbuf;
			}
			else {
				tb->sendBufTail->next = segbuf;
				tb->sendBufTail = segbuf;
				if (tb->sendBufunSent == NULL) {
					tb->sendBufunSent = segbuf;
				}
			}
			pthread_mutex_unlock(tb->bufMutex);
		}
		//send data in sendBuffer
		pthread_mutex_lock(tb->bufMutex);
		while (tb->unAck_segNum < GBN_WINDOW && tb->sendBufunSent != NULL){
			sip_sendseg(real_tcp_sockfd, &tb->sendBufunSent->seg);
			struct timeval curtime;
			gettimeofday(&curtime, NULL);
			tb->sendBufunSent->sentTime = curtime.tv_sec*1000000+curtime.tv_usec;
			if (tb->unAck_segNum == 0){
				//if unAck_seqNum != 0, that indicates sendBuf_timer has been started already?
				pthread_t sendbuftimer;
				pthread_create(&sendbuftimer,NULL, sendBuf_timer, (void*)tb);
			}
			tb->unAck_segNum++;
			if (tb->sendBufunSent != tb->sendBufTail){
				tb->sendBufunSent = tb->sendBufunSent->next;
			}
			else {
				tb->sendBufunSent = NULL;
			}
		}
		pthread_mutex_unlock(tb->bufMutex);
		return 1;
	}
	return -1;
}

// ?ょ?ょ?ょ?ょ?ょ???ょ?ょ?ょ?ょ?ょ?ょ?ょ??. ?ょ?ょ??ょ?ょID?ょ??ょ?ょ?ょ??. ??ょ?ょID?ょ?ょ?处窶TCB?ょ?窶?ょヘ.  
// ?ょ?ょ?ょ?ょ?ょ?帚IN???ょ?ょ?ょ. ????ょFINぇ?ょ, state?ょ??ょ?ょFINWAIT, ?ょ?ょ?ょ?ょ?ょ??ょ.
// ?ょ?ょ?ょ?ょ诌??蓖?玡state??ょ?ょCLOSED, ?ょ?ょ?帚INACK?耽鍃???ょ?ょ. ?ょ?ょ, ?ょ?ょ??ょ?帚IN_MAX_RETRY????ょぇ?ょ,
// state?ょ礛?FINWAIT, state?ょ??ょ?ょCLOSED, ?ょ?ょ?ょ-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_client_disconnect(int sockfd){
	client_tcb_t* tb = tcb[sockfd];
  	if (tb == NULL) return -1;
  	if (tb->state == CONNECTED){
    	seg_t finseg;
    	bzero(&finseg, sizeof(finseg));
    	finseg.header.src_port = tb->client_portNum;
    	finseg.header.dest_port = tb->server_portNum;
    	finseg.header.type = FIN;
    	int fin_rt = sip_sendseg(real_tcp_sockfd, &finseg);
    	printf("\033[32m[INFO]\033[0m Client FIN sent!\n");
    	tb->state = FINWAIT;
    	int retry = FIN_MAX_RETRY;
    	while (retry > 0){
      		select(real_tcp_sockfd+1,0,0,0, &(struct timeval){.tv_usec = FIN_TIMEOUT/1000});
      		if (tb->state == CLOSED){
				pthread_mutex_lock(tb->bufMutex);
				segBuf_t* bufptr = tb->sendBufHead;
				while (bufptr != tb->sendBufTail){
					segBuf_t* temp = bufptr;
					bufptr = bufptr->next;
					free(temp);
				}
				free(tb->sendBufTail);
				tb->sendBufHead = NULL;
				tb->sendBufunSent = NULL;
				tb->sendBufTail = NULL;
				tb->unAck_segNum = 0;
				pthread_mutex_unlock(tb->bufMutex);
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
		pthread_mutex_lock(tb->bufMutex);
		segBuf_t* bufptr = tb->sendBufHead;
		while (bufptr != tb->sendBufTail){
			segBuf_t* temp = bufptr;
			bufptr = bufptr->next;
			free(temp);
		}
		free(tb->sendBufTail);
		tb->sendBufHead = NULL;
		tb->sendBufunSent = NULL;
		tb->sendBufTail = NULL;
		tb->unAck_segNum = 0;
		pthread_mutex_unlock(tb->bufMutex);
    	return -1;
  	}
  	return -1;
}

// ?ょ?ょ?ょ?ょ?ょ??ree()?幻?TCB?ょヘ. ?ょ?ょ?ょ?ょヘ?ょ??疺ULL, ???(?ょ?ょ?ょ谔?ょ??)?ょ?ょ1,
// ア?ょ?(?ょ????ょ???)?ょ?ょ-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_client_close(int sockfd){
	client_tcb_t* tb = tcb[sockfd];
  	if (tb == NULL) return -1;
  	if (tb->state == CLOSED){
    	free(tb->bufMutex);
		free(tb);
    	tb = NULL;
    	return 1;
  	}
  	return -1;
}

// ?ょ?ょ?ょstcp_client_init()?ょ?ょ?ょ?很?. ?ょ?ょ?ょ?ょ?ょ?ょ???ょ?ょ??ょ?ょ??. 
// seghandler?ょ?ょ????ょ?ょ?ょsip_recvseg()?ょ?ょ?ょ碻?ょ. ?ょ?隈ip_recvseg()ア?ょ, ?ょ??ょ?篒窶?ょ?ょ?ょ?ょ?耽?鍃,
// ?很祘ょ?ょゎ. ?ょ?ょSTCP??窶?ょ??ょ?ょ?ょ?ょ?ょ??, ?ょ???ょ????ょ. ?ょ簁??ょ?帚SM?ょ?断ょ?ょ????.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void *seghandler(void* arg){
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
          			printf("\033[32m[INFO]\033[0m Client: SYN ACK!\n");
        		}
        		else if (tb->state == FINWAIT && recvseg.header.type == FINACK){
          			tb->state = CLOSED;
          			printf("\033[32m[INFO]\033[0m Client: FIN ACK!\n");
        		}
				else if (tb->state == CONNECTED && recvseg.header.type == DATAACK && tb->client_portNum == recvseg.header.dest_port){
					//receive DATA ACK
					//printf("\033[32m[INFO]\033[0m Client: DATA ACK!\n");
					pthread_mutex_lock(tb->bufMutex);
					//delete acked seg in sendbuffer
					if (recvseg.header.ack_num > tb->sendBufTail->seg.header.seq_num){
						tb->sendBufTail = NULL;
					}
					segBuf_t* bufptr = tb->sendBufHead;
					while (bufptr != NULL && bufptr->seg.header.seq_num < recvseg.header.ack_num){
						tb->sendBufHead = bufptr->next;
						segBuf_t* temp = bufptr;
						bufptr = bufptr->next;
						free(temp);
						tb->unAck_segNum--;
					}
					//send seg in sendbuffer
					while (tb->unAck_segNum < GBN_WINDOW && tb->sendBufunSent != NULL){
						sip_sendseg(real_tcp_sockfd, &tb->sendBufunSent->seg);
						struct timeval curtime;
						gettimeofday(&curtime, NULL);
						tb->sendBufunSent->sentTime = curtime.tv_sec*1000000+curtime.tv_usec;
						if (tb->unAck_segNum == 0){
							//is unAck_seqNum != 0, that indicates sendBuf_timer has been started already?
							pthread_t sendbuftimer;
							pthread_create(&sendbuftimer, NULL, sendBuf_timer, (void*)tb);
						}
						tb->unAck_segNum++;
						if (tb->sendBufunSent != tb->sendBufTail){
							tb->sendBufunSent = tb->sendBufunSent->next;
						}
						else {
							tb->sendBufunSent = NULL;
						}
					}
					pthread_mutex_unlock(tb->bufMutex);
				}
      		}
   		}
  	}
  	pthread_exit(NULL);
}

// ?ょ?ょ??ょ?ょ?耽??ょ?幻ル?ょ?ょ???ょ?ょ????. ?ょ?ょ?ょ?ょ?ょ?ょ???, ?ょ??ょ?ょ.
// ?ょ??(?ょ玡??ょ - ?ょ?ょ?耽??幻窶ゼ?ょ谔??琿??ょ??ょ) > DATA_TIMEOUT, ?幻??ょ???????.
// ?ょ?ょ?????ょ?ょ?, ?ょ????ょ?ょ?ょ?耽??幻窶ゼ?ょ谔???. ?ょ?ょ?幻ル?ょ?ょ??ょ?, ?ょ?ょ贝?ょ?刁?.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void* sendBuf_timer(void* clienttcb){
	client_tcb_t* tb = (client_tcb_t*)clienttcb;
	while(1) {
		select(0,0,0,0,&(struct timeval){.tv_usec = SENDBUF_POLLING_INTERVAL/1000});
		struct timeval curtime;
		gettimeofday(&curtime,NULL);
		
		//?ょ??nAck_segNum?0, ?ょ????幻ル?ょ?ょ?ょ???, ?断?.
		if(tb->unAck_segNum == 0) {
			pthread_exit(NULL);
		}
		else if(tb->sendBufHead->sentTime > 0 
			&& curtime.tv_sec*1000000+curtime.tv_usec - tb->sendBufHead->sentTime > DATA_TIMEOUT) {
			//timeout
			pthread_mutex_lock(tb->bufMutex);
			segBuf_t* bufptr = tb->sendBufHead;
			for (int i = 0; i < tb->unAck_segNum; i++){
				sip_sendseg(real_tcp_sockfd, &bufptr->seg);
				//&bufptr->seg or bufptr->seg?
				struct timeval resendtime;
				gettimeofday(&resendtime, NULL);
				bufptr->sentTime = resendtime.tv_sec*1000000 + resendtime.tv_usec;
				bufptr = bufptr->next;
			}
			pthread_mutex_unlock(tb->bufMutex);
		}
	}
}
