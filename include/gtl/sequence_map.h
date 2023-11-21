#pragma once

//////////////////////////////////////////////////////////////////////
//
// sequence_map.h: Sequence Map.
//
// 
// PWH
// 2023-11-06
//
//////////////////////////////////////////////////////////////////////

#include <set>
#include <map>
#include "sequence.h"

namespace gtl::seq::inline v01 {

	//-------------------------------------------------------------------------
	/// @brief sequence map (unit tree, sequence function map) manager
	template < typename tResult, typename tParam = tResult >
	class TSequenceMap {
	public:
		using this_t = TSequenceMap;
		using result_t = tResult;
		using param_t = tParam;
		using seq_t = TSequence<result_t>;
		using coro_t = seq_t::coro_t;

		using unit_id_t = std::string;

	public:
		using this_t = TSequenceMap;
		using handler_t = std::function<coro_t(seq_t&, param_t&&)>;
		using map_t = std::map<seq_id_t, handler_t>;

	private:
		mutable seq_t* m_sequence_driver{};	// only valid if (m_parent == nullptr). (std::variant<this_t*, seq_t*> : too verbose)
	protected:
		unit_id_t m_unit;
		this_t* m_parent{};
		std::set<this_t*> m_mapChildren;
		map_t m_mapFuncs;

	public:
		// constructors and destructor
		TSequenceMap(unit_id_t unit) : m_unit(std::move(unit)) {
		}
		TSequenceMap(unit_id_t unit, this_t& parent) : m_unit(std::move(unit)), m_parent(&parent) {
			m_parent->Register(this);
		}
		TSequenceMap(unit_id_t unit, seq_t& driver) : m_unit(std::move(unit)), m_sequence_driver(&driver) {
		}
		~TSequenceMap() {
			while (m_mapChildren.size()) {	// children can outlive parents
				Unregister(*m_mapChildren.begin());
			}
			m_mapChildren.clear();
			if (auto* parent = std::exchange(m_parent, nullptr))
				parent->Unregister(this);
		}
		TSequenceMap(TSequenceMap const&) = delete;
		TSequenceMap& operator = (TSequenceMap const&) = delete;
		TSequenceMap(TSequenceMap&& b) {
			if (this == &b)
				return;

			if (auto* parent = std::exchange(b.m_parent, nullptr)) {
				parent->Unregister(&b);
				m_parent = parent;
			}
			m_unit = std::exchange(b.m_unit, {});
			m_sequence_driver = std::exchange(b.m_sequence_driver, nullptr);
			m_mapChildren.swap(b.m_mapChildren);

			if (m_parent)
				m_parent->Register(this);
		}
		TSequenceMap& operator = (TSequenceMap&&) = delete;	// if this has some children, no way to remove children from parent
		//TSequenceMap& operator = (TSequenceMap&& b) {
		//	m_unit = std::move(b.m_unit);
		//	m_mapFuncs = std::move(b.m_mapFuncs);
		//	
		//	if (m_parent = std::exchange(b.m_parent, nullptr)) {
		//		m_parent->Unregister(&b);
		//		m_parent->Register(this);
		//	}
		// 
		//	// for children, update m_top
		//}
		inline this_t* GetTopMost() { auto* p = this; while (p->m_parent) p = p->m_parent; return p; }
		inline this_t const* GetTopMost() const { auto const* p = this; while (p->m_parent) p = p->m_parent; return p; }
		seq_t* GetSequenceDriver() const { if (auto* top = GetTopMost()) return top->m_sequence_driver; return nullptr; }
		auto const& GetUnitName() const { return m_unit; }
		auto* GetCurrentSequence() { return GetSequenceDriver()->GetCurrentSequence(); }
		auto const* GetCurrentSequence() const { return GetSequenceDriver()->GetCurrentSequence(); }
		//-----------------------------------
		/// @brief Register/Unregister this unit
		inline void Register(this_t* child) {
			if (child) {
				if (auto* p = std::exchange(child->m_parent, nullptr); p) {
					p->Unregister(child);
				}
				child->m_parent = this;
				child->m_sequence_driver = nullptr;
				m_mapChildren.insert(child);
			}
		}
		inline void Unregister(this_t* child) {
			if (child) {
				child->m_parent = nullptr;
				m_mapChildren.erase(child);
			}
		}

		//-----------------------------------
		/// @brief Bind/Unbind sequence function with name
		inline bool Bind(seq_id_t const& id, handler_t handler) {
			if (auto iter = m_mapFuncs.find(id); iter != m_mapFuncs.end())
				return false;
			m_mapFuncs[id] = handler;
			return true;
		}
		inline bool Unbind(seq_id_t const& id) {
			if (auto iter = m_mapFuncs.find(id); iter != m_mapFuncs.end()) {
				m_mapFuncs.erase(iter);
				return true;
			}
			return false;
		}
	protected:
		template < typename tSelf > requires std::is_base_of_v<this_t, tSelf>
		inline bool Bind(seq_id_t const& id, coro_t(tSelf::* handler)(seq_t&, param_t) ) {
			return Bind(id, std::bind(handler, (tSelf*)(this), std::placeholders::_1, std::placeholders::_2));
		}

