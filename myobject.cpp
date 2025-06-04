#include "myobject.h"

#include <QApplication>
#include <QtConcurrent>
#include <QDebug>
#include <QTimer>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

#include "qcosignal.hpp"

struct Marker
{
    Marker(QString tag)
        : tag(tag)
    {
        qDebug() << "Created:" << tag;
    }

    ~Marker()
    {
        qDebug() << "Destroyed:" << tag;
    }

    QString tag;
};

QString concurrent_with_result(int seconds)
{
    qDebug() << __PRETTY_FUNCTION__ << "sleeping for" << seconds << "seconds";
    QThread::sleep(seconds);
    qDebug() << __PRETTY_FUNCTION__ << "sleeping done";
    return QString("slept for %1 seconds").arg(seconds);
}

void concurrent_without_result(int seconds)
{
    qDebug() << __PRETTY_FUNCTION__ << "sleeping for" << seconds << "seconds";
    QThread::sleep(seconds);
    qDebug() << __PRETTY_FUNCTION__ << "sleeping done";
}

void MyObject::runTest(Async<>(MyObject::*testFunction)(void))
{
    qDebug() << "===================================================================================";
    QPointer<MyObject> test = new MyObject("test");
    Async<> result = (test->*testFunction)();
    result.m_state->exitLoop = true;
    QCoreApplication::instance()->exec();
    if (test) {
        delete test;
    }
}

MyObject::MyObject(QString name)
{
    setObjectName(name);
}

MyObject::~MyObject()
{
    qDebug() << __PRETTY_FUNCTION__ << objectName();
}

Async<> MyObject::demonstrationWhyIEvenBothered()
{
    qGuiApp->setQuitOnLastWindowClosed(false);
    QMessageBox::ButtonRole role = co_await messageBox("Do the deed?");
    if (role == QMessageBox::AcceptRole) {
        qDebug() << "running concurrent task";
        QString result = co_await QtConcurrent::run(&concurrent_with_result, 3);
        qDebug() << "concurrent task result:" << result;
    } else if (role == QMessageBox::RejectRole) {
        qDebug() << "suit yourself";
    } else {
        qDebug() << "wut" << role;
    }
}

Async<> MyObject::testAwaitSignal1()
{
    Marker m(__PRETTY_FUNCTION__);

    qDebug() << "setting emit timer for signal1";

    MyObject sender("sender");

    QTimer::singleShot(600, [&] {
        qDebug() << "timer done";
        emit sender.signal1(1);
    });

    auto [arg] = co_await CoSignal(&sender, &MyObject::signal1);

    qDebug() << "signal1 received:" << arg;
}

Async<> MyObject::testAwaitSignal2()
{
    Marker m(__PRETTY_FUNCTION__);

    qDebug() << "setting emit timer for signal2";

    MyObject sender("sender");

    QTimer::singleShot(600, [&] {
        qDebug() << "timer done";
        emit sender.signal2(2, "2");
    });

    auto [arg1, arg2] = co_await CoSignal(&sender, &MyObject::signal2);

    qDebug() << "signal2 received:" << arg1 << arg2;
}

Async<> MyObject::testAwaitSignal3()
{
    Marker m(__PRETTY_FUNCTION__);

    qDebug() << "setting emit timer for signal3";

    MyObject sender("sender");

    QTimer::singleShot(600, [&] {
        qDebug() << "timer done";
        emit sender.signal3();
    });

    co_await CoSignal(&sender, &MyObject::signal3);

    qDebug() << "signal3 received";
}

Async<> MyObject::testAwaitFutureWithResult()
{
    Marker m(__PRETTY_FUNCTION__);

    qDebug() << "awaiting concurrent future";

    QString result = co_await QtConcurrent::run(&concurrent_with_result, 1);

    qDebug() << "concurrent future result:" << result;
}

