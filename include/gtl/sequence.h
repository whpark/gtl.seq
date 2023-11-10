#pragma once

//////////////////////////////////////////////////////////////////////
//
// sequence.h: Sequence Loop (using co-routine) (Mocha::MIP like ...)
//
// PWH
// 2023-10-27
//
//////////////////////////////////////////////////////////////////////

#include <coroutine>
#include <future>
#include <vector>
#include <concepts>
#include <list>
#include <functional>
#include <optional>
#include <format>
#include <chrono>
#include <mutex>
#include <thread>
#include <set>
#include <map>

namespace gtl::seq::inline v01 {

	using seq_id_t = std::string;
	using clock_t = std::chrono::high_resolution_clock;
	using ms_t = std::chrono::milliseconds;

	//-------------------------------------------------------------------------
	/// @brief used for scheduling
	struct sState {
		clock_t::time_point tNextDispatch{};
		mutable clock_t::time_point tNextDispatchChild{clock_t::time_point::max()};	// cache
		//bool bDone{false};

		sState(clock_t::time_point t = clock_t::now()) : tNextDispatch(t) {}
		sState(clock_t::duration d) {
			tNextDispatch = (d.count() == 0) ? clock_t::time_point{} : clock_t::now() + d;
		}
		sState(std::suspend_always ) : tNextDispatch(clock_t::time_point::max()) {}
		sState(std::suspend_never ) : tNextDispatch{} {}
		sState(sState const&) = default;
		sState(sState&&) = default;
		sState& operator = (sState const&) = default;
		sState& operator = (sState&&) = default;

		void Clear() { *this = {}; }
	};

	//-------------------------------------------------------------------------
	/// @brief sequence dispatcher
	template < typename tResult = bool >
	struct TSequence {
	public:
		using this_t = TSequence;
		using result_t = tResult;
		struct promise_type;
		using coroutine_handle_t = std::coroutine_handle<promise_type>;
		//-------------------------------------------------------------------------
		/// @brief 
		struct promise_type {
			std::promise<result_t> m_result;
			std::exception_ptr m_exception;

			coroutine_handle_t get_return_object() {
				return coroutine_handle_t::from_promise(*this);
			}
			std::suspend_always initial_suspend() { return {}; }
			std::suspend_always final_suspend() noexcept { return {}; }
			void unhandled_exception() { m_exception = std::current_exception(); }

			std::suspend_always yield_value(result_t&& v) {
				m_result.set_value(std::move(v));
				return {};
			}
			//void return_void() {}
			void return_value(result_t&& v) {
				m_result.set_value(std::move(v));
			}
		};

		struct suspend_or_not {
			bool bAwaitReady{};
			bool await_ready() const noexcept { return bAwaitReady; }
			constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
			constexpr void await_resume() const noexcept {}
		};

	protected:
		this_t* m_parent{};
		coroutine_handle_t m_handle;
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
		explicit TSequence(seq_id_t name = "") : m_name(std::move(name)) {}
		TSequence(coroutine_handle_t&& h) : m_handle(std::exchange(h, nullptr)) { }
		TSequence(TSequence const&) = delete;
		TSequence& operator = (TSequence const&) = delete;
		TSequence(TSequence&& b) {
			m_name.swap(b.m_name);
			m_handle = std::exchange(b.m_handle, nullptr);
			//m_timeout = std::exchange(b.m_timeout, {});
			m_state = std::exchange(b.m_state, {});
			m_children.swap(b.m_children);
		}
		TSequence& operator = (TSequence&& b) {
			Destroy();
			m_name.swap(b.m_name);
			m_handle = std::exchange(b.m_handle, nullptr);
			//m_timeout = std::exchange(b.m_timeout, {});
			m_state = std::exchange(b.m_state, {});
			m_children.swap(b.m_children);
			return *this;
		}
		TSequence& operator = (coroutine_handle_t&& h) {
			if (m_handle != nullptr) {
				throw std::exception("TSequence::operator = (coroutine_handle_t&&) : already has handle");
			}
			m_handle = std::exchange(h, nullptr);
			return *this;
		}
		void SetName(seq_id_t name) {
			this->m_name = std::move(name);
		}
		auto const& GetName() const {
			return m_name;
		}

