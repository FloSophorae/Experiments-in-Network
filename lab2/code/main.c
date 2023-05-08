#include "stdio.h"
#include "stdlib.h"

#include "netinet/in.h"
#include "sys/socket.h"
#include "string.h"
#include "arpa/inet.h"

#include "packet.h"

//server ip: 47.105.85.28


const char* WEATHER_MESSAGE[9] ={
    "overcast",         //0
    "sunny",            //1
    "cloudy",           //2
    "rain",             //3
    "fog",              //4
    "rainstorm",        //5
    "thunderstorm",     //6
    "breeze",           //7
    "Please guess: ZmxhZ3tzYW5kX3N0MHJtfQ=="    //8
};


void hello(){
    //printf("\033c");
    printf("Welcome to NJUCS Weather Forecast Demo Program!\n");
    printf("Please input City Name in Chinese pinyin(e.g. nanjing or beijing)\n");
    printf("(c)cls, (#)exit\n");
}

void detail(){
    printf("\033c");
    printf("Please enter the given number to query\n");
    printf("1. today\n");
    printf("2. three days from today\n");
    printf("3. custom day by yourself\n");
    printf("(r)back, (c)cls, (#)exit\n");
    printf("===========================================================\n");
}


int readn(int fd, char *bp, size_t len){
	int cnt;
	int rc;
	cnt = len;
	while (cnt > 0){
		rc = recv(fd, bp, cnt, 0);
		if (rc < 0)				/* read error? */
		{
			//if ( errno == EINTR )	/* interrupted? */
			//	continue;			/* restart the read */
			return -1;				/* return error */
		}
		if ( rc == 0 )				/* EOF? */
			return len - cnt;		/* return short count */
		bp += rc;
		cnt -= rc;
	}
	return len;
}

int getdayseq(int* dayseq){
    char line[10];
    scanf("%s", line);
    int len = strlen(line);
    if (len != 1) return 0;
    else if (line[0] != '1' && line[0] != '2' && line[0] != '3' && line[0] != '4'
            && line[0] != '5' && line[0] != '6' && line[0] != '7' && line[0] != '8'
            && line[0] != '9'){
                return 0;
    }
    else {
        *dayseq = (line[0] - '0');
        return 1;
    }
}

int getinstr(char* instr){
    char line[10];
    scanf("%s", line);
    int len = strlen(line);
    if (len != 1) return 0;
    else if (line[0] != '1' && line[0] != '2' && line[0] != '3' 
            && line[0] != 'r' && line[0] != 'c' && line[0] != '#'){
        return 0;
    }
    else {
        *instr = line[0];
        return 1;
    }
}