Async<> MyObject::testAwaitFutureWithoutResult()
{
    Marker m(__PRETTY_FUNCTION__);

    qDebug() << "awaiting concurrent future";

    co_await QtConcurrent::run(&concurrent_without_result, 1);

    qDebug() << "concurrent future done";
}

Async<> MyObject::testSpawnCoroViaSignal()
{
    Marker m(__PRETTY_FUNCTION__);

    qDebug() << "setting slot timer";

    QTimer t;
    t.setInterval(100);
    t.setSingleShot(true);
    connect(&t, &QTimer::timeout, this, &MyObject::setPromiseResult);
    t.start();

    qDebug() << "awaiting future from promise";

    int result = co_await m_promise.future();

    qDebug() << "future result:" << result;
}

Async<> MyObject::testAwaitCoro()
{
    Marker m(__PRETTY_FUNCTION__);

    qDebug() << "awaiting sub-coroutine";

    int result = co_await coroSleep(1);

    qDebug() << "sub-coroutine result:" << result;
}

Async<> MyObject::testAwaitSignalOwnerDestroyed()
{
    Marker m(__PRETTY_FUNCTION__);

    qDebug() << "setting-up timers";

    QSharedPointer<MyObject> sender(new MyObject("sender"));
    QTimer::singleShot(100, this, [this] { deleteLater(); });
    QTimer::singleShot(200, sender.get(), [=]{ emit sender->signal1(1); });

    qDebug() << "awaiting signal";

    co_await CoSignal(sender.get(), &MyObject::signal1);

    qCritical() << __PRETTY_FUNCTION__ << "unreachable!";
}

Async<> MyObject::testAwaitSignalSenderDestroyed()
{
    Marker m(__PRETTY_FUNCTION__);

    qDebug() << "setting-up timers";

    MyObject *sender = new MyObject("sender");
    QTimer::singleShot(100, sender, &MyObject::deleteLater);
    QTimer::singleShot(200, sender, [=]{ emit sender->signal1(1); });

    qDebug() << "awaiting signal";

    co_await CoSignal(sender, &MyObject::signal1);

    qCritical() << __PRETTY_FUNCTION__ << "unreachable!";
}

Async<> MyObject::testAwaitFutureOwnerDestroyed()
{
    Marker m(__PRETTY_FUNCTION__);

    qDebug() << "setting-up timer";

    QTimer::singleShot(100, this, [this] { deleteLater(); });

    qDebug() << "awaiting concurrent";

    co_await QtConcurrent::run(&concurrent_without_result, 1);

    qCritical() << __PRETTY_FUNCTION__ << "unreachable!";
}

Async<> MyObject::testAwaitFutureFailed()
{
    Marker m(__PRETTY_FUNCTION__);

    qDebug() << "setting timer";

    QTimer::singleShot(100, this, &MyObject::failPromise);

    qDebug() << "awaiting future from promise";

    int result = co_await m_promise.future();

    qDebug() << "future result:" << result;
}

Async<> MyObject::testAwaitCoroUpstackDestroyed()
{
    Marker m(__PRETTY_FUNCTION__);

    qDebug() << "setting up destroy timer";
    QTimer::singleShot(100, this, &QObject::deleteLater);

    MyObject child("child");

    qDebug() << "awaiting chain top link";
    co_await chain({&child});
}

Async<> MyObject::testAwaitCoroDownstackDestroyed()
{
    Marker m(__PRETTY_FUNCTION__);

    MyObject *child = new MyObject("child");
    QTimer::singleShot(100, child, &QObject::deleteLater);

    qDebug() << "awaiting chain top link";
    co_await chain({child});
}

Async<> MyObject::testAwaitCoroMidstackDestroyed()
{
    Marker m(__PRETTY_FUNCTION__);

    MyObject *child1 = new MyObject("child1");
    MyObject child2("child2");
    qDebug() << "setting up destroy timer";
    QTimer::singleShot(100, child1, &QObject::deleteLater);

    qDebug() << "awaiting chain top link";
    co_await chain({child1, &child2});
}

