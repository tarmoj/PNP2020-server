#include "wsserver.h"
#include "QtWebSockets/qwebsocketserver.h"
#include "QtWebSockets/qwebsocket.h"
#include <QtCore/QDebug>
#include <QFile>
#include <QDateTime>
#include <QDate>
#include <QTime>


QT_USE_NAMESPACE



WsServer::WsServer(quint16 port, QObject *parent) :
    QObject(parent),
	m_pWebSocketServer(new QWebSocketServer(QStringLiteral("PNPServer"),
                                            QWebSocketServer::NonSecureMode, this)),
	m_clients(), slowClients(), fastClients(),
	slowInterval(60 *1000), fastInterval(slowInterval/3),
	m_oscAddress(nullptr), currentSection(0),
	sectionInMinutes(2)
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
		connect(&emulatorTimer, &QTimer::timeout, this, &WsServer::emulatorTimeout );

		QDateTime now = QDateTime::currentDateTime();
        QDateTime timeoftheaction = QDateTime::fromString("2021-10-25  19:00:00","yyyy-MM-dd  HH:mm:ss");;

		startTimer.setSingleShot(true);
		connect(&startTimer, &QTimer::timeout, this, &WsServer::startTimeout );
		qDebug() << "Alguseni: " << now.secsTo(timeoftheaction) << " sekundit";
		startTimer.start(now.secsTo(timeoftheaction)*1000);

		makeCommandList();
        //makeNamedCommandList(); // no named commands in 2021 version
	} else {
		emit newMessage("Could not start websocket server");
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
	pSocket->sendTextMessage("section|"+QString::number(currentSection+1));
    emit newConnection(m_clients.count());
	if (m_oscAddress) {
		m_oscAddress->sendData("/kliente",  QList<QVariant>() << m_clients.count());
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
	//QString peerAdress = pClient->peerAddress().toString();

	//emit newMessage(message);

	if (message.startsWith("name")) { // comes in "report result" where result 0|1
		QStringList messageParts = message.split(" ");
		if (messageParts.size()>=2) {
			QString name = messageParts[1];
			namedClients.insert(pClient, name);
			qDebug() << "Name players: " << namedClients.count() << " "  << namedClients.values();
		}
	}

	if (message.startsWith("report")) { // comes in "report result" where result 0|1
		QStringList messageParts = message.split(" ");
		if (messageParts.size()>=2) {
			bool result = static_cast<bool>(messageParts[1].toInt());
			int efficiency = messageParts[2].toInt();
			handleReport(pClient, result, efficiency );
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

		if (namedClients.contains(pClient)) {
			namedClients.remove(pClient);
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
	if (m_oscAddress) {
		m_oscAddress->sendData("/kunij2rgmisek2suni",  QList<QVariant>() << 100 );
	}
}

void WsServer::fastTimeout()
{
	if (fastClients.count()>0)  sendCommands(FAST);
	qDebug() << "Fast trigger";
	fastInterval *= 0.9;
	if (m_oscAddress) {
		m_oscAddress->sendData("/kiire",  QList<QVariant>() << 100  );
	}

}

void WsServer::counterTimeout()
{
	int slowRemaining = slowTimer.remainingTime()/1000;
	int fastRemaining = fastTimer.remainingTime()/1000;
	//qDebug() << "Timers: " << slowRemaining << " " << fastRemaining;
	emit newSlowRemaining(QString::number(slowRemaining));
	emit newFastRemaining(QString::number(fastRemaining));
	// send to clients as well
	sendToClients(SLOW, "countdown|"+QString::number(slowRemaining));
	sendToClients(FAST, "countdown|"+QString::number(fastRemaining));


	if (m_oscAddress) {
		int phaseSlow = int((1 - (float)slowTimer.remainingTime()/slowTimer.interval())*100);
		int phaseFast = int((1 - (float)fastTimer.remainingTime()/fastTimer.interval())*100);
		//qDebug() << "OSC jaoks aega jäänud: " << phaseSlow;
		m_oscAddress->sendData("/kunij2rgmisek2suni",  QList<QVariant>() << phaseSlow  );
		m_oscAddress->sendData("/kiire",  QList<QVariant>() << phaseFast  );

	}


}

void WsServer::sectionTimeout()
{
	toggleTimers(false);

	if (currentSection==7) {
		sendToClients(ALL, "section|VAIKUS");
		if (m_oscAddress) {
			m_oscAddress->sendData("/vaikus",  QList<QVariant>() << "Suur vaikus!");
		}

	}

	if (currentSection<7) {
		setSection(currentSection+1);
	}

}

void WsServer::emulatorTimeout()
{
	// generate random data for handleReport - to test over OSC
	if (m_oscAddress) {
		int client = qrand()%30, result = qrand()%2, efficiency = qrand()%101;
		qDebug() << "Emulate client: " << client << " " << result << " " << efficiency;
		m_oscAddress->sendData("/klient",   QList<QVariant>() << client << result << efficiency );
	}
	int nextTick = 1+ qrand()%10;  // vahemikus 1 sek..10 sek
	qDebug() << "Next client emulation after " << nextTick << " seconds.";
	emulatorTimer.setInterval(nextTick * 1000);
}

void WsServer::sendNamedCommand()
{
	int index = qrand() % namedCommands.count();
	QString name = namedCommands.at(index).first;
	QString command = namedCommands.at(index).second;
	int seconds = (sectionDuration - sectionTimer.remainingTime()/1000);
	QString time = QString("%1:%2").arg(seconds/60).arg(seconds%60);
	// find socket
	// socket->sendTextMessage(client, ..... )
	qDebug() << "Named command to: " << name << " - " << command << " at: " << time;
	emit newMessage(QString("Named command to: %1 - %2  - at %3").arg(name, command, time));
	if (namedClients.values().contains(name)) {
		QWebSocket * socket = namedClients.key(name);
		if (socket) {
			socket->sendTextMessage("command|*|"+command);
		}
	} else {
		qDebug() << "Seems that " << name << " is not connected!";
	}
}

void WsServer::startTimeout()
{
	qDebug() << "****** START! ******* ";
	setSection(0); // natuke kehv, sest ei esimene käsk automaatselt punane..
	toggleTimers(true);
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
		m_oscAddress->sendData("/k2sk",  QList<QVariant>() << command.length() ); // TODO
	}
}

/*


 172.20.10.2
/klient 5 1 0.677
/k2sk 12
/sektsioon 1
/kunij2rgmisek2suni 23
/kiire 23 -  0..100
/kliente -  liitumised ja lahkusmised

 * */


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
		sectionTimer.stop();
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
	if (section <0 || section>7) {
		qDebug() << "Section number out of range!";
		return;
	}

	qDebug() << "Section (index)" << section;
	currentSection = section;
	if (categories.count()>section) {
        currentCategory = categories[section];
	} else {
        currentCategory = "*";
	}


	if (section==0) {
		slowInterval = 40 * 1000; // in milliseconds
		fastSlowRatio = 2;
		fastInterval = int(slowInterval/fastSlowRatio);
		sectionDuration = sectionInMinutes*60; // in sections ? singleshot timer to stop timers?
	} else if (section==1) {
		slowInterval = 25 * 1000; // in milliseconds
		fastSlowRatio = 2;
		fastInterval = int(slowInterval/fastSlowRatio);
		sectionDuration = sectionInMinutes*60; // in sections ? singleshot timer to stop timers?
	} else if (section==2) {
		slowInterval = 35 * 1000; // in milliseconds
		fastSlowRatio = 2;
		fastInterval = int(slowInterval/fastSlowRatio);
		sectionDuration = sectionInMinutes*60; // in sections ? singleshot timer to stop timers?
	} else if (section==3) {
		slowInterval = 30 * 1000; // in milliseconds
		fastSlowRatio = 2;
		fastInterval = int(slowInterval/fastSlowRatio);
		sectionDuration = sectionInMinutes*60; // in sections ? singleshot timer to stop timers?
	} else if (section==4) {
		slowInterval = 25 * 1000; // in milliseconds
		fastSlowRatio = 2;
		fastInterval = int(slowInterval/fastSlowRatio);
		sectionDuration = sectionInMinutes*60; // in sections ? singleshot timer to stop timers?
	} else if (section==5) {
		slowInterval = 20 * 1000; // in milliseconds
		fastSlowRatio = 2;
		fastInterval = int(slowInterval/fastSlowRatio);
		sectionDuration = sectionInMinutes*60*3/2; // NB! section (index) 5 is longer and fast
        currentCategory = "*"; // NB ! ALL categories here!
	} else if (section==6) {
		slowInterval = 40 * 1000; // in milliseconds
		fastSlowRatio = 3;
		fastInterval = int(slowInterval/fastSlowRatio);
		sectionDuration = sectionInMinutes*60/2; // this section is slow and shorter
	} else if (section==7) {
		slowInterval = 20 * 1000; // in milliseconds
		fastSlowRatio = 2;
		fastInterval = int(slowInterval/fastSlowRatio);
		sectionDuration = sectionInMinutes*60;
        currentCategory = "*";//categories[2]; // TODO: better if just pass index, no need to copy
	}


	// send named commands - <section+1> times per section
//	for (int i=0; i<section+1; i++) {
//        //int interval = (qrand() % sectionDuration)*1000;
//        //QTimer::singleShot(interval, this, SLOT(sendNamedCommand()));
//	}


    sendToClients(ALL, "section|"+QString::number(section+1) + " " + currentCategory);

	emit newSection(section+1);
    emit newMessage("Section "+QString::number(section+1) + " " + currentCategory);
    qDebug()<< "Category: " << currentCategory;

	if (m_oscAddress) {
		m_oscAddress->sendData("/sektsioon",  QList<QVariant>() << currentSection + 1);
	}
	sectionTimer.setSingleShot(true);
	sectionTimer.start( sectionDuration * 1000 );
	toggleTimers(true);
}




void WsServer::sendCommands(int clientsType)
{

    QString category = currentCategory.isEmpty() ? "*" : currentCategory;


	// send OSC <- needs more
	sendCommandAsOSC(category, getCommand(category));

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

}

QString WsServer::getCommand(QString category)
{
	QStringList commands =(category=="*") ? allCommands.values() : allCommands.values(category);
	int index = qrand() % commands.count(); // %
	QString command = commands.at(index);
	//qDebug() << "Käsk: " << command;
	return command;
}

void WsServer::handleReport(QWebSocket *client, bool result, int efficiency)
{
	if (!client) {
		qDebug() << Q_FUNC_INFO << "Client is null!";
		return;
	}
	qDebug() << client->peerAddress().toString() << " raport: " << result << " tõhusus: " << efficiency;

	if (m_oscAddress) {
		m_oscAddress->sendData("/klient",  QList<QVariant>() << m_clients.indexOf(client) << static_cast<int>(result) << efficiency);
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
	categories.clear();
	QFile file(":/Commands.csv");
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
	QStringList temp = allCommands.keys();
	temp.removeDuplicates();
	// shuffle the list to categories
	while (!temp.isEmpty() ) {
		int index = qrand() % temp.count();
		categories  << temp.takeAt(index);
	}
	qDebug() << "Categories: " << categories;
}

void WsServer::makeNamedCommandList()
{
	QStringList andrus = QStringList() << "S" << "B" ;
	// in the file NamedCommands every line is set as: name | command

	namedCommands.clear();
	names.clear();
	QFile file(":/NamedCommands.csv");
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return;

	while (!file.atEnd()) {
		QString line = QString(file.readLine());
		QStringList parts= line.split("|");
		if (parts.count()>=2) {
			namedCommands.append(qMakePair(parts[0], parts[1].simplified().remove('\n') ));
			if (!names.contains(parts[0])) {
				names.append(parts[0]);
			}
		}
	}

	//names.removeDuplicates();
	qDebug() << "Added " << namedCommands.count() << " commands to Named commands list";
	qDebug() << "Names: "  << names;
}


