//created by hzj
//2023-3-27

#include "packet.h"
#include "stdio.h"
#include "stdlib.h"

#include "netinet/in.h"
#include "sys/socket.h"
#include "string.h"
#include "arpa/inet.h"


#include "pthread.h"


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


void help(){
    /*
        1. Login
        2. Logout
        3. View all the players and their status.
        4. Send a battle request to some player.
        5. Exit the game.
    */
    printf("=========================================================\n");
    printf("All the commands you can input are below:\n");
    printf("\t%-10s%-s", "login", "Log in your account.\n");
    printf("\t%-10s%-s", "logout", "Log out your account.\n");
    printf("\t%-10s%-s", "view", "View all the players and their status.\n");
    printf("\t%-10s%-s", "request", "Send a battle request to some player.\n");
    printf("\t%-10s%-s", "clear", "clear the screen.\n");
    printf("\t%-10s%-s", "exit", "Exit the game.\n");
    printf("\t%-10s%-s", "--help or -h", "Check help manual.\n");
    printf("When you are battling with someone, your choice should be:\n");
    printf("\t%-10s%-s", "r", "rocks.\n");
    printf("\t%-10s%-s", "s", "scissors.\n");
    printf("\t%-10s%-s", "p", "papers.\n");
    printf("When you receive others' battle request, you can answer:\n");
    printf("\t%-20s%-s", "yes/Yes/YES/Y/y", "Accept the battle request.\n");
    printf("\t%-20s%-s", "no/No/NO/N/n", "Refuse the battle request.\n");
    printf("=========================================================\n");
}



int login = 0;
char curusername[NAME_SIZE];
char rivalsname[NAME_SIZE];

int battling = 0;

pthread_mutex_t login_mutex;
pthread_mutex_t battling_mutex;



int needaccept = 0;
pthread_mutex_t needaccept_mutex;



