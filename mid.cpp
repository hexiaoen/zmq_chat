#include "mid.h"

mid::mid(QObject *parent) : QObject(parent)
{
    net = new client(parent);
    workThread = new QThread;
    net->moveToThread(workThread);
    connect(workThread, SIGNAL(finished()), this, SLOT(thread_finished()));

    connect(this, &mid::connectClicked, net, &client::connect_to_srv);

    connect(net, &client::srv_status,this, &mid::netStatusChange);
    connect(net, &client::peer_info_change,this, &mid::peer_info_change);
    connect(net, &client::get_talk_msg,this, &mid::get_talk_msg);

    workThread->start();
}

mid::~mid()
{
    workThread->terminate();
    delete net;
    delete workThread;
}

void mid::thread_finished()
{
    qDebug("thread exit");
}

void mid::connectCall(const QString &id)
{
    qDebug("connect clicked");
    emit netStatusChange(3);
    emit connectClicked(id);
}

void mid::snd_msg(const QString &id, const QString &msg)
{
    net->snd_msg(id, msg) ;
}
