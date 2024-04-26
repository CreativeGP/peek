#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QStandardItemModel>
#include <QModelIndex>
#include <QProcess>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void keyPressEvent(QKeyEvent*) override;

private:
    void updateProcessTree();
    void newWineProcess();

private:
    Ui::MainWindow *ui;
    QTimer *timer;
    QStandardItemModel *processTree = 0;
    QProcess *strace = 0, *wine = 0;
};
#endif // MAINWINDOW_H
