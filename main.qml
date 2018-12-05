import QtQuick 2.9
import QtQuick.Window 2.2
import QtQuick.Controls 1.1
import he.qt.client 1.0


Window {
    id:father
    visible: true
    width: 640
    height: 480
    title: qsTr("CHAT")

    property real brw: father.width/640
    property real brh: father.height/480
    property var  g_peer_info
    property var  local_id
    property var  net_ip



    function rw(num)
    {
        return num*brw
    }

    function rh(num)
    {
        return num * brh
    }

    function status_action(action)
    {
        if(action === "ONLINE")
        {
            status.text= local_id;

            send_btn.enabled = true
            disconnect_btn.enabled = true
            connect_btn.enabled = false

            recv_display.visible = true
            input_edit.visible = true
            client_id.visible = false

            status.anchors.horizontalCenterOffset =rw(-80)
            ip_status.anchors.horizontalCenterOffset = rw(-80)
        }
        else if(action === "connecting")
        {
            status.text= qsTr("connecting...")

            send_btn.enabled = false
            disconnect_btn.enabled = true
            connect_btn.enabled = false

            recv_display.visible = false
            input_edit.visible = false
            client_id.visible = true
            client_id.readOnly = true
        }
        else if(action === "CHECK_FAIL")
        {
            status.text= qsTr("check fail")

            send_btn.enabled = false
            disconnect_btn.enabled = false
            connect_btn.enabled = true

            recv_display.visible = false
            rte.text = ""
            input_edit.visible = false
            client_id.visible = true
            client_id.readOnly = false


        }
        else if(action === "NET_FAIL")
        {
            status.text= qsTr("connect fail")

            send_btn.enabled = false
            disconnect_btn.enabled = false
            connect_btn.enabled = true

            recv_display.visible = false
            rte.text = ""
            input_edit.visible = false
            client_id.visible = true
            client_id.readOnly = false
        }
    }

    Text
    {
        id: status
        text: "login please"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top:parent.top
        anchors.topMargin: rh(20)
    }

    Text
    {
        id: ip_status
        text: ""
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top:parent.top
        anchors.topMargin: rh(35)
    }

    Net
    {
        id: client
    }

    Connections
    {
        id: conn_id
        target: client

        signal msg_change_color(var ix)
        onNetStatusChange:
        {
            console.log("status ", net_status)

            if(net_status === 0)
            {
                status_action("ONLINE")
            }
            else if (net_status === 1)
            {
                status_action("CHECK_FAIL")
            }
            else if (net_status === 2)
            {
                status_action("NET_FAIL")
            }
            else if (net_status === 3)
            {
                status_action("connecting")
            }
        }

        onPeer_info_change:
        {
            g_peer_info = peer_info
            net_ip = g_peer_info[local_id]
            //console.log(net_ip)
            ip_status.text = net_ip

            on_line_peer.clear()
            for(var key in g_peer_info)
            {
                //console.log(key, "=", peer_info[key])
                if(key !== local_id)
                    on_line_peer.append({"peer_id": key, "peer_ip" :peer_info[key]})
            }
        }

        /*msg recv form peer, display it*/
        onGet_talk_msg:
        {
            for(var index =0;  index < on_line_peer.count; index++)
            {
                if(msg_id === on_line_peer.get(index).peer_id)
                    break;
            }

            console.log(msg)
            rte.text += msg_id + ":\n"+ msg+"\n"
            conn_id.msg_change_color(index)
        }
    }

    Button
    {
        id: send_btn
        anchors.left: input_edit.left
        anchors.leftMargin: rw(15)
        anchors.top:input_edit.bottom
        anchors.topMargin: rh(20)

        width: rw(140)
        height: rh(50)


        text: "send"
        enabled:  false
        onClicked:
        {
            var msg_id = on_line_peer.get(online_list.currentIndex).peer_id
            client.snd_msg(msg_id, ite.text)
            rte.text += ite.text
            ite.text = ""
        }
    }

    Button
    {
        id:disconnect_btn

        anchors.right: input_edit.right
        anchors.rightMargin: rw(15)
        anchors.top:input_edit.bottom
        anchors.topMargin: rh(20)

        width: rw(140)
        height: rh(50)

        text: "disconnect"
        enabled: false

        onClicked:
        {
            console.log(" disconnected")
            client.disconnect_from_srv()
        }
    }

    Button {
        id: connect_btn

        anchors.horizontalCenter: input_edit.horizontalCenter
        anchors.top:input_edit.bottom
        anchors.topMargin: rh(20)

        width: rw(140)
        height: rh(50)

        text: qsTr("connect")
        onClicked:

        {
            client.connectCall(client_id.text)
        }
    }

    Rectangle {
        id: recv_display

        anchors.horizontalCenter: status.horizontalCenter
        anchors.top:status.bottom
        anchors.topMargin: rh(20)

        width:  rw(460)
        height: rh(120)

        visible: false

        color: "lightsteelblue"
        border.color: "gray"

        TextEdit
        {
            id: rte
            anchors.fill: parent
        }
    }

    Rectangle {
        id: input_edit

        anchors.horizontalCenter: status.horizontalCenter
        anchors.top:recv_display.bottom
        anchors.topMargin: rh(10)

        width:  rw(460)
        height: rh(100)
        visible: false


        color: "lightsteelblue"
        border.color: "gray"
        TextEdit
        {
            id: ite
            anchors.fill: parent
        }
    }

    TextField
    {
        id: client_id

        text:"12345"

        anchors.horizontalCenter: status.horizontalCenter
        anchors.top: status.bottom
        anchors.topMargin: rh(200)

        onTextChanged:
        {
            local_id = text
        }
    }

    ListModel
    {
        id:on_line_peer
    }

    Component
    {
        id: online_delegate

        Rectangle
        {
            id : rect_id
            width: rw(120)
            height: rh(20)
            border.color: "lightsteelblue"
            Text
            {
                id: peer_msg
                anchors.centerIn: parent
                text: peer_id+"@"+peer_ip
                color: "black"
            }

            color: online_list.currentIndex===index? "green":"lightsteelblue" //选中颜色设置

            Connections
            {
                target: conn_id
                onMsg_change_color:
                {
                    if(ix === index && online_list.currentIndex !== index)
                    {
                        peer_msg.text = "New message!"
                        peer_msg.color = "red"
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked:
                {
                    online_list.currentIndex = index;
                    peer_msg.text = peer_id+"@"+peer_ip
                    peer_msg.color = "black"
                }
            }

        }
    }


    ListView
    {
        id:online_list
        anchors.horizontalCenter:parent.horizontalCenter
        anchors.horizontalCenterOffset: rw(180)
        anchors.top : status.top
        anchors.bottom: send_btn.bottom

        /*上下间隔*/
        //spacing:1


        /*滑动启用， 默认启用？*/
        interactive: true

        model: on_line_peer
        delegate: online_delegate
        /*
        delegate:Rectangle
        {
            id: online_rect
            //color: ListView.isCurrentItem?"green":"lightblue"
            //color: ListView.isCurrentItem?"green":"lightblue"
            border.color :"black"
            width:rw(140)
            height:rh(20)

            Text
            {
                text : peer_id + "@" + peer_ip
                color: online_rect.ListView.isCurrentItem?"white":"green"
            }
        }
*/
    }

}
