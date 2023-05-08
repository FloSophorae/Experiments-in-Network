//created by hzj
//2023-3-27

#ifndef PACKET_H_
#define PACKET_H_

#include "stdint.h"

#define NAME_SIZE 19
#define USER_NUM 30


#pragma pack(1)
typedef struct User{ //20
    char name[NAME_SIZE];
    uint8_t state; /*
        0-not used, for example, a user has logined once but logouted. 
        1-vacant
        2-battling
    */
    //uint8_t blood;
} User;


#pragma pack(1)
typedef struct PKT{
    uint16_t type;/* //2 bytes
            0x01,登录
				0x01, client->server, login
				0x02, server->client, login ok
				0x03, server->client, login not ok 
                0x04, client->server,logout
                0x05, server->client, logout ok
                0x06, server->client, logout not ok
			0x02,对战请求
				0x01, srcclient->server, I want to battle with someone
				0x02, server->dstclient, There is a battle request to you. Do you want to answer the battle request?
				0x03, dstclient->server, I accept the request
				0x04, dstclient->server, I don't want to accept the request
                0x05, server->srcclient, dst player accpet your request, ready to start
                0x06, server->srcclient, dst player doesn't accpet your request
                0x07, server->srcclient, error, dst doesn't exist or isn't logged in
                0x08, server->srccilent, error, dst is battling now 
                0x09, server->srcclient, error, src isn't logged in
                0x0a, server->dstclient, send at the same time when 0x0205, to inform dst to be ready to start
            0x03,对战内容
				0x01, client->server, my option
				0x02, server->client, 
					your option
					rival option
					winlose
					your blood
					rival blood
					game over?
			0x04,显示所有player
				0x01, client->server, request to display all the player?
				0x02, server->client, display all the users inline, answer 0x0401
                0x03, server->client, there's someone to login, inform all the users
                0x04, server->client, there's someone to logout, inform all the users
    */
    union{
        struct { //for 0x01xx, 0x02xx, 0x03xxx
            char srcusername[NAME_SIZE]; //19 bytes
            char dstusername[NAME_SIZE]; //19 bytes
            //uint8_t accpet; //1 byte, for 0x02xx, 1-accpet, 0-refuse
            struct { //6 bytes, for 0x03xx
                uint8_t srcopt; // 1-rock, 2-scissors, 3-paper
                uint8_t dstopt; 
                uint8_t srcblood;
                uint8_t dstblood;
                uint8_t winlose; //result of this round, 1-win, 2-lose， 3-tie
                uint8_t gameover; //0-game not over, 1-game over and you win, 2-game over and you lose
            } battledetail;
            char useless[36];
        } info;
        struct { //19+1+30*20 = 80 bytes, for 0x04xx,
            char logusername[NAME_SIZE];
            uint8_t usernum;//Is it too few?
            User users[USER_NUM];
        } userlist; //for 0x04xx
    };
} PKT;




#endif