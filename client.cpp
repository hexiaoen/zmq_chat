#include <QDebug>
#include <QtConcurrent/QtConcurrentRun>
#include "client.h"


/*srv_status:
 * 0:success
 * 1:check fail
 * 2:time out
 */

client::client(QObject *parent) : QObject(parent)
{
    context = zmq_ctx_new();

    int recv_timeout = 3000;
    req_sock = zmq_socket(context, ZMQ_REQ);
    zmq_setsockopt(req_sock, ZMQ_RCVTIMEO, &recv_timeout, sizeof(int));

    sub_sock = zmq_socket(context, ZMQ_SUB);
    zmq_setsockopt(sub_sock, ZMQ_SUBSCRIBE, "UP", 2);
    zmq_setsockopt(sub_sock, ZMQ_SUBSCRIBE, "DOWN", 4);
    zmq_connect(sub_sock, PUB_PEER);
    QtConcurrent::run(this, &client::poll_thread);

    talk_sock = new QUdpSocket;
    talk_sock->bind(9440);

    connect(talk_sock, &QUdpSocket::readyRead, this, &client::node_msg);
}

client::~client()
{
    delete talk_sock;
    zmq_close(req_sock);
    zmq_close(sub_sock);
    //zmq_ctx_destroy(context);
}

void client::node_msg()
{
    QHostAddress host;
    quint16 port;
    msg_t  msg;
    msg.clear();
    talk_sock->readDatagram((char *)&msg, msg.length(), &host, &port);

    if(msg.node_id != this->id)
        qDebug("msg id error: %s-->%s", msg.self_id, msg.node_id);

    QString peer_id = msg.self_id;
    QMap<QString, node_info_t>::iterator it = node_info_map.find(peer_id);

    if(msg.msg_type == MSG_HELLO)
    {
        if(it != node_info_map.end())
        {
            it->expire = QTime::currentTime();
            it->stat = STAT_CONNECTED;
        }
        else
        {
            node_info_t ni;
            ni.expire = QTime::currentTime();
            ni.ip = host.toString();
            ni.port = port;
            ni.expire = QTime::currentTime();
            ni.stat = STAT_CONNECTED;

            node_info_map.insert(peer_id, ni);
        }
        msg.pack(MSG_ACK, peer_id, this->id);
        talk_sock->writeDatagram((const char*)&msg, msg.length(),host, port);
    }
    else if(msg.msg_type == MSG_ACK || msg.msg_type == MSG_HEARTBEAT)
    {
        /*we do not handle peer node without register*/
        if(it == node_info_map.end())
            return;

        it->expire = QTime::currentTime();
        it->stat = STAT_CONNECTED;
    }
    else if(msg.msg_type == MSG_DATA)
    {
        /*we do not handle peer node without register*/
        if(it == node_info_map.end())
            return;

        it->expire = QTime::currentTime();
        it->stat = STAT_CONNECTED;

        get_talk_msg(peer_id, msg.content);
    }
}