void* handle_network(void* arg){
    int sockfd = *((int*)arg);
    while (1){
        PKT recvpkt;
        bzero(&recvpkt, sizeof(recvpkt));
        int recv_rt = readn(sockfd, (char*)&recvpkt, sizeof(recvpkt));
        if (recv_rt < 0){
            //recv error
        }
        //for debug
        //printf("\033[34m[DEBUG]\033[0m handle_newwork recvpkt type 0x0%x\n", htons(recvpkt.type));
        switch(htons(recvpkt.type)){
            case 0x0102:{ //login ok
                pthread_mutex_lock(&login_mutex);
                login = 1;
                strncpy(curusername, recvpkt.info.srcusername, NAME_SIZE);
                printf("\033[32m[INFO]\033[0m Login successfully!\n");
                pthread_mutex_unlock(&login_mutex);
                break;
            }
            case 0x0103:{ //lonin not ok
                printf("\033[31m[ERROR]\033[0m Login Error:The user name you enter exists already.\n");
                break;
            }
            case 0x0105:{ //logout ok
                pthread_mutex_lock(&login_mutex);
                pthread_mutex_lock(&needaccept_mutex);
                pthread_mutex_lock(&battling_mutex);
                //clear all state information
                login = 0;
                needaccept = 0;
                battling = 0;
                bzero(curusername, sizeof(curusername));
                bzero(rivalsname, sizeof(rivalsname));
                printf("\033[32m\033[32m[INFO]\033[0m\033[0m Logout successfully!\n");
                pthread_mutex_unlock(&login_mutex);
                pthread_mutex_unlock(&needaccept_mutex);
                pthread_mutex_unlock(&battling_mutex);
                break;
            }
            case 0x0106:{ //logout not ok
                printf("\033[31m[ERROR]\033[0m Logout Error: Maybe you haven't logged in!\n");
                break;
            }
            case 0x0202:{ //someone wants to battle with you
                printf("\033[32m[INFO]\033[0m There is a battle request from %s, do you want to accept?(yes/no):\n", recvpkt.info.srcusername);
                bzero(rivalsname, sizeof(rivalsname));
                strncpy(rivalsname, recvpkt.info.srcusername, NAME_SIZE);
                pthread_mutex_lock(&needaccept_mutex);
                needaccept = 1;
                //need enter yes or no in handle_input to decide whether to accpet battle request or not
                pthread_mutex_unlock(&needaccept_mutex);
                break;
            }
            case 0x0205:{ //your rival accepts you battle request
                printf("\033[32m[INFO]\033[0m Your rival has accepted your battle request! Now game begins!\n");
                pthread_mutex_lock(&battling_mutex);
                battling = 1;
                strncpy(rivalsname, recvpkt.info.dstusername, NAME_SIZE);
                pthread_mutex_unlock(&battling_mutex);
                break;
            }
            case 0x0206:{
                printf("\033[32m[INFO]\033[0m Sad! Your rival refuses your battle request!\n");
                break;
            }
            case 0x0207:{
                printf("\033[31m[ERROR]\033[0m Error: Your rival doesn't exist or isn't logged in!\n");
                break;
            }
            case 0x0208:{
                printf("\033[31m[ERROR]\033[0m Error: Your rival is battling with others now!\n");
                break;
            }
            case 0x0209:{
                printf("\033[31m[ERROR]\033[0m Error: You haven't logged in!\n");
                break;
            }
            case 0x020a:{
                printf("\033[32m[INFO]\033[0m Game is beginning now! Please get ready!\n");
                //pthread_mutex_lock(&needaccept_mutex);
                //needaccept = 0;
                //pthread_mutex_unlock(&needaccept_mutex);
                break;
            }
            case 0x0302:{ //battle detail
                char youropt[10];
                char rivalopt[10];
                if (recvpkt.info.battledetail.srcopt == 1){
                    strncpy(youropt, "rock\0", 10);
                }
                else if (recvpkt.info.battledetail.srcopt == 2){
                    strncpy(youropt, "scissors\0", 10);
                }
                else if (recvpkt.info.battledetail.srcopt == 3){
                    strncpy(youropt, "paper\0", 10);
                }  
                if (recvpkt.info.battledetail.dstopt == 1){
                    strncpy(rivalopt, "rock\0", 10);
                }
                else if (recvpkt.info.battledetail.dstopt == 2){
                    strncpy(rivalopt, "scissors\0", 10);
                }
                else if (recvpkt.info.battledetail.dstopt == 3){
                    strncpy(rivalopt, "paper\0", 10);
                }
                printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ battle detail ~~~~~~~~~~~~~~~~\n");
                printf("Your option: %s\n", youropt);
                printf("Rival's option: %s\n", rivalopt);
                if (recvpkt.info.battledetail.winlose == 1){
                    printf("Congratulations! You win this round!\n");
                }
                else if (recvpkt.info.battledetail.winlose == 2){
                    printf("Sad! You lose this round!\n");
                }
                else if (recvpkt.info.battledetail.winlose == 3){
                    printf("It's a dram this round!\n");
                }
                printf("Your bleed: %d\n", recvpkt.info.battledetail.srcblood);
                printf("Rival bleed: %d\n", recvpkt.info.battledetail.dstblood);
                if (recvpkt.info.battledetail.gameover == 1){
                    printf("Game is over! Congratulations! You have won the final victory\n");
                    pthread_mutex_lock(&battling_mutex);
                    battling = 0;
                    bzero(rivalsname, sizeof(rivalsname));
                    pthread_mutex_unlock(&battling_mutex);
                }
                else if (recvpkt.info.battledetail.gameover == 2){
                    printf("Game is over! I'm sorry that you lost!\n");
                    pthread_mutex_lock(&battling_mutex);
                    battling = 0;
                    bzero(rivalsname, sizeof(rivalsname));
                    pthread_mutex_unlock(&battling_mutex);
                }
                printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
                break;
            }
            case 0x0402:{
                int usernum = recvpkt.userlist.usernum;
                printf("~~~~~~~~~~~~~~~~~~~~~~~All the users~~~~~~~~~~~~~~~~~~~~~~\n");
                //printf("Number of users: %d\n", usernum);
                printf("All user are below:\n");
                printf("%-30s%-s\n", "Username", "State");
                for (int i = 0; i < usernum; i++){
                    if (recvpkt.userlist.users[i].state != 0 && strcmp(recvpkt.userlist.users[i].name, "")){
                        printf("%-30s%-s\n", recvpkt.userlist.users[i].name, (recvpkt.userlist.users[i].state==1)?"Vacant":"Battling");
                    }
                }
                printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
                break;
            }
            case 0x0403:{
                printf("\033[32m[INFO]\033[0m Inform Message: %s login!\n", recvpkt.userlist.logusername);
                break;
            }
            case 0x0404:{
                printf("\033[32m[INFO]\033[0m Inform Message: %s logout!\n", recvpkt.userlist.logusername);
                break;
            }
        } // end of switch
        
    } //end of while
    pthread_exit(NULL);
}



