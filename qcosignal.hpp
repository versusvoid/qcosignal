#pragma once

#include <coroutine>
#include <memory>

#include <QObject>
#include <QFuture>
#include <QTimer>

#ifdef COSIGNAL_DEBUG
#include <QCoreApplication>
#include <QDebug>
#endif

/*
 * minimal support for `co_await`-ing of QFuture<T>
 * doesn't handle cancellation or failure of QFuture
 * in those cases awating coroutine will hang in memory until its' owning QObject is destroyed
 *
 * also actually supports only copiable T - int, QString, etc.
 * should be enough for everyone, right?
 */
template<typename T>
struct FutureAwaiter
{
    FutureAwaiter(QFuture<T> future, QObject *object)
    {
        m_future = future;
        setup_then(object);
        // TODO?
        // setup_failed(object);
    }

    template<typename Dummy = T>
    requires std::is_void_v<T>
    void setup_then(QObject *object)
    {
        m_future.then(object, [this] () { m_handle.resume(); });
    }

    template<typename Dummy = T>
    requires (!std::is_void_v<T>)
    void setup_then(QObject *object)
    {
        m_future.then(object, [this] (T) { m_handle.resume(); });
    }

    bool await_ready() const
    {
        return m_future.isFinished();
    }

    void await_suspend(std::coroutine_handle<> handle)
    {
        m_handle = handle;
    }

    template<typename Dummy = T>
    requires std::is_void_v<T>
    void await_resume()
    {
        return;
    }

    template<typename Dummy = T>
    requires (!std::is_void_v<T>)
    T await_resume()
    {
        return m_future.result();
    }

private:
    QFuture<T> m_future;
    std::coroutine_handle<> m_handle;
};

// =============================================================================

/*
 * some forward declarations
 */

template<typename T = void>
struct CoroutineControllerBase;

template<typename T = void>
struct CoroutineController;

using Handle = std::coroutine_handle<CoroutineController<>>;

// =============================================================================

/*
 * state shared between publicly visible type Async<T>
 * and internal "promise_type" — CoroutineControllerBase<T>
 */
template<typename T>
struct SharedState
{
    SharedState(CoroutineControllerBase<T> *promise)
        : current(reinterpret_cast<CoroutineControllerBase<>*>(promise))
    {}

    CoroutineControllerBase<> *current = nullptr;
    /*
     * caller of the `current`, i.e.
     *
     *   Async<> Class::up()
     *   {
     *     ...
     *     co_await current();
     *     ...
     *   }
     */
    CoroutineControllerBase<> *up = nullptr;
    /*
     * called by the `current`, i.e.
     *
     *   Async<> Class::current()
     *   {
     *     ...
     *     co_await down();
     *     ...
     *   }
     */
    CoroutineControllerBase<> *down = nullptr;

    /*
     * std::optional<void> is forbidden, so when `T = void` using bool as result type
     * can be optimized to `bool result` via some template magic
     */
    std::optional<std::conditional_t<std::is_void_v<T>, bool, T>> result;

#ifdef COSIGNAL_DEBUG
    // for example running purposes
    bool exitLoop = false;
#endif
};

/*
 * publicly visible coroutine type
 * analogous to python's `asyncio.Task`
 *
 * fields are public for simplicity
 */
template<typename T = void>
struct Async
{
    Async(std::shared_ptr<SharedState<T>> state)
        : m_state(state)
    {}

    bool await_ready() const
    {
        return m_state->result.has_value();
    }

    void await_suspend(std::coroutine_handle<> untypedHandle);

    template<typename Dummy = T>
    requires std::is_void_v<T>
    void await_resume()
    {
        return;
    }

    template<typename Dummy = T>
    requires (!std::is_void_v<T>)
    T await_resume()
    {
        return m_state->result.value();
    }

    std::shared_ptr<SharedState<T>> m_state;
};

/*
 * main piece of code, controlling behavior of coroutines
 * (hence the name, because C++'s own "promise_type" is oh so unambiguous)
 *
 * binds itself to QObject's lifetime and follows it into deleted oblivion
 */
template<typename T>
struct CoroutineControllerBase
{
    template<typename... Args>
    CoroutineControllerBase(QObject &object, Args...)
        : m_object(&object)
        , m_state(new SharedState<T>(this))
    {
        // When `object` is being destroyed, also abort and destroy dangling coroutine_handle
        m_connection = QObject::connect(
            &object,
            &QObject::destroyed,
            [this] {
#ifdef COSIGNAL_DEBUG
                qDebug() << "aborting coroutine because owning object was destroyed";
#endif
                abort();
            }
        );
    }

#ifdef COSIGNAL_DEBUG
    virtual ~CoroutineControllerBase()
    {
        // some sanity checks
        Q_ASSERT(!m_state->up);
        Q_ASSERT(!m_state->down);

        // if this is main test coroutine => exiting event loop
        if (m_state->exitLoop) {
            QCoreApplication::instance()->exit();
        }
    }
#endif