	public:
		//-----------------------------------
		// Find Handler
		inline handler_t FindHandler(seq_id_t const& sequence) const {
			if (auto iter = m_mapFuncs.find(sequence); iter != m_mapFuncs.end())
				return iter->second;
			return nullptr;
		}

		//-----------------------------------
		// Find Map
	#if __cpp_explicit_this_parameter
		auto FindUnitDFS(this auto&& self, seq_id_t const& unit) -> decltype(&self) {
			if (self.m_unit == unit)
				return &self;
			for (auto* child : self.m_mapChildren) {
				if (auto* unitTarget = child->FindUnitDFS(unit))
					return unitTarget;
			}
			return nullptr;
		}
	#else
		this_t const* FindUnitDFS(seq_id_t const& unit) const {
			if (m_unit == unit)
				return this;
			for (auto* child : m_mapChildren) {
				if (auto* unitTarget = child->FindUnitDFS(unit))
					return unitTarget;
			}
			return nullptr;
		}
		inline this_t* FindUnitDFS(seq_id_t const& unit) {
			return const_cast<this_t*>( (const_cast<this_t const*>(this))->FindUnitDFS(unit) );
		}
	#endif
		//-----------------------------------
		template < typename tSelf >
			requires std::is_base_of_v<this_t, tSelf>
		inline auto CreateSequence(seq_t* parent, seq_id_t running, tSelf* self, coro_t(tSelf::*handler)(seq_t&, param_t), param_t params = {}) {
			if (!handler)
				throw std::exception("no handler");
			if (!parent)
				parent = self->GetCurrentSequence();	// current sequence
			if (!parent)
				parent = self->GetSequenceDriver();		// top most
			if (!parent)
				throw std::exception("no parent seq");
			return parent->CreateChildSequence<param_t>(
				std::move(running), std::bind(handler, self, std::placeholders::_1, std::placeholders::_2), std::move(params));
		}
		//-----------------------------------
		inline auto CreateSequence(seq_t* parent, unit_id_t unit, seq_id_t name, seq_id_t running, param_t params = {}) {
			this_t* unitTarget = unit.empty() ? this : GetTopMost()->FindUnitDFS(unit);
			if (!unitTarget)
				throw std::exception("no unit");
			if (!parent)
				parent = unitTarget->GetCurrentSequence();	// current sequence
			if (!parent)
				parent = unitTarget->GetSequenceDriver();	// top most
			if (!parent)
				throw std::exception("no parent seq");
			auto func = unitTarget->FindHandler(name);
			if (!func)
				throw std::exception("no handler");
			return parent->CreateChildSequence<param_t>(
				running.empty() ? std::move(name) : std::move(running), func, std::move(params));
		}

		// root sequence
		inline auto CreateRootSequence(unit_id_t const& unit, seq_id_t name, param_t params) {
			return CreateSequence(GetSequenceDriver(), unit, std::move(name), {}, std::move(params));
		}
		inline auto CreateRootSequence(seq_id_t name, param_t params = {}) {
			return CreateSequence(GetSequenceDriver(), {}, std::move(name), {}, std::move(params));
		}

		// child sequence
		inline auto CreateChildSequence(seq_t* parent, unit_id_t const& unit, seq_id_t name, param_t params) {
			return CreateSequence(parent, unit, std::move(name), {}, std::move(params));
		}
		inline auto CreateChildSequence(seq_id_t name, param_t params = {}) {
			if (auto* parent = GetCurrentSequence())
				return CreateSequence(parent, {}, std::move(name), {}, std::move(params));
			throw std::exception("CreateChildSequence() must be called from sequence function");
		}
		inline auto CreateChildSequence(unit_id_t const& unit, seq_id_t name, param_t params) {
			if (auto* parent = GetCurrentSequence())
				return CreateSequence(parent, unit, std::move(name), {}, std::move(params));
			throw std::exception("CreateChildSequence() must be called from sequence function");
		}

		// broadcast sequence
		size_t BroadcastSequence(seq_t& parent, seq_id_t name, param_t params = {}) {
			size_t count{};
			if (auto handler = FindHandler(name)) {
				parent.CreateChildSequence<std::shared_ptr<param_t>>(name, handler, std::move(params));
				count++;
			}
			for (auto* child : m_mapChildren) {
				count += child->BroadcastSequence(parent, name, params);
			}
			return count;
		}

		// co_await
		auto WaitFor(clock_t::duration d) {
			if (auto* cur = GetCurrentSequence())
				return cur->WaitFor(d);
			throw std::exception("WaitFor() must be called from sequence function");
		}
		// co_await
		auto WaitUntil(clock_t::time_point t) {
			if (auto* cur = GetCurrentSequence())
				return cur->WaitUntil(t);
			throw std::exception("WaitFor() must be called from sequence function");
		}
		// co_await
		auto WaitForChild() {
			if (auto* cur = GetCurrentSequence())
				return cur->WaitForChild();
			throw std::exception("WaitFor() must be called from sequence function");
		}
		auto Wait(std::function<bool()> pred, clock_t::duration interval, clock_t::duration timeout = clock_t::duration::max()) {
			if (auto* cur = GetCurrentSequence())
				return cur->Wait(std::move(pred), interval, timeout);
			throw std::exception("Wait() must be called from sequence function");
		}
	};

};
