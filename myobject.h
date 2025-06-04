#pragma once

#include <QObject>
#include <QMessageBox>

#include "qcosignal.hpp"

class MyObject: public QObject
{
    Q_OBJECT
public:
    static void runTest(Async<>(MyObject::*testFunction)(void));

    MyObject(QString name);
    virtual ~MyObject();

    Async<> demonstrationWhyIEvenBothered();

    Async<> testAwaitSignal1();
    Async<> testAwaitSignal2();
    Async<> testAwaitSignal3();
    Async<> testAwaitFutureWithResult();
    Async<> testAwaitFutureWithoutResult();

    Async<> testSpawnCoroViaSignal();

    Async<> testAwaitCoro();

    Async<> testAwaitSignalOwnerDestroyed();
    Async<> testAwaitSignalSenderDestroyed();

    Async<> testAwaitFutureOwnerDestroyed();
    Async<> testAwaitFutureFailed();

    Async<> testAwaitCoroUpstackDestroyed();
    Async<> testAwaitCoroDownstackDestroyed();
    Async<> testAwaitCoroMidstackDestroyed();
    Async<> testAwaitCoroBidirDestroyed();

    Async<> testShootInMyFingFootAndMiss();
    Async<> testShootInMyFingFootAndMiss2();
    Async<> testShootInMyFingFootAndHit();
signals:
    void signal1(int arg);
    void signal2(int arg, QString arg2);
    void signal3();
    void button(QMessageBox::ButtonRole role);

private slots:
    Async<> setPromiseResult();
    void failPromise();

private:
    Async<QMessageBox::ButtonRole> messageBox(QString question);
    Async<int> coroSleep(int seconds);
    Async<> chain(QList<MyObject*> objects);

    QPromise<int> m_promise;
};

class MessageBox : public QMessageBox
{
    Q_OBJECT
public:
    MessageBox(QString text);
    virtual ~MessageBox();

signals:
    void choice(QMessageBox::ButtonRole);
};
