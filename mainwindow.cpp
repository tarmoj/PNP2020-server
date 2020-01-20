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
	connect(wsServer, &WsServer::newSection, this->ui->sectionSpinBox, &QSpinBox::setValue);

	wsServer->setOscAddress(ui->oscServerLineEdit->text(), static_cast<quint16>(ui->portSpinBox->value()));


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

void MainWindow::on_startSectioButton_clicked()
{
	wsServer->setSection(ui->sectionSpinBox->value()-1);
}

void MainWindow::on_stopSectionButton_clicked()
{
	wsServer->toggleTimers(false);
}
