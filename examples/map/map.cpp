// gtl.seq.cpp : Defines the entry point for the application.
//

#include <string>
#include <source_location>

#include <fmt/core.h>
#include <fmt/xchar.h>
#include <fmt/chrono.h>

#include "gtl/sequence.h"
#include "gtl/sequence_map.h"

namespace gtl::seq::test {

	using namespace std::literals;
	//using namespace gtl::literals;
	namespace chrono = std::chrono;

	using seq_t = gtl::seq::TSequence<std::string>;
	using seq_map_t = gtl::seq::TSequenceMap<seq_t::result_t>;
	using coro_t = seq_t::coro_t;

	//-----
	class CApp : public seq_map_t {
	public:
		CApp(seq_t& driver) : seq_map_t("top", driver) {
		}
		void Run() {
			seq_t* driver = GetSequenceDriver();
			do {
				gtl::seq::clock_t::time_point t = driver->Dispatch();
				if (driver->IsDone())
					break;
				if (auto ts = t - gtl::seq::clock_t::now(); ts > 3s)
					t = gtl::seq::clock_t::now() + 3s;
				std::this_thread::sleep_until(t);
			} while (true);
		}
	};

	std::chrono::milliseconds ms(auto dur) {
		return std::chrono::duration_cast<std::chrono::milliseconds>(dur);
	}

	//-----
	class C1 : public seq_map_t {
	public:
		using this_t = C1;
		using base_t = seq_map_t;

		C1(unit_id_t const& id, seq_map_t& parent) : seq_map_t(id, parent) {
			Bind("task1", &this_t::Task1);
			Bind("task2", &this_t::Task2);
		}

	protected:

		coro_t Task1(seq_t& seq, param_t param) {
			auto t0 = gtl::seq::clock_t::now();
			auto funcname = "C1::Task1"; //std::source_location::current().function_name();

			fmt::print("{}: Begin\n", funcname);

			// call this->task2
			std::future<seq_t::result_t> future = CreateChildSequence("task2", fmt::format("Greeting from {}", funcname));
			co_await WaitForChild();
			fmt::print("{}: child done: {}\n", funcname, future.get());

			fmt::print("{}: End {}\n", funcname, ms(gtl::seq::clock_t::now() - t0));

			co_return "";
		}
		coro_t Task2(seq_t& seq, param_t param) {
			auto t0 = gtl::seq::clock_t::now();
			auto funcname = "C1::Task2"; //std::source_location::current().function_name();

			fmt::print("{}: Begin param: {}\n", funcname, param);

			// call c2::taskA
			std::future<seq_t::result_t> future = CreateChildSequence("c2", "taskA", fmt::format("Greeting from {}", funcname));
			co_await WaitForChild();
			fmt::print("{}: child done: {}\n", funcname, future.get());


			// step - wait for other thread
			fmt::print("SeqReturningInt : step3 wait...\n");
			auto i = 10;
			std::jthread count_down( [&](auto stop) {
				while (!stop.stop_requested()) {
					fmt::print("in other thread: count down {}\n", i--);
					std::this_thread::sleep_for(100ms);
				}
			});

			co_await seq.Wait([&, t0 = gtl::seq::clock_t::now()] {	// wait until i == 0
				return i == 0;
			}, 1ms);

			count_down.request_stop();
			count_down.join();


			auto str = fmt::format("{}: End : {}", funcname, ms(gtl::seq::clock_t::now() - t0));
			fmt::print("{}\n", str);

			co_return std::move(str);
		}

	};


	//-----
	class C2 : public seq_map_t {
	public:
		using this_t = C2;
		using base_t = seq_map_t;

		C2(unit_id_t const& id, seq_map_t& parent) : seq_map_t(id, parent) {
			Bind("taskA", &this_t::TaskA);
			Bind("taskB", &this_t::TaskB);
		}

	protected:
		coro_t TaskA(seq_t& seq, param_t param) {
			auto t0 = gtl::seq::clock_t::now();
			auto funcname = "C2::TaskA"; //std::source_location::current().function_name();

			fmt::print("{}: Begin param: {}\n", funcname, param);

			// call c2::taskA
			std::future<std::string> future = CreateChildSequence("taskB", fmt::format("Greeting from {}", funcname));
			co_await WaitForChild();
			fmt::print("{}: child done: {}\n", funcname, future.get());

			auto str = fmt::format("{}: End : {}", funcname, ms(gtl::seq::clock_t::now() - t0));
			fmt::print("{}\n", str);

			co_return std::move(str);
		}
		coro_t TaskB(seq_t& seq, param_t param) {
			auto t0 = gtl::seq::clock_t::now();
			auto funcname = "C2::TaskB"; //std::source_location::current().function_name();

			fmt::print("{}: Begin param: {}\n", funcname, param);

			co_await WaitFor(100ms);

			auto str = fmt::format("{}: End : {}", funcname, ms(gtl::seq::clock_t::now() - t0));
			fmt::print("{}\n", str);

			co_return std::move(str);
		}
	};

} // namespace gtl::seq::test


int main() {
	using namespace gtl::seq::test;

	{
		fmt::print("start\n");

		seq_t driver;
		CApp app(driver);


		C1 c1("c1", app);
		C2 c2("c2", app);

		c1.CreateRootSequence("task1");	// c1::task1 -> c1::task2 -> c2::taskA -> c2::taskB

		app.Run();

		fmt::print("done\n");

	}
}