    std::coroutine_handle<CoroutineControllerBase> make_handle()
    {
        return std::coroutine_handle<CoroutineControllerBase>::from_promise(*this);
    }

    void abort()
    {
        /*
         * gracefully aborting running coroutine
         */
        QObject::disconnect(m_connection);
        m_state->current = nullptr;

        /*
         * recursive quasi stack-unwinding
         * 1) descend down (from calling to called coroutine) to the lowest level,
         *    breaking links on the way
         * 2) destroy lowest coroutine frame
         * 3) backtrack to starting point, destroying previous frames
         * 4) ascend up, destroying frames on every step
         */

        /*
         * first and foremost — break links to prevent infinite recursion (and double free)
         */
        CoroutineControllerBase<> *down = m_state->down;
        if (down) {
            down->m_state->up = nullptr;
            m_state->down = nullptr;
        }

        CoroutineControllerBase<> *up = m_state->up;
        if (up) {
            up->m_state->down = nullptr;
            m_state->up = nullptr;
        }

        if (down) {
#ifdef COSIGNAL_DEBUG
            qDebug() << "aborting downstack coroutine because current was aborted";
#endif
            down->abort();
        }

        auto handle = make_handle();
        Q_ASSERT(handle);
        handle.destroy();

        // at this point `this` could be dangling pointer, so we should careful not to touch it

        if (up) {
#ifdef COSIGNAL_DEBUG
            qDebug() << "aborting upstack coroutine because current was aborted";
#endif
            up->abort();
        }
    }

    inline Async<T> get_return_object() noexcept { return Async<T>(m_state); }

    inline static std::suspend_never initial_suspend() noexcept { return {}; }
    inline static std::suspend_never final_suspend() noexcept { return {}; }

    void handle_return() noexcept
    {
        QObject::disconnect(m_connection);
        m_state->current = nullptr;

        if (!m_state->up) {
            return;
        }

        /*
         * if there is another coroutine, awaiting on `this` — awake it
         * (via event loop, so that `this` can be gracefully destroyed)
         */
        Q_ASSERT(reinterpret_cast<CoroutineControllerBase*>(m_state->up->m_state->down) == this);
        m_state->up->m_state->down = nullptr;

        auto handle = m_state->up->make_handle();
        QTimer::singleShot(0, [=] { handle.resume(); });

        m_state->up = nullptr;
    }

    inline static void unhandled_exception() noexcept
    {
        Q_ASSERT_X(false, __PRETTY_FUNCTION__, "not supported");
        std::abort();
    }

    template <typename A>
    inline static A&& await_transform(A&& someAsync)
    {
        // CoSignal<> and Async<> cases
        return std::move(someAsync);
    }

    template<typename K>
    FutureAwaiter<K> await_transform(QFuture<K> future)
    {
        return FutureAwaiter<K>(future, m_object);
    }

    QObject *const m_object;
    QMetaObject::Connection m_connection;
    const std::shared_ptr<SharedState<T>> m_state;
};

/*
 * "specialization" of coroutine controller for generic case and `T = void`
 *
 * it's strange and funny how such inheritance scheme is basically required,
 * because compiler forbids  simultaneous presence of both `return_value()` and `return_void()`
 * even when they are hidden under SFINAE
 */
template<typename T>
struct CoroutineController : CoroutineControllerBase<T>
{
#ifdef COSIGNAL_DEBUG
    // required only if there is `virtual ~CoroutineControllerBase()`
    template<typename... Args>
    CoroutineController(QObject &object, Args...)
        : CoroutineControllerBase<T>(object)
    {}
#endif

    inline void return_value(T&& v) noexcept
    {
        this->m_state->result.emplace(std::forward<T>(v));
        this->handle_return();
    }

    inline void return_value(T& v) noexcept
    {
        this->m_state->result.emplace(v);
        this->handle_return();
    }
};

template<>
struct CoroutineController<void> : CoroutineControllerBase<void>
{
#ifdef COSIGNAL_DEBUG
    // required only if there is `virtual ~CoroutineControllerBase()`
    template<typename... Args>
    CoroutineController(QObject &object, Args...)
        : CoroutineControllerBase<void>(object)
    {}
#endif

    inline void return_void() noexcept
    {
        m_state->result.emplace(true);
        handle_return();
    }
};

/*
 * `co_await`-ing on another coroutine (referenced by `untypedHandle`)
 */
