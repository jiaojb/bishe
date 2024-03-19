#ifndef CONSENSUSMODE_H
#define CONSENSUSMODE_H
#include <QUdpSocket>
#include <QTimer>
#include <QCoreApplication>
#include <QInputDialog>
#include <QDataStream>
#include <iostream>
#include "BlockChain.h"
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
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>

QMutex databaseMutex;
double euclideanDistance(int x1, int y1, int x2, int y2) {
    double distance = qSqrt(qPow((x2 - x1), 2) + qPow((y2 - y1), 2));

    return distance;
}

//监测周边节点行为
void checkAndUpdateTrustValue(QSqlDatabase& db, Client* clients ,int clientCount,int my_index, int port) {
    //qDebug() << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    QString infoTableName = QString("info_%1").arg(port);
    QString portOutputTableName = QString("output_%1").arg(port);
    QString portReceiveTableName = QString("receive_%1").arg(port);

    QSqlQuery selectInfoQuery(db);
    QSqlQuery selectOutputQuery(db);
    QSqlQuery selectReceiveQuery(db);
    QSqlQuery updateQuery(db);
    databaseMutex.lock();
    // 查询 info 表中的 id 和 trust_value
    if (!selectInfoQuery.exec(QString("SELECT id, trust_value FROM %1").arg(infoTableName))) {
        qDebug() << "Error: Failed to fetch data from " << infoTableName << " table:";
        //db.close();
        //return;
    }
    databaseMutex.unlock();
    while (selectInfoQuery.next()) {
        int id = selectInfoQuery.value(0).toInt();
        float trustValue = selectInfoQuery.value(1).toFloat();

        if(id == clients[my_index].port)
        {
            continue;
        }

        //XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

        selectReceiveQuery.prepare(QString("SELECT * FROM %1 WHERE id = :id AND message = :message ").arg(portReceiveTableName));
        selectReceiveQuery.bindValue(":id", id);
        QString str = "111111";
        selectReceiveQuery.bindValue(":message", str.toUtf8());
        if (!selectReceiveQuery.exec()) {
            qDebug() << "Error: Failed to fetch data from " << portReceiveTableName << " table:" << selectReceiveQuery.lastError().text();
            //db.close();
            //return;
            //continue;
        }

        // 如果没有找到匹配项，则将相应的 trust_value 减 1
        if (selectReceiveQuery.next()) {
            trustValue -= 1;

            //qDebug() <<id<<"trustValue -= 1 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
            // 更新 info 表中的 trust_value
            updateQuery.prepare(QString("UPDATE %1 SET trust_value = :trustValue WHERE id = :id").arg(infoTableName));
            updateQuery.bindValue(":trustValue", trustValue);
            updateQuery.bindValue(":id", id);
            databaseMutex.lock();
            if (!updateQuery.exec()) {
                qDebug() << "Error: Failed to update data in " << infoTableName << " table:" ;
                //db.close();
                //return;
            }
            databaseMutex.unlock();
            //break;
        }



    }


}
//非共识节点发送共识数据给共识节点
void sendTrustData(QUdpSocket& udpSocket, Client* clients, int clientCount, int my_index, int port,QSqlDatabase& db)
{
    if(clients[my_index].is_consensus_node == 0)
    {
    QByteArray byteArray;
    QString portInfoTableName = QString("info_%1").arg(port);
    QDataStream stream(&byteArray, QIODevice::WriteOnly);
    QSqlQuery query(db);
    //Trust_Message temp;
    if (!db.open()) {
        qDebug() << "Error: Failed to open database connection";
        return;
    }
    stream <<1;

    if (!query.exec(QString("SELECT * FROM %1").arg(portInfoTableName))) {
        qDebug() << "Error: Failed to fetch data from " << portInfoTableName << " table:" ;
        //db.close();
        return;
    } else {
        //qDebug() << "Data from " << portInfoTableName << " table:";
        while (query.next()) {
            int id = query.value(0).toInt();
            float trustValue = query.value(1).toFloat();
            //int blockDepth = query.value(2).toInt();
            stream <<id<<trustValue;

            //qDebug() << "ID:" << id << ", trustValue:" <<trustValue<<",blockDepth"<<blockDepth;
        }
    }
//    int flag =0;//判断有几个共识节点
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
        if(clients[i].is_consensus_node == 1)
        {

            udpSocket.writeDatagram(byteArray, clients[i].address, clients[i].port);
            //qDebug() << "send message";
        }
    }
//    if(flag == 0)
//    {
//        //qDebug() << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
//        updateInfoTableFromBlockchain(my_index,db,clients,clientCount,port);
//    }
    }

}

