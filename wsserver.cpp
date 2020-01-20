#include "wsserver.h"
#include "QtWebSockets/qwebsocketserver.h"
#include "QtWebSockets/qwebsocket.h"
#include <QtCore/QDebug>
#include <QFile>


QT_USE_NAMESPACE



WsServer::WsServer(quint16 port, QObject *parent) :
    QObject(parent),
	m_pWebSocketServer(new QWebSocketServer(QStringLiteral("PNPServer"),
                                            QWebSocketServer::NonSecureMode, this)),
	m_clients(), slowClients(), fastClients(),
	slowInterval(60 *1000), fastInterval(slowInterval/3),
	m_oscAddress(nullptr), currentSection(0)
//	startInterval(slowInterval), endInterval(slowInterval*0.75),
//	fastSlowRatio(3);

{
    if (m_pWebSocketServer->listen(QHostAddress::Any, port)) {
        qDebug() << "WsServer listening on port" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
                this, &WsServer::onNewConnection);
        connect(m_pWebSocketServer, &QWebSocketServer::closed, this, &WsServer::closed);
		connect(&slowTimer, &QTimer::timeout, this, &WsServer::slowTimeout );
		connect(&fastTimer, &QTimer::timeout, this, &WsServer::fastTimeout );
		connect(&counterTimer, &QTimer::timeout, this, &WsServer::counterTimeout );
		connect(&sectionTimer, &QTimer::timeout, this, &WsServer::sectionTimeout );
		makeCommandList();
		//setSection(0);

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
	if (m_oscAddress) {
		m_oscAddress->sendData("/error/clients",  QList<QVariant>() << m_clients.count());
	}
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
	if (slowClients.count()>0) sendCommands(SLOW);
	slowInterval *= 0.9; // get faster!
}

void WsServer::fastTimeout()
{
	if (fastClients.count()>0)  sendCommands(FAST);
	qDebug() << "Fast trigger";
	fastInterval *= 0.9;

}

void WsServer::counterTimeout()
{
	int slowRemaining = slowTimer.remainingTime()/1000;
	int fastRemaining = fastTimer.remainingTime()/1000;
	qDebug() << "Timers: " << slowRemaining << " " << fastRemaining;
	emit newSlowRemaining(QString::number(slowRemaining));
	emit newFastRemaining(QString::number(fastRemaining));
	// send to clients as well
	sendToClients(SLOW, "countdown|"+QString::number(slowRemaining));
	sendToClients(FAST, "countdown|"+QString::number(fastRemaining));
}

void WsServer::sectionTimeout()
{
	toggleTimers(false);
	if (currentSection<6) {
		setSection(currentSection+1);
	}
}


void WsServer::setOscAddress(QString host, quint16 port)
{
	qDebug()<<"Setting OSC address to: "<<host<<port;
	if (m_oscAddress == nullptr) {
		m_oscAddress = new QOscClient(QHostAddress(host), port, this);
	} else {
		m_oscAddress->setAddress(QHostAddress(host), port);
	}
}

void WsServer::sendCommandAsOSC(QString category, QString command)
{
	if (m_oscAddress) {
		m_oscAddress->sendData("/error/command",  QList<QVariant>() << category << command);
	}
}

void WsServer::toggleTimers(bool checked)
{
	if (checked) {
		slowTimer.start(slowInterval);
		fastTimer.start(fastInterval);
		counterTimer.start(1000);
		slowTimeout(); fastTimeout(); counterTimeout(); // first firing is not at start otherwise
	} else {
		slowTimer.stop();
		fastTimer.stop();
		counterTimer.stop();
	}
}

void WsServer::sendToClients(int clientsType, QString message)
{
	QList<QWebSocket *> clients;

	switch (clientsType) {
		case SLOW: clients = slowClients; break;
		case FAST: clients = fastClients; break;
		case ALL: clients= m_clients; break;
		default: return;

	}

	foreach (QWebSocket * socket, clients ) {
		if (socket) {
			socket->sendTextMessage(message);
		}
	}
}