template<typename T>
void Async<T>::await_suspend(std::coroutine_handle<> untypedHandle)
{
    /*
     * we assume that `this` is being `co_await`-ed by another Async<X> coroutine
     *
     * mixing different coroutine types in single app is like replacing shotgun
     * aimed at your foot with gatling gun — good luck
     */
    Handle& handle = reinterpret_cast<Handle&>(untypedHandle);
    CoroutineController<> *up = &handle.promise();

    // sanity checks

    // coroutine bound with `this` is still alive
    Q_ASSERT(m_state->current);

    // it isn't being `co_await`-ed by somebody else already
    Q_ASSERT(!m_state->up);

    // both coroutines resides in the same thread
    Q_ASSERT(up->m_object->thread() == m_state->current->m_object->thread());

    // linking couroutines with each other
    m_state->up = up;
    up->m_state->down = m_state->current;
}

/*
 * concept for Q_OBJECT
 * humbly copied from qcoro
 */
template<typename T>
concept QObjectConcept = requires(T *obj) {
    requires std::is_base_of_v<QObject, T>;
    requires std::is_same_v<decltype(T::staticMetaObject), const QMetaObject>;
};

/*
 * indicating to the compiler, that methods of the form
 *
 *   Async<T> C::foo(Args...)
 *
 * should be treated as coroutines and controlled by CoroutineController<T>
 *
 * in theory it may also catch
 *
 *   Async<T> foo(C& c, Args...)
 *
 * Also
 *
 *   Async<T> C::foo(Args...) const
 *
 * not supported, because
 *
 *   co_await QFuture<X>().then(this, [] ...
 *
 * requires `this` to not be const ¯\_(ツ)_/¯
 */
template<typename T, QObjectConcept C, typename... Args>
struct std::coroutine_traits<Async<T>, C&, Args...>
{
    using promise_type = CoroutineController<T>;
};

enum CoSignalFlags
{
    SingleShot = 1,
    DeleteSenderOnSignal = 2,
};

/*
 * support for `co_await`-ing Qt signals
 * aborts coroutine if sender is destroyed when awaiting
 */
template <QObjectConcept T, QObjectConcept F, typename... Args>
requires std::is_base_of_v<F, T>
struct CoSignal
{
    CoSignal(T* sender, void(F::*signal)(Args...), CoSignalFlags flags = CoSignalFlags::SingleShot)
        : m_sender(sender)
        , m_signal(signal)
        , m_flags(flags)
        , m_received(false)
    {}

    ~CoSignal()
    {
        QObject::disconnect(m_connection);
        QObject::disconnect(m_destroyedConnection);
    }

    bool await_ready() const
    {
        return m_received;
    }

    void await_suspend(std::coroutine_handle<> untypedHandle)
    {
        /*
         * also assuming `this` is `co_await`-ed by CoroutineController<X>
         */
        Handle& handle = reinterpret_cast<Handle&>(untypedHandle);

        if (!m_connection) {
            Q_ASSERT(m_sender);

            // `m_received == true` means `await_ready()` should've returned `true`
            // and this method should not have been called
            Q_ASSERT(!m_received);

            m_destroyedConnection = QObject::connect(
                m_sender,
                &QObject::destroyed,
                handle.promise().m_object,
                [this] {
#ifdef COSIGNAL_DEBUG
                    qDebug() << "aborting coroutine awaiting on signal because expected sender was destroyed";
#endif
                    this->m_sender = nullptr;
                    this->m_handle.promise().abort();
                }
            );

            m_connection = QObject::connect(
                m_sender,
                m_signal,
                handle.promise().m_object,
                [this](Args... args) {
                    this->m_result = {args...};
                    this->handle_signal();
                },
                (m_flags & CoSignalFlags::SingleShot) ? Qt::SingleShotConnection : Qt::AutoConnection
            );
        }

        m_handle = handle;
        m_received = false;
    }

    void handle_signal()
    {
        if (m_flags & CoSignalFlags::SingleShot) {
            QObject::disconnect(m_destroyedConnection);
            m_received = true;
        }
        if (m_flags & CoSignalFlags::DeleteSenderOnSignal) {
            QObject::disconnect(m_destroyedConnection);
            delete m_sender;
        }

        m_handle.resume();
    }

    /*
     * can be optimized for `Args = {void}` and `Args = {T}` cases along with `m_result`
     * via some glorious SFINAE magic, but good enough for demonstration
     */
    std::tuple<Args...> await_resume()
    {
        return m_result;
    }

private:
    QPointer<T> m_sender;
    void (F::*m_signal)(Args...);
    CoSignalFlags m_flags;
    bool m_received;

    Handle m_handle;

    QMetaObject::Connection m_connection;
    QMetaObject::Connection m_destroyedConnection;

    std::tuple<Args...> m_result;
};
