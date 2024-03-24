#ifndef PROCESSDATAMODE_H
#define PROCESSDATAMODE_H
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
#include "CommunicateMode.h"
QMutex databaseMutex1;
bool isTableExists(QSqlDatabase& db, const QString& tableName) {
    QStringList tables = db.tables();
    return tables.contains(tableName, Qt::CaseInsensitive);
}



//更新区块链共识数据
void updateConsensusData(QDataStream &inStream,QUdpSocket& udpSocket, Client* clients, int clientCount, int my_index, int port, QSqlDatabase& db,int Consensus_Num)
{
    QString portInfoTableName = QString("info_%1").arg(port);
     QSqlQuery query(db);
    while (!inStream.atEnd()) {
        //                // 从流中读取 ID 和 trust_value
        int id;
        float trust_value;
        inStream >> id >> trust_value;
        //qDebug() << "ID:" << id << ", trustValue:" <<trust_value;
        //
        QString selectQuery = QString("SELECT * FROM %1 WHERE id = :port").arg(portInfoTableName);
        query.prepare(selectQuery);
        query.bindValue(":port", id); // 假设 'id' 已经在之前设置好
        databaseMutex1.lock();
        if (!query.exec()) {
            qDebug() << "错误：无法执行查询：" << query.lastError().text();
            databaseMutex1.unlock(); // 在出现错误时释放互斥锁
            //db.close();
            return;
        }
        //query.finish();
        databaseMutex1.unlock(); // 在出现错误时释放互斥锁

        if (query.next()) {
            int id = query.value(0).toInt();
            float trustValue = query.value(1).toFloat();
          //  int blockDepth = query.value(2).toInt();
            trustValue = (trustValue+trust_value)/(2.0);
            QString updateQuery = QString("UPDATE %1 SET trust_value = :trustValue WHERE id = :id").arg(portInfoTableName);
            query.prepare(updateQuery);
            query.bindValue(":id", id);
            query.bindValue(":trustValue", trustValue);
             databaseMutex1.lock();
            if (!query.exec()) {
                qDebug() << "错误：无法更新记录：" << query.lastError().text();
                databaseMutex1.unlock();
                //db.close();
                return;
            }
            query.finish();
            databaseMutex1.unlock();
           // qDebug() << "共识 ID:" << id << ", 信任值:" << trustValue << ", 区块深度:" << blockDepth;
        } else {
            qDebug() << "未找到 ID 为：" << id << " 的记录";
        }

    }

    QString blockchainTableName = QString("blockchain_%1").arg(port);
    QSqlQuery updateQuery(db);
    if (!query.exec(QString("SELECT * FROM %1").arg(portInfoTableName))) {
        qDebug() << "Error: Failed to fetch data from " << portInfoTableName << " table:" << query.lastError().text();
        //db.close();
        return;
    } else {
        QByteArray byteArray;
        QDataStream stream(&byteArray, QIODevice::WriteOnly);

        // 将数据标记为共识数据
        stream << 1;

        // 将数据库中的数据写入数据流中
        while (query.next()) {
            int id = query.value(0).toInt();
            float trustValue = query.value(1).toFloat();
            int blockDepth = query.value(2).toInt();

            // 写入数据流
            stream << id << trustValue ;
            updateQuery.prepare(QString("UPDATE %1 SET trust_value = :trust_value, block_depth = block_depth + 1 WHERE id = :id").arg(blockchainTableName));
            updateQuery.bindValue(":trust_value", trustValue);
            //updateQuery.bindValue(":block_depth", blockDepth);
            updateQuery.bindValue(":id", id);
            qDebug() <<"共识节点ID "<<port<< " 共识 ID:" << id << ", 信任值:" << trustValue << ", 区块深度:" << blockDepth+1;
            databaseMutex1.lock();
            if (!updateQuery.exec()) {
                qDebug() << "Error: Failed to update data in " << blockchainTableName << " table:" << query.lastError().text();
               // db.close();
                 databaseMutex1.unlock();
                return;
            }
            updateQuery.finish();
             databaseMutex1.unlock();

        }

        // 发送共识数据给非共识节点
        for (int i = 0; i < clientCount; ++i) {
            if (clients[i].is_consensus_node == 0 && i != my_index && clients[i].is_out_node == 0) {
                udpSocket.writeDatagram(byteArray, clients[i].address, clients[i].port);
            }
        }


    }
    db.commit();

    send_new_consensus_data(inStream,udpSocket, clients, clientCount, my_index,  port,  db,Consensus_Num);//TODO

}


