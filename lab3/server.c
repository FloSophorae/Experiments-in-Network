//created by hzj
//2023-4-9


#include "packet.h"
#include "stdio.h"
#include "stdlib.h"

#include "pthread.h"

#include "netinet/in.h"
#include "sys/socket.h"
#include "string.h"
#include "arpa/inet.h"




typedef struct UserInfo{
    int connectfd; //to find the connection bewteen server and this user
    User user; //state of user contains whether this UserInfo is used or not
} UserInfo;
#define USERLIST_SIZE 100
UserInfo userlist[USER_NUM];
pthread_mutex_t userlist_mutex;



typedef struct BattleInfo{
    uint8_t used; //0-not used, 1-used, for battlelist
    //uint8_t round; //the number of rounds each side has engaged
    char username1[NAME_SIZE];
    int sockfd1;
    uint8_t blood1;
    char username2[NAME_SIZE];
    int sockfd2;
    uint8_t blood2;
} BattleInfo;
#define BATTLELIST_SIZE 100
BattleInfo battlelist[BATTLELIST_SIZE];
pthread_mutex_t battlelist_mutex;




typedef struct BattlePkt{
    uint8_t used; //0-not used, 1-used, for battlebuffer
    //uint8_t round; //seems not necessary? not, it's useful
    //char srcname[NAME_SIZE];
    //int srcsockfd;
    //char dstname[NAME_SIZE];
    //int dstsockfd;
    PKT pkt;
} BattlePkt;
#define BATTLEBUFFER_SIZE 100
BattlePkt battlebuffer[BATTLEBUFFER_SIZE];
pthread_mutex_t battlebuffer_mutex;

int pktcnt = 0;
pthread_mutex_t pktcnt_mutex;
pthread_cond_t pktcnt_cond;

#define THREAD_NUM 100
pthread_t threads[THREAD_NUM];

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


int battle(uint8_t op1, uint8_t op2){
    //0-tie, 1-op1 wins, 2-opt wins
    if (op1 == 1){
        if (op2 == 1) return 0;
        else if (op2 == 2) return 1;
        else if (op2 == 3) return 2;
    }
    else if (op1 == 2){
        if (op2 == 1) return 2;
        else if (op2 == 2) return 0;
        else if (op2 == 3) return 1;
    }
    else if (op1 == 3){
        if (op2 == 1) return 1;
        else if (op2 == 2) return 2;
        else if (op2 == 3) return 0;
    }
    else return -1;
}



