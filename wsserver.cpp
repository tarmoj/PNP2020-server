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
	slowInterval(20 *1000), fastInterval(slowInterval/4)

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
	slowClients << pSocket;
    emit newConnection(m_clients.count());
}


// TODO: kuidas kontrollida, kas mõni küsimus jäi vastamata? veebilehes -  kui järgmine tuleb sisse ja vastamata, siis saada EI
// kas kliendis count-down järgmise korralduseni
// serveri UI-s arv -  kui palju fast / kui palju slow? JAH / EI
// iga mängija kohta mingii element, mis vastavalt JAH/EI -  punane/roheline
// kogu statisikat iga kasutaja kohta, kas punane roheline
// üks suur pall terviku kohta (hall, kui 0), kas eraldi QML aken? kasutajaliides->QML
// kuidas teha, et klientidel nupud desaktiveeritud? Kui käsk tuleb sisse, siis enable, kui

void WsServer::processTextMessage(QString message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (!pClient) {
        return;
    }
	qDebug()<<message;
	QString peerAdress = pClient->peerAddress().toString();

	emit newMessage(message);

	if (message.startsWith("report")) { // comes in "report result" where result 0|1
		QStringList messageParts = message.split(" ");
		if (messageParts.size()>=2) {
			bool result = static_cast<bool>(messageParts[1].toInt());
			handleReport(pClient, result);
		}
	}



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
	qDebug() << "Slow trigger";
	sendCommands(SLOW);
}

void WsServer::fastTimeout()
{
	sendCommands(FAST);
	qDebug() << "Fast trigger";

}



void WsServer::sendCommands(int slowOrFast)
{
	QList<QWebSocket *> clients = (slowOrFast==FAST) ? fastClients : slowClients;
	if (slowOrFast == ALL) {
		clients = m_clients;
	}
	qDebug() << "Clients in " << slowOrFast << " " << clients.count();
	foreach (QWebSocket * socket, clients ) {
		if (socket) {
			QString category = "*";//currentCategories[0];
			QString command = getCommand(category);
			qDebug() << "Saadan kliendile " << socket->peerAddress().toString() << " kategoorias:  " << category << " korralduse: "  << command;
			socket->sendTextMessage("command|"+category+"|"+command); // sends as "command"|category|command
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

void WsServer::handleReport(QWebSocket *client, bool result)
{
	if (!client) {
		qDebug() << Q_FUNC_INFO << "Client is null!";
		return;
	}
	qDebug() << client->peerAddress().toString() << " raport: " << result;
	if (result) {
		if (fastClients.contains(client)) {  // if was in fast and done, move to slow
			if (!slowClients.contains(client)) {
				slowClients.append(client);
			}
			fastClients.removeAll(client);
		}
	} else {
		if (slowClients.contains(client)) { // if was in slow and undone, move to fast
			if (!fastClients.contains(client)) {
				fastClients.append(client);
			}
			slowClients.removeAll(client);
		}
	}
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


