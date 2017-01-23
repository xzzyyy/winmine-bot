#include "minesweeperplayer.h"
#include <QTime>
#include <QApplication>
#include <QWidget>
//#include <windows.h>
#include <QReadWriteLock>

extern QReadWriteLock lock;

MinesweeperPlayer::MinesweeperPlayer(QSize FieldSize, int BombsCount) :
    QObject(), FieldSize(FieldSize), BombsCount(BombsCount),
    ProbField(FieldSize.height(),
              QVector<float>(FieldSize.width(), float(BombsCount)/(FieldSize.width()*FieldSize.height()))),
    Unknowns(FieldSize.width()*FieldSize.height()),
    RandInit(0), ObjectsThread(QApplication::activeWindow())
{
    // ???
    Field= new int *[FieldSize.height()+2];
    for (int i= 0; i<FieldSize.height()+2; ++i) {
        Field[i]= new int[FieldSize.width()+2];
        for (int j= 0; j<FieldSize.width()+2; ++j)
            Field[i][j]= DO_NOT_KNOW;
        Field[i]++;
    }
    Field++;
    for (int i= -1; i<FieldSize.height()+1; i++)
        for (int j= -1; j<FieldSize.width()+1; j++)
            Field[i][j]= DO_NOT_KNOW;                               // Segmentation fault при y=9 (?)

    moveToThread(&ObjectsThread);
    ObjectsThread.start();
}

MinesweeperPlayer::~MinesweeperPlayer() {
    ObjectsThread.quit();
    for (int i= 0; i<FieldSize.height()+2; ++i)
        delete [] (Field[i-1]-1);   // Возможно работать не будет, по той же причине, по какой не работает
    delete [] (Field-1);            // range-based for loop
    qApp->thread()->msleep(100);    // Wait some time until thread commits suicide before killing it's master
}

bool MinesweeperPlayer::LessThan(const Square &First, const Square &Second) {
    return First.Probability > Second.Probability;
}

void MinesweeperPlayer::StopThread() {
    ObjectsThread.quit();
}

int MinesweeperPlayer::CountBombsAround(int x, int y) {
    int Bombs= 0;
    if (Field[y-1][x+1]==BOMB) Bombs++;
    if (Field[y  ][x+1]==BOMB) Bombs++;
    if (Field[y+1][x+1]==BOMB) Bombs++;
    if (Field[y+1][x  ]==BOMB) Bombs++;
    if (Field[y+1][x-1]==BOMB) Bombs++;
    if (Field[y  ][x-1]==BOMB) Bombs++;
    if (Field[y-1][x-1]==BOMB) Bombs++;
    if (Field[y-1][x  ]==BOMB) Bombs++;
    //qDebug("\tAround %d bombs", Bombs);
    return Bombs;
}

bool MinesweeperPlayer::TrySetBomb(const QPoint& C) {
    bool GoodPlaceForBomb= true;
    int Loc= 0;
    do {
        qDebug("\t>>> TrySetBomb cycle enter, Loc=%d", Loc);
        int x= -1, y= -1;
        Loc++;
        switch (Loc) {
        case 1: x= C.x()+1; y= C.y()-1; break;
        case 2: x= C.x()+1; y= C.y();   break;
        case 3: x= C.x()+1; y= C.y()+1; break;
        case 4: x= C.x();   y= C.y()+1; break;
        case 5: x= C.x()-1; y= C.y()+1; break;
        case 6: x= C.x()-1; y= C.y();   break;
        case 7: x= C.x()-1; y= C.y()-1; break;
        case 8: x= C.x();   y= C.y()-1;
        }
        if (x>0 && y>0 && x<FieldSize.width() && y<FieldSize.height() &&
                Field[y][x]>0 &&
                CountBombsAround(x, y)>Field[y][x])
            GoodPlaceForBomb= false;
    } while (Loc<=8 && GoodPlaceForBomb);
    qDebug("\tLocation (%d, %d) is a %s place for bomb", C.x(), C.y(), GoodPlaceForBomb ? "GOOD" : "BAD");
    return GoodPlaceForBomb;
}

