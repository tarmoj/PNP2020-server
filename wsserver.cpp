#include "wsserver.h"
#include "QtWebSockets/qwebsocketserver.h"
#include "QtWebSockets/qwebsocket.h"
#include <QtCore/QDebug>


QT_USE_NAMESPACE



WsServer::WsServer(quint16 port, QObject *parent) :
    QObject(parent),
	m_pWebSocketServer(new QWebSocketServer(QStringLiteral("PNPServer"),
                                            QWebSocketServer::NonSecureMode, this)),
	m_clients(), slowClients(), fastClients(),
	slowInterval(20 *1000), fastInterval(slowInterval*4)

{
    if (m_pWebSocketServer->listen(QHostAddress::Any, port)) {
        qDebug() << "WsServer listening on port" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
                this, &WsServer::onNewConnection);
        connect(m_pWebSocketServer, &QWebSocketServer::closed, this, &WsServer::closed);
		connect(&slowTimer, &QTimer::timeout, this, &WsServer::slowTimeout );
		connect(&fastTimer, &QTimer::timeout, this, &WsServer::fastTimeout );

		makeCommandList();

		slowTimer.start(slowInterval);
		fastTimer.start(fastInterval);

		// maybe later start from some function
	}


}



WsServer::~WsServer()
{
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}


void WsServer::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();

    connect(pSocket, &QWebSocket::textMessageReceived, this, &WsServer::processTextMessage);

    connect(pSocket, &QWebSocket::disconnected, this, &WsServer::socketDisconnected);

    m_clients << pSocket;
    emit newConnection(m_clients.count());
}



void WsServer::processTextMessage(QString message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (!pClient) {
        return;
    }
	qDebug()<<message;
	QString peerAdress = pClient->peerAddress().toString();

	emit newMessage(message);



}


void WsServer::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient) {
        m_clients.removeAll(pClient);
		if (slowClients.contains(pClient)) {
			slowClients.removeAll(pClient);
		}
		if (fastClients.contains(pClient)) {
			fastClients.removeAll(pClient);
		}

        emit newConnection(m_clients.count());
        pClient->deleteLater();
	}
}

void WsServer::slowTimeout()
{
	sendCommands(SLOW);
}

void WsServer::fastTimeout()
{
	sendCommands(FAST);
}



void WsServer::sendCommands(int slowOrFast)
{
	QList<QWebSocket *> clients = (slowOrFast==FAST) ? fastClients : slowClients;
	// test:
	getCommand("*");
	foreach (QWebSocket * socket, clients ) {
		if (socket) {
			QString category = "*";//currentCategories[0];
			QString command = getCommand(category);
			qDebug() << "Saadan kliendile " << socket->peerAddress().toString() << " kategoorias:  " << category << " korralduse: "  << command;
			//socket->sendTextMessage("category:  "+category+" command: "+command);
		}
	}

}

QString WsServer::getCommand(QString category)
{
	QStringList commands =(category=="*") ? allCommands.values() : allCommands.values(category);
	qDebug() << commands;
	int index = qrand() % commands.count(); // %
	QString command = commands.at(index);
	qDebug() << "Käsk: " << command;
	return command;
}

void WsServer::makeCommandList()
{
	// read form MYSQL table or file
	// test with
	allCommands.clear();
	// to test
	allCommands.insert("jäsemed", "lehvita käsi");
	allCommands.insert("jäsemed", "viibuta jalgu");
	allCommands.insert("inimesed", "tervita kedagi");
	allCommands.insert("inimesed", "vaata kedagi üksisilmi");
	allCommands.insert("male", "käi etturiga");
	allCommands.insert("male", "vangerda");

	categories = allCommands.keys();
}


