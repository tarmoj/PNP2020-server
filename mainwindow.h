#ifndef MainWindow_H
#define MainWindow_H

#include <QMainWindow>
#include "wsserver.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();

public slots:
	void setClientsCount(int clientsCount);

private slots:


	void on_setOscButton_clicked();

	void on_timersCheckBox_toggled(bool checked);

	void on_startSectioButton_clicked();

	void on_stopSectionButton_clicked();

private:
	Ui::MainWindow *ui;
	WsServer *wsServer;


};

#endif // MainWindow_H
