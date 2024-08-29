#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "client.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
  private slots:
    void on_connectButton_clicked();
    void on_sendButton_clicked();
    void on_fileButton_clicked();
    void on_readyRead();

  private:
    Ui::MainWindow* ui;
    Client* m_client;
};
#endif  // MAINWINDOW_H