void* handle_input(void* arg){
    int sockfd = *(int*)arg;
    help();
    while (1){
        char instr[10];
        scanf("%s", instr);
        
        if (!strcmp(instr, "--help") || !strcmp(instr, "-h")){
            help();
        }
        else if (!strcmp(instr, "login")){
            pthread_mutex_lock(&login_mutex);
            if (login == 1){
                printf("\033[31m[ERROR]\033[0m Login Error: You have logged in!\n");
            }
            else {
                char username[NAME_SIZE];
                printf("Please enter your name:");
                scanf("%s", username);
                printf("The name you enter: %s\n", username);
                PKT sendpkt;
                bzero(&sendpkt, sizeof(sendpkt));
                sendpkt.type = htons(0x0101);
                strncpy(sendpkt.info.srcusername, username, NAME_SIZE);
                int send_rt = send(sockfd, &sendpkt, sizeof(sendpkt), 0);
                if (send_rt < 0){
                    printf("send failed\n");
                    exit(-1);
                }
            }
            pthread_mutex_unlock(&login_mutex);
        }
        else if (!strcmp(instr, "logout")){
            pthread_mutex_lock(&login_mutex);
            if (login == 0){
                printf("\033[31m[ERROR]\033[0m Error: you haven't log in yet!\n");
            }
            else {
                PKT sendpkt;
                bzero(&sendpkt, sizeof(sendpkt));
                sendpkt.type = htons(0x0102);
                strncpy(sendpkt.info.srcusername, curusername, NAME_SIZE);
                int send_rt = send(sockfd, &sendpkt, sizeof(sendpkt), 0);
                if (send_rt < 0){
                    printf("send failed\n");
                    exit(-1);
                }
            }
            pthread_mutex_unlock(&login_mutex);
        }
        else if (!strcmp(instr, "view")){
            pthread_mutex_lock(&login_mutex);
            pthread_mutex_lock(&needaccept_mutex);
            pthread_mutex_lock(&battling_mutex);
            if (login == 0){
                printf("\033[31m[ERROR]\033[0m Error: you haven't log in yet!\n");
            }
            else if (needaccept == 1){
                printf("\033[31m[ERROR]\033[0m Error: you should enter yes or no!\n");
            }
            else if (battling == 1){
                //printf("Error: you are battling with someone else, you can't enter this command!\n");
                printf("\033[31m[ERROR]\033[0m Input Error: your choice should be r(rock), s(scissors), p(paper)!\n");
            }
            else {
                PKT sendpkt;
                bzero(&sendpkt, sizeof(sendpkt));
                sendpkt.type = htons(0x0401);
                strncpy(sendpkt.info.srcusername, curusername, NAME_SIZE);
                int send_rt = send(sockfd, &sendpkt, sizeof(sendpkt), 0);
                if (send_rt < 0){
                    printf("send failed\n");
                    exit(-1);
                }
            }
            pthread_mutex_unlock(&login_mutex);
            pthread_mutex_unlock(&needaccept_mutex);
            pthread_mutex_unlock(&battling_mutex);
        }
        else if (!strcmp(instr, "request")){
            pthread_mutex_lock(&login_mutex);
            pthread_mutex_lock(&needaccept_mutex);
            pthread_mutex_lock(&battling_mutex);
            if (login == 0){
                printf("\033[31m[ERROR]\033[0m Error: you haven't log in yet!\n");
            }
            else if (needaccept == 1){
                printf("\033[31m[ERROR]\033[0m Error: you shouldn't enter yes or no!\n");
            }
            else if (battling == 1){
                //printf("Error: you are battling with someone else, you can't enter this command!\n");
                printf("\033[31m[ERROR]\033[0m Input Error: your choice should be r(rock), s(scissors), p(paper)!\n");
            }
            else {
                char therivalname[NAME_SIZE];
                printf("Please enter rival's name you want to battle with:");
                scanf("%s", therivalname);
                if (!strcmp(therivalname, curusername)){
                    printf("\033[31m[ERROR]\033[0m] Error: you should enter your name as the rival's name!\n");
                }
                else {
                    PKT sendpkt;
                    bzero(&sendpkt, sizeof(sendpkt));
                    sendpkt.type = htons(0x0201);
                    strncpy(sendpkt.info.srcusername, curusername, NAME_SIZE);
                    strncpy(sendpkt.info.dstusername, therivalname, NAME_SIZE);
                    int send_rt = send(sockfd, &sendpkt, sizeof(sendpkt), 0);
                    if (send_rt < 0){
                        printf("send failed\n");
                        exit(-1);
                    }
                }
            }
            pthread_mutex_unlock(&login_mutex);
            pthread_mutex_unlock(&needaccept_mutex);
            pthread_mutex_unlock(&battling_mutex); 
        }
        else if (!strcmp(instr, "N") || !strcmp(instr, "n") || 
                !strcmp(instr, "no") || !strcmp(instr, "NO") ||
                !strcmp(instr, "No")){
            pthread_mutex_lock(&login_mutex);
            pthread_mutex_lock(&needaccept_mutex);
            pthread_mutex_lock(&battling_mutex);
            if (login == 0){
                printf("\033[31m[ERROR]\033[0m Error: you haveb't login!\n");
            }
            else if (needaccept == 0){
                printf("\033[31m[ERROR]\033[0m Error: there is no user that wants battle with you. you can't enter this command!\n");
            }
            else if (battling == 1){
                //printf("Error: you are battling with someone else, you can't enter this command!\n");
                printf("\033[31m[ERROR]\033[0m Input Error: your choice should be r(rock), s(scissors), p(paper)!\n");
            }
            else {
                needaccept = 0;
                PKT backpkt;
                bzero(&backpkt, sizeof(backpkt));
                backpkt.type = htons(0x0204);
                strncpy(backpkt.info.srcusername, curusername, NAME_SIZE);
                strncpy(backpkt.info.dstusername, rivalsname, NAME_SIZE);
                int back_rt = send(sockfd, &backpkt, sizeof(backpkt), 0);
                if (back_rt < 0){
                    //send error
                }
            }
            pthread_mutex_unlock(&login_mutex);
            pthread_mutex_unlock(&needaccept_mutex);
            pthread_mutex_unlock(&battling_mutex);
        }
        else if (!strcmp(instr, "Y") || !strcmp(instr, "y") || 
                !strcmp(instr, "YES") || !strcmp(instr, "yes") ||
                !strcmp(instr, "Yes")){ 
            pthread_mutex_lock(&login_mutex);
            pthread_mutex_lock(&needaccept_mutex);
            pthread_mutex_lock(&battling_mutex);
            if (login == 0){
                printf("\033[31m[ERROR]\033[0m Error: you haveb't login!\n");
            }
            else if (needaccept == 0){
                printf("\033[31m[ERROR]\033[0m Error: there is no user that wants battle with you. you can't enter this command!\n");
            }
            else if (battling == 1){
                //printf("Error: you are battling with someone else, you can't enter this command!\n");
                printf("\033[31m[ERROR]\033[0m Input Error: your choice should be r(rock), s(scissors), p(paper)!\n");
            }
            else {
                battling = 1;
                needaccept = 0;
                PKT backpkt;
                bzero(&backpkt, sizeof(backpkt));
                backpkt.type = htons(0x0203);
                strncpy(backpkt.info.srcusername, curusername, NAME_SIZE);
                strncpy(backpkt.info.dstusername, rivalsname, NAME_SIZE);
                int back_rt = send(sockfd, &backpkt, sizeof(backpkt), 0);
                if (back_rt < 0){
                    //send error
                }
            }
            pthread_mutex_unlock(&login_mutex);
            pthread_mutex_unlock(&needaccept_mutex);
            pthread_mutex_unlock(&battling_mutex);
        }
        else if (!strcmp(instr, "r") || !strcmp(instr, "s") || !strcmp(instr, "p")){
            pthread_mutex_lock(&login_mutex);
            pthread_mutex_lock(&needaccept_mutex);
            pthread_mutex_lock(&battling_mutex);
            if (login == 0){
                printf("\033[31m[ERROR]\033[0m Error: you haveb't login, you can't enter this command!\n");
            }
            else if (needaccept == 1){
                printf("\033[31m[ERROR]\033[0m Error: you need yes or no.\n");
            }
            else if (battling == 0){
                printf("\033[31m[ERROR]\033[0m Error: you are not battling with someone else, you can't enter this command!\n");
            }
            else {
                PKT sendpkt;
                bzero(&sendpkt, sizeof(sendpkt));
                sendpkt.type = htons(0x0301);
                strncpy(sendpkt.info.srcusername, curusername, NAME_SIZE);
                strncpy(sendpkt.info.dstusername, rivalsname, NAME_SIZE);
                if (!strcmp(instr, "r")){
                    sendpkt.info.battledetail.srcopt = 1;
                }
                else if (!strcmp(instr, "s")){
                    sendpkt.info.battledetail.srcopt = 2;
                }
                else if (!strcmp(instr, "p")){
                    sendpkt.info.battledetail.srcopt = 3;
                }
                int send_rt = send(sockfd, &sendpkt, sizeof(sendpkt), 0);
                if (send_rt < 0){
                    //printf("send failed\n");
                    //exit(-1); //?
                    //printf("Please enter your option again!\n");
                }
            }
            pthread_mutex_unlock(&login_mutex);
            pthread_mutex_unlock(&needaccept_mutex);
            pthread_mutex_unlock(&battling_mutex);
        }
        else if (!strcmp(instr, "clear")){
            printf("\033c");
            help();
        }
        else if (!strcmp(instr, "exit")){
            pthread_mutex_lock(&login_mutex);
            if (login){
                printf("\033[31m[ERROR]\033[0m Exit Error: You are currently in a logged-in state, please log out first.\n");
            }
            else {
                exit(0);
                pthread_exit(NULL);
                return NULL;
            }
            pthread_mutex_unlock(&login_mutex);
        }
        else {
            printf("\033[31m[ERROR]\033[0m Input Error: there is no command %s, you can enter --help or -h to check help manual!\n", instr);
        }

    }
    pthread_exit(NULL);
}


int main(int argc, char* argv[]){
    int sockfd;
    struct sockaddr_in serveraddr;


    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(4321);
    serveraddr.sin_addr.s_addr = htons(INADDR_ANY);
    //inet_pton(AF_INET, INADDR_ANY, &serveraddr.sin_addr);
    int connect_rt = connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    if (connect_rt < 0){
        printf("connect failed\n");
        exit(-1);
    }

    pthread_t thread_input;
    pthread_t thread_network;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
    pthread_mutex_init(&login_mutex, NULL);
    pthread_mutex_init(&battling_mutex, NULL);
    pthread_mutex_init(&needaccept_mutex, NULL);

    pthread_create(&thread_network, &attr, handle_network, &sockfd);
    pthread_create(&thread_input, &attr, handle_input, &sockfd);

    pthread_join(thread_input, NULL);
    pthread_join(thread_network, NULL);

    pthread_attr_destroy(&attr);
    pthread_mutex_destroy(&login_mutex);
    pthread_mutex_destroy(&battling_mutex);
    pthread_mutex_destroy(&needaccept_mutex);

    pthread_exit(NULL);
    return 0;
}