void updateTrustValuesFromStream(QSqlDatabase& db, const QString& portInfoTableName, QDataStream& inStream ) {
    QSqlQuery query(db);
    //QString portInfoTableName = QString("info_%1").arg(port);
    while (!inStream.atEnd()) {
        int receivedID;
        float receivedTrustValue;
        inStream >> receivedID >> receivedTrustValue;

        QString selectQuery = QString("SELECT * FROM %1 WHERE id = :port").arg(portInfoTableName);
        query.prepare(selectQuery);
        query.bindValue(":port", receivedID);
         databaseMutex1.lock();
        if (!query.exec()) {
            qDebug() << "错误：无法执行查询：" << query.lastError().text();
           // db.close();
             databaseMutex1.unlock();
            return;
        }
        //query.finish();
         databaseMutex1.unlock();
        if (query.next()) {
            int idFromDB = query.value(0).toInt();
            float trustValueFromDB = query.value(1).toFloat();
            int blockDepth = query.value(2).toInt();
            float updatedTrustValue = (trustValueFromDB * (blockDepth + 1) + receivedTrustValue) / (blockDepth + 2.0);

            QString updateQuery = QString("UPDATE %1 SET trust_value = :trustValue WHERE id = :id").arg(portInfoTableName);
            query.prepare(updateQuery);
            query.bindValue(":id", idFromDB);
            query.bindValue(":trustValue", updatedTrustValue);
             databaseMutex1.lock();
            if (!query.exec()) {
                qDebug() << "错误：无法更新记录：" << query.lastError().text();
               //db.close();
                return;
                 databaseMutex1.unlock();
            }
            query.finish();
             databaseMutex1.unlock();
           // qDebug() << "ID:" << idFromDB << ", 信任值:" << updatedTrustValue << ", 区块深度:" << blockDepth;
        }
        else {
            qDebug() << "未找到 ID 为：" << receivedID << " 的记录";
        }
    }
    db.commit();
}

//提出blockchain信息，更新info_和clients信息
void tongbu( Client* clients, int& my_index, int port, QSqlDatabase& db) {
    QString blockchainTableName = QString("blockchain_%1").arg(port);
        QString infoTableName = QString("info_%1").arg(port);

        QSqlQuery selectQuery(db);
        QSqlQuery updateQuery(db);
        int count =0;
        if (!selectQuery.exec(QString("SELECT * FROM %1").arg(blockchainTableName))) {
            qDebug() << "Error: Failed to fetch data from " << blockchainTableName << " table:";
           // db.close();
            return;
        } else {
            while (selectQuery.next()) {
                int id = selectQuery.value(0).toInt();
                float trustValue = selectQuery.value(1).toFloat();
                int blockDepth = selectQuery.value(2).toInt();

                // 更新info表中的数据
                updateQuery.prepare(QString("UPDATE %1 SET trust_value = :trust_value, block_depth = :block_depth WHERE id = :id").arg(infoTableName));
                updateQuery.bindValue(":trust_value", trustValue);
                updateQuery.bindValue(":block_depth", blockDepth);
                updateQuery.bindValue(":id", id);
                 databaseMutex1.lock();
                if (!updateQuery.exec()) {
                    qDebug() << "Error: Failed to update data in " << infoTableName << " table123:" << updateQuery.lastError().text();
                    //db.close();
                     databaseMutex1.unlock();
                    return;
                }
                updateQuery.finish();
                 databaseMutex1.unlock();
                int id_ = selectQuery.value(0).toInt();
                float trust_value = selectQuery.value(1).toFloat();
                int block_depth = selectQuery.value(2).toInt();
               // int is_consensus_node = selectQuery.value(3).toInt();
                // int is_bad_node = selectQuery.value(4).toInt();
                int x = selectQuery.value(5).toInt();
                int y = selectQuery.value(6).toInt();
                clients[count].address = QHostAddress("127.0.0.1");
                clients[count].port = id_;
                if (port == id) {
                            my_index = count;

                        }
                clients[count].x=x;
                clients[count].y=y;
                //clients[count].is_consensus_node=is_consensus_node;
                //clients[count].is_bad_node=is_bad_node;
                clients[count].trust_value=trust_value;
                clients[count].block_depth=block_depth;
                if(trust_value<6)
                {
                    clients[count].is_out_node = 1;
                   // qDebug() <<"clients[count].is_out_node = 1 port ="<<clients[count].port;
                }
                count++;
            }
            //qDebug() << "Data updated successfully in " << infoTableName << " table!";
        }
        db.commit();
}

