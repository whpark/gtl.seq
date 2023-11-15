// gtl.seq.cpp : Defines the entry point for the application.
//

#include <string>
#include <source_location>

#include <fmt/core.h>
#include <fmt/xchar.h>
#include <fmt/chrono.h>

//#include <ctre.hpp>

#include "gtl/sequence.h"
#include "gtl/sequence_any.h"
#include "gtl/sequence_map.h"

using seq_t = gtl::seq::TSequence<std::string>;
using coro_t = seq_t::coro_t;
using seq_map_t = gtl::seq::TSequenceMap<seq_t::result_t>;

using namespace std::literals;
namespace chrono = std::chrono;
//using namespace gtl::literals;

//std::future<int> asyncAdd(int a, int b) {
//	return std::async(std::launch::async, [a, b]() {
//		return a + b;
//	});
//}
//
//std::future<void> asyncPrint(int value) {
//	co_await std::chrono::seconds(1);
//	//std::cout << "Async value: " << value << std::endl;
//}
//

coro_t Sequence1(seq_t& seq) {

	namespace chrono = std::chrono;
	auto t0 = chrono::steady_clock::now();

	// do print something
	fmt::print("step1\n");

	fmt::print("waiting 1 sec, and must be timeout.\n");
	bool bOK = co_await seq.Wait([t0 = gtl::seq::clock_t::now()] {
		auto t = gtl::seq::clock_t::now();
		fmt::print("wating ... {}\n", std::chrono::duration_cast<std::chrono::milliseconds>(t-t0));
		return t-t0 > 3s;
	}, 100ms, 1s);
	fmt::print("waiting result : {}\n", bOK ? "OK" : "Timeout");

	fmt::print("waiting 1 sec, and may be or may not be timeout.\n");
	bOK = co_await seq.Wait([t0 = gtl::seq::clock_t::now()] {
		auto t = gtl::seq::clock_t::now();
		fmt::print("wating ... {}\n", std::chrono::duration_cast<std::chrono::milliseconds>(t-t0));
		return t-t0 > 1s;
	}, 100ms, 2s);
	fmt::print("waiting result : {}\n", bOK ? "OK" : "Timeout");


	// Wait For 1s
	co_await seq.WaitFor(40ms);

	// do print something
	auto t1 = chrono::steady_clock::now();
	fmt::print("step2 : {:>8}\n", chrono::duration_cast<chrono::milliseconds>(t1 - t0));

	co_await seq.WaitUntil(gtl::seq::clock_t::now() + 1ms);

	auto t2 = chrono::steady_clock::now();
	fmt::print("step3 : {:>8}\n", chrono::duration_cast<chrono::milliseconds>(t2 - t1));

	co_return fmt::format("{} ended. take {}", seq.GetName(), chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - t0));
}

coro_t TopSeq(seq_t&);
coro_t Child1(seq_t&);
coro_t Child1_1(seq_t&);
coro_t Child1_2(seq_t&);
coro_t Child2(seq_t&);


coro_t TopSeq(seq_t& seq) {

	auto sl = std::source_location::current();
	auto funcname = seq.GetName();// sl.function_name();

	// step 1
	fmt::print("{}: Begin\n", funcname);
	fmt::print("{}: Creating Child1\n", funcname);
	auto t0 = gtl::seq::clock_t::now();
	auto f = seq.CreateChildSequence("Child1", &Child1);

	co_await seq.WaitForChild();

	// step 2
	auto t1 = gtl::seq::clock_t::now();
	fmt::print("{}: Child 1 Done, {}\n", funcname, chrono::duration_cast<chrono::milliseconds>(t1-t0));

	auto t2 = gtl::seq::clock_t::now();
	fmt::print("{}: WaitFor 100ms, {}\n", funcname, chrono::duration_cast<chrono::milliseconds>(t2 - t1));
	co_await seq.WaitFor(100ms);

	// step 3
	fmt::print("{}: End\n", funcname);

	co_return "";
}

