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

	void sendCommands(int clientsType);
	QString getCommand(QString category);
	void handleReport(QWebSocket *client, bool result);
	void makeCommandList();
	void setOscAddress(QString host, quint16 port);
	void sendCommandAsOSC(QString category, QString command);
	void toggleTimers(bool checked);
	void sendToClients(int clientsType, QString message);

Q_SIGNALS:
    void closed();
    void newConnection(int connectionsCount);
	void newMessage(QString message);
	void newSlowRemaining(QString seconds);
	void newFastRemaining(QString seconds);


private Q_SLOTS:
    void onNewConnection();
    void processTextMessage(QString message);
    void socketDisconnected();
	void slowTimeout();
	void fastTimeout();
	void counterTimeout();


private:
    QWebSocketServer *m_pWebSocketServer;
	QList<QWebSocket *> m_clients;
	QList<QWebSocket *> slowClients, fastClients;
	QStringList categories, currentCategories;
	QMultiHash <QString, QString> allCommands; // key: category, value: command
	QTimer slowTimer, fastTimer, counterTimer;
	int slowInterval, fastInterval;
	QOscClient * m_oscAddress;


};



#endif // WSSERVER_H
