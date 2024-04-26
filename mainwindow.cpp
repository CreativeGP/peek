#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QScrollBar>
#include <QKeyEvent>
#include <QPushButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->runButton, &QPushButton::clicked, this, &MainWindow::newWineProcess);


    timer = new QTimer(this);
    timer->setInterval(500/*ms*/);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateProcessTree);
    timer->start();
    // timerInterval();
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_F5) {
        updateProcessTree();
    }
}

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/**
 * Get a process name from its PID.
 * @param pid PID of the process
 * @param name Name of the process
 *
 * Source: http://stackoverflow.com/questions/15545341/process-name-from-its-pid-in-linux
 */
static void get_process_name(const pid_t pid, char * name) {
    char procfile[BUFSIZ];
    sprintf(procfile, "/proc/%d/cmdline", pid);
    FILE* f = fopen(procfile, "r");
    if (f) {
        size_t size;
        size = fread(name, sizeof (char), sizeof (procfile), f);
        if (size > 0) {
            if ('\n' == name[size - 1])
                name[size - 1] = '\0';
        }
        fclose(f);
    }
}

std::string getLsof(std::string pid) {
    std::ostringstream oss;

    // Open a pipe to execute the lsof command and read its output
    std::array<char, 128> buffer;
    std::shared_ptr<FILE> pipe(popen(("lsof -p "+pid).c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("Failed to execute the lsof command.");
    }

    // Read the command output line by line
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        oss << buffer.data();
    }

    return oss.str();
}

// Function to parse the process information
void parseProcessInfo(const std::string& line, std::vector<std::string>& tokens) {
    std::istringstream iss(line);
    std::string token;
    while (std::getline(iss, token, ' ')) {
        if (!token.empty())
            tokens.push_back(token);
    }
}

// Function to build the process tree recursively
void buildProcessTree(QStandardItem* item, const std::string& parentPid) {
    std::ifstream procFile("/proc/" + parentPid + "/task/" + parentPid + "/children");
    if (!procFile.is_open()) {
        std::cerr << "Failed to open file for process " << parentPid << std::endl;
        return;
    }

    char buffer[1024];
    std::string line;
    while (std::getline(procFile, line)) {
        std::vector<std::string> tokens;
        parseProcessInfo(line, tokens);

        // TODO: update tree
        int i = 0;
        for (std::string pid : tokens) {
            get_process_name(std::stoi(pid), buffer);
            QStandardItem* child = new QStandardItem(QString::fromStdString(pid+" ") + QString(buffer));
            item->appendRow(child);
            buildProcessTree(child, pid);
            i++;
        }
    }

    procFile.close();
}

void MainWindow::updateProcessTree() {
    // update
    QScrollBar *vscr = 0;
    if (!processTree) {
        processTree = new QStandardItemModel();
    } else {
        // vscr = ui->treeView->verticalScrollBar();
    }
    processTree->clear();
    QStandardItem* rootItem = processTree->invisibleRootItem();

    // Build the process tree
    buildProcessTree(rootItem, "1");

    // Print the process tree
    std::cout << "Process Tree:\n";
    for (int row = 0; row < processTree->rowCount(); ++row) {
        QStandardItem* item = processTree->item(row);
        std::cout << item->text().toStdString() << std::endl;
    }

    ui->treeView->setModel(processTree);
    ui->treeView->expandAll();
    // if (vscr) ui->treeView->setVerticalScrollBar(vscr);

    connect(ui->treeView, &QTreeView::doubleClicked, this, [this]() {
        QModelIndexList selectedIndexes = ui->treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            const QModelIndex &a = selectedIndexes.at(0);
            const std::string &pid = a.data(Qt::DisplayRole).toString().split(" ").at(0).toStdString();
            const QString &tabText = a.data(Qt::DisplayRole).toString();
            ui->textBrowser->setText(QString::fromStdString(getLsof(pid)));

            // change tab
            ui->tabWidget->setCurrentWidget(ui->tab_2);
            ui->tabWidget->setTabText(1, tabText);

            // start tracing
            if (strace) {
                strace->close();
                strace = 0;
            }
            strace = new QProcess(this);
            strace->setProcessChannelMode(QProcess::MergedChannels);
            connect(strace, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
                // error
            });
            connect(strace, &QProcess::started, this, [this]() {
                ui->traceBrowser->clear();
            });
            connect(strace, &QProcess::readyReadStandardOutput, this, [this]() {
                while (strace->canReadLine()) {
                    QByteArray line = strace->readLine();
                    QString inserted = QString(line.trimmed());
                    if (inserted.contains(ui->searchTraceBrowser->text())) // TODO yokunai
                        if (ui && ui->traceBrowser) ui->traceBrowser->append(inserted);
                }
            });
            QStringList arguments;
            arguments << "-p" << QString::fromStdString(pid);
            strace->start("strace", arguments);
        }
    });
    std::cout << "a" << std::endl;
}

void MainWindow::newWineProcess() {
    // change tab
    ui->tabWidget->setCurrentWidget(ui->tab_2);
    ui->tabWidget->setTabText(1, "[wine]"+ui->newProcess->text());
    ui->searchTraceBrowser->setText("KERNEL32");

    if (!wine) wine = new QProcess(this);
    else wine->close();
    wine->setReadChannel(QProcess::StandardError);
    connect(wine, &QProcess::started, this, [this]() {
        ui->traceBrowser->clear();
    });
    connect(wine, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        std::cerr << err << std::endl;
        // error
    });
    connect(wine, &QProcess::readyRead, this, [this]() {
        static int indent = 0;
        while (wine->canReadLine()) {
            QByteArray line = wine->readLine();
            QString inserted = QString(line.trimmed());
            if (inserted.contains("Ret")) indent--;

            if (inserted.contains(ui->searchTraceBrowser->text()) && !inserted.contains("Heap")&& !inserted.contains("mem")) { // TODO yokunai
                if (ui && ui->traceBrowser) ui->traceBrowser->append(QString().fill(' ',indent) + inserted);
            }
            if (inserted.contains("Call")) indent++;
        }
    });
    qputenv("WINEDEBUG", "+relay");
    wine->start("wine", QStringList() << ui->newProcess->text());

}

MainWindow::~MainWindow()
{
    delete strace;
    delete ui;
}
