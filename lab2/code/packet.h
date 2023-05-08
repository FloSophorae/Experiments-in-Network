//created by hzj
//2023-3-18

#ifndef PACKET_H_
#define PACKET_H_




#define NAME_SIZE 30
#define WT_SIZE 90


//data packet sendto server
//33 bytes
#pragma pack(1)
typedef struct data_sdpkt{
    uint16_t type; // 2 bytes  
    /*
        0X0100  输入要查询的城市
        0X0201  查询1天的天气，对应输入数字为1或3
        0X0202  查询3天的天气，对应输入数字为2
    */
    char name[NAME_SIZE]; //30  bytes
    uint8_t number; //1 bytes
} data_sdpkt;

#pragma pack(1)
typedef struct date{
    uint16_t year; // 2 bytes
    uint8_t month; // 1 bytes
    uint8_t day;   // 1 bytes
} date;

#pragma pack(1)
typedef struct weather_temperature{
    uint8_t weather;
    uint8_t temperature;
} weather_temperature;

//data packet recvfrom server
//127 bytes
#pragma pack(1)
typedef struct data_rvpkt{
    uint16_t type; //2 bytes
    /*
        0x0100  没有对应城市的名字 
        0x2000  找到对应城市的名字，开始查询，
        0x0341  对应于data_sdpkt.type == 0x0201, 每次返回一天的城市天气
        0x0342  对应于data_sdpkt.type == 0x0202, 每次返回三天的城市天气
    */
    char name[NAME_SIZE]; //30 bytes
    date today; //4 bytes
    uint8_t daynumber; //1 bytes
    weather_temperature wtarr[WT_SIZE];
}data_rvpkt;






#endif
