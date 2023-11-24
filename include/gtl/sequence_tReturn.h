#pragma once

//////////////////////////////////////////////////////////////////////
//
// sequence_any.h: xSequenceAny - coroutines having templatized return type
//
// PWH
// 2023-11-13
//
//////////////////////////////////////////////////////////////////////

#include <coroutine>
#include <future>
#include <list>
#include <functional>
#include <optional>
#include <chrono>
#include <mutex>
#include <thread>
#include <exception>
#include <memory>
#include <type_traits>
#include <utility>

#include "sequence_coroutine_handle.h"

namespace gtl::seq::inline v01 {

	//-------------------------------------------------------------------------
	/// @brief sequence dispatcher
	class xSequenceTReturn {
	public:
		using this_t = xSequenceTReturn;
		template < typename tResult >
		using tcoro_t = TCoroutineHandle<tResult>;

	protected:
		this_t* m_parent{};
		std::unique_ptr<ICoroutineHandle> m_handle;
		inline thread_local static this_t* s_seqCurrent{};
		std::thread::id m_threadID{std::this_thread::get_id()};	// NOT const. may be created from other thread (injection)
		seq_id_t m_name;
		//clock_t::time_point m_timeout{clock_t::time_point::max()};
		sState m_state;

		std::list<this_t> m_children;
	public:
		mutable std::mutex m_mtxChildren;

	public:
		// constructor
		explicit xSequenceTReturn(seq_id_t name = "") : m_name(std::move(name)) {}
		xSequenceTReturn(xSequenceTReturn const&) = delete;
		xSequenceTReturn& operator = (xSequenceTReturn const&) = delete;
		xSequenceTReturn(xSequenceTReturn&& b) {
			m_name.swap(b.m_name);
			m_handle = std::exchange(b.m_handle, nullptr);
			//m_timeout = std::exchange(b.m_timeout, {});
			m_state = std::exchange(b.m_state, {});
			m_children.swap(b.m_children);
		}
		xSequenceTReturn& operator = (xSequenceTReturn&& b) {
			Destroy();
			m_name.swap(b.m_name);
			m_handle = std::exchange(b.m_handle, nullptr);
			//m_timeout = std::exchange(b.m_timeout, {});
			m_state = std::exchange(b.m_state, {});
			m_children.swap(b.m_children);
			return *this;
		}
		void SetName(seq_id_t name) {
			this->m_name = std::move(name);
		}
		auto const& GetName() const {
			return m_name;
		}

		// destructor
		~xSequenceTReturn() {
			Destroy();
		}
		inline void Destroy() {
			m_name.clear();
			if (auto h = std::exchange(m_handle, nullptr); h and h->Valid()) {
				h->Destroy();
			}
		}

		/// @brief 
		/// @return 
		inline bool IsDone() const {
			return m_children.empty() and (!m_handle or m_handle->Done());
		}

		/// @brief 
		/// @return current running sequence
		static this_t* GetCurrentSequence() { return s_seqCurrent; }

		/// @brief 
		/// @return working thread id
		auto GetWorkingThreadID() const { return m_threadID; }

		/// @brief 
		/// @return Get Next Dispatch Time
		/// this function seems to be very heavy. need to optimize.
		template <bool bRefreshChild = false>
		clock_t::time_point GetNextDispatchTime() const {
			auto t = clock_t::time_point::max();
			if constexpr (bRefreshChild) {
				for (auto const& child : m_children) {
					t = std::min(t, child.GetNextDispatchTime<bRefreshChild>());
				}
			}
			else {
				if (m_children.size())
					t = std::min(t, m_state.tNextDispatchChild);
			}
			if (m_children.empty() and m_handle and m_handle->Valid() and !m_handle->Done())
				t = std::min(t, m_state.tNextDispatch);
			return t;
		}

		/// @brief update child's next dispatch time
		/// @return shortest next dispatch time
		clock_t::time_point UpdateNextDispatchTime() {
			auto t = clock_t::time_point::max();
			for (auto& child : m_children) {
				t = std::min(t, child.UpdateNextDispatchTime());
			}
			m_state.tNextDispatchChild = t;
			if (m_children.empty() and m_handle and m_handle->Valid() and !m_handle->Done())
				t = std::min(t, m_state.tNextDispatch);
			return t;
		}

