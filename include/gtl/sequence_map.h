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

#include "sequence.h"

namespace gtl::seq::inline v01 {

	//-------------------------------------------------------------------------
	/// @brief sequence map (unit tree, sequence function map) manager
	template < typename tParamIn, typename tParamOut = tParamIn >
	class TSequenceMap {
	public:
		using this_t = TSequenceMap;
		using unit_id_t = std::string;
		using param_in_t = tParamIn;
		using param_out_t = tParamOut;
		using sequence_t = xSequence;

	public:
		using this_t = TSequenceMap;
		struct sParam {
			param_in_t in{};
			param_out_t out{};
		};
		using handler_t = std::function<xSequence(std::shared_ptr<sParam>)>;
		using map_t = std::map<seq_id_t, handler_t>;

	private:
		xSequence* m_sequence_driver{};
		this_t* m_top{};
	protected:
		unit_id_t m_unit;
		this_t* m_parent{};
		std::set<this_t*> m_mapChildren;
		map_t m_mapFuncs;

	public:
		// constructors and destructor
		TSequenceMap(unit_id_t unit, this_t& parent) : m_unit(std::move(unit)), m_parent(&parent), m_top(parent.m_top), m_sequence_driver(parent.m_sequence_driver) {
			m_parent->Register(this);
		}
		TSequenceMap(unit_id_t unit, xSequence& driver) : m_unit(std::move(unit)), m_sequence_driver(&driver), m_top(this) {
		}
		~TSequenceMap() {
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
				m_top = parent->m_top;
			}
			m_unit = std::exchange(b.m_unit, {});
			m_top = std::exchange(b.m_top, nullptr);
			m_sequence_driver = std::exchange(b.m_sequence_driver, nullptr);
			m_mapChildren.swap(b.m_mapChildren);

			if (m_parent)
				m_parent->Register(this);
		}
		TSequenceMap& operator = (TSequenceMap&&) = delete;	// if has some children, no way to remove children from parent

		auto GetSequenceDriver() const { return m_sequence_driver; }
		auto const& GetUnitName() const { return m_unit; }

		//-----------------------------------
		/// @brief Register/Unregister this unit
		inline void Register(this_t* child) {
			if (child) {
				m_mapChildren.insert(child);
			}
		}
		inline void Unregister(this_t* child) {
			if (child) {
				m_mapChildren.erase(child);
			}
		}

		//-----------------------------------
		/// @brief Bind/Unbind sequence function with name
		inline void Bind(seq_id_t const& id, handler_t handler) {
			m_mapFuncs[id] = handler;
		}
		inline void Unbind(seq_id_t const& id) {
			if (auto iter = m_mapFuncs.find(id); iter != m_mapFuncs.end())
				m_mapFuncs.erase(iter);
		}
		template < typename tSelf, typename self_handler_t = std::function<xSequence(tSelf* self, std::shared_ptr<typename base_t::sParam>)> >
		inline void Bind(seq_id_t const& id, tSelf* self, self_handler_t handler) {
			Bind(id, std::bind(handler, self, std::placeholders::_1));
		}

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
		template < typename tSelf, typename self_handler_t = std::function<xSequence(tSelf* self, std::shared_ptr<sParam>)> >
			requires std::is_base_of_v<this_t, tSelf>
		inline xSequence& CreateSequence(xSequence* parent, seq_id_t name, seq_id_t running, tSelf* self, self_handler_t handler, std::shared_ptr<sParam> params = std::make_shared<sParam>()) {
			if (!parent)
				parent = ((this_t*)self)->m_sequence_driver->GetCurrentSequence();
			if (!parent)
				parent = ((this_t*)self)->m_sequence_driver;
			if (!parent)
				throw std::exception("no parent seq");
			if (handler) {
				return parent->CreateChildSequence<std::shared_ptr<sParam>>(
					running.empty() ? std::move(name) : std::move(running),
					std::bind(handler, self, std::placeholders::_1), std::move(params));
			}
			else {
				auto func = self->FindHandler(name);
				if (!func)
					throw std::exception("no handler");
				return parent->CreateChildSequence<std::shared_ptr<sParam>>(
					running.empty() ? std::move(name) : std::move(running), func, std::move(params));
			}
		}
		//-----------------------------------
		inline xSequence& CreateSequence(xSequence* parent, unit_id_t unit, seq_id_t name, seq_id_t running, std::shared_ptr<sParam> params = std::make_shared<sParam>()) {
			this_t* unitTarget = unit.empty() ? this : m_top->FindUnitDFS(unit);
			if (!unitTarget)
				throw std::exception("no unit");
			if (!parent)
				parent = unitTarget->m_sequence_driver->GetCurrentSequence();
			if (!parent)
				parent = unitTarget->m_sequence_driver;
			if (!parent)
				throw std::exception("no parent seq");
			auto func = unitTarget->FindHandler(name);
			if (!func)
				throw std::exception("no handler");
			return parent->CreateChildSequence<std::shared_ptr<sParam>>(
				running.empty() ? std::move(name) : std::move(running), func, std::move(params));
		}