struct Double {
    long double x;
    Double(double x): x(x) {}
    operator long double() {
        return x;
    }
};

long double operator!(Double x) {
    if (x==0)
        //return 0;
        return 1;       // При вычислении Untouched происходит деление на 0 в этом выражении "!Double(PureProbability-1 - (BombsLeft-1))", если ! возвращает 0
    long double Res= 1;
    for (int i= 2; i<x.x+1; i++)
        Res*= i;
    return Res;
}

template <typename T> void RemoveDuplicates(QList<T> &List) {
    for (int i= 0; i<List.size(); i++)
        for (int j= 0; j<List.size(); j++)
            if (i!=j && List[i]==List[j]) {
                List.removeAt(j);
                j--;
            }
}

void MinesweeperPlayer::Sort(QList<Square> &Q) {
    int i= 0;
    for (int y= 0; y<FieldSize.width(); y++)
        for (int x= 0; x<FieldSize.width(); x++) {
            Square S{0, QPoint(x, y), DO_NOT_KNOW};
            if (Q.contains(S))
                Q.swap(Q.indexOf(S), i++);
        }
}

void MinesweeperPlayer::Sort(QList<QPoint> &Q) {
    int i= 0;
    for (int y= 0; y<FieldSize.width(); y++)
        for (int x= 0; x<FieldSize.width(); x++) {
            if (Q.contains(QPoint(x, y)))
                Q.swap(Q.indexOf(QPoint(x, y)), i++);
        }
}

