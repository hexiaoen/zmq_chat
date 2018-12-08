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
    char buf[1024] = "";
    talk_sock->readDatagram(buf, sizeof (buf));
}

void client::poll_thread()
{

    while(true)
    {
        zmq_pollitem_t items[]=
        {
            {sub_sock, 0, ZMQ_POLLIN, 0},
        };

       if(zmq_poll(items, 1, 1000*30) < 0)
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
           }
           else if(!strncmp(pre, "DOWN", 4))
           {
               /*local sub connect to peer pub*/
               QVariantMap::Iterator it = peer_info.find(update_id);
               if(it != peer_info.end())
                    peer_info.erase(it);
           }


           free(pre);
           free(update_id);
           free(update_ip);
           emit peer_info_change(peer_info);
       }

    }
}

void client::snd_msg(const QString &id, const QString &msg)
{
    QStringList qlist = peer_info[id].toString().split(":");
    QHostAddress node(qlist.first());

    talk_sock->writeDatagram(msg.toStdString().c_str(), msg.length(), node, qlist.last().toUShort());
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
                    QStringList qlist = it.value().toString().split(":");
                    QHostAddress node(qlist.first());

                    talk_sock->writeDatagram(it.key().toStdString().c_str(), it.key().toStdString().length(), node, qlist.last().toUShort());
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