void updateInfoTableFromBlockchain(QDataStream &inStream,int my_index,QSqlDatabase& db, Client* clients, int clientCount, int port) {
    QString portOutputTableName = QString("output_%1").arg(port);
    QString portReceiveTableName = QString("receive_%1").arg(port);
    bool portOutputExists = isTableExists(db, portOutputTableName);
    bool portReceiveExists = isTableExists(db, portReceiveTableName);
    QSqlQuery query(db);
    int a =clientCount;
    a++;
    QSqlQuery updateQuery(db);
    QString blockchainTableName = QString("blockchain_%1").arg(port);
    QSqlQuery selectQuery(db);
    int blockDepth =0;

    if (!selectQuery.exec(QString("SELECT * FROM %1").arg(blockchainTableName))) {
        qDebug() << "Error: Failed to fetch data from " << blockchainTableName << " table:";
        // db.close();
        return;
    }
    else
    {
        if (selectQuery.next())
            blockDepth = selectQuery.value(2).toInt();
    }

    while (!inStream.atEnd())
    {
        //                // 从流中读取 ID 和 trust_value
        int id;
        float trust_value;
        inStream >> id >> trust_value;
        //int blockDepth = query.value(2).toInt();

        // 写入数据流

        updateQuery.prepare(QString("UPDATE %1 SET trust_value = :trust_value, block_depth = block_depth + 1 WHERE id = :id").arg(blockchainTableName));
        updateQuery.bindValue(":trust_value", trust_value);
       // updateQuery.bindValue(":block_depth", blockDepth);
        updateQuery.bindValue(":id", id);
        qDebug() <<"非共识节点ID "<<port<< " 共识 ID:" << id << ", 信任值:" << trust_value << ", 区块深度:" << blockDepth+1;
         databaseMutex1.lock();
        if (!updateQuery.exec()) {
            qDebug() << "Error: Failed to update data in " << blockchainTableName << " table:" << query.lastError().text();
           // db.close();
             databaseMutex1.unlock();
            return;
        }
        updateQuery.finish();
         databaseMutex1.unlock();

    }


    db.commit();


    tongbu(  clients,  my_index,  port, db);
    if (portOutputExists) {
            if (!query.exec(QString("DELETE FROM %1").arg(portOutputTableName))) {
                qDebug() << "Error: Failed to clear port_output table:" ;
               // db.close();
                return;
            }
    //        if (!query.exec(QString("DROP TABLE %1").arg(portInfoTableName))) {
    //            qDebug() << "Error: Failed to clear port_info table:" ;
    //            db.close();
    //            return;
    //        }
    } else {
        if (!query.exec(QString("CREATE TABLE IF NOT EXISTS %1 ("
                                "id INTEGER , "
                                "message QByteArray,"
                                "hash_result QByteArray "
                                ")").arg(portOutputTableName))) {
            qDebug() << "Error: Failed to create port_output table:" ;
            //db.close();
            return;
        } else {
            //qDebug() << "port_output table created successfully!";
        }
    }

    if (portReceiveExists) {
        if (!query.exec(QString("DELETE FROM %1").arg(portReceiveTableName))) {
            qDebug() << "Error: Failed to clear port_receive table:" ;
            //db.close();
            return;
        }
    } else {
        if (!query.exec(QString("CREATE TABLE IF NOT EXISTS %1 ("
                                "id INTEGER , "
                                "message QByteArray,"
                                "hash_result QByteArray "
                                ")").arg(portReceiveTableName))) {
            qDebug() << "Error: Failed to create port_receive table:" ;
           //db.close();
            return;
        } else {
            qDebug() << "port_receive table created successfully!";
        }
    }
    //db.close();
}

void update_client(QUdpSocket& udpSocket, Client* clients,int my_index,int port,int clientCount)
{
    Message temp;

    Client xx;
    temp.is_val = 3;
    temp.Source_ID = xx;
    temp.ID2 = xx;
    temp.ID1 = xx;
    QString str = QString::number(port); // 将整数转换为字符串
    temp.buffer= str.toUtf8(); // 将字符串转换为 QByteArray


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
    for (int i = 0; i < clientCount; i++) {
        if (clients[i].port == port) {
            continue;
        }
        if(euclideanDistance(clients[my_index].x,clients[my_index].y,clients[i].x,clients[i].y)>2)
        {
            continue;
        }
        udpSocket.writeDatagram(byteArray, clients[i].address, clients[i].port);
    }

}