		/// @brief propagate next dispatch time to parent
		void PropagateNextDispatchTime() {
			clock_t::time_point tWhen = GetNextDispatchTime<false>();

			std::optional<std::scoped_lock<std::mutex>> lock;
			if (std::this_thread::get_id() != m_threadID)
				lock.emplace(m_mtxChildren);

			// refresh parent's next dispatch time
			for (auto* parent = m_parent; parent; parent = parent->m_parent) {
				if constexpr (true) {
					// compare parent's next dispatch time is shorter, time and determine earlier break
					if (parent->m_state.tNextDispatchChild <= tWhen)
						break;
					parent->m_state.tNextDispatchChild = std::min(parent->m_state.tNextDispatchChild, tWhen);
				}
				else {
					// no comapre, just update
					parent->m_state.tNextDispatchChild = std::min(parent->m_state.tNextDispatchChild, tWhen);
				}
			}
		}

		/// @brief reserves next dispatch time. NOT dispatch, NOT reserve dispatch itself.
		bool ReserveResume(clock_t::time_point tWhen = {}) {
			if (!m_handle or !m_handle->Valid() or m_handle->Done())
				return false;
			m_state.tNextDispatch = tWhen;

			PropagateNextDispatchTime();
			return true;
		}
		bool ReserveResume(clock_t::duration dur) { return ReserveResume(dur.count() ? clock_t::now() + dur : clock_t::time_point{}); }

		/// @brief 
		/// @return direct child sequence count
		auto CountChild() const { return m_children.size(); }

		/// @brief 
		/// @param name Task Name
		/// @param func coroutine function
		/// @param ...args for coroutine function. must be moved or copied.
		/// @return 
		template < typename tResult, typename ... tArgs >
		std::future<tResult> CreateChildSequence(seq_id_t name, std::function<tcoro_t<tResult>(this_t&, tArgs&& ...)> func, tArgs&& ... args) {
			if constexpr (false) {	// todo: do I need this?
				if (std::this_thread::get_id() != m_threadID) {
					throw xException("CreateChildSequence() must be called from the same thread as the driver");
				}
			}

			// lock if called from other thread
			std::optional<std::scoped_lock<std::mutex>> lock;
			if (std::this_thread::get_id() != m_threadID)
				lock.emplace(m_mtxChildren);

			// create child sequence
			m_children.emplace_back(std::move(name));
			auto& seq = m_children.back();
			// coroutine. coroutine parameters are to be moved (or copied)
			auto handle = std::make_unique<tcoro_t<tResult>>(func(seq, std::forward(args)...));
			auto future = handle->promise().m_result.get_future();
			seq.m_handle = std::move(handle);
			seq.m_parent = this;
			seq.m_threadID = m_threadID;
			return std::move(future);
		}
		template < typename tResult, typename ... tArgs >
		auto CreateChildSequence(seq_id_t name, TCoroutineHandle<tResult>(*func)(this_t&, tArgs&& ...), tArgs&& ... args) {
			std::function<tcoro_t<tResult>(this_t&, tArgs&& ...)> f = func;
			return CreateChildSequence(std::move(name), std::move(f), std::forward<tArgs>(args)...);
		}

		/// @brief Find Child Sequence (Direct Child Only)
		/// @param name 
		/// @return child sequence. if not found, empty child sequence.
	#if __cpp_explicit_this_parameter
		auto FindDirectChild(this auto&& self, seq_id_t const& name) -> decltype(&self) {
			std::optional<std::scoped_lock<std::mutex>> lock;
			if (std::this_thread::get_id() != self.m_threadID)
				lock.emplace(self.m_mtxChildren);

			for (auto& child : self.m_children) {
				if (child.m_name == name)
					return &child;
			}
			return nullptr;
		}
	#else
		this_t const* FindDirectChild(seq_id_t const& name) const {
			std::optional<std::scoped_lock<std::mutex>> lock;
			if (std::this_thread::get_id() != m_threadID)
				lock.emplace(m_mtxChildren);

			for (auto& child : m_children) {
				if (child.m_name == name)
					return &child;
			}
			return nullptr;
		}
		inline this_t* FindDirectChild(seq_id_t const& name) {
			return const_cast<this_t*> ( (const_cast<this_t const*>(this))->FindDirectChild(name) );
		}
	#endif

		/// @brief Find Child Sequence (Depth First Search)
		/// @param name 
		/// @return child sequence. if not found, empty child sequence.
	#if __cpp_explicit_this_parameter
		auto FindChildDFS(this auto&& self, seq_id_t const& name) -> decltype(&self) {
			// todo: if called from other thread... how? use recursive mutex ?? too expansive
			if (std::this_thread::get_id() != self.m_threadID)
				return nullptr;

			for (auto& child : self.m_children) {
				if (child.m_name == name)
					return &child;
			}
			for (auto& child : self.m_children) {
				if (auto* c = child.FindChildDFS(name))
					return c;
			}
			return nullptr;
		}
	#else
		this_t const* FindChildDFS(seq_id_t const& name) const {
			// todo: if called from other thread... how? use recursive mutex ?? too expansive
			if (std::this_thread::get_id() != m_threadID)
				return nullptr;

			for (auto& child : m_children) {
				if (child.m_name == name)
					return &child;
			}
			for (auto& child : m_children) {
				if (auto* c = child.FindChildDFS(name))
					return c;
			}
			return nullptr;
		}
		inline this_t* FindChildDFS(seq_id_t const& name) {
			return const_cast<this_t*>( (const_cast<this_t const*>(this))->FindChildDFS(name) );
		}
	#endif

