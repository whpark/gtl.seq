// gtl.seq.cpp : Defines the entry point for the application.
//

#include <algorithm>
#include <coroutine>
#include <future>

//-------------------------------------------------------------------------
template < typename tCoroutineHandle >
struct TPromise {
	using result_t = int;
	std::promise<result_t> m_result;
	std::exception_ptr m_exception;

	tCoroutineHandle get_return_object() {
		return tCoroutineHandle::from_promise(*this);
	}
	std::suspend_always initial_suspend() { return {}; }
	std::suspend_always final_suspend() noexcept { return {}; }
	void unhandled_exception() { m_exception = std::current_exception(); }

	void return_value(result_t&& v) {
		m_result.set_value(std::move(v));
	}
};

//-------------------------------------------------------------------------
class xCoroutineHandle : public std::coroutine_handle<TPromise<xCoroutineHandle>> {
public:
	using base_t = std::coroutine_handle<TPromise<xCoroutineHandle>>;
	using promise_type = TPromise<xCoroutineHandle>;
public:
	xCoroutineHandle(std::coroutine_handle<promise_type>&& h) : base_t(std::exchange(h, nullptr)) {}
};

//-------------------------------------------------------------------------
struct sState {
public:
	struct sPredicate {
		std::function<bool()> func;
		std::promise<bool> result;
	};
	sPredicate pred;

public:
	sState() {}
	sState(std::suspend_always) {}
	sState(std::suspend_never) {}
};

//-------------------------------------------------------------------------
/// @brief sequence dispatcher
class xSequenceTR {
protected:
	sState m_state;
public:
	auto Wait() {
		m_state.pred.result = {};
		struct sWaitForCondition : public std::suspend_always {
			mutable std::future<bool> future;
			bool await_resume() const noexcept { return future.get(); }
		};
		return sWaitForCondition{.future = m_state.pred.result.get_future()};
	}

};

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

xCoroutineHandle Seq1(xSequenceTR& seq) {
	auto bOK = co_await seq.Wait();
	auto bOK2 = co_await seq.Wait();
	//bOK = co_await seq.Wait();
	co_return 0;
}

int main() {
}

