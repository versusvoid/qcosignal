#include <QApplication>
#include <QTimer>

#include "myobject.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QTimer t;
    t.setInterval(500);
    t.callOnTimeout([]{ qDebug() << "{event loop still alive and kicking}"; });
    t.start();

    MyObject::runTest(&MyObject::testAwaitSignal1);
    MyObject::runTest(&MyObject::testAwaitSignal2);
    MyObject::runTest(&MyObject::testAwaitSignal3);
    MyObject::runTest(&MyObject::testAwaitFutureWithResult);
    MyObject::runTest(&MyObject::testAwaitFutureWithoutResult);
    MyObject::runTest(&MyObject::testSpawnCoroViaSignal);
    MyObject::runTest(&MyObject::testAwaitCoro);
    MyObject::runTest(&MyObject::testAwaitSignalOwnerDestroyed);
    MyObject::runTest(&MyObject::testAwaitSignalSenderDestroyed);
    MyObject::runTest(&MyObject::testAwaitFutureOwnerDestroyed);

    MyObject::runTest(&MyObject::testAwaitCoroUpstackDestroyed);
    MyObject::runTest(&MyObject::testAwaitCoroDownstackDestroyed);
    MyObject::runTest(&MyObject::testAwaitCoroMidstackDestroyed);

    MyObject::runTest(&MyObject::testShootInMyFingFootAndMiss);
    MyObject::runTest(&MyObject::testShootInMyFingFootAndMiss2);

    MyObject::runTest(&MyObject::demonstrationWhyIEvenBothered);

    MyObject::runTest(&MyObject::testShootInMyFingFootAndHit);

    return 0;
}
