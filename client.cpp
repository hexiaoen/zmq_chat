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
    zmq_connect(sub_sock, PUB_PEER);

    talk_sock = zmq_socket(context, ZMQ_ROUTER);
    zmq_setsockopt(talk_sock, ZMQ_RCVTIMEO, &recv_timeout, sizeof(int));
    if( zmq_bind(talk_sock, "tcp://*:9441") <0)
        qDebug(zmq_strerror(zmq_errno()));

    QtConcurrent::run(this, &client::poll_thread);
}

client::~client()
{
    zmq_close(req_sock);
    zmq_close(talk_sock);
    zmq_ctx_destroy(context);
}

void client::poll_thread()
{

    while(true)
    {
        zmq_pollitem_t items[]=
        {
            {sub_sock, 0, ZMQ_POLLIN, 0},
            {talk_sock, 0, ZMQ_POLLIN, 0}
        };

       if(zmq_poll(items, 2, -1) < 0)
       {
           qDebug("zmq_poll error: %s", zmq_strerror(zmq_errno()));
           break;
       }

       if(items[0].revents & ZMQ_POLLIN)
       {
           /*first frame is prefix */
           char *pre = s_recvmore(sub_sock);
           free(pre);

           /*send frame is id*/
           char *update_id = s_recvmore(sub_sock);

           char *update_ip = s_recv(sub_sock);
           peer_info.insert(update_id, update_ip);
           free(update_id);
           free(update_ip);
           emit peer_info_change(peer_info);
       }

       if(items[1].revents & ZMQ_POLLIN)
       {
           qDebug("Recv msg from peer");
            router_info[id] = 1;
            char *peer_id = s_recvmore(talk_sock) ;
            char *empty = s_recvmore(talk_sock);
            char *msg = s_recv(talk_sock);

            get_talk_msg(peer_id, msg);

            free(peer_id);
            free(empty);
            free(msg);
       }

    }

}

void client::snd_msg(const QString &id, const QString &msg)
{
    if(router_info[id])
    {
        QString peer_ip =  peer_info[id].toString();
        QString peer_router = "tcp://"+ peer_ip +":9441";
        zmq_connect(talk_sock, peer_router.toStdString().c_str());
        router_info[id] = 1;
    }

    s_sendmore(talk_sock, id.toStdString().c_str());
    s_sendmore(talk_sock, "");
    s_sendmore(talk_sock, msg.toStdString().c_str());
}


void client::connect_to_srv(const QString &id)
{
    this->id = id;
    zmq_setsockopt(req_sock,ZMQ_IDENTITY, id.toStdString().c_str(), id.size());
    zmq_setsockopt(talk_sock,ZMQ_IDENTITY, id.toStdString().c_str(), id.size());
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

#if 0
            QVariantMap::iterator it = peer_info.begin();
            while(it != peer_info.end())
            {
                qDebug()<<it.key()<<it.value();
            }
#endif
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
