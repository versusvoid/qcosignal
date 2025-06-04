# Qt bound coroutines

Basic coroutines implementation in [single header](qcosignal.hpp) for exactly two use-cases:

1. awaiting for some signal:
```cpp
    ...
    co_await CoSignal(sender, &Sender::signal);
    ...
```
2. awaiting for concurrent result:
```cpp
    ...
    QString result = co_await QtConcurrent::run(&concurrent_function_with_result, arg);
    ...
```

Coroutines are defined as methods of `QObject`-derived class:
```cpp
Async<> MyObject::testAwaitFutureWithResult()
{
    qDebug() << "awaiting concurrent future";
    QString result = co_await QtConcurrent::run(&concurrent_with_result, 1);
    qDebug() << "concurrent future result:" << result;
}
```
and are bound to `QObject* this` (i.e. destroyed when `this` is destroyed).

Coroutine awaiting on signal will also be destroyed if expected sender is destroyed,
so there won't be indefinite hangs.

Implementation is missing some opportunities for move-semantics optimization, but I'm lacking
enough instinctive understanding of it in C++.

Probably not "serious production"-ready, but fully intended to be used in my pet project and
extended as necessary.
