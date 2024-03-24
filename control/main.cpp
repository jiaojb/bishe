#include <QUdpSocket>
#include <QTimer>
#include <QCoreApplication>
#include <QInputDialog>
#include <QDataStream>
#include <iostream>
#include <QThread>
#include <QtMath>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <QSqlError>
#include <QCryptographicHash>
#include <QMutex>
#include "BlockChain.h"
QByteArray hashByteArray(const QByteArray& data, QCryptographicHash::Algorithm hashAlgorithm = QCryptographicHash::Md5) {
    // 创建哈希对象
    QCryptographicHash hash(hashAlgorithm);

    // 添加要哈希的数据
    hash.addData(data);

    // 返回哈希结果
    return hash.result();
}
void sendData(QUdpSocket& udpSocket) {
    Message temp;

    Client xx;
    temp.is_val = 2;
    temp.Source_ID = xx;
    temp.ID2 = xx;
    temp.ID1 = xx;

    temp.buffer = "8888";

    xx.x = 1;
    xx.y = 1;
    temp.hops=0;
    temp.hash_result = hashByteArray(temp.buffer, QCryptographicHash::Sha256);
    QByteArray byteArray;
    QDataStream stream(&byteArray, QIODevice::WriteOnly);

    stream <<temp.is_val<< temp.Source_ID.port
           << temp.ID1.port
           << temp.ID2.port<< temp.ID2.x << temp.ID2.y
           << temp.buffer<<temp.hash_result<<temp.hops;

    udpSocket.writeDatagram(byteArray, QHostAddress("127.0.0.1"), 8081);
}
void processData(QUdpSocket& udpSocket, Client* clients, int clientCount, int my_index, int port,QSqlDatabase& db) {
    while (udpSocket.hasPendingDatagrams())
    {

        QByteArray buffer;
        buffer.resize(udpSocket.pendingDatagramSize());
        //qDebug() << "Received message 1234655";
        QHostAddress sender;
        quint16 senderPort = 0;
        udpSocket.readDatagram(buffer.data(), buffer.size(), &sender, &senderPort);
        QDataStream inStream(&buffer, QIODevice::ReadOnly);
        QSqlQuery query(db);
        if(senderPort == 0)
           {
            continue;
        }

        Message receivedMessage;

        int is_trust=0;
        inStream >>is_trust;
        if(is_trust == 0)//为工作信息
        {
            inStream >> receivedMessage.Source_ID.port
                    >> receivedMessage.ID1.port
                    >> receivedMessage.ID2.port>> receivedMessage.ID2.x >> receivedMessage.ID2.y
                    >> receivedMessage.buffer >> receivedMessage.hash_result>> receivedMessage.hops;
            qDebug() << "Received message from"  << senderPort << ":" << receivedMessage.buffer;

        }
        else if(is_trust == 2)//为变坏信息
        {
           //turnBad(inStream, clients, my_index,  senderPort);
           // qDebug() << "YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY";

        }
        else if(is_trust == 3)//为更新client信息（已启动）
        {
           // qDebug() << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
            //update_exit_client(inStream, clients, clientCount,senderPort);
            inStream >> receivedMessage.Source_ID.port
                    >> receivedMessage.ID1.port
                    >> receivedMessage.ID2.port>> receivedMessage.ID2.x >> receivedMessage.ID2.y
                    >> receivedMessage.buffer >> receivedMessage.hash_result>> receivedMessage.hops;
            qDebug() << "Received message from"  << senderPort << ":" << receivedMessage.buffer;

        }
        else if(is_trust == 4)//为更新共识节点信息
        {
            //qDebug() << "4444444444444444444444444";
            //update_consensus_node(inStream,udpSocket, clients, clientCount, my_index,  port,  db,3,senderPort);
            int max_port[20];
            inStream>> max_port[0]
                    >> max_port[1];
           // qDebug() << "XXXXXXXXXXXmax_port[0]:"<<max_port[0]<<"XXXXXXXXXXXXXXmax_port[1]:"<<max_port[1];
            if(senderPort == 0)
            {
                return;
            }
            for(int i =0;i<clientCount;i++)
            {
                clients[i].is_consensus_node =0;
                if(clients[i].port == max_port[0] || clients[i].port == max_port[1])
                {
                    clients[i].is_consensus_node =1;
                }
            }
            qDebug() <<"当前共识节点："<<max_port[0]<<" and "<<max_port[1];
        }
        else//共识信息
        {

                    while (!inStream.atEnd()) {
                        int id;
                        float trust_value;
                        inStream >> id >> trust_value;
                        qDebug() << "ID:" << id << ", trustValue:" <<trust_value;
                    }



        }
    }

}
int main(int argc, char *argv[])
{

    QCoreApplication a(argc, argv);
    Client clients[20];
    int clientCount =20;
    int my_index =-1;
    int count =0;
    int uav[20] = {8080,8081,8082,8083};
    int u_count =4;
    // 创建UDP socket
    //.\untitled.exe 1234 8081 8080,8081,8082,8083
    for (int i =0;i<u_count;i++)
    {
        clients[count].address = QHostAddress("127.0.0.1");
        clients[count].port = uav[i];
        clients[count].x=0;
        clients[count].y=0;
        clients[count].is_consensus_node=0;
        clients[count].is_bad_node=0;
        clients[count].trust_value=0;
        clients[count].block_depth=0;
        clients[count].is_run_node = 1;
        clients[count].is_out_node = 0;
        count++;
    }

    QUdpSocket udpSocket;
    // 绑定到本地端口
    int port;
//    qDebug() << "Enter a port: ";
//    std::cin >> port;
    port = 1234;
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    QString database = QString("BlockChain_%1.db").arg(port);
    db.setDatabaseName(database);

    if (!db.open()) {
        qDebug() << "Error: Failed to open database:" ;
        return -1;
    } else {
        //qDebug() << "Database opened successfully!";
    }
    udpSocket.bind(QHostAddress::AnyIPv4, static_cast<quint16>(port));
    // 暂停程序执行 5 秒钟
    //QThread::sleep(5);
    sendData(udpSocket);
    qDebug() << "send message";
    QObject::connect(&udpSocket, &QUdpSocket::readyRead, [&]() {
        processData(udpSocket, clients, clientCount, my_index, port,db);
    });
    return a.exec();
}
