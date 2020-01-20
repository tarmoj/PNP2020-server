#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	wsServer = new WsServer(9009);

	connect(wsServer, SIGNAL(newConnection(int)), this, SLOT(setClientsCount(int)));
	connect(wsServer, &WsServer::newSlowRemaining, this->ui->slowRemainingLabel, &QLabel::setText);
	connect(wsServer, &WsServer::newFastRemaining, this->ui->fastRemainingLabel, &QLabel::setText);

	//connect(wsServer, SIGNAL(newMessage(QString)), ui->messagesTextEdit, SLOT(append(QString)) );
	//connect(wsServer, SIGNAL(newParameters(QString)), ui->parametersTextEdit, SLOT(setText(QString)) );


	//connect(wsServer, SIGNAL(pausedChanged(bool)), ui->checkBox, SLOT(setChecked(bool))  );

	//on_setOscButton_clicked();


	}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::setClientsCount(int clientsCount)
{
	ui->numberOfClientsLabel->setText(QString::number(clientsCount));
}


void MainWindow::on_setOscButton_clicked()
{
	wsServer->setOscAddress(ui->oscServerLineEdit->text(), static_cast<quint16>(ui->portSpinBox->value())   );
}

void MainWindow::on_timersCheckBox_toggled(bool checked)
{
	wsServer->toggleTimers(checked);
}
