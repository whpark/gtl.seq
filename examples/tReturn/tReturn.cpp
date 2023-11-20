// gtl.seq.cpp : Defines the entry point for the application.
//

#include <string>
#include <source_location>

#include <fmt/core.h>
#include <fmt/xchar.h>
#include <fmt/chrono.h>
#include "gtl/sequence.h"
#include "gtl/sequence_tReturn.h"
#include "gtl/sequence_map.h"

namespace gtl::seq::test {

	using namespace std::literals;
	//using namespace gtl::literals;
	namespace chrono = std::chrono;

	using seq_t = gtl::seq::xSequenceTReturn;
	template < typename tReturn >
	using tcoro_t = seq_t::tcoro_t<tReturn>;

	tcoro_t<std::string> SeqReturningString(gtl::seq::v01::xSequenceTReturn& seq) {
		auto t0 = chrono::steady_clock::now();
		auto str = fmt::format("{} ended. take {}", seq.GetName(), chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - t0));

		co_return std::move(str);
	}

	tcoro_t<int> SeqReturningInt(gtl::seq::v01::xSequenceTReturn& seq) {
		bool bOK = co_await seq.Wait([t0 = gtl::seq::clock_t::now()] {
			auto t = gtl::seq::clock_t::now();
			fmt::print("SeqReturningInt : {}\n", chrono::duration_cast<chrono::milliseconds>(t - t0));
			return t - t0 > 1s;
		}, 100ms, 2s);
		co_return 3141592;
	}

}	// namespace gtl::seq::test

int main() {
	using namespace gtl::seq::test;

	{
		if constexpr (true) {
			gtl::seq::v01::xSequenceTReturn driver;

			fmt::print("Creating 2 sequences returning string and int respectively\n");

			std::future<std::string> f1 = driver.CreateChildSequence("SeqReturningString", &SeqReturningString);
			std::future<int> f2 = driver.CreateChildSequence("SeqReturningInt", &SeqReturningInt);

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

}
