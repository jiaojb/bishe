#ifndef COMMUNICATEMODE_H
#define COMMUNICATEMODE_H
#include <QUdpSocket>
#include <QTimer>
#include <QCoreApplication>
#include <QInputDialog>
#include <QDataStream>
#include <iostream>
#include <QtMath>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <QSqlError>
#include <QCryptographicHash>
#include <QMutex>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>
#include "BlockChain.h"
#include "ConsensusMode.h"
int SKIPCOUNT = 5;

QMutex databaseMutex;
double euclideanDistance(int x1, int y1, int x2, int y2) {
    double distance = qSqrt(qPow((x2 - x1), 2) + qPow((y2 - y1), 2));

    return distance;
}

QByteArray hashByteArray(const QByteArray& data, QCryptographicHash::Algorithm hashAlgorithm = QCryptographicHash::Md5) {
    // 创建哈希对象
    QCryptographicHash hash(hashAlgorithm);

    // 添加要哈希的数据
    hash.addData(data);

    // 返回哈希结果
    return hash.result();
}
//发送数据
void sendData(QUdpSocket& udpSocket, Client* clients, int clientCount, int my_index, int port,QSqlDatabase& db) {
    Message temp;
    if (!db.open()) {
        qDebug() << "Error: Failed to open database connection";
        return;
    }
    temp.is_val = 0;
    temp.Source_ID = clients[my_index];
    temp.ID2 = clients[my_index];
    temp.ID1 = clients[my_index];
    if(clients[my_index].is_bad_node == 1)
    {
            temp.buffer = "111111";
    }
    else
    {
        temp.buffer = "12345";
    }
    temp.hops=0;
    //哈希处理
    temp.hash_result = hashByteArray(temp.buffer, QCryptographicHash::Sha256);
    QByteArray byteArray;
    QDataStream stream(&byteArray, QIODevice::WriteOnly);
    QSqlQuery query(db);
    QString portReceiveTableName = QString("output_%1").arg(port);
    stream <<temp.is_val<< temp.Source_ID.port
           << temp.ID1.port
           << temp.ID2.port<< temp.ID2.x << temp.ID2.y
           << temp.buffer<<temp.hash_result<<temp.hops;
    query.prepare("INSERT INTO " + portReceiveTableName + "(id,message,hash_result) VALUES (:port,:message,:hash_result)");
    for (int i = 0; i < clientCount; i++) {
        if (clients[i].port == port) {
            continue;
        }
        if(euclideanDistance(clients[my_index].x,clients[my_index].y,clients[i].x,clients[i].y)>2)
        {
            continue;
        }
        //qDebug() << "send message";
        if(clients[i].is_out_node == 1)
        {
            continue;
        }
        udpSocket.writeDatagram(byteArray, clients[i].address, clients[i].port);

        query.bindValue(":port", clients[i].port);
        query.bindValue(":message", temp.buffer);
        query.bindValue(":hash_result", temp.hash_result);
         databaseMutex.lock();
        if (!query.exec()) {
           // qDebug() << "Error: Failed to insert data into table:" <<portReceiveTableName<< query.lastError().text();
             databaseMutex.unlock();
        }
        query.finish();
         databaseMutex.unlock();
    }
    query.finish();
    db.commit();
}

//设置变坏
void turnBad(QDataStream& inStream, Client* clients,  int my_index, quint16 senderPort){

    //qDebug() << "YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY";
    Message receivedMessage;
    inStream >> receivedMessage.Source_ID.port
            >> receivedMessage.ID1.port
            >> receivedMessage.ID2.port>> receivedMessage.ID2.x >> receivedMessage.ID2.y
            >> receivedMessage.buffer >> receivedMessage.hash_result>> receivedMessage.hops;
    receivedMessage.is_val =0;
    if(senderPort == 0)
    {
        return;
    }
    if (receivedMessage.buffer == QByteArray("8888")) {
        // byteArray 与 "8888" 相同
        //qDebug() << "ASDFGHJKL";
        clients[my_index].is_bad_node = 1;
    }

}

