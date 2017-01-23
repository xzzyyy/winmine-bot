#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsPixmapItem>
#include <QLineEdit>
#include <QSet>
#include <QSyntaxHighlighter>

#include "minesweeperplayer.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

signals:
    void NewInfo(void* AppearedRaw);
    
private slots:
    void on_actionPlay_Minesweeper_triggered();
    void on_actionMouse_Click_triggered();
    void on_actionScreenshot_triggered();
    void OpenField(const QPoint Guess, MinesweeperPlayer::MouseButton button);
    void Scan();
    void PrintInfo(float bombProbability);
    void makeEmptyLine();

private:
    Ui::MainWindow *ui;

    class MyHighlighter *highlighter;
    bool dangerousSquare;

    QGraphicsPixmapItem *ScreenshotPic;
    QImage OldField;

    QLineEdit *LineEditX;
    QLineEdit *LineEditY;

    int X, Y;       // Screen coords of left-upper square
    int W, H;
    MinesweeperPlayer* Player;

    QImage SquareTemplate;

    QList<QPoint> Guesses;

    QSet<QPoint> deducedBombs;  // Здесь мы сохраняем все поля, в которые тыкнули сами, а значит добавлять их в Appeared смысла нет

    int dbgFieldIndex = 0;

    //void Scan(int x, int y, QVector<QVector<bool>>& Checked, QImage& NewField, QList<MinesweeperPlayer::Square>* Appeared);
    void GetFieldParameters(const QImage Screenshot);
};

class MyHighlighter: public QSyntaxHighlighter {
public:
    MyHighlighter(QTextDocument *doc) : QSyntaxHighlighter(doc) { }
    void highlightBlock(const QString &text);
};

#endif // MAINWINDOW_H
