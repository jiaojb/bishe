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
//#include <openssl/ecdsa.h>


// 全局互斥锁，用于同步数据库访问
QMutex databaseMutex;
int SKIPCOUNT = 5;

QByteArray hashByteArray(const QByteArray& data, QCryptographicHash::Algorithm hashAlgorithm = QCryptographicHash::Md5) {
    // 创建哈希对象
    QCryptographicHash hash(hashAlgorithm);

    // 添加要哈希的数据
    hash.addData(data);

    // 返回哈希结果
    return hash.result();
}

double euclideanDistance(int x1, int y1, int x2, int y2) {
    double distance = qSqrt(qPow((x2 - x1), 2) + qPow((y2 - y1), 2));

    return distance;
}

bool isTableExists(QSqlDatabase& db, const QString& tableName) {
    QStringList tables = db.tables();
    return tables.contains(tableName, Qt::CaseInsensitive);
}
//更新区块链共识数据
void updateConsensusData(QDataStream &inStream,QUdpSocket& udpSocket, Client* clients, int clientCount, int my_index, int port, QSqlDatabase& db)
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
        databaseMutex.lock();
        if (!query.exec()) {
            qDebug() << "错误：无法执行查询：" << query.lastError().text();
            databaseMutex.unlock(); // 在出现错误时释放互斥锁
            //db.close();
            return;
        }
        //query.finish();
        databaseMutex.unlock(); // 在出现错误时释放互斥锁
        if (query.next()) {
            int id = query.value(0).toInt();
            float trustValue = query.value(1).toFloat();
          //  int blockDepth = query.value(2).toInt();
            trustValue = (trustValue+trust_value)/(2.0);
            QString updateQuery = QString("UPDATE %1 SET trust_value = :trustValue WHERE id = :id").arg(portInfoTableName);
            query.prepare(updateQuery);
            query.bindValue(":id", id);
            query.bindValue(":trustValue", trustValue);
             databaseMutex.lock();
            if (!query.exec()) {
                qDebug() << "错误：无法更新记录：" << query.lastError().text();
                databaseMutex.unlock();
                //db.close();
                return;
            }
            query.finish();
            databaseMutex.unlock();
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
            qDebug() << "共识 ID:" << id << ", 信任值:" << trustValue << ", 区块深度:" << blockDepth;
            //qDebug() <<"gengxinblock";
             databaseMutex.lock();
            if (!updateQuery.exec()) {
                qDebug() << "Error: Failed to update data in " << blockchainTableName << " table:" << query.lastError().text();
               // db.close();
                 databaseMutex.unlock();
                return;
            }
            updateQuery.finish();
             databaseMutex.unlock();

        }

        // 发送共识数据给非共识节点
        for (int i = 0; i < clientCount; ++i) {
            if (clients[i].is_consensus_node == 0 && i != my_index && clients[i].is_out_node == 0) {
                udpSocket.writeDatagram(byteArray, clients[i].address, clients[i].port);
            }
        }


    }
    db.commit();

    send_new_consensus_data();//TODO

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
         databaseMutex.lock();
        if (!query.exec()) {
            qDebug() << "错误：无法执行查询：" << query.lastError().text();
           // db.close();
             databaseMutex.unlock();
            return;
        }
        //query.finish();
         databaseMutex.unlock();
        if (query.next()) {
            int idFromDB = query.value(0).toInt();
            float trustValueFromDB = query.value(1).toFloat();
            int blockDepth = query.value(2).toInt();
            float updatedTrustValue = (trustValueFromDB * (blockDepth + 1) + receivedTrustValue) / (blockDepth + 2.0);

            QString updateQuery = QString("UPDATE %1 SET trust_value = :trustValue WHERE id = :id").arg(portInfoTableName);
            query.prepare(updateQuery);
            query.bindValue(":id", idFromDB);
            query.bindValue(":trustValue", updatedTrustValue);
             databaseMutex.lock();
            if (!query.exec()) {
                qDebug() << "错误：无法更新记录：" << query.lastError().text();
               //db.close();
                return;
                 databaseMutex.unlock();
            }
            query.finish();
             databaseMutex.unlock();
           // qDebug() << "ID:" << idFromDB << ", 信任值:" << updatedTrustValue << ", 区块深度:" << blockDepth;
        }
        else {
            qDebug() << "未找到 ID 为：" << receivedID << " 的记录";
        }
    }
    db.commit();
}
//处理工作数据
void processDataAndForward(QDataStream& inStream,QUdpSocket& udpSocket, Client* clients, int clientCount, int my_index, int port, QSqlDatabase& db, quint16 senderPort) {
    // ... 上述代码
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
                 databaseMutex.lock();
                if (!updateQuery.exec()) {
                    qDebug() << "Error: Failed to update data in " << infoTableName << " table123:" << updateQuery.lastError().text();
                    //db.close();
                     databaseMutex.unlock();
                    return;
                }
                updateQuery.finish();
                 databaseMutex.unlock();
                int id_ = selectQuery.value(0).toInt();
                float trust_value = selectQuery.value(1).toFloat();
                int block_depth = selectQuery.value(2).toInt();
                int is_consensus_node = selectQuery.value(3).toInt();
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
                clients[count].is_consensus_node=is_consensus_node;
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
        //qDebug() << "共识 ID:" << id << ", 信任值:" << trustValue << ", 区块深度:" << blockDepth;
         databaseMutex.lock();
        if (!updateQuery.exec()) {
            qDebug() << "Error: Failed to update data in " << blockchainTableName << " table:" << query.lastError().text();
           // db.close();
             databaseMutex.unlock();
            return;
        }
        updateQuery.finish();
         databaseMutex.unlock();

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
//变坏
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


void update_consensus_node(QDataStream& inStream, Client* clients, int clientCount, quint16 senderPort){

    //qDebug() << "YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY";
    int port1,port2;
    inStream>> port1
            >> port2;
    if(senderPort == 0)
    {
        return;
    }
    for(int i =0;i<clientCount;i++)
    {
        clients[i].is_consensus_node =0;
        if(clients[i].port == port1 || clients[i].port == port2)
        {
            clients[i].is_consensus_node =1;
        }
    }

}
void processData(QUdpSocket& udpSocket, Client* clients, int clientCount, int my_index, int port,QSqlDatabase& db) {
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
            qDebug() << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
            update_consensus_node(inStream, clients, clientCount,senderPort);
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
                    updateConsensusData(inStream,udpSocket, clients, clientCount, my_index,  port,  db);
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
    //查寻
//        if (!query.exec(QString("SELECT * FROM %1").arg(portReceiveTableName))) {
//                qDebug() << "Error: Failed to fetch data from " << portReceiveTableName << " table:" ;
//                db.close();
//                return;
//            } else {
//                qDebug() << "Data from " << portReceiveTableName << " table:";
//                while (query.next()) {
//                    int id = query.value(0).toInt();
//                    QString message = query.value(1).toString();
//                    QString hash = query.value(2).toString();


//                    qDebug() << "ID:" << id << ", message:" <<message<<",hash"<<hash;
//                }
//            }

}
//修改
void checkAndUpdateTrustValue(QSqlDatabase& db, Client* clients ,int clientCount,int my_index, int port) {//监测周边节点行为
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
//        int ff =0;
//        for (int j=0;j<clientCount;j++)
//        {
//            if(clients[j].port == id && euclideanDistance(clients[j].x,clients[j].y,clients[my_index].x,clients[my_index].y) > 2.0)
//            {
//                ff =1;
//            }
//        }

//        if(ff)
//        {
//            continue;
//        }
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


        //XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
        // 查询 output 表中的 id、message 和 hash_result
//        selectOutputQuery.prepare(QString("SELECT id, message, hash_result FROM %1 WHERE id = :id").arg(portOutputTableName));
//        selectOutputQuery.bindValue(":id", id);
//        databaseMutex.lock();
//        if (!selectOutputQuery.exec()) {
//            qDebug() << "Error: Failed to fetch data from " << portOutputTableName << " table:";
//           // db.close();8080
//            //return;
//        }
//        databaseMutex.unlock();
//        int count =0;
//       // selectOutputQuery.bindValue(":id", id);
//        while (selectOutputQuery.next() && count < 3) {
//            //qDebug()<< "XXXXXXXXXXXXXXXXXXXXXX ";
//            int outputId = selectOutputQuery.value(0).toInt();
//            QByteArray message = selectOutputQuery.value(1).toByteArray();
//            //QByteArray hashResult = selectOutputQuery.value(2).toByteArray();

//            // 查询 receive 表中是否有相同的 id、message 和 hash
//            selectReceiveQuery.prepare(QString("SELECT * FROM %1 WHERE id = :id AND message = :message ").arg(portReceiveTableName));
//            selectReceiveQuery.bindValue(":id", outputId);
//            selectReceiveQuery.bindValue(":message", message);
//            //selectReceiveQuery.bindValue(":hash", hashResult);
//            //qDebug() << "Output ID:" << outputId << "Message:" << message ;

//            if (!selectReceiveQuery.exec()) {
//                qDebug() << "Error: Failed to fetch data from " << portReceiveTableName << " table:" << selectReceiveQuery.lastError().text();
//                //db.close();
//                //return;
//                //continue;
//            }

//            // 如果没有找到匹配项，则将相应的 trust_value 减 1
//            if (!selectReceiveQuery.next()) {
//                trustValue -= 1;
//                //qDebug() <<id<<"trustValue -= 1 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
//                // 更新 info 表中的 trust_value
//                updateQuery.prepare(QString("UPDATE %1 SET trust_value = :trustValue WHERE id = :id").arg(infoTableName));
//                updateQuery.bindValue(":trustValue", trustValue);
//                updateQuery.bindValue(":id", id);
//                databaseMutex.lock();
//                if (!updateQuery.exec()) {
//                    qDebug() << "Error: Failed to update data in " << infoTableName << " table:" ;
//                    //db.close();
//                    //return;
//                }
//                databaseMutex.unlock();
//                break;
//            }
            //count++;
//        }
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

//创世块建立
void createBlockChainTable(QSqlDatabase& db, int port,QList<int> intList)
{
    // Open SQLite database


    // Create BlockChain table
    QSqlQuery query(db);
    QString database =  QString("blockchain_%1").arg(port);
    if (!query.exec(QString("DROP TABLE IF EXISTS %1").arg(database))) {
            qDebug() << "Error: Failed to drop BlockChain table:";
            //db.close();
            return;
        } else {
            // qDebug() << "BlockChain table dropped successfully!";
        }
    if (!query.exec(QString("CREATE TABLE IF NOT EXISTS %1("
                                 "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                 "trust_value FLOAT, "
                                 "block_depth INTEGER, "
                                 "is_consensus_node INTEGER,"
                                 "is_bad_node INTEGER,"
                                 "x INTEGER,"
                                 "y INTEGER)").arg(database))) {
            qDebug() << "Error: Failed to create BlockChain table:";
            //db.close();
            return;
        } else {
           // qDebug() << "BlockChain table created successfully!";
        }
    // Close the database connection
    QString insertQuery = QString("INSERT INTO %1 (id, trust_value, block_depth, is_consensus_node, is_bad_node, x, y) "
                                      "VALUES (:id, :trust_value, :block_depth, :is_consensus_node, :is_bad_node, :x, :y)").arg(database);
        // Example data to be inserted
        query.prepare(insertQuery);
    Point p;
    p.x = 1;
    p.y = 1;
    int clientCount =  intList.size();

    for (int i = 0; i < clientCount; i++) {

        query.bindValue(":id", intList[i]);
        query.bindValue(":trust_value", 10); // Example trust value
        query.bindValue(":block_depth", 0); // Example block depth
        query.bindValue(":is_consensus_node", 0); // Example is consensus node
        query.bindValue(":is_bad_node", 0);
        query.bindValue(":x", p.x); // X value from clients array
        query.bindValue(":y", p.y); // Y value from clients array
        databaseMutex.lock();
        query.exec();
        query.finish();
        databaseMutex.unlock();
        // Execute the query
//        if (!query.exec()) {
//            qDebug() << "Error: Failed to insert data into BlockChain table:" ;
//        } else {
//            qDebug() << "Data inserted successfully into BlockChain table!";
//        }
    }


       // Prepare the query
        //设置共识节点
    QString updateQuery = QString("UPDATE %1 SET "
                              "is_consensus_node = :is_consensus_node WHERE id = :id").arg(database);

        // 准备查询
        query.prepare(updateQuery);

        // 绑定值到占位符
        query.bindValue(":id", intList[1]);

        query.bindValue(":is_consensus_node", 1);

        // 执行更新
        databaseMutex.lock();
        if (!query.exec()) {
            qDebug() << "错误：无法在 BlockChain 表中更新数据：" ;
        }
        query.finish();
        databaseMutex.unlock();

        //设置共识节点
    updateQuery = QString("UPDATE %1 SET "
                              "is_consensus_node = :is_consensus_node WHERE id = :id").arg(database);

        // 准备查询
        query.prepare(updateQuery);

        // 绑定值到占位符
        query.bindValue(":id", intList[2]);

        query.bindValue(":is_consensus_node", 1);

        // 执行更新
        databaseMutex.lock();
        if (!query.exec()) {
            qDebug() << "错误：无法在 BlockChain 表中更新数据：" ;
        }
        query.finish();
        databaseMutex.unlock();

//        //设置坏节点
//        updateQuery = QString("UPDATE %1 SET "
//                                  "is_bad_node = :is_bad_node WHERE id = :id").arg(database);

//            // 准备查询
//            query.prepare(updateQuery);

//            // 绑定值到占位符
//            query.bindValue(":id", 8081);

//            query.bindValue(":is_bad_node", 1);

//            // 执行更新
//            databaseMutex.lock();
//            if (!query.exec()) {
//                qDebug() << "错误：无法在 BlockChain 表中更新数据：" ;
//            }
//            query.finish();
//            databaseMutex.unlock();

        //查寻
            if (!query.exec(QString("SELECT * FROM %1").arg(database))) {
                qDebug() << "Error: Failed to fetch data from " << database << " table:" ;
                // db.close();
                return;
            } else {
                qDebug() << "Data from " << database << " table:";
                while (query.next()) {
                    int id = query.value(0).toInt();
                    float trust_value = query.value(1).toFloat();
                    int block_depth = query.value(2).toInt();
                    int is_consensus_node = query.value(3).toInt();
                    int is_bad_node = query.value(4).toInt();
                    int x = query.value(5).toInt();
                    int y = query.value(6).toInt();

                    qDebug() << "ID:" << id << ", Trust Value:" << trust_value << ", Block Depth:" << block_depth
                             << ", Is Consensus Node:" << is_consensus_node << ", Is BAD Node:" << is_bad_node << ", X:" << x << ", Y:" << y;
                }
            }
            db.commit();

}

//创世块建立


//void createBlockChainTable(QSqlDatabase& db, int port,QList<int> intList)
//{
//    // Open SQLite database


//    // Create BlockChain table
//    QSqlQuery query(db);
//    QString database =  QString("blockchain_%1").arg(port);
//    if (!query.exec(QString("DROP TABLE IF EXISTS %1").arg(database))) {
//            qDebug() << "Error: Failed to drop BlockChain table:";
//            //db.close();
//            return;
//        } else {
//            // qDebug() << "BlockChain table dropped successfully!";
//        }
//    if (!query.exec(QString("CREATE TABLE IF NOT EXISTS %1("
//                                 "id INTEGER PRIMARY KEY AUTOINCREMENT, "
//                                 "trust_value FLOAT, "
//                                 "block_depth INTEGER, "
//                                 "is_consensus_node INTEGER,"
//                                 "is_bad_node INTEGER,"
//                                 "x INTEGER,"
//                                 "y INTEGER)").arg(database))) {
//            qDebug() << "Error: Failed to create BlockChain table:";
//            //db.close();
//            return;
//        } else {
//           // qDebug() << "BlockChain table created successfully!";
//        }
//    // Close the database connection
//    QString insertQuery = QString("INSERT INTO %1 (id, trust_value, block_depth, is_consensus_node, is_bad_node, x, y) "
//                                      "VALUES (:id, :trust_value, :block_depth, :is_consensus_node, :is_bad_node, :x, :y)").arg(database);
//        // Example data to be inserted
//        query.prepare(insertQuery);
//    Point p[10];
//    p[0].x = 1;
//    p[0].y = 1;

//    p[1].x = 3;
//    p[1].y = 1;

//    p[2].x = 2;
//    p[2].y = 2;

//    p[3].x = 2;
//    p[3].y = 3;

//    p[4].x = 3;
//    p[4].y = 3;
//    int clientCount = 5;
//    for (int i = 0, host = 8080; i < clientCount; i++, host++) {
//        query.bindValue(":id", host);
//        query.bindValue(":trust_value", 10); // Example trust value
//        query.bindValue(":block_depth", 0); // Example block depth
//        query.bindValue(":is_consensus_node", 0.0); // Example is consensus node
//        query.bindValue(":is_bad_node", 0);
//        query.bindValue(":x", p[i].x); // X value from clients array
//        query.bindValue(":y", p[i].y); // Y value from clients array
//        databaseMutex.lock();
//        query.exec();
//        query.finish();
//        databaseMutex.unlock();
//        // Execute the query
////        if (!query.exec()) {
////            qDebug() << "Error: Failed to insert data into BlockChain table:" ;
////        } else {
////            qDebug() << "Data inserted successfully into BlockChain table!";
////        }
//    }


//       // Prepare the query
//        //设置共识节点
//    QString updateQuery = QString("UPDATE %1 SET "
//                              "is_consensus_node = :is_consensus_node WHERE id = :id").arg(database);

//        // 准备查询
//        query.prepare(updateQuery);

//        // 绑定值到占位符
//        query.bindValue(":id", 8082);

//        query.bindValue(":is_consensus_node", 1);

//        // 执行更新
//        databaseMutex.lock();
//        if (!query.exec()) {
//            qDebug() << "错误：无法在 BlockChain 表中更新数据：" ;
//        }
//        query.finish();
//        databaseMutex.unlock();

//        //设置共识节点
//    updateQuery = QString("UPDATE %1 SET "
//                              "is_consensus_node = :is_consensus_node WHERE id = :id").arg(database);

//        // 准备查询
//        query.prepare(updateQuery);

//        // 绑定值到占位符
//        query.bindValue(":id", 8084);

//        query.bindValue(":is_consensus_node", 1);

//        // 执行更新
//        databaseMutex.lock();
//        if (!query.exec()) {
//            qDebug() << "错误：无法在 BlockChain 表中更新数据：" ;
//        }
//        query.finish();
//        databaseMutex.unlock();

//        //设置坏节点
//        updateQuery = QString("UPDATE %1 SET "
//                                  "is_bad_node = :is_bad_node WHERE id = :id").arg(database);

//            // 准备查询
//            query.prepare(updateQuery);

//            // 绑定值到占位符
//            query.bindValue(":id", 8081);

//            query.bindValue(":is_bad_node", 1);

//            // 执行更新
//            databaseMutex.lock();
//            if (!query.exec()) {
//                qDebug() << "错误：无法在 BlockChain 表中更新数据：" ;
//            }
//            query.finish();
//            databaseMutex.unlock();

//        //查寻
//            if (!query.exec(QString("SELECT * FROM %1").arg(database))) {
//                qDebug() << "Error: Failed to fetch data from " << database << " table:" ;
//                // db.close();
//                return;
//            } else {
//                qDebug() << "Data from " << database << " table:";
//                while (query.next()) {
//                    int id = query.value(0).toInt();
//                    float trust_value = query.value(1).toFloat();
//                    int block_depth = query.value(2).toInt();
//                    int is_consensus_node = query.value(3).toInt();
//                    int is_bad_node = query.value(4).toInt();
//                    int x = query.value(5).toInt();
//                    int y = query.value(6).toInt();

//                    qDebug() << "ID:" << id << ", Trust Value:" << trust_value << ", Block Depth:" << block_depth
//                             << ", Is Consensus Node:" << is_consensus_node << ", Is BAD Node:" << is_bad_node << ", X:" << x << ", Y:" << y;
//                }
//            }
//            db.commit();

//}



void createOrClearTableForPort(QSqlDatabase& db,int port) {//num几个

    QString portInfoTableName = QString("info_%1").arg(port);
    QString portOutputTableName = QString("output_%1").arg(port);
    QString portReceiveTableName = QString("receive_%1").arg(port);

    // Check if tables exist
    bool portInfoExists = isTableExists(db, portInfoTableName);
    bool portOutputExists = isTableExists(db, portOutputTableName);
    bool portReceiveExists = isTableExists(db, portReceiveTableName);

    QSqlQuery query(db);

    if (portInfoExists) {
        // Clear port_info table
        if (!query.exec(QString("DELETE FROM %1").arg(portInfoTableName))) {
            qDebug() << "Error: Failed to clear port_info table:" ;
           // db.close();
            return;
        }
//        if (!query.exec(QString("DROP TABLE %1").arg(portInfoTableName))) {
//            qDebug() << "Error: Failed to clear port_info table:" ;
//            db.close();
//            return;
//        }
    } else {
        // Create port_info table
        if (!query.exec(QString("CREATE TABLE IF NOT EXISTS %1 ("
                                 "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                 "trust_value FLOAT, "
                                 "block_depth INTEGER)").arg(portInfoTableName))) {
            qDebug() << "Error: Failed to create port_info table:" << query.lastError().text() ;
           // db.close();
            return;
        } else {//

            //qDebug() << "port_info table created successfully!";
        }
    }
    QString database = QString("blockchain_%1").arg(port);
    QSqlQuery insertQuery(db);
    //初始化info_port
    if (!query.exec(QString("SELECT * FROM %1").arg(database))) {
        qDebug() << "Error: Failed to fetch data from " << database << " table:" ;
        // db.close();
        return;
    } else {
        //qDebug() << "Data from " << database << " table:";
        while (query.next()) {
            //                    int id = query.value(0).toInt();
            //                    QString message = query.value(1).toString();
            int id = query.value(0).toInt();
            float trustValue = query.value(1).toFloat();
            int blockDepth = query.value(2).toInt();
            insertQuery.prepare(QString("INSERT INTO %1 (id, trust_value, block_depth) VALUES (:id, :trust_value, :block_depth)").arg(portInfoTableName));
            insertQuery.bindValue(":id", id);
            insertQuery.bindValue(":trust_value", trustValue);
            insertQuery.bindValue(":block_depth", blockDepth);
            databaseMutex.lock();
            if (!insertQuery.exec()) {
                qDebug() << "Error: Failed to insert data into destination_table:" << insertQuery.lastError().text();
                //             db.close();
                databaseMutex.unlock();
                return;
            }
            insertQuery.finish();
            databaseMutex.unlock();
            // qDebug() << "ID:" << id << ", trustValue:" <<trustValue<<",blockDepth"<<blockDepth;
        }
    }
//        //查寻
//            if (!query.exec(QString("SELECT * FROM %1").arg(portInfoTableName))) {
//                    qDebug() << "Error: Failed to fetch data from " << portInfoTableName << " table:" ;
//                    db.close();
//                    return;
//                } else {
//                    qDebug() << "Data from " << portInfoTableName << " table:";
//                    while (query.next()) {
//                        int id = query.value(0).toInt();
//                        float trustValue = query.value(1).toFloat();
//                        int blockDepth = query.value(2).toInt();


//                        qDebug() << "ID:" << id << ", trustValue:" <<trustValue<<",blockDepth"<<blockDepth;
//                    }
//                }
    // Repeat the same process for port_output and port_receive tables...

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
          //  db.close();
            return;
        } else {
            //qDebug() << "port_output table created successfully!";
        }
    }

    if (portReceiveExists) {
        if (!query.exec(QString("DELETE FROM %1").arg(portReceiveTableName))) {
            qDebug() << "Error: Failed to clear port_receive table:" ;
          //  db.close();
            return;
        }
    } else {
        if (!query.exec(QString("CREATE TABLE IF NOT EXISTS %1 ("
                                 "id INTEGER , "
                                 "message QByteArray,"
                                "hash_result QByteArray "
                                ")").arg(portReceiveTableName))) {
            qDebug() << "Error: Failed to create port_receive table:" ;
          //  db.close();
            return;
        } else {
            qDebug() << "port_receive table created successfully!";
        }
    }

    //db.close();
    db.commit();
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

int main(int argc, char *argv[]) {


    QCoreApplication a(argc, argv);
    int control_port;
    control_port = atoi(argv[1]);
     qDebug() << "control_port: "<<control_port;
    // 创建UDP socket
    QUdpSocket udpSocket;
    // 绑定到本地端口
    int port;
    //qDebug() << "Enter a port: ";
    //::cin >> port;
    port= atoi(argv[2]);
    udpSocket.bind(QHostAddress::AnyIPv4, static_cast<quint16>(port));

    Client clients[20];
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    QString database = QString("BlockChain_%1.db").arg(port);
    db.setDatabaseName(database);

        if (!db.open()) {
            qDebug() << "Error: Failed to open database:" ;
            return -1;
        } else {
            //qDebug() << "Database opened successfully!";
        }
        // 从命令行参数中获取字符串
        QString arg3 = QString(argv[3]);

        // 使用正则表达式匹配所有整数部分（包括负数）
        QRegularExpression re("-?\\d+");
        QStringList integers;

        // 迭代匹配的结果，并将整数添加到列表中
        QRegularExpressionMatchIterator matchIter = re.globalMatch(arg3);
        while (matchIter.hasNext()) {
            QRegularExpressionMatch match = matchIter.next();
            QString integer = match.captured();
            integers.append(integer);
        }

        // 将字符串列表中的整数转换为 int 类型
        QList<int> intList;
        for (const QString &str : integers) {
            intList.append(str.toInt());
        }

        // 输出转换后的整数列表
       // qDebug() << "转换后的整数列表：" << intList;

    createBlockChainTable(db,port,intList);//设置创世块

        // Create QSqlQuery instance
        QSqlQuery query(db);

        // Execute query to count records in BlockChain table
        if (!query.exec(QString("SELECT COUNT(*) FROM blockchain_%1").arg(port))) {
            qDebug() << "Error: Failed to count records:" ;
          //  db.close();
            return -1;
        }

        // Retrieve the count
   int clientCount = 0;
   if (query.next()) {
    clientCount = query.value(0).toInt();
    }
    int my_index = -1;
    int count=0;
    if (!query.exec(QString("SELECT * FROM blockchain_%1").arg(port))) {
            qDebug() << "Error: Failed to fetch data from BlockChain table:" << query.lastError().text();
          //  db.close();

        } else {
           // qDebug() << "Data from BlockChain table:";
            while (query.next()) {
                int id = query.value(0).toInt();
                float trust_value = query.value(1).toFloat();
                int block_depth = query.value(2).toInt();
                int is_consensus_node = query.value(3).toInt();
                 int is_bad_node = query.value(4).toInt();
                int x = query.value(5).toInt();
                int y = query.value(6).toInt();
                clients[count].address = QHostAddress("127.0.0.1");
                clients[count].port = id;
                if (port == id) {
                            my_index = count;

                        }
                clients[count].x=x;
                clients[count].y=y;
                clients[count].is_consensus_node=is_consensus_node;
                clients[count].is_bad_node=is_bad_node;
                clients[count].trust_value=trust_value;
                clients[count].block_depth=block_depth;
                clients[count].is_run_node = 0;
                clients[count].is_out_node = 0;
                count++;
            }
        }
    clients[my_index].is_run_node = 1;
    clients[count].x=0;
    clients[count].y=0;
    clients[count].is_consensus_node=0;
    clients[count].is_bad_node=0;
    clients[count].trust_value=0;
    clients[count].block_depth=0;
    clients[count].is_run_node = 1;
    clients[count].is_out_node = 0;
    clientCount++;
   // update_client(udpSocket, clients, my_index,port,clientCount);
    //createOrClearTableForPort(db,port);//设置自身数据库
    qDebug() << port << "号无人机启动，监听端口" << port;


    createOrClearTableForPort(db,port);//设置自身数据库
   // changetable(db,port);
    // 创建定时器，广播client信息  ，此处广播
    QTimer sendTimer_clients;
    sendTimer_clients.setInterval(5000); // 5秒发送一次数据

    // 创建定时器 发送业务数据，数据格式定义  ，此处广播
    QTimer sendTimer_transactions;
    sendTimer_transactions.setInterval(5000); // 5秒发送一次数据

    // 创建定时器 发送共识数据，发送评估数据集，此处组播
    QTimer sendTimer_trust_estimated;
    sendTimer_trust_estimated.setInterval(10000); // 10秒发送一次数据

    QObject::connect(&sendTimer_transactions, &QTimer::timeout, [&]() {
            sendData(udpSocket, clients, clientCount, my_index, port,db);
        });

    QTimer sendTimer_trust_consensus;
    sendTimer_trust_consensus.setInterval(7000); // 12秒发送一次数据
    //提出blockchain信息，更新info_和clients信息
    QTimer sendTimer_tongbu;
    sendTimer_tongbu.setInterval(9000); // 1秒发送一次数据

    QTimer sendTimer_gongshi;
    sendTimer_gongshi.setInterval(10000); // 1秒发送一次数据

    QTimer sendTimer_Score;
    sendTimer_Score.setInterval(6000); // 1秒发送一次数据
    // 定时器触发 主观信任数据发送
    //非共识节点发送共识数据给共识节点
     QObject::connect(&sendTimer_trust_estimated, &QTimer::timeout, [&]() {
         sendTrustData(udpSocket, clients, clientCount, my_index, port,db);

     });
    //共识节点之间相互发送共识数据
     QObject::connect(&sendTimer_gongshi, &QTimer::timeout, [&]() {
         gongshi(udpSocket, clients, clientCount, my_index, port,db);

     });
    //处理接收到的数据
     QObject::connect(&udpSocket, &QUdpSocket::readyRead, [&]() {
             processData(udpSocket, clients, clientCount, my_index, port,db);
         });

    //共识节点将共识数据发送给非共识节点
     QObject::connect(&sendTimer_trust_consensus, &QTimer::timeout, [&]() {
             consensusData(udpSocket, clients, clientCount, my_index, port,db);
         });
        //提出blockchain信息，更新info_和clients信息
     QObject::connect(&sendTimer_tongbu, &QTimer::timeout, [&]() {
             tongbu( clients, my_index, port,db);
         });
     QObject::connect(&sendTimer_Score, &QTimer::timeout, [&]() {
         checkAndUpdateTrustValue(db, clients, clientCount, my_index, port);

     });
     QObject::connect(&sendTimer_clients, &QTimer::timeout, [&]() {
         update_client(udpSocket, clients, my_index,port,clientCount);

      });
    // 启动定时器， 考虑发数据给控制台
    sendTimer_trust_estimated.start();

    sendTimer_transactions.start();

    sendTimer_trust_consensus.start();

    sendTimer_tongbu.start();

    sendTimer_gongshi.start();

    sendTimer_Score.start();

    sendTimer_clients.start();
    return a.exec();
}
