#ifndef MINESWEEPERPLAYER_H
#define MINESWEEPERPLAYER_H

#include <QObject>
#include <QSize>
#include <QPoint>
#include <QThread>
#include <QVector>
#include <QDebug>

#define BEGIN       -2
#define BOMB        -1
#define DO_NOT_KNOW -3
#define PARTIALLY_KNOWN -4
#define WE_HAVE_WON -5

#define DOESNT_MATTER    0

#define SCAN_AFTER_CLICK_DELAY  100

#define HESITATION_PAUSE    5000

class MinesweeperPlayer : public QObject
{
    Q_OBJECT
public:
    enum MouseButton { LEFT, RIGHT };
    struct Square {
        float Probability;
        QPoint Loc;
        int Number;
        bool operator==(Square b) { return Loc==b.Loc; }
        Square next(int xshift, int yshift) { Square New; New.Loc= Loc+QPoint(xshift, yshift); return New; }
    };
private:
    QSize FieldSize;
    int BombsCount;
    int **Field;
    QVector<QVector<float>> ProbField;
    int Unknowns;
    class RandInitializer {
    public:
        RandInitializer();
    } *RandInit;

    QPoint Guess;

    QThread ObjectsThread;

    int CountBombsAround(int x, int y);
    bool TrySetBomb(const QPoint &C);
    void Sort(QList<Square> &Q);
    void Sort(QList<QPoint> &Q);
public:
    MinesweeperPlayer(QSize FieldSize, int BombsCount);
    ~MinesweeperPlayer();
    static bool LessThan(const Square &First, const Square &Second);
    void StopThread();
signals:
    void TryThis(const QPoint Guess, MinesweeperPlayer::MouseButton button);
    void NeedNewInfo();
    void DangerousSquare(float bombProbability);
    void endOfGame();
public slots:
    void ItWasIt(void* AppearedRaw);
};

inline uint qHash(QPoint S) {
    return S.y() * 1000 + S.x();
};

#endif // MINESWEEPERPLAYER_H
