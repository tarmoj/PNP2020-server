#ifndef WSSERVER_H
#define WSSERVER_H

#include <QObject>
#include <QStringList>
#include <QMultiHash>
#include <QTimer>



QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

struct VoteResults {
	int count;
	double sum;
	double average;
} ;

#define SLOW 0
#define FAST 1

class WsServer : public QObject
{
    Q_OBJECT
public:
	explicit WsServer(quint16 port, QObject *parent = nullptr);
    ~WsServer();

	void sendCommands(int slowOrFast);
	QString getCommand(QString category);
	void handleReport(QWebSocket *client, bool result);
	void makeCommandList();

Q_SIGNALS:
    void closed();
    void newConnection(int connectionsCount);
	void newMessage(QString message);



private Q_SLOTS:
    void onNewConnection();
    void processTextMessage(QString message);
    void socketDisconnected();
	void slowTimeout();
	void fastTimeout();


private:
    QWebSocketServer *m_pWebSocketServer;
	QList<QWebSocket *> m_clients;
	QList<QWebSocket *> slowClients, fastClients;
	QStringList categories, currentCategories;
	QMultiHash <QString, QString> allCommands; // key: category, value: command
	QTimer slowTimer, fastTimer;
	int slowInterval, fastInterval;


};



#endif // WSSERVER_H