		// destructor
		~TSequence() {
			Destroy();
		}
		inline void Destroy() {
			m_name.clear();
			if (auto h = std::exchange(m_handle, nullptr); h) {
				h.destroy();
			}
		}

		/// @brief 
		/// @return 
		inline bool IsDone() const {
			return m_children.empty() and (!m_handle or m_handle.done());
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
			if (m_children.empty() and m_handle and !m_handle.done())
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
			if (m_children.empty() and m_handle and !m_handle.done())
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
			if (!m_handle or m_handle.done())
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
		template < typename ... targs >
		std::future<result_t> CreateChildSequence(seq_id_t name, std::function<this_t(targs ...)> func, targs... args) {
			if constexpr (false) {	// todo: do I need this?
				if (std::this_thread::get_id() != m_threadID) {
					throw std::exception("CreateChildSequence() must be called from the same thread as the driver");
				}
			}

			// lock if called from other thread
			std::optional<std::scoped_lock<std::mutex>> lock;
			if (std::this_thread::get_id() != m_threadID)
				lock.emplace(m_mtxChildren);

			// create child sequence
			m_children.emplace_back(func(std::move(args)...));	// coroutine parameters are to be moved (or copied)
			auto& self = m_children.back();
			self.m_parent = this;
			self.m_threadID = m_threadID;
			self.m_name = std::move(name);
			return self.m_handle.promise().m_result.get_future();
		}
		inline auto CreateChildSequence(seq_id_t name, std::function<this_t()> func) {
			return CreateChildSequence<>(std::move(name), func);
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
				throw std::exception("Dispatch() must be called from the same thread as the driver");
				return {};
			}
			clock_t::time_point tNextDispatch{clock_t::time_point::max()};
			if (Dispatch(tNextDispatch))
				return tNextDispatch;
			return clock_t::time_point::max();
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

			if (s_seqCurrent) [[ unlikely ]] {
				throw std::exception("Dispatch() must NOT be called from Dispatch. !!! No ReEntrance");
				return false;
			}

			// Dispatch Child Sequences
			for (bool bContinue{true}; bContinue;) {
				bContinue = false;
				do {
					auto const t0 = clock_t::now();
					auto& tNextDispatchChild = m_state.tNextDispatchChild;
					tNextDispatchChild = clock_t::time_point::max();	// suspend (do preset for there is no child sequence)
					std::scoped_lock lock{m_mtxChildren};
					for (auto iter = m_children.begin(); iter != m_children.end();) {
						//tNextDispatchChild = clock_t::time_point::max();	// suspend
						auto& child = *iter;

						// Check Time
						if (auto t = child.GetNextDispatchTime(); t > t0) {	// not yet
							tNextDispatchChild = std::min(tNextDispatchChild, t);
							iter++;
							continue;
						}

						// Dispatch Child
						if (child.Dispatch(tNextDispatchChild)) {	// no more child or child done
							iter++;
						}
						else {
							iter = m_children.erase(iter);
							continue;
						}
					}
					if (m_state.tNextDispatchChild > t0)
						break;
				} while (m_children.size());

				// if no more child sequence, Dispatch Self
				if (m_children.empty() and m_handle and !m_handle.done()) {
					m_state.tNextDispatch = clock_t::time_point::max();
					//m_handle.promise().m_result.reset();

					// Dispatch
					s_seqCurrent = this;
					m_handle.resume();
					s_seqCurrent = nullptr;

					//if (auto& promise = m_handle.promise(); promise.m_result) {
					//	m_result.set_value(std::move(*promise.m_result));
					//}
					bContinue = !m_children.empty();	// if new child sequence added, continue to dispatch child
					if (m_handle.promise().m_exception) {
						std::rethrow_exception(m_handle.promise().m_exception);
					}
				}
			}
			tNextDispatchOut = std::min(tNextDispatchOut, GetNextDispatchTime());
			return !IsDone();
		}

	};	// TSequence


};