		/// @brief main dispatch function
		/// @return next dispatch time
		clock_t::time_point Dispatch() {
			if (std::this_thread::get_id() != m_threadID) [[ unlikely ]] {
				throw xException("Dispatch() must be called from the same thread as the driver");
				return {};
			}
			clock_t::time_point tNextDispatch{clock_t::time_point::max()};
			if (Dispatch(tNextDispatch))
				return tNextDispatch;
			return clock_t::time_point::max();
		}

		// co_await
		auto Wait(std::function<bool()> pred, clock_t::duration interval, clock_t::duration timeout = clock_t::duration::max()) {
			m_state.pred.t0 = clock_t::now();
			m_state.pred.func = std::move(pred);
			m_state.pred.interval = interval;
			m_state.pred.timeout = timeout;
			m_state.pred.result = {};
			ReserveResume(interval);

			struct sWaitForCondition : public std::suspend_always {
				mutable std::future<bool> future;
				bool await_resume() const noexcept { return future.get(); }
			};
			return sWaitForCondition{.future = m_state.pred.result.get_future()};
		}

		// co_await
		auto WaitFor(clock_t::duration d) {
			ReserveResume(d);
			return std::suspend_always{};
		}
		// co_await
		auto WaitUntil(clock_t::time_point t) {
			ReserveResume(t);
			return std::suspend_always{};
		}
		// co_await
		auto WaitForChild() {
			ReserveResume(clock_t::duration{});
			return suspend_or_not{ .bAwaitReady = m_children.empty()};
		}

	protected:
		/// @brief Dispatch.
		/// @return true if need next dispatch
		bool Dispatch(clock_t::time_point& tNextDispatchOut) {
			auto const t0 = clock_t::now();

			if (s_seqCurrent) [[ unlikely ]] {
				throw xException("Dispatch() must NOT be called from Dispatch. !!! No ReEntrance");
				return false;
			}

			// Dispatch Child Sequences
			for (bool bContinue{true}; bContinue;) {
				bContinue = false;
				do {
					//auto const t0 = clock_t::now();
					auto& tNextDispatchChild = m_state.tNextDispatchChild;
					tNextDispatchChild = clock_t::time_point::max();	// suspend (do preset for there is no child sequence)
					std::scoped_lock lock{m_mtxChildren};
					for (auto iter = m_children.begin(); iter != m_children.end();) {
						auto& child = *iter;

						// Check Time
						if (auto t = child.GetNextDispatchTime(); t > t0) {	// not yet
							tNextDispatchChild = std::min(tNextDispatchChild, t);
							iter++;
							continue;
						}

						// Dispatch Child
						if (child.Dispatch(tNextDispatchChild)) {
							iter++;
						}
						else {
							// no more child or child done
							iter = m_children.erase(iter);
						}
					}
					if (m_state.tNextDispatchChild > t0)
						break;
				} while (m_children.size());

				// if no more child sequence, Dispatch Self
				if (m_children.empty() and m_handle and m_handle->Valid() and !m_handle->Done()) {
					m_state.tNextDispatch = clock_t::time_point::max();
					//m_handle.promise().m_result.reset();

					// Dispatch
					s_seqCurrent = this;
					if (m_state.pred.func) {
						auto& pred = m_state.pred;
						if (t0 - pred.t0 > pred.timeout) {
							pred.func = nullptr;
							pred.result.set_value(false);
							m_handle->Resume();
						}
						else if (pred.func()) {
							pred.func = nullptr;
							pred.result.set_value(true);
							m_handle->Resume();
						}
						else {
							ReserveResume(t0+m_state.pred.interval);
						}
					}
					else {
						m_handle->Resume();
					}
					s_seqCurrent = nullptr;

					//if (auto& promise = m_handle.promise(); promise.m_result) {
					//	m_result.set_value(std::move(*promise.m_result));
					//}
					bContinue = !m_children.empty();	// if new child sequence added, continue to dispatch child
					if (auto e = m_handle->Exception()) {
						std::rethrow_exception(e);
					}
				}
			}
			tNextDispatchOut = std::min(tNextDispatchOut, GetNextDispatchTime());
			return !IsDone();
		}

	};	// xSequenceTReturn


};
