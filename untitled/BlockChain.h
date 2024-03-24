#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H
#include <QUdpSocket>
struct Client {
    QHostAddress address;
    quint16 port;
    float trust_value;
    int block_depth;
    //int isStrong;
    int is_consensus_node;
    int is_bad_node;   
    int is_run_node;//已启动
    int is_out_node;//已踢出
    int x;
    int y;
};

struct Message {
    int is_val;
    Client Source_ID;
    Client ID1;
    Client ID2;
    QByteArray buffer;
    QByteArray hash_result;
    int hops; // 转发次数

};



struct Trust_Message {
    int source_port;
    int port;
    float trust_value;
};


struct Point {
    int x;
    int y;
};
#endif // BLOCKCHAIN_H