void WsServer::setSection(int section)
{
	if (section <0 || section>6) {
		qDebug() << "Section number out of range!";
		return;
	}

	qDebug() << "Section (index)" << section;

	if (section==0) {
		currentSection = 0;
		slowInterval = 60 * 1000; // in milliseconds
		fastSlowRatio = 3;
		fastInterval = slowInterval/fastSlowRatio;
		sectionDuration = 2*60; // in sections ? singleshot timer to stop timers?

		currentCategories.clear();
		if (categories.count()>0)
			currentCategories << categories[0]; // TODO: better if just pass index, no need to copy
		else
			qDebug() << "No categories?";
	} else if (section==0) {
		currentSection = 1;
		slowInterval = 50 * 1000; // in milliseconds
		fastSlowRatio = 3;
		fastInterval = slowInterval/fastSlowRatio;
		sectionDuration = 2*60; // in sections ? singleshot timer to stop timers?

		currentCategories.clear();
		if (categories.count()>0)
			currentCategories << categories[1]; // TODO: better if just pass index, no need to copy
		else
			qDebug() << "No categories?";
	} else if (section==2) {
		currentSection = 1;
		slowInterval = 45 * 1000; // in milliseconds
		fastSlowRatio = 2;
		fastInterval = slowInterval/fastSlowRatio;
		sectionDuration = 2*60; // in sections ? singleshot timer to stop timers?

		currentCategories.clear();
		if (categories.count()>0)
			currentCategories << categories[2]; // TODO: better if just pass index, no need to copy
		else
			qDebug() << "No categories?";
	} else if (section==3) {
		currentSection = 1;
		slowInterval = 40 * 1000; // in milliseconds
		fastSlowRatio = 2;
		fastInterval = slowInterval/fastSlowRatio;
		sectionDuration = 10*60; // in sections ? singleshot timer to stop timers?

		currentCategories.clear();
		if (categories.count()>0)
			currentCategories << categories[3]; // TODO: better if just pass index, no need to copy
		else
			qDebug() << "No categories?";
	} else if (section==4) {
		currentSection = 1;
		slowInterval = 30 * 1000; // in milliseconds
		fastSlowRatio = 2;
		fastInterval = slowInterval/fastSlowRatio;
		sectionDuration = 10*60; // in sections ? singleshot timer to stop timers?

		currentCategories.clear();
		if (categories.count()>0)
			currentCategories << "*";//categories[]; // TODO: better if just pass index, no need to copy
		else
			qDebug() << "No categories?";
	} else if (section==5) {
		currentSection = 1;
		slowInterval = 60 * 1000; // in milliseconds
		fastSlowRatio = 3;
		fastInterval = slowInterval/fastSlowRatio;
		sectionDuration = 10*60; // in sections ? singleshot timer to stop timers?

		currentCategories.clear();
		if (categories.count()>0)
			currentCategories << categories[2]; // TODO: better if just pass index, no need to copy
		else
			qDebug() << "No categories?";
	toggleTimers(true);
	} else if (section==6) {
		currentSection = 1;
		slowInterval = 20 * 1000; // in milliseconds
		fastSlowRatio = 2;
		fastInterval = slowInterval/fastSlowRatio;
		sectionDuration = 10*60; // in sections ? singleshot timer to stop timers?

		currentCategories.clear();
		if (categories.count()>0)
			currentCategories << "*";//categories[2]; // TODO: better if just pass index, no need to copy
		else
			qDebug() << "No categories?";
	}
	sendToClients(ALL, "section|"+QString::number(section));

	emit newSection(currentSection+1);
	if (m_oscAddress) {
		m_oscAddress->sendData("/error/section",  QList<QVariant>() << currentSection + 1);
	}
	sectionTimer.setSingleShot(true);
	sectionTimer.start( sectionDuration * 1000 );
	toggleTimers(true);
}




void WsServer::sendCommands(int clientsType)
{

	QString category = "*";//currentCategories.count()<1 ? "*" : currentCategories[0];
	QString command = getCommand(category);

	QString message = "command|"+category+"|"+command;
	QList<QWebSocket *> clients;
	switch (clientsType) {
		case SLOW: clients = slowClients; break;
		case FAST: clients = fastClients; break;
		case ALL: clients= m_clients; break;
		default: return;

	}

	foreach (QWebSocket * socket, clients ) {
		if (socket) {
			socket->sendTextMessage("command|"+category+"|"+getCommand(category));
		}
	}

	// send OSC <- needs more
	sendCommandAsOSC(category, getCommand(category));
}

QString WsServer::getCommand(QString category)
{
	QStringList commands =(category=="*") ? allCommands.values() : allCommands.values(category);
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

	if (m_oscAddress) {
		m_oscAddress->sendData("/error/report",  QList<QVariant>() << m_clients.indexOf(client) << static_cast<int>(result));
	}

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
	allCommands.clear();
	QFile file("../PNP2020-server/Commands.csv");
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return;

	while (!file.atEnd()) {
		QString line = QString(file.readLine());
		QStringList parts= line.split("|");
		if (parts.count()>=2) {
			allCommands.insert(parts[0], parts[1].remove('\n'));
		}
	}

	qDebug() << "Added " << allCommands.count() << " commands to list";
	// TODO: shuffle allCommands by category? or find algorythn to take random index
	categories = allCommands.keys();
}


