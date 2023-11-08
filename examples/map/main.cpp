// gtl.seq.cpp : Defines the entry point for the application.
//

#include <string>
#include <source_location>

#include <fmt/core.h>
#include <fmt/xchar.h>
#include <fmt/chrono.h>

#include "gtl/sequence.h"
#include "gtl/sequence_map.h"

using seq_t = gtl::seq::xSequence;
using seq_map_t = gtl::seq::TSequenceMap<std::string, std::string>;

using namespace std::literals;
namespace chrono = std::chrono;
//using namespace gtl::literals;

//-----
class CApp : public seq_map_t {
public:
	CApp(seq_t& driver) : seq_map_t("top", driver) {
	}
	void Run() {
		auto* driver = GetSequenceDriver();
		do {
			auto t = driver->Dispatch();
			if (driver->IsDone())
				break;
			if (auto ts = t - gtl::seq::clock_t::now(); ts > 3s)
				t = gtl::seq::clock_t::now() + 3s;
			std::this_thread::sleep_until(t);
		} while (true);
	}
};


//-----
class C1 : public seq_map_t {
public:
	using this_t = C1;
	using base_t = seq_map_t;

	C1(unit_id_t const& id, seq_map_t& parent) : seq_map_t(id, parent) {
		Bind("task1", this, &this_t::Task1);
		Bind("task2", this, &this_t::Task2);
	}

protected:
	seq_t Task1(std::shared_ptr<seq_map_t::sParam> param) {
		//auto sl = std::source_location::current();
		auto funcname = "Task1";// sl.function_name();

		fmt::print("{}: Begin\n", funcname);

		auto param2 = std::make_shared<seq_map_t::sParam>();
		param2->in = "Greeting from c1::Task1 ==> c1::Task2";
		CreateChildSequence("task2", param2);
		co_await WaitForChild();

		fmt::print("{}: End. {}\n", funcname, param2->out);

		co_return;
	}
	seq_t Task2(std::shared_ptr<seq_map_t::sParam> param) {
		//auto sl = std::source_location::current();
		auto funcname = "Task2";// sl.function_name();

		fmt::print("{}: Begin - {}\n", funcname, param->in);

		auto param2 = std::make_shared<seq_map_t::sParam>();
		param2->in = "Greeting from c1::Task2 ==> c2::TaskA";
		CreateChildSequence("c2", "taskA", param2);
		co_await WaitForChild();

		fmt::print("{}: End\n", funcname);

		param->out = "OK";

		co_return;
	}

};


//-----
class C2 : public seq_map_t {
public:
	using this_t = C2;
	using base_t = seq_map_t;

	C2(unit_id_t const& id, seq_map_t& parent) : seq_map_t(id, parent) {
		Bind("taskA", this, &this_t::TaskA);
		Bind("taskB", this, &this_t::TaskB);
	}

protected:
	seq_t TaskA(std::shared_ptr<seq_map_t::sParam> param) {
		//auto sl = std::source_location::current();
		auto funcname = "TaskA";// sl.function_name();

		fmt::print("{}: Begin - {}\n", funcname, param->in);

		auto param2 = std::make_shared<seq_map_t::sParam>();
		param2->in = "Greeting from c2::TaskA ==> c2::TaskB";
		CreateChildSequence("c2", "taskB", param2);
		co_await WaitForChild();

		fmt::print("{}: End\n", funcname);

		//param->out = "OK";

		co_return;
	}
	seq_t TaskB(std::shared_ptr<seq_map_t::sParam> param) {
		//auto sl = std::source_location::current();
		auto funcname = "TaskB";// sl.function_name();

		fmt::print("{}: Begin - {}\n", funcname, param->in);

		auto param2 = std::make_shared<seq_map_t::sParam>();
		param2->in = "Greeting from c1::Task2 ==> c2::TaskA";
		co_await WaitFor(100ms);

		fmt::print("{}: End\n", funcname);

		//param->out = "OK";

		co_return;
	}
};


int main() {

	fmt::print("start\n");

	seq_t driver;
	CApp app(driver);


	C1 c1("c1", app);
	C2 c2("c2", app);

	c1.CreateRootSequence("task1");	// c1::task1 -> c1::task2 -> c2::taskA -> c2::taskB

	app.Run();

	fmt::print("end\n");


}