//处理工作数据
void processDataAndForward(QDataStream& inStream,QUdpSocket& udpSocket, Client* clients, int clientCount, int my_index, int port, QSqlDatabase& db, quint16 senderPort) {

    // 将上述代码放在这个函数内部
    QSqlQuery query(db);

    QString portreceive_TableName = QString("receive_%1").arg(port);
    query.prepare("INSERT INTO " + portreceive_TableName + "(id,message,hash_result) VALUES (:port,:message,:hash_result)");
    Message receivedMessage;
    inStream >> receivedMessage.Source_ID.port
            >> receivedMessage.ID1.port
            >> receivedMessage.ID2.port>> receivedMessage.ID2.x >> receivedMessage.ID2.y
            >> receivedMessage.buffer >> receivedMessage.hash_result>> receivedMessage.hops;
    receivedMessage.is_val =0;
    if(senderPort == 0)
    {
        return;
    }
    //计算距离查看是否为邻居节点
    if(euclideanDistance(receivedMessage.ID2.x,receivedMessage.ID2.y,
                         clients[my_index].x,clients[my_index].y) > 2.0)//不是
    {
        return;
    }
    //丢弃

    if(receivedMessage.Source_ID.port == clients[my_index].port
            && receivedMessage.ID1.port != clients[my_index].port)
    {
        return;
    }
    //接收不再转发

    //写入receive
    query.bindValue(":port", receivedMessage.ID2.port);
    query.bindValue(":message",receivedMessage.buffer);
    query.bindValue(":hash_result", receivedMessage.hash_result);
     databaseMutex.lock();
    if (!query.exec()) {
        qDebug() << "Error: Failed to insert data into table:" <<portreceive_TableName << query.lastError().text();
     databaseMutex.unlock();
    }
    //query.finish();
     databaseMutex.unlock();

    if( receivedMessage.ID1.port == clients[my_index].port)//自己发给邻居节点
    {
       // qDebug() << "Received message from" << senderPort << ":" << receivedMessage.buffer;
        // 在这里处理接收到的数据
        return;
    }
    receivedMessage.hops++;
    if (receivedMessage.hops >= SKIPCOUNT) { // 设置最大转发次数为 5 次
        return;
    }
    qDebug() << "Received message from"  << senderPort << ":" << receivedMessage.buffer;


    receivedMessage.ID1 = receivedMessage.ID2;
    receivedMessage.ID2 = clients[my_index];
    QByteArray byteArray;
    QDataStream stream(&byteArray, QIODevice::WriteOnly);

    if(clients[my_index].is_bad_node == 1)
    {
//        if(receivedMessage.hops%2 == 0)
//        {
//            receivedMessage.buffer = "111111";
//            receivedMessage.hash_result = hashByteArray(receivedMessage.buffer);
//        }
        receivedMessage.buffer = "111111";
        receivedMessage.hash_result = hashByteArray(receivedMessage.buffer);
        //
    }
    else
    {
        receivedMessage.buffer = "12345";
        receivedMessage.hash_result = hashByteArray(receivedMessage.buffer);
    }

    stream <<receivedMessage.is_val<< receivedMessage.Source_ID.port
          << receivedMessage.ID1.port
          << receivedMessage.ID2.port<< receivedMessage.ID2.x << receivedMessage.ID2.y
          << receivedMessage.buffer<<receivedMessage.hash_result<<receivedMessage.hops;
    QString portoutput_TableName = QString("output_%1").arg(port);
    query.prepare("INSERT INTO " + portoutput_TableName + "(id,message,hash_result) VALUES (:port,:message,:hash_result)");
    for (int i = 0; i < clientCount; i++) {
        if (clients[i].port == port) {
            continue;
        }
        //qDebug() << "send message";
        if(euclideanDistance(clients[i].x,clients[i].y,
                             clients[my_index].x,clients[my_index].y) > 2.0)//不是
        {
            continue;
        }
        if(clients[i].is_out_node == 1)
        {
            continue;
        }
        udpSocket.writeDatagram(byteArray, clients[i].address, clients[i].port);
        query.bindValue(":port", clients[i].port);
        query.bindValue(":message",receivedMessage.buffer);
        query.bindValue(":hash_result", receivedMessage.hash_result);
         databaseMutex.lock();
        if (!query.exec()) {
            qDebug() << "Error: Failed to insert data into table:" <<portoutput_TableName<< query.lastError().text();
             databaseMutex.unlock();
        }
        query.finish();
         databaseMutex.unlock();
    }
    db.commit();
}