//handle pkt recvived for client
void* recv_handler(void* arg){
    int clientfd = *(int*)arg; //the socketfd connected with client
    while (1){
        PKT recvpkt;
        bzero(&recvpkt, sizeof(recvpkt));
        int recv_rt = readn(clientfd, (char*)&recvpkt, sizeof(recvpkt));
        if (recv_rt < 0){
            //recv error
            break; //??
        }
        switch (htons(recvpkt.type)){
            case 0x0101:{ //user login
                //printf("\033[32m[INFO]\033[0m %s wants to login\n", recvpkt.info.srcusername);
                int nameexist = 0;
                pthread_mutex_lock(&userlist_mutex);
                for (int i = 0; i < USERLIST_SIZE; i++){
                    if (userlist[i].user.state != 0 && !strcmp(userlist[i].user.name, recvpkt.info.srcusername)){
                        nameexist = 1;
                        break;
                    }
                }
                if (nameexist){
                    printf("\033[31m[ERROR]\033[0m Login Error: %s exists\n", recvpkt.info.srcusername);
                    PKT backpkt; //send error information back to client
                    bzero(&backpkt, sizeof(backpkt));
                    backpkt.type = htons(0x0103);
                    strncpy(backpkt.info.srcusername, recvpkt.info.srcusername, NAME_SIZE);
                    int send_rt = send(clientfd, &backpkt, sizeof(backpkt), 0);
                    if (send_rt < 0){
                        //error
                    }
                }
                else {
                    int index = 0; //first-fit to look for vacant position in userlist
                    for (index = 0; index < USERLIST_SIZE; index++){
                        if (userlist[index].user.state == 0){
                            break;
                        }
                    }
                    if (index == USERLIST_SIZE){
                        //there is no space for user to login
                        //too many users are inline at the same time
                        printf("\033[31m[ERROR]\033[0m %s login error, there are too many users online!\n", recvpkt.info.srcusername);
                        PKT sendpkt;
                        bzero(&sendpkt, sizeof(sendpkt));
                        strncpy(sendpkt.info.srcusername, recvpkt.info.srcusername, NAME_SIZE);
                        sendpkt.type = htons(0x0103);
                        int send_rt = send(clientfd, &sendpkt, sizeof(sendpkt), 0);
                        if (send_rt < 0){
                            //error
                        }
                    }
                    else {
                        printf("\033[32m[INFO]\033[0m %s login successfully!\n", recvpkt.info.srcusername);
                        userlist[index].user.state = 1;
                        //userlist[index].user.blood = 3;
                        userlist[index].connectfd = clientfd;
                        strncpy(userlist[index].user.name, recvpkt.info.srcusername, NAME_SIZE);
                        PKT backpkt;
                        bzero(&backpkt, sizeof(backpkt));
                        backpkt.type = htons(0x0102);
                        strncpy(backpkt.info.srcusername, recvpkt.info.srcusername, NAME_SIZE);
                        int send_rt = send(clientfd, &backpkt, sizeof(backpkt), 0);
                        if (send_rt < 0){
                            //error
                        }
                        //inform all the players that there's someone to login
                        //printf("[DEBUG] Now, I will inform all the users inline that %s login.\n", recvpkt.info.srcusername);
                        PKT informpkt;
                        bzero(&informpkt, sizeof(informpkt));
                        informpkt.type = htons(0x0403);
                        strncpy(informpkt.userlist.logusername, recvpkt.info.srcusername, NAME_SIZE);
                        for (int i = 0; i < USERLIST_SIZE; i++){
                            if (i != index && userlist[i].user.state != 0){
                                int inform_rt = send(userlist[i].connectfd, &informpkt, sizeof(informpkt), 0);
                                if (inform_rt < 0){
                                    //send error
                                }
                            }
                        }
                    }

                }
                pthread_mutex_unlock(&userlist_mutex);
                break;
            }
            case 0x0102:{ //user logout
                //printf("\033[32m[INFO]\033[0m %s wants to logout.\n", recvpkt.info.srcusername);
                int nameexist = 0;
                pthread_mutex_lock(&userlist_mutex);
                int i; 
                for (i = 0; i < USERLIST_SIZE; i++){
                    if (userlist[i].user.state != 0 && !strcmp(userlist[i].user.name, recvpkt.info.srcusername)){
                        nameexist = 1;
                        break;
                    }
                }
                if (nameexist){
                    printf("\033[32m[INFO]\033[0m %s logout successfully!\n", recvpkt.info.srcusername);
                    userlist[i].user.state = 0; //duplicated, can be commented out
                    bzero(&userlist[i], sizeof(userlist[i])); //This is important!!!
                    PKT sendpkt;
                    bzero(&sendpkt, sizeof(sendpkt));
                    sendpkt.type = htons(0x0105);
                    int send_rt = send(clientfd, &sendpkt, sizeof(sendpkt), 0);
                    if (send_rt < 0){
                        //error
                    }
                    //inform all the players that there's someone to login
                    PKT informpkt;
                    bzero(&informpkt, sizeof(informpkt));
                    informpkt.type = htons(0x0404);
                    strncpy(informpkt.userlist.logusername, recvpkt.info.srcusername, NAME_SIZE);
                    for (int j = 0; j < USERLIST_SIZE; j++){
                        if (j != i && userlist[j].user.state != 0){
                            int inform_rt = send(userlist[j].connectfd, &informpkt, sizeof(informpkt), 0);
                            if (inform_rt < 0){
                                //send error
                            }
                        }
                    }
                }
                else {
                    printf("\033[31m[ERROR]\033[0m %s logout error!\n", recvpkt.info.srcusername);
                    PKT sendpkt;
                    bzero(&sendpkt, sizeof(sendpkt));
                    sendpkt.type = htons(0x0106);
                    int send_rt = send(clientfd, &sendpkt, sizeof(sendpkt), 0);
                    if (send_rt < 0){
                        //error
                    }
                }
                pthread_mutex_unlock(&userlist_mutex);
                break;
            }
            case 0x0201:{ //battle request
                pthread_mutex_lock(&userlist_mutex);
                char srcname[NAME_SIZE];
                char dstname[NAME_SIZE];
                strncpy(srcname, recvpkt.info.srcusername, NAME_SIZE);
                strncpy(dstname, recvpkt.info.dstusername, NAME_SIZE);
                printf("\033[32m[INFO]\033[0m %s wants to battle with %s\n", srcname, dstname);
                int srcin = 1, dstin = 0;
                int dstbattling = 0;
                int srcindex = 0, dstindex = 0;
                for (int i = 0; i < USERLIST_SIZE; i++){
                    if (!strcmp(userlist[i].user.name, srcname)){
                        srcindex = i;
                        if (userlist[i].user.state == 0){
                            srcin = 0;
                            //break; //!!!! can't break!!!
                        }
                        //else if (userlist[i].user.state == 2){}
                    }
                    if (!strcmp(userlist[i].user.name, dstname)){ 
                        //if there is user whose name is dstname, this user's state can't be 0
                        if (userlist[i].user.state == 1) {
                            dstin = 1;
                            dstbattling = 0;
                        }
                        if (userlist[i].user.state == 2) {
                            dstbattling = 1;
                            dstin =1;
                        }
                        dstindex = i;
                    }
                }
                if (!srcin){
                    //src doesn't log in
                    //so when user logout, corresponding user in userlist must be cleared
                    printf("\033[31m[ERROR]\033[0m battle-request-error: src %s doesn't login.\n", srcname);
                    PKT sendpkt;
                    bzero(&sendpkt, sizeof(sendpkt));
                    sendpkt.type = htons(0x0209);
                    int send_rt = send(clientfd, &sendpkt, sizeof(sendpkt), 0);
                    if (send_rt < 0){
                        //send error
                    }
                }
                else if (!dstin){
                    //error, dst doesn't log in
                    printf("\033[31m[ERROR]\033[0m battle-request-error: dst %s doesn't login.\n", dstname);
                    PKT sendpkt;
                    bzero(&sendpkt, sizeof(sendpkt));
                    sendpkt.type = htons(0x0207);
                    int send_rt = send(clientfd, &sendpkt, sizeof(sendpkt), 0);
                    if (send_rt < 0){
                        //send error
                    }
                }
                else if (dstbattling){
                    //error, dst is battling now
                    printf("\033[31m[ERROR]\033[0m battle-request-error: dst %s is battling now.\n", dstname);
                    PKT sendpkt;
                    bzero(&sendpkt, sizeof(sendpkt));
                    sendpkt.type = htons(0x0208);
                    int send_rt = send(clientfd, &sendpkt, sizeof(sendpkt), 0);
                    if (send_rt < 0){
                        //send error
                    }
                }
                else {
                    //according dstidx founded
                    //send request to dst client
                    printf("\033[32m[INFO]\033[0m waiting dst %s accepts the battle request.\n", dstname);
                    PKT sendpkt;
                    bzero(&sendpkt, sizeof(sendpkt));
                    sendpkt.type = htons(0x0202);
                    
                    strncpy(sendpkt.info.srcusername, srcname, NAME_SIZE);
                    strncpy(sendpkt.info.dstusername, dstname, NAME_SIZE); //actually this's unnecessary
                    int send_rt = send(userlist[dstindex].connectfd, &sendpkt, sizeof(sendpkt), 0);
                    if (send_rt < 0){
                        //send error
                    }
                    //for debug
                    //printf("send ok\n");
                }
                pthread_mutex_unlock(&userlist_mutex);
                break;
            }
            case 0x0203:{ //dst accepts battle request from src
                /*
                               request          
                    srcclient --------> server --------> dstclient
                    the name of variables are based on the above, src, dst
                */
                // accept battle request from others
                pthread_mutex_lock(&userlist_mutex);
                char srcname[NAME_SIZE];
                char dstname[NAME_SIZE];
                strncpy(srcname, recvpkt.info.dstusername, NAME_SIZE);
                strncpy(dstname, recvpkt.info.srcusername, NAME_SIZE);
                printf("\033[32m[INFO]\033[0m %s accepts the battle request from %s!\n", srcname, dstname);
                int srcidx = 0, dstidx;
                for (int i = 0; i < USERLIST_SIZE; i++){
                    if (userlist[i].user.state != 0 && !strcmp(userlist[i].user.name, srcname)){
                        srcidx = i;
                    }
                    if (userlist[i].user.state != 0 && !strcmp(userlist[i].user.name, dstname)){
                        dstidx = i;
                    }
                }
                PKT tosrcpkt;
                bzero(&tosrcpkt, sizeof(tosrcpkt));
                tosrcpkt.type = htons(0x0205);
                strncpy(tosrcpkt.info.srcusername, srcname, NAME_SIZE);
                strncpy(tosrcpkt.info.dstusername, dstname, NAME_SIZE);
                int src_rt = send(userlist[srcidx].connectfd, &tosrcpkt, sizeof(tosrcpkt), 0);
                if (src_rt < 0) {
                    //send error
                }
                PKT todstpkt; //just for better user experience
                bzero(&todstpkt, sizeof(todstpkt));
                todstpkt.type = htons(0x020a);
                strncpy(todstpkt.info.srcusername, dstname, NAME_SIZE);
                strncpy(todstpkt.info.dstusername, srcname, NAME_SIZE);
                int dst_rt = send(clientfd, &todstpkt, sizeof(todstpkt), 0);
                if (dst_rt < 0) {
                    //send error
                }
                //modify user's state in userlist
                userlist[srcidx].user.state = 2;
                //userlist[srcidx].user.blood = 3;
                userlist[dstidx].user.state = 2;
                //userlist[dstidx].user.blood = 3;
                //add this battle connection into battlelist
                pthread_mutex_lock(&battlelist_mutex);
                for (int i = 0; i < BATTLELIST_SIZE; i++){
                    if (battlelist[i].used == 0){
                        battlelist[i].used = 1;
                        //battlelist[i].round = 0;
                        battlelist[i].sockfd1 = userlist[srcidx].connectfd;
                        battlelist[i].sockfd2 = clientfd;
                        battlelist[i].blood1 = 3;
                        battlelist[i].blood2 = 3;
                        strncpy(battlelist[i].username1, srcname, NAME_SIZE);
                        strncpy(battlelist[i].username2, dstname, NAME_SIZE);
                        break;
                    }
                }
                pthread_mutex_unlock(&battlelist_mutex);
                pthread_mutex_unlock(&userlist_mutex);
                break;
            }
            case 0x0204:{ //dst refuses battle request from src
                pthread_mutex_lock(&userlist_mutex);
                char srcname[NAME_SIZE];
                char dstname[NAME_SIZE];
                strncpy(srcname, recvpkt.info.dstusername, NAME_SIZE);
                strncpy(dstname, recvpkt.info.srcusername, NAME_SIZE);
                printf("\033[32m[INFO]\033[0m %s refuses the battle request from %s!\n", srcname, dstname);
                int srcidx = 0;
                for (int i = 0; i < USERLIST_SIZE; i++){
                    if (userlist[i].user.state != 0 && !strcmp(userlist[i].user.name, srcname)){
                        srcidx = i;
                        break;
                    }
                }
                PKT tosrcpkt;
                bzero(&tosrcpkt, sizeof(tosrcpkt));
                tosrcpkt.type = htons(0x0206);
                strncpy(tosrcpkt.info.srcusername, srcname, NAME_SIZE);
                strncpy(tosrcpkt.info.dstusername, dstname, NAME_SIZE);
                int src_rt = send(userlist[srcidx].connectfd, &tosrcpkt, sizeof(tosrcpkt), 0);
                if (src_rt < 0) {
                    //send error
                }
                pthread_mutex_unlock(&userlist_mutex);
                break;
            }
            case 0x0301:{ //battle detail
                printf("\033[32m[INFO]\033[0m battle-detail: %s-%s\n", recvpkt.info.srcusername, recvpkt.info.dstusername);
                
                //add battlepkt into battlebuffer, first-fit
                pthread_mutex_lock(&battlebuffer_mutex);
                for (int i = 0; i < BATTLEBUFFER_SIZE; i++){
                    if (battlebuffer[i].used == 0){
                        battlebuffer[i].used = 1;
                        memcpy(&battlebuffer[i].pkt, &recvpkt, sizeof(recvpkt));
                        break;
                    }
                }
                pthread_mutex_unlock(&battlebuffer_mutex);

                //upgrate pktcnt
                pthread_mutex_lock(&pktcnt_mutex);
                pktcnt++;
                pthread_cond_signal(&pktcnt_cond);
                pthread_mutex_unlock(&pktcnt_mutex);
                break;
            }
            case 0x0401:{ //client wants to view all the users inline
                pthread_mutex_lock(&userlist_mutex);
                printf("\033[32m[INFO]\033[0m %s wants to view all the users online\n", recvpkt.info.srcusername);
                PKT backpkt;
                bzero(&backpkt, sizeof(backpkt));
                backpkt.type = htons(0x0402);
                int useridx = 0;
                for (int i = 0; i < USERLIST_SIZE; i++){
                    if (userlist[i].user.state != 0){
                        memcpy(&backpkt.userlist.users[useridx], &userlist[i].user, sizeof(userlist[i].user));
                        useridx++;
                    }
                }
                backpkt.userlist.usernum = useridx;
                int send_rt = send(clientfd, &backpkt, sizeof(backpkt), 0);
                if (send_rt < 0){
                    //send error
                }
                pthread_mutex_unlock(&userlist_mutex);
                break;    
            }
        }

    }
    pthread_exit(NULL);

}



