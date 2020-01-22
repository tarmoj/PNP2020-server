#ifndef WSSERVER_H
#define WSSERVER_H

#include <QObject>
#include <QStringList>
#include <QMultiHash>
#include <QTimer>

#include "qosc/qoscclient.h"



QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

struct VoteResults {
	int count;
	double sum;
	double average;
} ;

#define SLOW 0
#define FAST 1
#define ALL 2

class WsServer : public QObject
{
    Q_OBJECT
public:
	explicit WsServer(quint16 port, QObject *parent = nullptr);
    ~WsServer();

	QTimer emulatorTimer;

	void sendCommands(int clientsType);
	QString getCommand(QString category);
	void handleReport(QWebSocket *client, bool result, int efficiency);
	void makeCommandList();
	void setOscAddress(QString host, quint16 port);
	void sendCommandAsOSC(QString category, QString command);
	void toggleTimers(bool checked);
	void sendToClients(int clientsType, QString message);
	void setSection(int section);
	void setSectionInMinutes(int minutes) { sectionInMinutes = minutes;}

Q_SIGNALS:
    void closed();
    void newConnection(int connectionsCount);
	void newMessage(QString message);
	void newSlowRemaining(QString seconds);
	void newFastRemaining(QString seconds);
	void newSection(int sectionIndex);



private Q_SLOTS:
    void onNewConnection();
    void processTextMessage(QString message);
    void socketDisconnected();
	void slowTimeout();
	void fastTimeout();
	void counterTimeout();
	void sectionTimeout();
	void emulatorTimeout();


private:
    QWebSocketServer *m_pWebSocketServer;
	QList<QWebSocket *> m_clients;
	QList<QWebSocket *> slowClients, fastClients;
	QStringList categories, currentCategories;
	QMultiHash <QString, QString> allCommands; // key: category, value: command
	QTimer slowTimer, fastTimer, counterTimer, sectionTimer;
	int slowInterval, fastInterval;
	QOscClient * m_oscAddress;
	int currentSection;
	int startInterval, endInterval;
	int sectionDuration;
	double fastSlowRatio;
	int sectionInMinutes;

/*
	void defineSections();

	class Section {
	public:
		Section();
		~Section();

		int startInterval, endInterval;
		QTimer slowTimer, fastTimer;
		int slowInterval, fastInterval;
		//double fastSlowRatio; // how many times is the fast
		int duration; // in seconds
		int startTime;
	public slots:
		void slowTimeout();
		void fastTimeout();
	};

	Section section[8];
*/

};



#endif // WSSERVER_H