bool compareTrust(const Trust_Message &a, const Trust_Message &b) {
    return a.trust_value > b.trust_value;
}
void send_new_consensus_data(QDataStream &inStream,QUdpSocket& udpSocket, Client* clients, int clientCount, int my_index, int port, QSqlDatabase& db,int consensus_num)
{
    QString portInfoTableName = QString("info_%1").arg(port);
    QSqlQuery query(db);
    Trust_Message temp[20];
    int count = 0;
    if (!query.exec(QString("SELECT * FROM %1").arg(portInfoTableName))) {
        qDebug() << "Error: Failed to fetch data from " << portInfoTableName << " table:" ;
        db.close();
        return;
    }
    else {
        qDebug() << "Data from " << portInfoTableName << " table:";
        while (query.next()) {
            temp[count].port = query.value(0).toInt();
            temp[count].trust_value = query.value(1).toFloat();
            count++;
           //qDebug() << "ID:" << id << ", trustValue:" <<trustValue<<",blockDepth"<<blockDepth;
        }
    }
    std::sort(temp, temp + 20, compareTrust);
    int max_port[20];
    int i=0;
    int count1 =0;
    while (i<clientCount&&count1<consensus_num)
    {
        for(int j =0;j<clientCount;j++)
        {
            if(clients[j].port == temp[i].port && clients[j].is_run_node ==1 &&clients[j].is_bad_node == 0)
            {
                max_port[count1] = temp[i].port;
                count1++;
            }
        }
       i++;
    }

    qDebug() << "max_port[i]" << max_port[0] << ", max_port[i]" <<max_port[1];
    for(int i =0;i<clientCount;i++)
    {
        clients[i].is_consensus_node =0;
        if(clients[i].port == temp[0].port || clients[i].port == temp[1].port)
        {
            clients[i].is_consensus_node =1;
        }
    }

    QString blockchainTableName = QString("blockchain_%1").arg(port);
    QSqlQuery updateQuery(db);
    updateQuery.prepare(QString("UPDATE %1 SET  is_consensus_node = 0 ").arg(blockchainTableName));
    databaseMutex.lock();
    if (!updateQuery.exec()) {
        qDebug() << "Error: Failed to update data in " << blockchainTableName << " table:" << query.lastError().text();
        // db.close();
        databaseMutex.unlock();
        return;
    }
    updateQuery.finish();
    //databaseMutex.unlock();

    for(int i = 0;i<consensus_num;i++)
    {
        updateQuery.prepare(QString("UPDATE %1 SET  is_consensus_node = 1 WHERE id = :id").arg(blockchainTableName));
        updateQuery.bindValue(":id", max_port[i]);

        //databaseMutex.lock();
        if (!updateQuery.exec()) {
            qDebug() << "Error: Failed to update data in " << blockchainTableName << " table:" << query.lastError().text();
            // db.close();
            databaseMutex.unlock();
            return;
        }
        updateQuery.finish();

    }
    databaseMutex.unlock();
    db.commit();
    QByteArray byteArray;
    QDataStream stream(&byteArray, QIODevice::WriteOnly);
    stream <<4<< max_port[0]
               <<max_port[1];
    //修改XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
    for (int i = 0; i < clientCount; i++) {
            if (clients[i].port == port) {
                continue;
            }
            if(euclideanDistance(clients[my_index].x,clients[my_index].y,clients[i].x,clients[i].y)>2)
            {
                continue;
            }
            //qDebug() << "send message";
            if(clients[i].is_out_node == 1)
            {
                continue;
            }
            udpSocket.writeDatagram(byteArray, clients[i].address, clients[i].port);
    }
}


#endif // COMMUNICATEMODE_H
