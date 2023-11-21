# gtl.seq

## Overview
gtl.seq is a modern C++ library offering an alternative to traditional switch-case state machines. By leveraging the power of coroutines, this library simplifies the implementation of state machines, making your code more readable, maintainable, and efficient.

## Features
- Single Threaded : You can run all sub-routines(coroutines) in the UI thread.
- Sequence : coroutine task (switch-case state machine routines)
- Tree-like child sequences : create sub state machine and waits for all child sequences
- Event-map like sequence invoke.

## Examples
- simple sequence
	examples/basic.cpp

```
using namespace gtl::seq;

using seq_t = gtl::seq::TSequence<std::string>;
using coro_t = seq_t::coro_t;

// sub-routine
coro_t Sequence(seq_t& seq) {
	namespace chrono = std::chrono;
	auto t0 = chrono::steady_clock::now();

	// Wait For 40ms
	co_await seq.WaitFor(40ms);

	// do print something

	auto t1 = chrono::steady_clock::now();
	fmt::print("step1 : {:>8}\n", (t1 - t0));

	co_await seq.WaitUntil(gtl::seq::clock_t::now() + 100ms);
	auto t2 = chrono::steady_clock::now();
	fmt::print("step2 : {:>8}\n", (t2 - t1));

	co_return fmt::format("{} ended. take {}",
		seq.GetName(), (chrono::steady_clock::now() - t0));
}


int main() {
	try {
		seq_t driver;

		fmt::print("\n\nBegin : Simple\n");

		// start simple sequence
		std::future<seq_t::result_t> future = driver.CreateChildSequence("SimpleSequence", &Sequence1);
		do {
			auto t = driver.Dispatch();
			if (driver.IsDone())
				break;
			std::this_thread::sleep_until(t);
		} while (!driver.IsDone());
		fmt::print("Sequence1 result : {}\n", future.get());

		fmt::print("End : Simple\n");
	} catch (std::exception& e) {
		fmt::print("Exception : {}\n", e.what());
	}

}

```


- tree-like sequence
	examples/basic.cpp
```
namespace gtl::seq {

using namespace std::literals;
//using namespace gtl::literals;
namespace chrono = std::chrono;

using seq_t = gtl::seq::TSequence<std::string>;
using coro_t = seq_t::coro_t;

coro_t TopSeq(seq_t&);
coro_t Child1(seq_t&);
coro_t Child1_1(seq_t&);
coro_t Child1_2(seq_t&);
coro_t Child2(seq_t&);

coro_t TopSeq(seq_t& seq) {

	// step 1
	std::future<std::string> f = seq.CreateChildSequence("Child1", &Child1);
	co_await seq.WaitForChild();

	co_return f.get();
}

coro_t Child1(seq_t& seq) {
	// step 1
	// Creates child sequences
	seq.CreateChildSequence("Child1_1", &Child1_1);
	seq.CreateChildSequence("Child1_2", &Child1_2);
	co_await seq.WaitForChild();

	co_return "";
}

coro_t Child1_1(seq_t& seq) {
	auto t0 = gtl::seq::clock_t::now();

	// Loop - 1s
	for (int i = 0; i < 5; i++) {
		co_await seq.WaitFor(200ms);
	}
	
	co_return "";
}

coro_t Child1_2(seq_t& seq) {

	// Loop - 1s
	for (int i = 0; i < 5; i++) {
		co_await seq.WaitFor(200ms);
	}
	fmt::print("{}: End. Creating Child1_1, Child1_2\n", funcname);
	
	co_return "";
}

coro_t Child2(seq_t& seq) {
	co_return "";
}

}	// namespace gtl::seq

int main() {
	try {
		seq_t driver;

		// start sequence
		std::future<seq_t::result_t> future = driver.CreateChildSequence("SimpleSequence", &Sequence1);

		// co-routine driver
		do {
			auto t = driver.Dispatch();
			if (driver.IsDone())
				break;
			std::this_thread::sleep_until(t);
		} while (!driver.IsDone());
	} catch (std::exception& e) {
		fmt::print("Exception : {}\n", e.what());
	}
}

```


License
This project is licensed under MIT License.