		// root sequence
		inline xSequence& CreateRootSequence(unit_id_t const& unit, seq_id_t name, std::shared_ptr<sParam> params = std::make_shared<sParam>()) {
			return CreateSequence(m_sequence_driver, unit, std::move(name), {}, std::move(params));
		}
		inline xSequence& CreateRootSequence(seq_id_t name, std::shared_ptr<sParam> params = std::make_shared<sParam>()) {
			return CreateSequence(m_sequence_driver, {}, std::move(name), {}, std::move(params));
		}

		// child sequence
		inline xSequence& CreateChildSequence(xSequence* parent, unit_id_t const& unit, seq_id_t name, std::shared_ptr<sParam> params = std::make_shared<sParam>()) {
			return CreateSequence(parent, unit, std::move(name), {}, std::move(params));
		}
		inline xSequence& CreateChildSequence(seq_id_t name, std::shared_ptr<sParam> params = std::make_shared<sParam>()) {
			if (auto* parent = m_sequence_driver->GetCurrentSequence())
				return CreateSequence(parent, {}, std::move(name), {}, std::move(params));
			throw std::exception("CreateChildSequence() must be called from sequence function");
		}
		inline xSequence& CreateChildSequence(unit_id_t const& unit, seq_id_t name, std::shared_ptr<sParam> params = std::make_shared<sParam>()) {
			if (auto* parent = m_sequence_driver->GetCurrentSequence())
				return CreateSequence(parent, unit, std::move(name), {}, std::move(params));
			throw std::exception("CreateChildSequence() must be called from sequence function");
		}

		// broadcast sequence
		size_t BroadcastSequence(xSequence& parent, seq_id_t name, std::shared_ptr<sParam> params = std::make_shared<sParam>()) {
			size_t count{};
			if (auto handler = FindHandler(name)) {
				parent.CreateChildSequence<std::shared_ptr<sParam>>(name, handler, std::move(params));
				count++;
			}
			for (auto* child : m_mapChildren) {
				count += child->BroadcastSequence(parent, name, params);
			}
			return count;
		}

		// co_await
		auto WaitFor(clock_t::duration d) {
			if (auto* cur = m_sequence_driver->GetCurrentSequence())
				return cur->WaitFor(d);
			throw std::exception("WaitFor() must be called from sequence function");
		}
		// co_await
		auto WaitUntil(clock_t::time_point t) {
			if (auto* cur = m_sequence_driver->GetCurrentSequence())
				return cur->WaitUntil(t);
			throw std::exception("WaitFor() must be called from sequence function");
		}
		// co_await
		auto WaitForChild() {
			if (auto* cur = m_sequence_driver->GetCurrentSequence())
				return cur->WaitForChild();
			throw std::exception("WaitFor() must be called from sequence function");
		}
	};

};
