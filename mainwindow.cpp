#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDesktopWidget>
#include <QScreen>
#include <QLabel>
#include <QMouseEvent>
#include <windows.h>
#include <QTime>
#include <QReadWriteLock>

QReadWriteLock lock;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow), ScreenshotPic(0), Player(0), SquareTemplate("FieldUnknown.png")
{
    ui->setupUi(this);

    //QGraphicsScene *Scene= new QGraphicsScene(this);
    //ui->graphicsView->setScene(Scene);

    LineEditX= new QLineEdit(this);
    LineEditY= new QLineEdit(this);
    LineEditX->setFixedWidth(40);
    LineEditY->setFixedWidth(40);
    QLabel *LabelX= new QLabel("X", this);
    QLabel *LabelY= new QLabel("Y", this);
    ui->mainToolBar->insertWidget(ui->actionMouse_Click, LabelX);
    ui->mainToolBar->insertWidget(ui->actionMouse_Click, LineEditX);
    ui->mainToolBar->insertWidget(ui->actionMouse_Click, LabelY);
    ui->mainToolBar->insertWidget(ui->actionMouse_Click, LineEditY);

    highlighter = new MyHighlighter(ui->plainTextEdit->document());
}

MainWindow::~MainWindow()
{
    if (Player)
        delete Player;
    delete highlighter;
    delete ui;
    // ???
}

void MainWindow::on_actionPlay_Minesweeper_triggered()
{
    WId DesktopId= QApplication::desktop()->winId();
    QScreen *Screen= QApplication::screens()[0];
    QPixmap DesktopImage= Screen->grabWindow(DesktopId);

    GetFieldParameters(DesktopImage.toImage());
}

void MainWindow::on_actionMouse_Click_triggered()
{
    X= 95;
    Y= 176;
    OpenField(QPoint(LineEditX->text().toUShort(), LineEditY->text().toUShort()), MinesweeperPlayer::LEFT);
}

void MainWindow::on_actionScreenshot_triggered()
{
    QPixmap DesktopImage= QApplication::screens()[0]->grabWindow(QApplication::desktop()->winId());
    if (!ScreenshotPic) {
        delete ScreenshotPic;
        //ScreenshotPic= ui->graphicsView->scene()->addPixmap(DesktopImage);
    }
    else
        ScreenshotPic->setPixmap(DesktopImage);
}

void MainWindow::GetFieldParameters(const QImage Screenshot) {
    dbgFieldIndex = 0;
    deducedBombs.clear();
    dangerousSquare = false;

    for (Y= 0; Y<Screenshot.height()-SquareTemplate.width(); Y++)
        for (X= 0; X<Screenshot.width()-SquareTemplate.width(); X++) {
            if (SquareTemplate==Screenshot.copy(X, Y, SquareTemplate.width(), SquareTemplate.height()))
                goto Out;
        }
Out:
    qDebug("First field square X=%d, Y=%d", X, Y);
    for (W= 1; SquareTemplate==
         Screenshot.copy(X+W*SquareTemplate.width(), Y, SquareTemplate.width(), SquareTemplate.height()); W++)
        {}
    for (H= 1; SquareTemplate==
         Screenshot.copy(X, Y+H*SquareTemplate.height(), SquareTemplate.width(), SquareTemplate.height()); H++)
        {}
    qDebug("Field's width and height: W=%d, H=%d", W, H);
    OldField= Screenshot.copy(X, Y, W*SquareTemplate.width(), H*SquareTemplate.height());
    // Определим количество бомб
#define SCAN_START_X    -10
#define SCAN_START_Y    -50
#define SCAN_END_X      50
#define SCAN_END_Y      0
    QVector<QImage> Digits= {QImage("0.png"), QImage("1.png"), QImage("2.png"), QImage("3.png"), QImage("4.png"),
                             QImage("5.png"), QImage("6.png"), QImage("7.png"), QImage("8.png"), QImage("9.png")};
    int digit_x= -1, digit_y, d1= -1, d2, d3;
    for (digit_y= Y+SCAN_START_Y; digit_y < Y+SCAN_END_Y-Digits[0].width(); digit_y++)
        for (digit_x= X+SCAN_START_X; digit_x < X+SCAN_END_X-Digits[0].width(); digit_x++)
            if (Digits[0]==Screenshot.copy(digit_x, digit_y, Digits[0].width(), Digits[0].height()))
                { d1= 0; goto Out2; }
            else if (Digits[1]==Screenshot.copy(digit_x, digit_y, Digits[1].width(), Digits[1].height()))
                { d1= 1; goto Out2; }
Out2:
    for (d2= 0; d2<Digits.size(); d2++)
        if (Digits[d2]==Screenshot.copy(digit_x+Digits[d1].width(), digit_y, Digits[d2].width(), Digits[d2].height()))
            break;
    for (d3= 0; d3<Digits.size(); d3++)
        if (Digits[d3]==Screenshot.copy(digit_x+Digits[d1].width()+Digits[d2].width(),
                                        digit_y, Digits[d3].width(), Digits[d3].height()))
            break;
    int BombsCount= d1*100+d2*10+d3;
    qDebug("Scanned bombs amount = %d", BombsCount);

    delete Player;
    Player= new MinesweeperPlayer(QSize(W, H), BombsCount);
    connect(this, SIGNAL(NewInfo(void*)), Player, SLOT(ItWasIt(void*)));
    qRegisterMetaType<MinesweeperPlayer::MouseButton>("MinesweeperPlayer::MouseButton");    // Без этого следующая команда не работает
    connect(Player, SIGNAL(TryThis(QPoint, MinesweeperPlayer::MouseButton)), this, SLOT(OpenField(QPoint, MinesweeperPlayer::MouseButton)));
    connect(Player, SIGNAL(NeedNewInfo()), this, SLOT(Scan()));
    connect(Player, SIGNAL(DangerousSquare(float)), this, SLOT(PrintInfo(float)));
    connect(Player, SIGNAL(endOfGame()), this, SLOT(makeEmptyLine()));

    emit NewInfo(reinterpret_cast<void*>(new QList<MinesweeperPlayer::Square>()));
}