//deal with the battle content between the warring parties
void* battle_worker(void* arg){
    while (1){
        pthread_mutex_lock(&pktcnt_mutex);
        pthread_cond_wait(&pktcnt_cond, &pktcnt_mutex);
        //wait until there is pkt in battlebuffer

        pthread_mutex_lock(&battlelist_mutex);
        pthread_mutex_lock(&battlebuffer_mutex);

        for (int i = 0; i < BATTLELIST_SIZE; i++){
            if (battlelist[i].used != 0){
                int useridx1 = -1, useridx2 = -1;
                for (int j = 0; j < BATTLEBUFFER_SIZE; j++){
                    if (!strcmp(battlebuffer[j].pkt.info.srcusername, battlelist[i].username1) 
                        && !strcmp(battlebuffer[j].pkt.info.dstusername, battlelist[i].username2)
                        ){
                        if (useridx1 == -1) useridx1 = j;
                        //first-fit, this is very important
                    }
                    else if (!strcmp(battlebuffer[j].pkt.info.dstusername, battlelist[i].username1) 
                        && !strcmp(battlebuffer[j].pkt.info.srcusername, battlelist[i].username2)
                        ){
                        if (useridx2 == -1) useridx2 = j;
                    }
                }
                if (useridx1 != -1 && useridx2 != -1){
                    printf("\033[34m[DEBUG]\033[0m found battle pkt %s - %s\n", battlelist[i].username1, battlelist[i].username2);
                    PKT pkt1; //to user1
                    PKT pkt2; //to user2
                    bzero(&pkt1, sizeof(pkt1));
                    bzero(&pkt2, sizeof(pkt2));
            
                    pkt1.type = htons(0x0302);
                    strncpy(pkt1.info.srcusername, battlelist[i].username1, NAME_SIZE);
                    strncpy(pkt1.info.dstusername, battlelist[i].username2, NAME_SIZE);
                    pkt1.info.battledetail.srcopt = battlebuffer[useridx1].pkt.info.battledetail.srcopt;
                    pkt1.info.battledetail.dstopt = battlebuffer[useridx2].pkt.info.battledetail.srcopt;
           
                
                    pkt2.type = htons(0x0302);
                    strncpy(pkt2.info.srcusername, battlelist[i].username2, NAME_SIZE);
                    strncpy(pkt2.info.dstusername, battlelist[i].username1, NAME_SIZE);
                    pkt2.info.battledetail.srcopt = battlebuffer[useridx2].pkt.info.battledetail.srcopt;
                    pkt2.info.battledetail.dstopt = battlebuffer[useridx1].pkt.info.battledetail.srcopt;

                    int battle_rt = battle(battlebuffer[useridx1].pkt.info.battledetail.srcopt, battlebuffer[useridx2].pkt.info.battledetail.srcopt);
                    if (battle_rt == 0){ //tie
                        pkt1.info.battledetail.winlose = 3;
                        pkt2.info.battledetail.winlose = 3;
                    }
                    else if (battle_rt == 1){ //useridx1 wins
                        pkt1.info.battledetail.winlose = 1;
                        pkt2.info.battledetail.winlose = 2;
                        battlelist[i].blood2--;
                        if (battlelist[i].blood2 <= 0){
                            pkt1.info.battledetail.gameover = 1;
                            pkt2.info.battledetail.gameover = 2;
                            battlelist[i].used = 0;
                            //modify user's state in userlist
                            pthread_mutex_lock(&userlist_mutex);
                            for (int k = 0; k < USERLIST_SIZE; k++){
                                if (!strcmp(userlist[k].user.name, battlelist[i].username1)){
                                    userlist[k].user.state = 1;//vacant
                                }
                                if (!strcmp(userlist[k].user.name, battlelist[i].username2)){
                                    userlist[k].user.state = 1;//vacant
                                }
                            }
                            pthread_mutex_unlock(&userlist_mutex);
                        }
                    }
                    else if (battle_rt == 2){
                        pkt1.info.battledetail.winlose = 2;
                        pkt2.info.battledetail.winlose = 1;
                        battlelist[i].blood1--;
                        if (battlelist[i].blood1 <= 0){
                            pkt1.info.battledetail.gameover = 2;
                            pkt2.info.battledetail.gameover = 1;
                            battlelist[i].used = 0;
                            pthread_mutex_lock(&userlist_mutex);
                            for (int k = 0; k < USERLIST_SIZE; k++){
                                if (!strcmp(userlist[k].user.name, battlelist[i].username1)){
                                    userlist[k].user.state = 1;//vacant
                                }
                                if (!strcmp(userlist[k].user.name, battlelist[i].username2)){
                                    userlist[k].user.state = 1;//vacant
                                }
                            }
                            pthread_mutex_unlock(&userlist_mutex);
                        }
                    }
                    pkt1.info.battledetail.srcblood = battlelist[i].blood1;
                    pkt1.info.battledetail.dstblood = battlelist[i].blood2;

                    pkt2.info.battledetail.srcblood = battlelist[i].blood2;
                    pkt2.info.battledetail.dstblood = battlelist[i].blood1;
                
                    int send_rt_1 = send(battlelist[i].sockfd1, &pkt1, sizeof(pkt1), 0);
                    int send_rt_2 = send(battlelist[i].sockfd2, &pkt2, sizeof(pkt2), 0);
                    if (send_rt_1 < 0 || send_rt_2 < 0){
                        //send error
                    }
                    pktcnt -= 2;
                    //printf("\033[34m[DEBUG]\033[0m pktcnt:%d\n", pktcnt);
                    //for debug
                    bzero(&battlebuffer[useridx1], sizeof(battlebuffer[useridx1]));
                    bzero(&battlebuffer[useridx2], sizeof(battlebuffer[useridx2]));
                }// end of if (useridx1 != -1 && useridx2 != -1)
            }
        }

        pthread_mutex_unlock(&battlelist_mutex);
        pthread_mutex_unlock(&battlebuffer_mutex);
        pthread_mutex_unlock(&pktcnt_mutex);
    } //end of while
    
    pthread_exit(NULL);
}