coro_t Child1(seq_t& seq) {
	auto sl = std::source_location::current();
	auto funcname = seq.GetName();// sl.function_name();

	// step 1
	fmt::print("{}: Begin\n", funcname);
	fmt::print("{}: Creating Child1_1, Child1_2\n", funcname);
	auto t0 = gtl::seq::clock_t::now();
	seq.CreateChildSequence("Child1_1", &Child1_1);
	seq.CreateChildSequence("Child1_2", &Child1_2);

	co_await seq.WaitForChild();

	auto t1 = gtl::seq::clock_t::now();
	fmt::print("{}: Child1_1, Child1_2 Done. {}\n", funcname, chrono::duration_cast<chrono::milliseconds>(t1 - t0));

	// step 3
	fmt::print("{}: End\n", funcname);
	
	co_return "";
}

coro_t Child1_1(seq_t& seq) {

	auto sl = std::source_location::current();
	auto funcname = seq.GetName();// sl.function_name();

	auto t0 = gtl::seq::clock_t::now();

	// step 1
	fmt::print("{}: Begin\n", funcname);
	for (int i = 0; i < 5; i++) {
		auto t1 = gtl::seq::clock_t::now();
		fmt::print("{}: doing some job... and wait for 200ms : {}\n", funcname, chrono::duration_cast<chrono::milliseconds>(t1-t0));
		co_await seq.WaitFor(200ms);
	}
	fmt::print("{}: End. Creating Child1_1, Child1_2\n", funcname);
	
	co_return "";
}

coro_t Child1_2(seq_t& seq) {
	auto sl = std::source_location::current();
	auto funcname = seq.GetName();// sl.function_name();

	auto t0 = gtl::seq::clock_t::now();

	// step 1
	fmt::print("{}: Begin\n", funcname);
	for (int i = 0; i < 5; i++) {
		auto t1 = gtl::seq::clock_t::now();
		fmt::print("{}: doing some job... and wait for 200ms : {}\n", funcname, chrono::duration_cast<chrono::milliseconds>(t1 - t0));
		co_await seq.WaitFor(200ms);
	}
	fmt::print("{}: End. Creating Child1_1, Child1_2\n", funcname);
	
	co_return "";
}

coro_t Child2(seq_t& seq) {
	co_return "";
}

gtl::seq::v01::TCoroutineHandle<std::string> SeqReturningString(gtl::seq::v01::xSequenceAny& seq) {
	auto t0 = chrono::steady_clock::now();
	auto str = fmt::format("{} ended. take {}", seq.GetName(), chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - t0));

	co_return std::move(str);
}

gtl::seq::v01::TCoroutineHandle<int> SeqReturningInt(gtl::seq::v01::xSequenceAny& seq) {

	co_return 3141592;
}


int main() {

	if constexpr (true) try {
		seq_t driver;

		fmt::print("Begin\n");

		// start simple sequence
		auto future = driver.CreateChildSequence("SimpleSequence", &Sequence1);
		do {
			auto t = driver.Dispatch();
			if (driver.IsDone())
				break;
			if (auto ts = t - gtl::seq::clock_t::now(); ts > 3s)
				t = gtl::seq::clock_t::now() + 3s;
			std::this_thread::sleep_until(t);
		} while (!driver.IsDone());
		fmt::print("Sequence1 result : {}\n", future.get());

		fmt::print("\n");

		// start tree sequence
		driver.CreateChildSequence("TreeSequence", &TopSeq);
		do {
			auto t = driver.Dispatch();
			if (driver.IsDone())
				break;
			if (auto ts = t - gtl::seq::clock_t::now(); ts > 3s)
				t = gtl::seq::clock_t::now() + 3s;
			std::this_thread::sleep_until(t);
		} while (!driver.IsDone());

		fmt::print("End\n");
	} catch (std::exception& e) {
		fmt::print("Exception : {}\n", e.what());
	}


	if constexpr (true) {
		gtl::seq::v01::xSequenceAny driver;

		fmt::print("Creating 2 sequences returning string and int respectively\n");

		auto f1 = driver.CreateChildSequence("SeqReturningString", &SeqReturningString);
		auto f2 = driver.CreateChildSequence("SeqReturningInt", &SeqReturningInt);

		do {
			auto t = driver.Dispatch();
			if (driver.IsDone())
				break;
			if (auto ts = t - gtl::seq::clock_t::now(); ts > 3s)
				t = gtl::seq::clock_t::now() + 3s;
			std::this_thread::sleep_until(t);
		} while (!driver.IsDone());

		fmt::print("Result of SeqReturningString : {}\n", f1.get());
		fmt::print("Result of SeqReturningInt : {}\n", f2.get());

		fmt::print("End\n");
	}

}