void MainWindow::OpenField(const QPoint Guess, MinesweeperPlayer::MouseButton button) {
    qDebug() << Q_FUNC_INFO << QTime::currentTime();
    qDebug("We are trying field with x = %d, y = %d", Guess.x(), Guess.y());
    if (dangerousSquare) {
        qApp->processEvents();
        QThread::msleep(HESITATION_PAUSE);  // Отображение происходит не напрямую, а через event loop, поэтому задержка в PrintInfo работает не так, как ожидалось. Нужно обработать события перед задержкой, потому что обработка этого слота может состояться раньше, чем обработка отображения текста
    }
    else
        QThread::msleep(100);

    uint xPixels= SquareTemplate.width() * (Guess.x()+0.5) + X;
    uint yPixels= SquareTemplate.height() * (Guess.y()+0.5) + Y;
    qDebug("Coords of mouse click x = %d, y = %d", xPixels, yPixels);

    uint x= xPixels * 65535 / QApplication::screens()[0]->size().width();
    uint y= yPixels * 65535 / QApplication::screens()[0]->size().height();

    mouse_event(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE, x, y, 0, 0);
    if (button == MinesweeperPlayer::LEFT) {
        for (int i=0; i<2; i++) {
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        }
        Guesses.append(Guess);
    } else {
        mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
        mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
        deducedBombs << Guess;
    }

    // Новый способ симуляции кликов мышки не решает проблему, когда старые программы не реагируют на клики, сгенерированные
    // запущенной из Qt программы
    /*INPUT mouseInp;
    mouseInp.mi.dx = x;
    mouseInp.mi.dy = y;
    mouseInp.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    SendInput(1, &mouseInp, sizeof(mouseInp));
    QThread::msleep(300);
    for (int i=0; i<2; i++) {
        mouseInp.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE;
        SendInput(1, &mouseInp, sizeof(mouseInp));
        QThread::msleep(300);
        mouseInp.mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_ABSOLUTE;
        SendInput(1, &mouseInp, sizeof(mouseInp));
        QThread::msleep(300);
    }*/

}