void client::poll_thread()
{

    while(true)
    {
        zmq_pollitem_t items[]=
        {
            {sub_sock, 0, ZMQ_POLLIN, 0},
        };

       if(zmq_poll(items, 1, CHECK_INTERVAL) < 0)
       {
           qDebug("zmq_poll error: %s", zmq_strerror(zmq_errno()));
           break;
       }

       if(items[0].revents & ZMQ_POLLIN)
       {
           /*first frame is prefix */
           char *pre = s_recvmore(sub_sock);
           /*second frame is id*/
           char *update_id = s_recvmore(sub_sock);
           char *update_ip = s_recv(sub_sock);

           if(!strncmp(pre,"UP", 2))
           {
               /*local sub connect to peer pub*/
               qDebug()<<update_id<<"@"<<update_ip;
               peer_info.insert(update_id, update_ip);

               QStringList qlist = QString(update_id).split(":");

               node_info_t ni;
               ni.ip = qlist.first();
               ni.port = qlist.last().toUShort();
               ni.stat = STAT_HELLO;
               ni.expire = QTime::currentTime();
               node_info_map.insert(update_id, ni);
           }
           else if(!strncmp(pre, "DOWN", 4))
           {
               /*local sub connect to peer pub*/
               QVariantMap::Iterator it = peer_info.find(update_id);
               if(it != peer_info.end())
                    peer_info.erase(it);

               node_info_map.remove(update_id);
           }


           free(pre);
           free(update_id);
           free(update_ip);
           emit peer_info_change(peer_info);
       }


       QMap<QString, node_info_t>::iterator it = node_info_map.begin();
       while(it != node_info_map.end())
       {
           msg_t msg;
           QHostAddress node_ip(it->ip);
            if(it->stat == STAT_CONNECTED )
            {
                if(it->expire.secsTo(QTime::currentTime()) > CHECK_INTERVAL)
                    it->stat = STAT_HELLO;
                else
                {
                    msg.pack(MSG_HEARTBEAT, it.key(), this->id);
                    talk_sock->writeDatagram((const char *)&msg, msg.length(), node_ip, it->port);
                }
            }
            else if(it->stat == STAT_HELLO)
            {
                /*this node is offline*/
                if(it->expire.secsTo(QTime::currentTime()) > CHECK_INTERVAL * MAX_TRY_COUNT)
                {
                    peer_info.remove(it.key());
                    peer_info_change(peer_info);
                    it = node_info_map.erase(it);
                    continue;
                }

                msg.pack(MSG_HELLO, it.key(), this->id);
                talk_sock->writeDatagram((const char *)&msg, msg.length(), node_ip, it->port);
            }
            it++;
       }
    }
}

void client::snd_msg(const QString &id, const QString &data)
{
    QMap<QString, node_info_t>::iterator it;
    it = node_info_map.find(id);
    if(it == node_info_map.end())
        return ;

    if(it->stat != STAT_CONNECTED)
        qDebug("peer is not connected");

    msg_t msg;
    msg.pack(MSG_DATA, id, this->id, data);
    QHostAddress node_ip(it->ip);
    talk_sock->writeDatagram((const char *)&msg, msg.length(), node_ip, it->port);
}


void client::connect_to_srv(const QString &id)
{
    this->id = id;
    zmq_setsockopt(req_sock,ZMQ_IDENTITY, id.toStdString().c_str(), id.size());
    zmq_connect(req_sock, LOGIN_PEER);

    /*step 1: check id*/
    s_send(req_sock, "CHECK_ID");

    char *msg = nullptr;
    if((msg = s_recv(req_sock)))
    {
        if(!strcmp(msg, "OK"))
        {
            free (msg);
            msg = nullptr;
            emit srv_status(0);

            QHostAddress srv(SRV_IP);
            talk_sock->writeDatagram(id.toStdString().c_str(), id.length(), srv, 9440);

            /*step 2 request peer info*/
            s_send(req_sock, "PEER_INFO");
            do
            {
                char *id = s_recv(req_sock);
                if(id)
                {
                    /*id length == 0,means message transport finish*/
                    if(strlen(id) == 0)
                        break;
                    char *ip = s_recv(req_sock);
                    if(ip)
                        peer_info.insert(id, ip);
                }
            }while(has_more(req_sock));

            QVariantMap::iterator it = peer_info.begin();
            while(it != peer_info.end())
            {
                if(it.key() != this->id)
                {
                    /*send hello to every peer node*/
                    QStringList qlist = it.value().toString().split(":");
                    QHostAddress node_ip(qlist.first());

                    msg_t hello_msg;
                    hello_msg.pack(MSG_HELLO, it.key(), this->id);
                    talk_sock->writeDatagram((const char *)&hello_msg, hello_msg.length(), node_ip, qlist.last().toUShort());

                    /*save every peer node info in node_stat */
                    node_info_t ni;
                    ni.ip = qlist.first();
                    ni.port = qlist.last().toUShort();
                    ni.stat = STAT_HELLO;
                    ni.expire = QTime::currentTime();
                    node_info_map.insert(hello_msg.node_id, ni);
                }

                it++;
            }

            emit peer_info_change(peer_info);
        }
        else
        {
            emit srv_status(1);
            free (msg);
        }
    }
    else
        emit srv_status(2);

}
