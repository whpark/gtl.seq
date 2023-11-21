# gtl.seq

## Overview
gtl.seq is a modern C++ library offering an alternative to traditional switch-case state machines. By leveraging the power of coroutines, this library simplifies the implementation of state machines, making your code more readable, maintainable, and efficient.

## Features
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



License
This project is licensed under MIT License.