void MainWindow::Scan() {
    qDebug() << Q_FUNC_INFO << QTime::currentTime();
    static QVector<QImage> FieldPics{QImage("FieldUnknown.png"), QImage("FieldBomb.png"), QImage("FieldOk.png"),
                                     QImage("FieldNone.png"),
                                     QImage("Field1.png"), QImage("Field2.png"), QImage("Field3.png"), QImage("Field4.png"),
                                     QImage("Field5.png"), QImage("Field6.png"), QImage("Field7.png"), QImage("Field8.png"),
                                     QImage("FieldBombNotClicked.png")};

    QImage NewField= QApplication::screens()[0]->grabWindow( QApplication::desktop()->winId() ).toImage().copy(
                X, Y, W*SquareTemplate.width(), H*SquareTemplate.height());
    QVector<QVector<bool>> Checked(H, QVector<bool>(W, false));
    QList<MinesweeperPlayer::Square>* Appeared= new QList<MinesweeperPlayer::Square>;
    Appeared->reserve(1000);
    int level= 0;
    std::function<void(int, int)> Scan= [&](int x, int y) {
        if (x<0 || y<0 || x==W || y==H || Checked[y][x])
            return;
        qDebug() << "+ level" << ++level;
        qDebug("Scanning field (%d, %d)", x, y);
        QImage SquareInQuestion;
        if ((SquareInQuestion= NewField.copy(x*SquareTemplate.width(), y*SquareTemplate.height(),
                                             SquareTemplate.width(), SquareTemplate.height()))==
                OldField.copy(x*SquareTemplate.width(), y*SquareTemplate.height(),
                              SquareTemplate.width(), SquareTemplate.height())) {
            qDebug() << "- level" << --level;
            return;                                                                 // Nothing new..
        }
        for (int i= 0; i<=FieldPics.size(); i++) {
            if (i==FieldPics.size()) {
                qDebug("Found unknown sign, terminating..");
                qDebug() << "Unknown image have been saved to Dbg.png succesfully =" << SquareInQuestion.save("Dbg.png");
                qDebug() << "Field image have been saved to DbgField.png succesfully =" << NewField.save("DbgField.png");
                while (1) { }
            }
            if (SquareInQuestion==FieldPics[i]) {
                Checked[y][x]= true;
                //qDebug("Field (%d, %d) have been checked", x, y);
                switch (i) {
                case 0: i= DO_NOT_KNOW; break;
                case 1: i = BOMB; break;
                case 2: i= WE_HAVE_WON; break;      // Если появились флажки -- значит мы победили -- сами мы флажки не ставим
                case 12: i = BOMB; break;
                default: i-=3;
                }
                if (!deducedBombs.contains(QPoint(x, y)))
                    Appeared->append({0, QPoint(x, y), i});
                Scan(x+1, y-1);
                Scan(x+1, y);
                Scan(x+1, y+1);
                Scan(x, y+1);
                Scan(x-1, y+1);
                Scan(x-1, y);
                Scan(x-1, y-1);
                Scan(x, y-1);
                break;
            }
        }
        qDebug() << "- level" << --level;
    };
    qDebug(">>> Enter scanning new fields process");
    QThread::msleep(100);
    for (QPoint& Guess: Guesses)
        Scan(Guess.x(), Guess.y());
    qDebug() << "Out of scan.., Guesses.size()==" << Guesses.size();
    Guesses.clear();
    qDebug() << "Bug searching 1";

    NewField.save(QString("move") + QString::number(dbgFieldIndex++) + ".png");

    // Если ничего нового не появилось, то мы молодцы
    if (Appeared->isEmpty()) {
        qDebug() << "Good work!";
        return;
    }
    qDebug() << "Bug searching 2";
    // Если бомба, то умираем
    if ((*Appeared)[0].Number==BOMB) {
        qDebug() << "Oh my god! There is a bomb..";
        ui->plainTextEdit->insertPlainText("Fail\n");
        makeEmptyLine();
        return;
    } else if (dangerousSquare) {
        ui->plainTextEdit->insertPlainText("Ok\n");
        dangerousSquare = false;
    }
    qDebug() << "Bug searching 3";
    OldField= NewField;
    qDebug() << "Bug searching 4";
    lock.lockForWrite();
    emit NewInfo(reinterpret_cast<void*>(Appeared));
    qDebug() << "Bug searching 5, Appeared.size()==" << Appeared->size();
    lock.unlock();
}

void MainWindow::PrintInfo(float bombProbability) {
    ui->plainTextEdit->insertPlainText(QString("Probability of bomb = %1.. ").arg(bombProbability));
    ui->plainTextEdit->ensureCursorVisible();
    dangerousSquare = true;
}

void MyHighlighter::highlightBlock(const QString &text) {
    static const char *okText = "Ok", *failText = "Fail";
    int index = text.indexOf(okText);
    if (index != -1)
        setFormat(index, sizeof okText, QColor(Qt::green));
    index = text.indexOf(failText);
    if (index != -1)
        setFormat(index, sizeof failText, QColor(Qt::red));
}

void MainWindow::makeEmptyLine() {
    ui->plainTextEdit->insertPlainText("\n");
}