void MinesweeperPlayer::ItWasIt(void* AppearedRaw) {
#define CUT_MAGIC_NUMBER    20
    qDebug() << Q_FUNC_INFO;
    QList<Square>* Appeared = reinterpret_cast<QList<Square>* >(AppearedRaw);

    if (!RandInit)
        RandInit= new RandInitializer;

    bool weHaveWon = false;
    for (Square &S: *Appeared) {
        if (S.Number == WE_HAVE_WON)          // Если появились флажки -- значит мы победили -- сами мы флажки не ставим
            weHaveWon = true;
        Field[S.Loc.y()][S.Loc.x()]= S.Number;
        qDebug() << "Appeared field " << S.Loc;
    }
    qDebug("Appeared new fields count = %d", Appeared->size());
    Unknowns-= Appeared->size();
    lock.lockForWrite();
    delete Appeared;
    lock.unlock();

    QList<Square> Queue;
    bool Beginning= true;
    for (int i= 0; i<FieldSize.height(); ++i)
        for (int j= 0; j<FieldSize.width(); ++j) {
            if ( (Field[i][j]==DO_NOT_KNOW || Field[i][j]==PARTIALLY_KNOWN) &&
                    (Field[i-1][j+1]>0 ||
                    Field[i][j+1]>0 ||
                    Field[i+1][j+1]>0 ||
                    Field[i+1][j]>0 ||
                    Field[i+1][j-1]>0 ||
                    Field[i][j-1]>0 ||
                    Field[i-1][j-1]>0 ||
                    Field[i-1][j]>0) )
                Queue.append({DO_NOT_KNOW, QPoint(j, i), Field[i][j]});
            if (Beginning && Field[i][j]!=DO_NOT_KNOW)
                Beginning= false;
        }
    qDebug("Analyzed squares count = %d", Queue.size());
    // Если ничего нового не открылось, мы молодцы
    if (Queue.isEmpty() && !Beginning && !BombsCount || weHaveWon) {
        emit endOfGame();
        qDebug() << "Uhhh.. We saved:)";
        return;
    }

    char Buffer[200]; Buffer[0]= 0;         // Буфер для printf

    QList<QList<Square>> Queues;
    QList<QPoint> QueueCopy;
    for (Square S: Queue)
        QueueCopy.append(S.Loc);
    while (!QueueCopy.isEmpty()) {                                  // Находим независимые друг от друга части поля
        QList<QPoint> SQ;
        std::function<void(QPoint, QPoint)> AddSquare= [&QueueCopy, &SQ, this, &AddSquare](QPoint S, QPoint OldS) {
            if (S.x()<0 || S.y()<0 || S.x()>=FieldSize.width() || S.y()>=FieldSize.height() ||
                    !QueueCopy.contains(S) && !(Field[S.y()][S.x()]>0) || SQ.contains(S) ||
                    (QueueCopy.contains(S) && QueueCopy.contains(OldS)) ||
                    (Field[S.y()][S.x()]>0 && Field[OldS.y()][OldS.x()]>0))
                return;
            SQ.append(S);
            AddSquare(S+QPoint( 1,-1), S);
            AddSquare(S+QPoint( 1, 0), S);
            AddSquare(S+QPoint( 1, 1), S);
            AddSquare(S+QPoint( 0, 1), S);
            AddSquare(S+QPoint(-1, 1), S);
            AddSquare(S+QPoint(-1, 0), S);
            AddSquare(S+QPoint(-1,-1), S);
            AddSquare(S+QPoint( 0,-1), S);
        };
        AddSquare(QueueCopy.first(), QPoint(-1, -1));
        RemoveDuplicates<QPoint>(SQ);
        for (int i= 0; i<SQ.size(); i++)
            if (!QueueCopy.contains(SQ[i])) {
                SQ.removeAt(i);
                i--;
            } else
                QueueCopy.removeOne(SQ[i]);
        Sort(SQ);
        qDebug() << Buffer;
        qDebug() << "New SubQueue with size " << SQ.size();
        Buffer[0]= 0;
        QList<Square> SQs;
        for (QPoint &P: SQ) {
            sprintf(Buffer+strlen(Buffer), "(%d,%d)", P.x(), P.y());
            SQs.append({0, P, DO_NOT_KNOW});
        }
        qDebug() << Buffer;
        Queues.append(SQs);
    };

    //--------------------------------------------------------------------------------------------------------------------------
    // Находим распределение бомб нужное нам для подсчета вероятностей для одного автономного участка поля связанных квадратиков
    auto GetPossibleBombs= [this, Buffer](const QList<Square> &Queue, QHash<int, QHash<QPoint, long long>> &BombsAmountsLocal,
                                          QHash<int, long long>& VariantsWithXBombs) mutable {
        QVector<int> Combinations(Queue.size());
        QList<QList<QPoint>> Orientirss;
        QList<QList<QList<Square>>> CutVariants;
        QList<QList<Square>> Variants;
        QList<QPoint> BorderPoints;

        //--------------------------------------------------------------------------------------------------------------------------
        // Проверка комбинации бомб на корректность -- она не должна противоречить цифрам
        auto CheckQueue= [this, &BorderPoints](QList<Square>& Queue, const bool Full= false) -> bool {
            bool GoodVariant= true;
            auto GoodComplete= [this](int x, int y) -> bool {
                //qDebug() << "Field[y][x]==" << Field[y][x];
                return Field[y][x]<0 || CountBombsAround(x, y)==Field[y][x];
            };
            auto GoodEnough= [this](int x, int y) -> bool {
                return Field[y][x]<0 || CountBombsAround(x, y)<=Field[y][x];
            };
            auto GoodSmart= [this, &BorderPoints, &GoodEnough, &GoodComplete](int x, int y) -> bool {
                if (/*BorderPoints.contains(QPoint(x+1, y-1)) || BorderPoints.contains(QPoint(x+1, y)) ||
                        BorderPoints.contains(QPoint(x+1,y+1)) || BorderPoints.contains(QPoint(x,y+1)) ||
                        BorderPoints.contains(QPoint(x-1,y+1)) || BorderPoints.contains(QPoint(x-1,y)) ||
                        BorderPoints.contains(QPoint(x-1,y-1)) || BorderPoints.contains(QPoint(x,y-1))*/
                        BorderPoints.contains(QPoint(x, y)))
                    return GoodEnough(x, y);
                else
                    return GoodComplete(x, y);
            };
            for (Square &S: Queue)
                Field[S.Loc.y()][S.Loc.x()]= S.Number;
            for (Square &S: Queue)
                //qDebug() << "Queue: " << S.Loc;
                if (Full) {
                    if (!GoodComplete(S.Loc.x()+1, S.Loc.y()-1) || !GoodComplete(S.Loc.x()+1, S.Loc.y()) ||
                            !GoodComplete(S.Loc.x()+1, S.Loc.y()+1) || !GoodComplete(S.Loc.x(), S.Loc.y()+1) ||
                            !GoodComplete(S.Loc.x()-1, S.Loc.y()+1) || !GoodComplete(S.Loc.x()-1, S.Loc.y()) ||
                            !GoodComplete(S.Loc.x()-1, S.Loc.y()-1) || !GoodComplete(S.Loc.x(), S.Loc.y()-1)) {
                        GoodVariant= false;
                        break;
                    }
                } else {
                    if (!GoodSmart(S.Loc.x()+1, S.Loc.y()-1) || !GoodSmart(S.Loc.x()+1, S.Loc.y()) ||
                            !GoodSmart(S.Loc.x()+1, S.Loc.y()+1) || !GoodSmart(S.Loc.x(), S.Loc.y()+1) ||
                            !GoodSmart(S.Loc.x()-1, S.Loc.y()+1) || !GoodSmart(S.Loc.x()-1, S.Loc.y()) ||
                            !GoodSmart(S.Loc.x()-1, S.Loc.y()-1) || !GoodSmart(S.Loc.x(), S.Loc.y()-1)) {
                        GoodVariant= false;
                        break;
                    }
                }
            return GoodVariant;
        };
        //--------------------------------------------------------------------------------------------------------------------------

        for (int Idx= Queue.size()-1; Idx>=0; Idx-= CUT_MAGIC_NUMBER) {     // Queue режется на кусочки по CUT_MAGIC_NUMBER квадратиков
            QList<QPoint> Orientirs;
            int OldIdx= Idx;
            int Border= Idx+1-CUT_MAGIC_NUMBER;
            if (Border<0)
                Border= 0;
            for (Square &S: Queue.mid(Border, OldIdx-Border+1)) {
                if (Field[S.Loc.y()-1][S.Loc.x()+1]>=0) Orientirs.append(QPoint(S.Loc.x()+1, S.Loc.y()-1));
                if (Field[S.Loc.y()  ][S.Loc.x()+1]>=0) Orientirs.append(QPoint(S.Loc.x()+1, S.Loc.y()  ));
                if (Field[S.Loc.y()+1][S.Loc.x()+1]>=0) Orientirs.append(QPoint(S.Loc.x()+1, S.Loc.y()+1));
                if (Field[S.Loc.y()+1][S.Loc.x()  ]>=0) Orientirs.append(QPoint(S.Loc.x()  , S.Loc.y()+1));
                if (Field[S.Loc.y()+1][S.Loc.x()-1]>=0) Orientirs.append(QPoint(S.Loc.x()-1, S.Loc.y()+1));
                if (Field[S.Loc.y()  ][S.Loc.x()-1]>=0) Orientirs.append(QPoint(S.Loc.x()-1, S.Loc.y()  ));
                if (Field[S.Loc.y()-1][S.Loc.x()-1]>=0) Orientirs.append(QPoint(S.Loc.x()-1, S.Loc.y()-1));
                if (Field[S.Loc.y()-1][S.Loc.x()  ]>=0) Orientirs.append(QPoint(S.Loc.x()  , S.Loc.y()-1));
            }
            RemoveDuplicates<QPoint>(Orientirs);
            Orientirss.append(Orientirs);
        }
        for (QList<QPoint> &O: Orientirss)          // Находим точки на границе прилегающих кусочков
            for (QPoint &P: O)
                for (QList<QPoint> &O2: Orientirss)
                    if (O2!=O && O2.contains(P))
                        BorderPoints.append(P);
        RemoveDuplicates<QPoint>(BorderPoints);
        if (Queue.size()>CUT_MAGIC_NUMBER) {
            qDebug() << "BorderPoints:";
            for (QPoint &BP: BorderPoints)
                qDebug() << BP;
        }

        for (int Idx= Queue.size()-1; Idx>=0; Idx-= CUT_MAGIC_NUMBER) {         // Перебираем все варианты в каждом из кусочков
            CutVariants.append(QList<QList<Square>>());
            int Border;
            Combinations.fill(1);
            int OldIdx= Idx;
            Border= Idx+1-CUT_MAGIC_NUMBER;
            if (Border<0)
                Border= 0;

            Buffer[0]= 0;
            for (int i= Border; i<=Idx; i++)
                sprintf(Buffer+strlen(Buffer), "(%d,%d) ", Queue[i].Loc.x()+1, Queue[i].Loc.y()+1);
            qDebug(Buffer);

            QList<Square> SubQueue= Queue.mid(Border, OldIdx-Border+1);
            if (Queue.size()>CUT_MAGIC_NUMBER && Border==0)
                for (Square& S: SubQueue)
                    qDebug() << S.Loc;
            do {                    // Ядро алгоритма -- здесь осуществляется перебор всех возможных комбинаций и выбор подходящих
                do {                                                // Рассмотреть следующую комбинацию
                    Combinations[Idx]++;
                    int SQIdx= Idx-Border;
                    if (Combinations[Idx]>1) {
                        Combinations[Idx]= 0;
                        Field[SubQueue[SQIdx].Loc.y()][SubQueue[SQIdx].Loc.x()]= DO_NOT_KNOW;
                        Idx--;
                    } else {
                        Field[SubQueue[SQIdx].Loc.y()][SubQueue[SQIdx].Loc.x()]= BOMB;
                        break;
                    }
                } while (Idx>=Border);

                //qDebug("\tNew variant! It's %s", GoodVariant ? "GOOD" : "BAD");
                for (Square &S: SubQueue)
                    S.Number= Field[S.Loc.y()][S.Loc.x()];
                if (CheckQueue(SubQueue))
                    CutVariants.last().append(SubQueue);
                /*if (++Counter==10000)
                    qDebug() << "Combinations processed (*10k)" << ++BigCounter;
                Counter%=10000;*/

                /*QString DebugStr;                                   // Вывести отладочную информацию
                for (Square& S: Queue)
                    DebugStr+= Field[S.Loc.y()][S.Loc.x()]==BOMB ? "b" : "-";
                qDebug() << DebugStr;
                thread()->msleep(100);*/

                Idx= OldIdx;
            } while (Combinations.mid(Border, OldIdx-Border+1).contains(0));
            for (const Square& S: Queue)
                Field[S.Loc.y()][S.Loc.x()]= DO_NOT_KNOW;           // Почистим поле от наших предположений
        }

        qDebug("CutVariants.size()==%d", CutVariants.size());
        int cutVariantsNumber = 1;
        for (int i= 0; i<CutVariants.size(); i++) {
            qDebug("There are %d variants in CutVariants[%d]", CutVariants[i].size(), i);
            cutVariantsNumber *= CutVariants[i].size();
        }
        if (CutVariants.size() >= 2)
            qDebug() << "Overall amount of variants to check =" << cutVariantsNumber;

        for (int i=0; i<CutVariants.size()/2; i++)          // Разворачиваем подочереди, чтобы они приняли правильный порядок
            CutVariants.swap(i, CutVariants.size()-i-1);

        int Size= 1;                        // Подсчет параметров для слияния подочередей
        for (int i= 0; i<CutVariants.size(); i++)
            Size*= CutVariants[i].size();
        int Period[CutVariants.size()];
        Period[CutVariants.size()-1]= 1;
        qDebug("Period[...]==%d", Period[CutVariants.size()-1]);
        for (int i= CutVariants.size()-2; i>=0; i--) {
            Period[i]= Period[i+1]*CutVariants[i+1].size();
            qDebug("Period[%d]==%d", i, Period[i]);
        }

        qDebug("Assembling variants...");   // Слияние подочередей
        for (int i= 0; i<Size; i++) {
            if (i && i%100000 == 0)
                qDebug() << (QString("%1").arg(i/100000) + "00000") << QTime::currentTime();

            QList<Square> Q;
            Q.append(CutVariants[0][i/Period[0]%CutVariants[0].size()]);
            bool badVariant = false;
            //Buffer[0]= 0;
            for (int j= 1; j<CutVariants.size(); j++) {
                Q.append(CutVariants[j][i/Period[j]%CutVariants[j].size()]);
                //sprintf(Buffer+strlen(Buffer), "%5d ", i/Period[j]%CutVariants[j].size());
                Sort(Q);
                if (j != CutVariants.size() && !CheckQueue(Q, false)) {    // Промежуточная проверка, мы отметём много заведомо неподходящих вариантов здесь и существенно сократим время, когда cutVariantsNumber > 500k
                    badVariant = true;
                    break;
                }
            }
            if (!badVariant && CheckQueue(Q, true))
                Variants.append(Q);
            for (Square &S: Q)
                Field[S.Loc.y()][S.Loc.x()] = DO_NOT_KNOW;
            //qDebug(Buffer);
        }

        /*qDebug("There are %d variants of bombs placement after gluing pieces, there are some junk though", Variants.size());
        QList<int> Unneeded;                    // Удаляем невозможные комбинации, которые получились в большом кол-ве в результате слияния
        for (int i= 0; i<Variants.size(); i++) {
            if (!CheckQueue(Variants[i], true))
                Unneeded.append(i);
            if (!(i%100000))
                qDebug() << "100k * " << i/100000;
        }
        for (int i=Unneeded.size()-1; i>=0; i--)
            Variants.removeAt(Unneeded[i]);*/

        qDebug("There are %d variants of bombs placement", Variants.size());
        if (Variants.size()==0) {
            qDebug() << "Variants.size()==0, execution suspended..";
            while (1) {
            }
        }

        for (const Square& S: Queue)
            Field[S.Loc.y()][S.Loc.x()]= DO_NOT_KNOW;           // Почистим поле от наших предположений

        // Подсчитываем кол-во бомб в каждом квадратике для каждого варианта общего кол-ва бомб
        for (int i= 0; i<Variants.size(); i++) {
            QHash<QPoint, int> BombsMap;
            int BombsAmount= 0;
            for (Square &S: Variants[i]) {
                BombsAmount+= S.Number==BOMB ? 1 : 0;
                BombsMap[S.Loc]+= S.Number==BOMB ? 1 : 0;
            }
            //qDebug("BombsCount==%d, BombsAmount==%d", BombsCount, BombsAmount);
            if (BombsAmount > BombsCount)   // Некоторые варианты не подходят, потому что мы знаем, что бомб осталось меньше, чем в этом варианте
                continue;

            BombsAmountsLocal[BombsAmount];     // Если такого элемента нет, создаем
            for (QPoint P: BombsMap.keys())
                BombsAmountsLocal[BombsAmount][P]+= BombsMap[P];
            VariantsWithXBombs[BombsAmount] += 1;
        }
    };
    //--------------------------------------------------------------------------------------------------------------------------

    QHash<int, QHash<QPoint, long long>> BombsAmounts;
    //BombsAmounts[0];                                // Добавляем элемент ноль (имеет смысл в начале игры)
    QHash<int, long long> VariantsWithXBombs;   // В ситуации, когда много единичных квадратиков, количество вариантов превышает int
    if (!Queues.isEmpty()) {
        GetPossibleBombs(Queues[0], BombsAmounts, VariantsWithXBombs);
    }
    for (int i = 1; i < Queues.size(); i++) {
        QHash<int, QHash<QPoint, long long>> BombsAmountsLocal;
        QHash<int, long long> VariantsWithXBombsLocal;
        // Получаем кол-во бомб в одном квадратике при разных суммарных количествах бомб (так надо для подсчета вероятностей)
        GetPossibleBombs(Queues[i], BombsAmountsLocal, VariantsWithXBombsLocal);

        QHash<int, QHash<QPoint, long long>> NewBombsAmounts;
        typedef QHash<QPoint, long long> QHASH;
        //------------------------------------------------------------------
        auto UniteMaps= [](QHASH &Out, const QHASH &In1, long long Vars1, const QHASH &In2, int Vars2) {
            for (QPoint S: In1.keys() + In2.keys())
                Out[S]+= In1[S]*Vars2+In2[S]*Vars1;
        };
        //------------------------------------------------------------------
        QHash<int, long long> VariantsWithXBombsNew;
        for (int BA: BombsAmounts.keys())
            for (int BAL: BombsAmountsLocal.keys()) {
                qDebug() << "BA =" << BA << "VariantsWithXBombs[BA] =" << VariantsWithXBombs[BA] <<
                            "BAL =" << BAL << "VariantsWithXBombsLocal[BAL] =" << VariantsWithXBombsLocal[BAL];
                if (BA+BAL <= BombsCount) {           // Мы не должны рассматривать варианты, когда кол-во бомб больше, чем есть на поле
                    UniteMaps(NewBombsAmounts[BA+BAL], BombsAmounts[BA], VariantsWithXBombs[BA], BombsAmountsLocal[BAL],
                              VariantsWithXBombsLocal[BAL]);
                    VariantsWithXBombsNew[BA+BAL] += VariantsWithXBombs[BA] * VariantsWithXBombsLocal[BAL];
                }
            }
        VariantsWithXBombs = VariantsWithXBombsNew;

        BombsAmounts= NewBombsAmounts;
    }

    int SafeSquares= 0;
    int KnownBombs= 0;

    if (VariantsWithXBombs.isEmpty())
        VariantsWithXBombs[0]++;                  // Начало игры, нет вариантов

    // Все возможные комбинации
    long double All= 0;
    // Нетронутые поля
    long double Untouched= 0;
    QMap<int, double> UntouchedVariants;
    int PureProbability= Unknowns-Queue.size();
    for (int Bombs: VariantsWithXBombs.keys()) {
        int BombsLeft= BombsCount-Bombs;
        sprintf(Buffer, "Bombs=%d, VariantsWithXBombs[Bombs]=%lld", Bombs, VariantsWithXBombs[Bombs]); qDebug(Buffer);
        All+= (BombsLeft && PureProbability-BombsLeft) ? !(Double)PureProbability * VariantsWithXBombs[Bombs] /
                                                         ( !Double(BombsLeft) * !Double( PureProbability - BombsLeft) )
                                                       : VariantsWithXBombs[Bombs];
        sprintf(Buffer, "BombsCount==%d, PureProbability==%d, BombsLeft==%d, BombsAmounts[Bombs]==%lld", BombsCount,
               PureProbability, BombsLeft, VariantsWithXBombs[Bombs]); qDebug() << Buffer;
        UntouchedVariants[Bombs]= PureProbability-BombsLeft ? (BombsLeft ? !Double(PureProbability) /
                                                                           ( !Double(BombsLeft) * !Double(PureProbability-BombsLeft) )
                                                                         : 0)
                                                            : 1;
        Untouched+= BombsLeft>1 ? !Double(PureProbability-1) * VariantsWithXBombs[Bombs] /
                                  ( !Double(BombsLeft-1) * !Double(PureProbability-1 - (BombsLeft-1)) )
                                : (BombsLeft==1 ? 1
                                                : 0);
    }
    sprintf(Buffer, "All=%Le", All);                qDebug(Buffer);
    sprintf(Buffer, "Untouched=%Le", Untouched);    qDebug(Buffer);

    for (int i= 0; i<Queue.size(); ++i) {
        double Sum= 0;
        for (int BombsAmount: BombsAmounts.keys()) {
            long long VariantsWithBombHere= BombsAmounts[BombsAmount][Queue[i].Loc];

            Sum+= VariantsWithBombHere * (UntouchedVariants[BombsAmount] ? UntouchedVariants[BombsAmount] : 1);
            sprintf(Buffer, "Sum+= %lld * %e[%d] == %e", VariantsWithBombHere, UntouchedVariants[BombsAmount], BombsAmount, Sum);
            qDebug() << Buffer;
        }
        sprintf(Buffer, "Queue[i].Probability= %e / %Le", Sum, All); qDebug(Buffer);
        Queue[i].Probability= Sum / All;
        qDebug() << "Probability of having bomb in square" << Queue[i].Loc+QPoint(1,1) << "is" << Queue[i].Probability;
        if (Queue[i].Probability==0)
            ++SafeSquares;
        else if (Queue[i].Probability==1)
            KnownBombs++;
        ProbField[Queue[i].Loc.y()][Queue[i].Loc.x()]= Queue[i].Probability;
        Field[Queue[i].Loc.y()][Queue[i].Loc.x()]= Queue[i].Probability==1 ? BOMB : PARTIALLY_KNOWN;

        if (Queue[i].Probability == 1)
            emit TryThis(Queue[i].Loc, RIGHT);
    }
    qDebug("There %d safe squares", SafeSquares);
    BombsCount-= KnownBombs;
    qDebug("Bombs found %d, BombsCount(cp2)==%d", KnownBombs, BombsCount);

    // Заполняем поле вероятностей значениями неисследованных квадратиков
    float UnknownProb= Untouched / All;
    qDebug("UnknownProb=%f", UnknownProb);
    for (int y= 0; y<FieldSize.height(); y++)
        for (int x= 0; x<FieldSize.width(); x++) {
            if (Field[y][x]==PARTIALLY_KNOWN)
                continue;
            bool KnownField= false;
            for (Square &S: Queue)
                if (S.Loc.x()==x && S.Loc.y()==y) {
                    KnownField= true;
                    break;
                }
            if (!KnownField)
                ProbField[y][x]= UnknownProb;
        }

    // Формируем массив квадратиков, в которые будем тыкать
    float SmallestProb= 1;
    QList<QPoint> Chosens;
    for (int y= 0; y<FieldSize.height(); y++)
        for (int x= 0; x<FieldSize.width(); x++)
            if ((Field[y][x]==DO_NOT_KNOW || Field[y][x]==PARTIALLY_KNOWN) && ProbField[y][x]<=SmallestProb) {
                qDebug("ProbField[%d][%d]==%f", y+1, x+1, ProbField[y][x]);
                if (ProbField[y][x]<SmallestProb) {
                    Chosens.clear();
                    SmallestProb= ProbField[y][x];
                }
                Chosens << QPoint(x, y);
            }

    // Тыкаем в квадратики -- либо сразу в несколько, если уверены, что там нет бомб, либо в поле с минимальной вероятностью
    if (!SmallestProb) {     // SmallestProb отвечает за случаи, когда UnknownProb где-то равна 0, а SafeSquares работает только для полей в Queue
        for (QPoint& P: Chosens)
            emit TryThis(P, LEFT);
    } else {
        qDebug("Nothing is safe :(");
        qDebug("Unknown fields count = %d, the smallest probability = %f, squares with it = %d",
               Unknowns-Queue.size(), SmallestProb, Chosens.size());
        uint RandomNumber;
        qDebug("Random number from 0 to %d = %d", Chosens.size(), RandomNumber= qrand() * qulonglong(Chosens.size()) / RAND_MAX);
        if (!Beginning)                         // Поле генерится после первого тычка, так что сделать первый шаг всегда безопасно
            emit DangerousSquare(SmallestProb);
        emit TryThis(Chosens[ RandomNumber ], LEFT);
    }
    Unknowns -= KnownBombs;     // Вынужден был перенести сюда, потому что в условии выше используется Unknowns и искажает выводящийся результат в случаях, когда в Queue нашёлся квадратик с вероятностью 1
    // Design flaw: если не подождать здесь, тогда сначала начнёт выполняться OpenField, остановится, выполнится Scan, а только потом продолжит OpenField и значит будет открыто новое поле. Игра остановится, потому что преждевременный Scan не увидел новых полей.
    QThread::msleep(100 * (SafeSquares?Chosens.size():1) + 200 + (SmallestProb && !Beginning ? HESITATION_PAUSE : 0));
    emit NeedNewInfo();
    qDebug() << Q_FUNC_INFO << "exits..";
}

MinesweeperPlayer::RandInitializer::RandInitializer() {
    auto Time= QTime::currentTime();
    qsrand(Time.msec() + Time.second()*1000 + Time.minute()*1000*60 + Time.hour()*1000*60*60);
}