//选出下轮的共识节点
void update_consensus_node(QDataStream &inStream,QUdpSocket& udpSocket, Client* clients, int clientCount, int my_index, int port, QSqlDatabase& db,int consensus_num,int senderPort){

    //qDebug() << "YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY";
    //int StrongPort;
    int max_port[20];
    for(int j=0;j<consensus_num;j++)
    {
        inStream>> max_port[j];

    }

//  测试
//    for(int j=0;j<consensus_num;j++)
//    {

//         qDebug() << "max_port["<<j+1<<"]:"<<max_port[j]<<"     ";
//    }

    if(senderPort == 0)
    {
        return;
    }
    for(int i =0;i<clientCount-1;i++)
    {
        clients[i].is_consensus_node =0;
//        if(clients[i].port == max_port[0] || clients[i].port == max_port[1])
//        {
//            clients[i].is_consensus_node =1;
//        }
        for(int j=0;j<consensus_num;j++)
        {
            if(clients[i].port==max_port[j])
                 clients[i].is_consensus_node =1;
        }
    }
//    ******************************************测试***************************************************
//    for(int j=0;j<clientCount-1;j++)
//    {
//        qDebug()<<clients[j].is_consensus_node;
//    }
    QString blockchainTableName = QString("blockchain_%1").arg(port);
    QSqlQuery updateQuery(db);
    updateQuery.prepare(QString("UPDATE %1 SET  is_consensus_node = 0 ").arg(blockchainTableName));
    databaseMutex1.lock();
    if (!updateQuery.exec()) {
        qDebug() << "Error: Failed to update data in " << blockchainTableName << " table:" ;
        // db.close();
        databaseMutex1.unlock();
        return;
    }
    updateQuery.finish();
    //databaseMutex.unlock();

    for(int i = 0;i<consensus_num;i++)
    {
        updateQuery.prepare(QString("UPDATE %1 SET  is_consensus_node = 1 WHERE id = :id").arg(blockchainTableName));
        updateQuery.bindValue(":id", max_port[i]);
    }
    //databaseMutex.lock();
    if (!updateQuery.exec()) {
        qDebug() << "Error: Failed to update data in " << blockchainTableName << " table:" ;
        // db.close();
        databaseMutex1.unlock();
        return;
    }
    updateQuery.finish();
    databaseMutex1.unlock();
    db.commit();
}

void update_exit_client(QDataStream& inStream, Client* clients, int clientCount, quint16 senderPort){

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
    int temp_port = receivedMessage.buffer.toInt();
    for(int i =0;i<clientCount;i++)
    {
        if(clients[i].port == temp_port)
        {
            clients[i].is_run_node = 1;
           // qDebug() << "YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY";
        }
    }

}

//（核心)处理信息
void processData(QUdpSocket& udpSocket, Client* clients, int clientCount, int my_index, int port,QSqlDatabase& db,int Consensus_Num) {
    while (udpSocket.hasPendingDatagrams())
    {
        QString portInfoTableName = QString("info_%1").arg(port);
        QByteArray buffer;
        buffer.resize(udpSocket.pendingDatagramSize());
        //qDebug() << "Received message 1234655";
        QHostAddress sender;
        quint16 senderPort = 0;
        udpSocket.readDatagram(buffer.data(), buffer.size(), &sender, &senderPort);
        QDataStream inStream(&buffer, QIODevice::ReadOnly);
        QSqlQuery query(db);
        QString portreceive_TableName = QString("receive_%1").arg(port);
        Message receivedMessage;
        int f=0;
        for(int i=0 ;i<clientCount;i++)
        {
            if(clients[i].is_out_node == 1 && clients[i].port ==senderPort)
            {
                f =1;
            }
        }
        if(f == 1)
        {
            continue;
        }

        //query.prepare("INSERT INTO " + portreceive_TableName + "(id,message,hash_result) VALUES (:port,:message,:hash_result)");
        // 读取数据并将其放入 receivedMessage 对象中
        int is_trust=0;
        inStream >>is_trust;
        if(is_trust == 0)//为工作信息
        {
            processDataAndForward(inStream,udpSocket, clients, clientCount, my_index, port, db, senderPort);

        }
        else if(is_trust == 2)//为变坏信息
        {
           turnBad(inStream, clients, my_index,  senderPort);
           // qDebug() << "YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY";

        }
        else if(is_trust == 3)//为更新client信息（已启动）
        {
           // qDebug() << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
            update_exit_client(inStream, clients, clientCount,senderPort);
        }
        else if(is_trust == 4)//为更新共识节点信息
        {
            //qDebug() << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
            update_consensus_node(inStream,udpSocket, clients, clientCount, my_index,  port,  db,Consensus_Num,senderPort);
        }
        else//共识信息
        {
            int flag =0;
            if (clients[my_index].is_consensus_node == 1) //是共识节点
            {
                for(int j=0;j<clientCount;j++)
                {
                    if(clients[j].is_consensus_node == 1 && clients[j].port == senderPort)
                    {
                        flag =1;
                    }
                }
                if(flag == 1)//是共识节点所发
                {
                    updateConsensusData(inStream,udpSocket, clients, clientCount, my_index,  port,  db,Consensus_Num);
                }
                else//不是共识节点所发
                {
                    updateTrustValuesFromStream(db, portInfoTableName,inStream);
                }

            }
            else//不是共识节点
            {
                   updateInfoTableFromBlockchain(inStream,my_index,db,clients,clientCount,port);
            }
        }
    }

}

#endif // PROCESSDATAMODE_H