int main(int argc, char* argv[]){
    int listenfd;
    socklen_t client_len;
    struct sockaddr_in clientaddr, servaddr;

    pthread_attr_t attr;
    
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
    pthread_mutex_init(&userlist_mutex, NULL);
    pthread_mutex_init(&battlelist_mutex, NULL);
    pthread_mutex_init(&battlebuffer_mutex, NULL);

    pthread_mutex_init(&pktcnt_mutex, NULL);
    pthread_cond_init(&pktcnt_cond, NULL);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(4321);
    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    listen(listenfd, 1024);
    
    int thread_index = 0; 
    pthread_create(&threads[thread_index++], &attr, battle_worker, NULL);
    while (1){
        client_len = sizeof(clientaddr);
        int connectfd = accept(listenfd, (struct sockaddr*)&clientaddr, &client_len);
        if (connectfd < 0){
            //connect error
        }
        else {
            printf("\033[32m[INFO]\033[0m There is client to connect\n");
            pthread_create(&threads[thread_index++], &attr, recv_handler, &connectfd);
        }

    }
    for (int i = 0; i < thread_index; i++){
        pthread_join(threads[i], NULL);
    }

    pthread_attr_destroy(&attr);
    pthread_mutex_destroy(&userlist_mutex);
    pthread_mutex_destroy(&battlelist_mutex);
    pthread_mutex_destroy(&battlebuffer_mutex);

    pthread_mutex_destroy(&pktcnt_mutex);
    pthread_cond_destroy(&pktcnt_cond);
    pthread_exit(NULL);

    return 0;
}