int main(){
    int sockfd;
    struct sockaddr_in serveraddr;
    char ipdstaddr[] = "47.105.85.28";
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(4321);
    inet_pton(AF_INET, ipdstaddr, &serveraddr.sin_addr);
    int connect_rt = connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    
    if (connect_rt < 0){
        //perror(1, errno, "connect failed");
    }
    printf("\033c");
    hello();
    char name_line[NAME_SIZE+1];
    while (1){
        scanf("%s", name_line);
        int nameline_len = strlen(name_line);
        if (nameline_len == 1 && name_line[0] == 'c'){
            printf("\033c");
            hello();
            continue;
        }
        else if (nameline_len == 1 && name_line[0] == '#'){
            return 0;
        }
        data_sdpkt sdpkt;
        bzero(&sdpkt, sizeof(sdpkt));
        sdpkt.type = htons(0x0100);
        strncpy(sdpkt.name, name_line, nameline_len+1);
        int send_rt = send(sockfd, &sdpkt, sizeof(sdpkt), 0);
        if (send_rt < 0){
            perror("send failed");
        }
        data_rvpkt rvpkt;
        int rvpkt_len = 127;
        int recv_rt = readn(sockfd, (char*)&rvpkt, rvpkt_len);
        //recv error?
        if (htons(rvpkt.type) == 0x0200){ //0x0200
            printf("Sorry, Server don't have weather information for %s!\n", rvpkt.name);
            hello();
        }
        else if (htons(rvpkt.type) == 0x0100){ //0x0100
            detail();
            
            while (1){ //need to modify
                char instr;
                int legal = getinstr(&instr);
                if (legal == 0){
                    printf("Input error!\n");
                    continue;
                }
                else if (instr == 'r') {
                    printf("\033c");
                    hello();
                    break; 
                }
                else if (instr == '#') return 0;
                else if (instr == 'c'){
                    printf("\033c");
                    detail();
                    continue;
                }
                else if (instr == '1'){
                    data_sdpkt wtsdpkt; //weather information packet
                    bzero(&wtsdpkt, sizeof(wtsdpkt));
                    wtsdpkt.type = htons(0x0201);
                    strncpy(wtsdpkt.name, name_line, nameline_len+1);
                    wtsdpkt.number = 1;
                    send_rt = send(sockfd, &wtsdpkt, sizeof(wtsdpkt), 0);
                    if (send_rt < 0){
                        perror("send failed");
                    }
                    data_rvpkt wtrvpkt;
                    bzero(&wtrvpkt, sizeof(wtrvpkt));
                    recv_rt = readn(sockfd, (char*)&wtrvpkt, rvpkt_len);
                    //recv error?
                    if (htons(wtrvpkt.type) == 0x0341){
                        printf("City: %s\tToday is: %d/%d/%d\tWeather information is as follows:\n", wtrvpkt.name, ntohs(wtrvpkt.today.year), (int)wtrvpkt.today.month, (int)wtrvpkt.today.day);
                        printf("Today's Weather is:%s;\tTemp:%d\n", WEATHER_MESSAGE[wtrvpkt.wtarr[0].weather], wtrvpkt.wtarr[0].temperature);
                        //这里需不需要考虑一下daynumber不是1的情况
                    }
                }
                else if (instr == '2'){
                    data_sdpkt wtsdpkt; //weather information packet
                    bzero(&wtsdpkt, sizeof(wtsdpkt));
                    wtsdpkt.type = htons(0x0202);
                    strncpy(wtsdpkt.name, name_line, nameline_len+1);
                    wtsdpkt.number = 3;
                    send_rt = send(sockfd, &wtsdpkt, sizeof(wtsdpkt), 0);
                    if (send_rt < 0){
                        perror("send failed");
                    }
                    data_rvpkt wtrvpkt;
                    bzero(&wtrvpkt, sizeof(wtrvpkt));
                    recv_rt = readn(sockfd, (char*)&wtrvpkt, rvpkt_len);
                    if (htons(wtrvpkt.type) == 0x0342){
                        printf("City: %s\tToday is: %d/%d/%d\tWeather information is as follows:\n", wtrvpkt.name, ntohs(wtrvpkt.today.year), (int)wtrvpkt.today.month, (int)wtrvpkt.today.day);
                        int daynumber = (int)wtrvpkt.daynumber;
                        for (int i = 1; i <= daynumber; i++){
                            printf("The %dth day's Weather is:%s;\tTemp:%d\n", i, WEATHER_MESSAGE[wtrvpkt.wtarr[i-1].weather], wtrvpkt.wtarr[i-1].temperature);
                        }
                    }
                }
                else if (instr == '3'){
                    data_sdpkt wtsdpkt; //weather information packet
                    bzero(&wtsdpkt, sizeof(wtsdpkt));
                    wtsdpkt.type = htons(0x0201);
                    strncpy(wtsdpkt.name, name_line, nameline_len+1);
                    int dayseq;
                    printf("Please enter the day number(below 10, e.g. 1 means today):");
                    while (getdayseq(&dayseq) == 0) {
                        printf("Input error\n");
                        printf("Please enter the day number(below 10, e.g. 1 means today):");
                    }
                    if (dayseq == 9) {
                        printf("Sorry, no given day's weather information for city %s!\n", name_line);
                        continue;
                    }
                    wtsdpkt.number = dayseq;
                    send_rt = send(sockfd, &wtsdpkt, sizeof(wtsdpkt), 0);
                    if (send_rt < 0){
                        perror("send failed");
                    }
                    data_rvpkt wtrvpkt;
                    bzero(&wtrvpkt, sizeof(wtrvpkt));
                    recv_rt = readn(sockfd, (char*)&wtrvpkt, rvpkt_len);
                    if (htons(wtrvpkt.type) == 0x0341){
                        printf("City: %s\tToday is: %d/%d/%d\tWeather information is as follows:\n", wtrvpkt.name, ntohs(wtrvpkt.today.year), wtrvpkt.today.month, wtrvpkt.today.day);
                        int daynumber = wtrvpkt.daynumber;
                        printf("The %dth day's Weather is:%s;\tTemp:%d\n", daynumber, WEATHER_MESSAGE[wtrvpkt.wtarr[0].weather], wtrvpkt.wtarr[0].temperature);
                    }
                }
            }
        }
        
    }

}