//共识节点间相互发送共识数据
void gongshi(QUdpSocket& udpSocket, Client* clients, int clientCount, int my_index, int port,QSqlDatabase& db)
{
    //qDebug() << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    if(clients[my_index].is_consensus_node == 1)
    {
    QByteArray byteArray;
    QString portInfoTableName = QString("info_%1").arg(port);
    QDataStream stream(&byteArray, QIODevice::WriteOnly);
    QSqlQuery query(db);
    //Trust_Message temp;
    if (!db.open()) {
        qDebug() << "Error: Failed to open database connection";
        return;
    }
    stream <<1;

    if (!query.exec(QString("SELECT * FROM %1").arg(portInfoTableName))) {
        qDebug() << "Error: Failed to fetch data from " << portInfoTableName << " table:" ;
        //db.close();
        return;
    } else {
        //qDebug() << "Data from " << portInfoTableName << " table:";
        while (query.next()) {
            int id = query.value(0).toInt();
            float trustValue = query.value(1).toFloat();
            //int blockDepth = query.value(2).toInt();
            stream <<id<<trustValue;

            //qDebug() << "ID:" << id << ", trustValue:" <<trustValue<<",blockDepth"<<blockDepth;
        }
    }
    int flag =0;//判断有几个共识节点
    for (int i = 0; i < clientCount; i++) {

        if(euclideanDistance(clients[my_index].x,clients[my_index].y,clients[i].x,clients[i].y)>2)
        {
            continue;
        }
        //qDebug() << "send message";
        if(clients[i].is_out_node == 1)
        {
            continue;
        }
        if(clients[i].is_consensus_node == 1 )
        {
            if(i!=my_index)
            {
                flag =1;
            }

            udpSocket.writeDatagram(byteArray, clients[i].address, clients[i].port);
            //qDebug() << "send message";
        }
    }
    if(flag == 0)
    {
        QDataStream empty;
        //qDebug() << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
        //(empty,udpSocket,clients,clientCount,my_index,port,db);
    }
    }

}

//共识节点将共识数据发送给非共识节点
void consensusData(QUdpSocket& udpSocket, Client* clients, int clientCount, int my_index, int port, QSqlDatabase& db) {
    QString portInfoTableName = QString("info_%1").arg(port);
    if (clients[my_index].is_consensus_node == 1 ) {
        QSqlQuery query(db);
        if (!query.exec(QString("SELECT * FROM %1").arg(portInfoTableName))) {
            qDebug() << "Error: Failed to fetch data from " << portInfoTableName << " table:" ;
            //db.close();
            return;
        } else {
            QByteArray byteArray;
            QDataStream stream(&byteArray, QIODevice::WriteOnly);

            // 将数据添加到数据流中
            stream << 1; // 标记为共识数据

            while (query.next()) {
                int id = query.value(0).toInt();
                float trustValue = query.value(1).toFloat();
                //int blockDepth = query.value(2).toInt();

                // 将数据写入数据流
                stream << id << trustValue ;
            }
        for (int i = 0; i < clientCount; i++)
        {
            if(clients[i].is_out_node == 1)
            {
                continue;
            }
            if(clients[i].is_consensus_node != 1)
            {
                udpSocket.writeDatagram(byteArray, clients[i].address, clients[i].port);
            }

        }
    }

    //qDebug() << "ID:" << port << "发送共识数据";
    }
}


#endif // CONSENSUSMODE_H