Async<> MyObject::testAwaitCoroBidirDestroyed()
{
    Marker m(__PRETTY_FUNCTION__);

    qDebug() << "setting up destroy timer";
    QTimer::singleShot(100, this, &QObject::deleteLater);

    MyObject child("child");

    qDebug() << "awaiting chain top link";
    co_await chain({&child});
}

Async<> MyObject::testShootInMyFingFootAndMiss()
{
    Marker m(__PRETTY_FUNCTION__);

    MyObject *that = this;

    QTimer::singleShot(500, [=] { delete that; });

    co_await QtConcurrent::run(&concurrent_without_result, 1);
    qDebug() << "still in" << __PRETTY_FUNCTION__;
}

Async<> MyObject::testShootInMyFingFootAndMiss2()
{
    Marker m(__PRETTY_FUNCTION__);

    deleteLater();

    co_await QtConcurrent::run(&concurrent_without_result, 1);
    qDebug() << "still in" << __PRETTY_FUNCTION__;
}

Async<> MyObject::testShootInMyFingFootAndHit()
{
    Marker m(__PRETTY_FUNCTION__);

    MyObject *that = this;

    co_await QtConcurrent::run(&concurrent_without_result, 1);
    qDebug() << "back in" << __PRETTY_FUNCTION__;
    // emitting some signal that calls some sync function that results in:
    delete that;
    qDebug() << "still in" << __PRETTY_FUNCTION__;
}

Async<> MyObject::setPromiseResult()
{
    Marker m(__PRETTY_FUNCTION__);
    co_await QtConcurrent::run(&concurrent_without_result, 1);
    m_promise.addResult(1);
    m_promise.finish();
    // corouting awaiting on QFuture of m_promise will wake up
    // before current corouting is destroyed, because we are in the same thread
}

void MyObject::failPromise()
{
    Marker m(__PRETTY_FUNCTION__);
    m_promise.setException(QException());
}

Async<QMessageBox::ButtonRole> MyObject::messageBox(QString question)
{
    MessageBox *box = new MessageBox(question);
    box->show();

    // meh
    auto [role] = co_await CoSignal(box, &MessageBox::choice, CoSignalFlags::DeleteSenderOnSignal);
    co_return role;
}

Async<int> MyObject::coroSleep(int seconds)
{
    Marker m(__PRETTY_FUNCTION__);
    co_await QtConcurrent::run(&concurrent_without_result, seconds);
    co_return seconds;
}

Async<> MyObject::chain(QList<MyObject*> objects)
{
    Marker m(QString("%1 %2(%3)").arg(__PRETTY_FUNCTION__).arg(objectName()).arg(objects.size()));

    if (objects.size()) {
        qDebug() << "descending";
        MyObject *child = objects.at(0);
        co_await child->chain(objects.mid(1));
        co_return;
    }

    qDebug() << "awaiting concurrent";
    co_await QtConcurrent::run(&concurrent_without_result, 1);
}

MessageBox::MessageBox(QString text)
{
    setText(text);

    QPushButton *yes = addButton("Yes", QMessageBox::AcceptRole);
    QObject::connect(
        yes,
        &QPushButton::clicked,
        this,
        [this]{ emit this->choice(QMessageBox::AcceptRole); }
    );

    QPushButton *no = addButton("No", QMessageBox::RejectRole);
    QObject::connect(
        no,
        &QPushButton::clicked,
        this,
        [this]{ emit this->choice(QMessageBox::RejectRole); }
    );

    QPushButton *goAway = addButton("I don't know", QMessageBox::DestructiveRole);
    QObject::connect(goAway, &QPushButton::clicked, this, &QObject::deleteLater);
    setEscapeButton(goAway);
}

MessageBox::~MessageBox()
{
    qDebug() << __PRETTY_FUNCTION__;
}
