#pragma once
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <chrono>
#include <cassert>
#include <Variant.h>
#include "IWorld.h"
#include "IProcess.h"
#include "Entity.h"
#include "ComponentContainer.h"
#include "TypeTraits.h"
#include "Tuple.h"

namespace au {
	namespace detail {
		struct RemovalAction {
			size_t id;
		};

		template<typename... ComponentTypes>
		struct ComponentChangeInfo {
			size_t index;
			size_t removeLength;

			Entity<sizeof...(ComponentTypes)> owner;
			variant<ComponentTypes..., RemovalAction> data;
			bool destructive;
		};
	}

	class InvalidProcessStateException : public std::runtime_error {
	public:
		InvalidProcessStateException() : std::runtime_error("This operation is not allowed in the current process state.")
		{
		}
	};

	class AuthorityException : public std::runtime_error {
	public:
		AuthorityException() : std::runtime_error("Another iterator is the current authority for one or more of the requested types.")
		{
		}
	};

	class MissingAuthorityException : public std::runtime_error {
	public:
		MissingAuthorityException() : std::runtime_error("Insufficient authority fields")
		{
		}
	};

	template<typename... T>
	using ComponentSet = type_tuple < T... > ;

	template<typename... T>
	using AuthoritySet = type_tuple<T...>;

	template<typename... T>
	using OptionalSet = type_tuple <T...>;

	class WorldMetricsBase {
	public:
		struct ComponentMetrics_t {
			size_t TypeId = 0;
			size_t DeleteOps = 0;
			size_t AddOps = 0;
			double UpdateTime = 0.0;
		};

		double EntityUpdateTime = 0.0;
		double ComponentUpdateTime = 0.0;
		double ProcessExecutionTime = 0.0;
		double EventHandlingTime = 0.0;
		double TotalProcessTime = 0.0;

		virtual ComponentMetrics_t GetComponentMetrics(size_t componentTypeIdx) const
		{
			return{};
		}

		virtual size_t CountComponentMetrics() const
		{
			return 0;
		}
	};

	template<size_t componentCount>
	class WorldMetrics : public WorldMetricsBase {
	public:
		ComponentMetrics_t ComponentMetrics[componentCount];

		ComponentMetrics_t GetComponentMetrics(size_t idx) const override
		{
			return ComponentMetrics[idx];
		}

		size_t CountComponentMetrics() const override
		{
			return componentCount;
		}
	};

	// Forward decs
	template<typename... ComponentTypes>
	class EntityTemplateManager;

	template<typename... ComponentTypes>
	class World : public IWorld {
		struct ProcessData {
			IProcess* Process;
			bool Enabled;
		};
		struct AuthorityData {
			bool Requested;
			void* RequestSource;
		};
	public:
		using entity_type = Entity<sizeof...(ComponentTypes)>;
		using TemplateManagerType = EntityTemplateManager<ComponentTypes...>;
		using Metrics = WorldMetrics<sizeof...(ComponentTypes)>;
	private:
		using ComponentAction = detail::ComponentChangeInfo<ComponentTypes...>;
		using ComponentStorage = std::tuple<ComponentContainer<ComponentTypes>...>;
		using ComponentsTypeTuple = type_tuple<ComponentTypes...>;

		struct QueueRemoval;
		struct SwapBuffers;
		struct AddPendingComponents;
		struct RequestAuthority;

		friend QueueRemoval;
		friend SwapBuffers;
		friend AddPendingComponents;
		friend RequestAuthority;

		// Index pool
		static size_t GetNextGuid()
		{
			static size_t guid = kInvalidEntityGuid + 1;
			return guid++;
		}

		std::vector<entity_type> AvailableEntities;
		std::vector<entity_type> mEntities;
		std::vector<entity_type> mPendingEntityAdditions;
		std::vector<entity_type> mPendingEntityRemovals;
		mutable std::vector<entity_type> mEntitySearchList;
		mutable bool mEntitySearchListValid = false;

		ComponentStorage mComponents;
		std::vector<ComponentAction> mPendingComponentActions;
		int mComponentCountDelta[sizeof...(ComponentTypes)];

		std::vector<std::vector<ProcessData>> mProcessGroups;
		std::vector<size_t> mDisabledProcessGroups;

		AuthorityData mAuthorityExists[sizeof...(ComponentTypes)];
		bool mProcessing = false;

		Metrics mMetrics;
		void* mUserPtr = nullptr;
	public:
		World()
		{
			memset(mComponentCountDelta, 0, sizeof(mComponentCountDelta));
			memset(mAuthorityExists, 0, sizeof(mAuthorityExists));
		}

		~World()
		{
			foreach_tuple_element(mComponents, DestroyComponents());

			for (auto& procgroup : mProcessGroups)
			{
				for (auto& procdata : procgroup)
					delete procdata.Process;
			}
		}

		// Make worlds noncopyable
		World(World const&) = delete;
		World(World&&) = delete;
		World& operator=(World const&) = delete;

		Metrics GetMetrics() const
		{
			return mMetrics;
		}

		EntityRef AddEntity() override
		{
			if (mProcessing)
			{
				return QueueAddEntity();
			}
			else
			{
				entity_type ent;

				if (!AvailableEntities.empty())
				{
					ent = AvailableEntities.back();
					AvailableEntities.pop_back();
					mEntities[ent.Index] = ent;
					mEntitySearchListValid = false;
				}
				else
				{
					ent.Guid = GetNextGuid();
					ent.Index = mEntities.size();
					memset(ent.ComponentCount, 0, sizeof(ent.ComponentCount));
					memset(ent.InternalComponentCount, 0, sizeof(ent.InternalComponentCount));
					mEntities.push_back(ent);
					mEntitySearchList.push_back(ent);
				}

				return{ ent.Guid, ent.Index, this, 0 };
			}
		}

		EntityRef AddEntity(int userValue)
		{
			if (mProcessing)
			{
				return QueueAddEntity();
			}
			else
			{
				entity_type ent;
				ent.UserValue = userValue;

				if (!AvailableEntities.empty())
				{
					ent = AvailableEntities.back();
					AvailableEntities.pop_back();
					mEntities[ent.Index] = ent;
					mEntitySearchListValid = false;
				}
				else
				{
					ent.Guid = GetNextGuid();
					ent.Index = mEntities.size();
					memset(ent.ComponentCount, 0, sizeof(ent.ComponentCount));
					memset(ent.InternalComponentCount, 0, sizeof(ent.InternalComponentCount));
					mEntities.push_back(ent);
					mEntitySearchList.push_back(ent);
				}

				return{ ent.Guid, ent.Index, this, userValue };
			}
		}

		EntityRef QueueAddEntity()
		{
			entity_type ent;
			if (!AvailableEntities.empty())
			{
				ent = AvailableEntities.back();
				AvailableEntities.pop_back();
			}
			else
			{
				ent.Guid = GetNextGuid();
				ent.Index = kInvalidEntityIndex;
				memset(ent.ComponentCount, 0, sizeof(ent.ComponentCount));
				memset(ent.InternalComponentCount, 0, sizeof(ent.InternalComponentCount));
			}

			mPendingEntityAdditions.push_back(ent);
			return{ ent.Guid, ent.Index, this, 0 };
		}

		bool RemoveEntity(EntityRef ent) override
		{
			const entity_type* fent = FindEntityPtr(ent.Guid);

			if (fent != nullptr)
			{
				for (auto& entToRemove : mPendingEntityRemovals)
				{
					if ((entToRemove.Guid == fent->Guid) && (entToRemove.Index == fent->Index))
						return true;
				}

				mPendingEntityRemovals.push_back(*fent);
				return true;
			}
			else
				return false;
		}

		EntityRef Migrate(World<ComponentTypes...>* destination, EntityRef migrated_entity)
		{
			std::vector<EntityRef> performed_migrations;
			std::vector<EntityRef> inherited_migrations;
			inherited_migrations.push_back(migrated_entity);

			EntityRef destination_entity = PerformMigration(destination, migrated_entity, &inherited_migrations);

			auto eref_sorter = [](EntityRef lhs, EntityRef rhs)
			{
				return lhs.Guid < rhs.Guid;
			};

			using mismatch_it = std::vector<EntityRef>::iterator;
			auto mismatch_entity_rhs = [](mismatch_it first1, mismatch_it last1, mismatch_it first2, mismatch_it last2) -> mismatch_it
			{
				auto cmp = [](EntityRef lhs, EntityRef rhs)
				{
					return lhs.Guid == rhs.Guid;
				};

				while ((first1 != last1) && (first2 != last2) && cmp(*first1, *first2))
				{
					first1++;
					first2++;
				}

				return first2;
			};

			performed_migrations.push_back(destination_entity);
			while (performed_migrations.size() != inherited_migrations.size())
			{
				std::sort(performed_migrations.begin(), performed_migrations.end(), eref_sorter);
				std::sort(inherited_migrations.begin(), inherited_migrations.end(), eref_sorter);
				auto it = mismatch_entity_rhs(performed_migrations.begin(), performed_migrations.end(), 
											  inherited_migrations.begin(), inherited_migrations.end());

				performed_migrations.push_back(PerformMigration(destination, *it, &inherited_migrations));
			}

			// Execute the pending updates locally
			std::sort(mPendingComponentActions.begin(), mPendingComponentActions.end(), [](const ComponentAction& lhs, const ComponentAction& rhs)
			{
				return (lhs.index < rhs.index) || ((lhs.index == rhs.index) && (lhs.owner.Index < rhs.owner.Index)) ||
					((lhs.index == rhs.index) && (lhs.owner.Index == rhs.owner.Index) && (lhs.owner.Guid < rhs.owner.Guid));
			});
			foreach_tuple_element(mComponents, AddPendingComponents(this));
			mPendingComponentActions.clear();
			memset(mComponentCountDelta, 0, sizeof(mComponentCountDelta));
			foreach_tuple_element(mComponents, SwapBuffers(this));

			// Execute the pending updates on the destination
			std::sort(destination->mPendingComponentActions.begin(), destination->mPendingComponentActions.end(), [](const ComponentAction& lhs, const ComponentAction& rhs)
			{
				return (lhs.index < rhs.index) || ((lhs.index == rhs.index) && (lhs.owner.Index < rhs.owner.Index)) ||
					((lhs.index == rhs.index) && (lhs.owner.Index == rhs.owner.Index) && (lhs.owner.Guid < rhs.owner.Guid));
			});
			foreach_tuple_element(destination->mComponents, AddPendingComponents(this));
			destination->mPendingComponentActions.clear();
			memset(destination->mComponentCountDelta, 0, sizeof(mComponentCountDelta));
			foreach_tuple_element(destination->mComponents, SwapBuffers(this));

			for (auto& entity : performed_migrations)
				foreach_tuple_element(destination->mComponents, ComponentMigrationNotifier(destination, destination->FindEntityPtr(entity.Guid)));

			Log::Trace(CODELOC, "Migrated entity ", destination_entity.Guid, " from world ", (int) this, " to ", (int) destination);
			return destination_entity;
		}

		void ReserveEntities(size_t count) override
		{
			if (AvailableEntities.size() < count)
			{
				mEntities.reserve(mEntities.size() + (count - AvailableEntities.size()));
				mEntitySearchList.reserve(mEntities.capacity());
			}
		}

		size_t CountEntities() const override
		{
			return mEntities.size() - AvailableEntities.size();
		}

		size_t CountPendingEntities() const
		{
			return mPendingEntityAdditions.size() - mPendingEntityRemovals.size();
		}

		EntityRef GetEntity(size_t idx) const override
		{
			if (idx >= mEntities.size())
				throw std::out_of_range("index exceeds entity count");
			else
			{
				entity_type ent = mEntities[idx];
				return{ ent.Guid, ent.Index, (IWorld*)this, ent.UserValue };
			}
		}

		EntityRef FindEntity(size_t guid) const override
		{
			const entity_type* fent = FindEntityPtr(guid);

			if (fent)
				return{ fent->Guid, fent->Index, (IWorld*)this, fent->UserValue };
			else
				return EntityRef::Invalid;
		}

		EntityRef FindEntityExt(size_t guid) const override
		{
			const entity_type* fent = FindEntityPtrExt(guid);

			if (fent)
				return{ fent->Guid, fent->Index, (IWorld*)this, fent->UserValue };
			else
				return EntityRef::Invalid;
		}

		/// Verifies the specified entity's validity and if it is contained within the current entity vector.
		/// 
		bool IsValid(EntityRef entity) const override
		{
			if (!entity.IsValid() || (entity.Owner != this))
				return false;
			if (entity.Index >= mEntities.size())
				return false;

			return mEntities[entity.Index].Guid == entity.Guid;
		}

		template<typename T>
		bool AddComponent(EntityRef ent, T data)
		{
			if (mProcessing)
				return QueueAddComponent(ent, data);
			else
			{
				auto* entp = FindEntityPtr(ent.Guid);
				if (!entp)
					return QueueAddComponent(ent, data);

				auto& container = std::get<typename ComponentContainer<T>>(mComponents).PresentBuffer;
				auto it = FindLastComponentBelongingToEntity(container, *entp);
				size_t dist = std::distance(container.begin(), it);
				data.OwnerIndex = entp->Index;
				container.insert(it, data);
				entp->ComponentCount[index_of<T, ComponentTypes...>::value]++;
				entp->InternalComponentCount[index_of<T, ComponentTypes...>::value]++;

				AddComponentImpl(entp->Guid, entp->Index, entp->UserValue, dist, data);
				return true;
			}
		}

		template<typename T>
		bool QueueAddComponent(EntityRef ent, T data)
		{
			auto* entp = FindEntityPtrExt(ent.Guid);
			if (!entp)
				return false;

			auto& container = std::get<typename ComponentContainer<T>>(mComponents);
			auto& buffer = mProcessing ? container.FutureBuffer : container.PresentBuffer;
			auto it = FindLastComponentBelongingToEntity(buffer, *entp);
			data.OwnerIndex = entp->Index;

			ComponentAction action = {
				std::distance(buffer.begin(), it),
				0,
				*entp,
				data,
				false,
			};

			mComponentCountDelta[ComponentsTypeTuple::index_of<T>::value]++;
			mPendingComponentActions.push_back(action);

			return true;
		}

		template<typename T>
		bool QueueRemoveComponent(EntityRef ent, size_t idx = 0)
		{
			auto* entp = FindEntityPtr(ent.Guid);
			if (!entp)
				return false;

			if (idx >= entp->ComponentCount[ComponentsTypeTuple::index_of<T>::value])
			{
				return false;
			}
			else
			{
				auto& container = std::get<typename ComponentContainer<T>>(mComponents);
				auto& buffer = mProcessing ? container.FutureBuffer : container.PresentBuffer;
				auto it = FindFirstComponentBelongingToEntity(buffer, *entp);

				if (it != buffer.end())
				{
					it += idx;

					ComponentAction removalAction = {
						std::distance(buffer.begin(), it),
						1,
						*entp,
						detail::RemovalAction{ T::Id() },
						true,
					};

					for (ComponentAction& action : mPendingComponentActions)
					{
						if (action.destructive && (action.index == removalAction.index) &&
							(action.removeLength == removalAction.removeLength) &&
							(action.owner.Guid == removalAction.owner.Guid))
							return true;
					}

					mComponentCountDelta[ComponentsTypeTuple::index_of<T>::value]--;
					mPendingComponentActions.push_back(removalAction);

					return true;
				}

				return false;
			}
		}

		void* GetRawComponent(EntityRef ent, size_t componentId, unsigned char idx = 0) override
		{
			return GetRawComponentImpl<ComponentTypes...>(ent, componentId, idx);
		}

		unsigned char CountRawComponents(EntityRef ent, size_t componentId) const override
		{
			return CountRawComponentsImpl<ComponentTypes...>(ent, componentId);
		}

		void* GetRawFutureComponent(EntityRef ent, size_t componentId, unsigned char idx = 0) override
		{
			return GetRawFutureComponentImpl<ComponentTypes...>(ent, componentId, idx);
		}

		unsigned char CountRawFutureComponents(EntityRef ent, size_t componentId) const override
		{
			return CountRawFutureComponentsImpl<ComponentTypes...>(ent, componentId);
		}

		/// Attempts to return a pointer to the specified component contained within a present buffer
		/// Returns null if the entity's invalid or does not possess this component
		/// NOTE: This function is unsafe and does not verify if there's already an authority for this component type
		/// and also does not prevent any data races.
		template<typename T>
		T* GetComponent(EntityRef ent, unsigned char idx = 0)
		{
			return GetComponentInContainer<T>(ent, std::get<typename ComponentContainer<T>>(mComponents).PresentBuffer, idx);
		}

		/// Attempts to return a pointer to the specified component contained within a future buffer
		/// Returns null if the entity's invalid or does not possess this component
		/// NOTE: This function is unsafe and does not verify if there's already an authority for this component type.
		template<typename T>
		T* GetFutureComponent(EntityRef ent, unsigned char idx = 0)
		{
			return GetComponentInContainer<T>(ent, std::get<typename ComponentContainer<T>>(mComponents).FutureBuffer, idx);
		}

		template<typename T>
		unsigned char CountComponents(EntityRef ent) const
		{
			auto* entp = FindEntityPtr(ent.Guid);

			if (!entp)
				return 0;
			else
				return entp->ComponentCount[ComponentsTypeTuple::index_of<T>::value];
		}

		template<typename T>
		unsigned char CountInternalComponents(EntityRef ent) const
		{
			auto* entp = FindEntityPtr(ent.Guid);

			if (!entp)
				return 0;
			else
				return entp->InternalComponentCount[ComponentsTypeTuple::index_of<T>::value];
		}

		void AddProcess(IProcess* proc, size_t procGroup) override
		{
			while (mProcessGroups.size() <= procGroup)
			{
				mProcessGroups.emplace_back();
			}

			mProcessGroups[procGroup].push_back({ proc, true });
		}

		void RemoveProcess(IProcess* proc) override
		{
			for (auto& procgroup : mProcessGroups)
			{
				for (auto it = procgroup.begin(); it != procgroup.end(); it++)
				{
					if (it->Process == proc)
					{
						procgroup.erase(it);
						return;
					}
				}
			}
		}

		IProcess* GetProcessById(size_t id) const override
		{
			for (auto& procgroup : mProcessGroups)
			{
				for (auto& procdata : procgroup)
				{
					if (procdata.Process->GetProcessTypeId() == id)
					{
						return procdata.Process;
					}
				}
			}

			return nullptr;
		}

		template<typename T>
		T* GetProcess() const
		{
			return (T*) GetProcessById(T::ProcessTypeId);
		}

		void SetProcessEnabled(size_t processTypeId, bool enabled)
		{
			for (auto& procgroup : mProcessGroups)
			{
				for (auto& procdata : procgroup)
				{
					if (procdata.Process->GetProcessTypeId() == processTypeId)
					{
						procdata.Enabled = enabled;
						return;
					}
				}
			}
		}

		bool GetProcessEnabled(size_t processTypeId) const
		{
			for (auto& procgroup : mProcessGroups)
			{
				for (auto& procdata : procgroup)
				{
					if (procdata.Process->GetProcessTypeId() == processTypeId)
					{
						return procdata.Enabled;
					}
				}
			}

			return false;
		}

		void SetProcessGroupEnabled(size_t group_id, bool enabled)
		{
			auto it = std::find(mDisabledProcessGroups.begin(), mDisabledProcessGroups.end(), group_id);

			if (enabled && (it != mDisabledProcessGroups.end()))
			{
				mDisabledProcessGroups.erase(it);
			}
			else if (!enabled && (it == mDisabledProcessGroups.end()))
			{
				mDisabledProcessGroups.push_back(group_id);
			}
		}

		inline bool GetProcessGroupEnabled(size_t group_id) const
		{
			return std::find(mDisabledProcessGroups.begin(), mDisabledProcessGroups.end(), group_id) == mDisabledProcessGroups.end();
		}

		void Process(double timeSec) override
		{
			auto start_time = std::chrono::high_resolution_clock::now();

			mProcessing = true;
			memset(&mMetrics, 0, sizeof(Metrics));

			// Update Entities
			ExecuteQueuedEntityActions();
			std::chrono::duration<double> delta_time = std::chrono::high_resolution_clock::now() - start_time;
			mMetrics.EntityUpdateTime = delta_time.count();

			// Update components
			start_time = std::chrono::high_resolution_clock::now();
			std::sort(mPendingComponentActions.begin(), mPendingComponentActions.end(), [](const ComponentAction& lhs, const ComponentAction& rhs)
			{
				return (lhs.index < rhs.index) || ((lhs.index == rhs.index) && (lhs.owner.Index < rhs.owner.Index)) ||
					((lhs.index == rhs.index) && (lhs.owner.Index == rhs.owner.Index) && (lhs.owner.Guid < rhs.owner.Guid));
			});
			foreach_tuple_element(mComponents, AddPendingComponents(this));
			mPendingComponentActions.clear();
			memset(mComponentCountDelta, 0, sizeof(mComponentCountDelta));
			delta_time = std::chrono::high_resolution_clock::now() - start_time;
			mMetrics.ComponentUpdateTime = delta_time.count();

			// Execute processes
			start_time = std::chrono::high_resolution_clock::now();
			for (auto& procgroup : mProcessGroups)
			{
				for (auto& procdata : procgroup)
				{
					if (procdata.Enabled && GetProcessGroupEnabled(procdata.Process->GetProcessGroupId()))
						procdata.Process->Execute(timeSec);
				}

				memset(mAuthorityExists, 0, sizeof(mAuthorityExists));
			}

			delta_time = std::chrono::high_resolution_clock::now() - start_time;
			mMetrics.ProcessExecutionTime = delta_time.count();

			// Handle post process events
			start_time = std::chrono::high_resolution_clock::now();
			// TODO: Publish the event library
			delta_time = std::chrono::high_resolution_clock::now() - start_time;
			mMetrics.EventHandlingTime = delta_time.count();

			// Housekeeping
			start_time = std::chrono::high_resolution_clock::now();
			foreach_tuple_element(mComponents, SwapBuffers(this));

			for (auto& entity : mEntities)
				memcpy(entity.ComponentCount, entity.InternalComponentCount, sizeof(entity.InternalComponentCount));

			mProcessing = false;
			delta_time = std::chrono::high_resolution_clock::now() - start_time;
			mMetrics.TotalProcessTime = delta_time.count();

			// Handle post swap events
			start_time = std::chrono::high_resolution_clock::now();
			// TODO: Publish the event library
			delta_time = std::chrono::high_resolution_clock::now() - start_time;
			mMetrics.EventHandlingTime += delta_time.count();

			mMetrics.TotalProcessTime += mMetrics.ComponentUpdateTime + mMetrics.EntityUpdateTime +
				mMetrics.ProcessExecutionTime + mMetrics.EventHandlingTime;
		}

		template <typename AuthSet, typename OptionSet, typename RequiredSet>
		struct ComponentIterator {
		private:
			template<bool editable>
			inline typename std::enable_if<editable, bool>::type GetNumComponents(const entity_type& ent, std::size_t component_index)
			{
				return ent.InternalComponentCount[component_index];
			}

			template<bool editable>
			inline typename std::enable_if<!editable, bool>::type GetNumComponents(const entity_type& ent, std::size_t component_index)
			{
				return ent.ComponentCount[component_index];
			}

			inline bool CheckComponentPresence(const entity_type& ent, std::size_t component_index)
			{
				return (ent.ComponentCount[component_index] > 0);
			}

			template <typename TypeSeq>
			inline typename std::enable_if<TypeSeq::is_last, bool>::type HasComponentsImpl(const entity_type& ent)
			{
				return CheckComponentPresence(ent, ComponentsTypeTuple::index_of<typename TypeSeq::head>::value);
			}

			template <typename TypeSeq>
			inline typename std::enable_if<!TypeSeq::is_last, bool>::type HasComponentsImpl(const entity_type& ent)
			{
				return CheckComponentPresence(ent, ComponentsTypeTuple::index_of<typename TypeSeq::head>::value) && 
					HasComponentsImpl<TypeSeq::tail>(ent);
			}

			template <typename CompSet>
			inline typename std::enable_if<CompSet::count != 0, bool>::type HasComponents(const entity_type& ent)
			{
				return HasComponentsImpl<CompSet::as_type_seq>(ent);
			}

			template <typename CompSet>
			inline typename std::enable_if<CompSet::count == 0, bool> ::type HasComponents(const entity_type& ent)
			{
				return true;
			}

			template <typename TypeSeq, bool editable>
			inline typename std::enable_if<TypeSeq::is_last, bool>::type HasAnyComponentsImpl(const entity_type& ent)
			{
				return (GetNumComponents<editable>(ent, ComponentsTypeTuple::index_of<typename TypeSeq::head>::value) > 0);
			}

			template <typename TypeSeq, bool editable>
			inline typename std::enable_if<!TypeSeq::is_last, bool>::type HasAnyComponentsImpl(const entity_type& ent)
			{
				return (GetNumComponents<editable>(ent, ComponentsTypeTuple::index_of<typename TypeSeq::head>::value) > 0) ||
					HasAnyComponentsImpl<TypeSeq::tail, editable>(ent);
			}

			template <typename CompSet, bool editable>
			inline typename std::enable_if<CompSet::count != 0, bool>::type HasAnyComponents(const entity_type& ent)
			{
				return HasAnyComponentsImpl<CompSet::as_type_seq, editable>(ent);
			}

			template <typename CompSet, bool editable>
			inline typename std::enable_if<CompSet::count == 0, bool>::type HasAnyComponents(const entity_type& ent)
			{
				return true;
			}
		protected:
			static const std::size_t TotalComponentCount = AuthSet::count + RequiredSet::count + OptionSet::count * 2;

			World* mOwner;
			size_t mCurEntityIndex = kInvalidEntityIndex;
			size_t mEntitySkipCount = 0;
			bool mOutdatedIndex = true;
			bool mInitialState = true;
			int mCurComponentIndices[TotalComponentCount]; // Contains, in order: Required, Auth, Optionals
		public:
			ComponentIterator(World* e) : mOwner(e)
			{
				memset(mCurComponentIndices, 0, sizeof(mCurComponentIndices));
			}

			virtual ~ComponentIterator()
			{
			}

			bool SeekTo(entity_type entity)
			{
				mOwner->mEntities[entity.Index]
			}

			bool Advance()
			{
				entity_type cent;

				do
				{
					if (mCurEntityIndex == kInvalidEntityIndex)
					{
						if (!mInitialState)
						{
							return false;
						}
						else
						{
							mCurEntityIndex = 0;
							mInitialState = false;
						}
					}
					else
						mCurEntityIndex++;

					if (mCurEntityIndex >= mOwner->mEntities.size())
						break;

					mEntitySkipCount++;
					cent = mOwner->mEntities[mCurEntityIndex];
				} while ((cent.Guid == kInvalidEntityGuid) || !HasComponents<RequiredSet>(cent) || !HasComponents<AuthSet>(cent));

				mOutdatedIndex = true;
				return mCurEntityIndex != mOwner->mEntities.size();
			}

			bool Advance(size_t count)
			{
				if (count > 0)
				{
					size_t advanced = 0;

					while ((count != advanced) && Advance())
						advanced++;
				}

				mOutdatedIndex = true;
				return mCurEntityIndex != mOwner->mEntities.size();
			}

			EntityRef GetEntityRef()
			{
				auto ent = mOwner->mEntities[mCurEntityIndex];
				return{ ent.Guid, mCurEntityIndex, mOwner, ent.UserValue };
			}

			template<typename T>
			size_t Count()
			{
				if (mCurEntityIndex == kInvalidEntityIndex)
					throw std::runtime_error("invalid iterator");

				return mOwner->mEntities[mCurEntityIndex].ComponentCount[ComponentsTypeTuple::index_of<T>::value];
			}

			template<typename T>
			size_t CountEdit()
			{
				if (mCurEntityIndex == kInvalidEntityIndex)
					throw std::runtime_error("invalid iterator");

				return mOwner->mEntities[mCurEntityIndex].InternalComponentCount[ComponentsTypeTuple::index_of<T>::value];
			}

			template<typename T>
			const T& Get(size_t index = 0)
			{
				static_assert(RequiredSet::contains<T>::value, "T must be one of the iterator's required components.");
				int compIndex = RequiredSet::index_of<T>::value;

				if (mCurEntityIndex == kInvalidEntityIndex)
					throw std::runtime_error("invalid iterator");
				if (mOutdatedIndex)
					UpdateIndices();

				const auto& container = std::get<ComponentContainer<T>>(mOwner->mComponents);
				return container.PresentBuffer[mCurComponentIndices[compIndex] + index];
			}

			template<typename T>
			T const* GetOptional(size_t index = 0)
			{
				static_assert(OptionSet::contains<T>::value, "T must be one of the iterator's optional components.");
				int compIndex = RequiredSet::count + AuthSet::count + OptionSet::index_of<T>::value;

				if (mCurEntityIndex == kInvalidEntityIndex)
					throw std::runtime_error("invalid iterator");
				if (mOutdatedIndex)
					UpdateIndices();

				const auto& container = std::get<ComponentContainer<T>>(mOwner->mComponents);
				if ((mCurComponentIndices[compIndex] + index) >= container.PresentBuffer.size())
					return nullptr;
				else
				{
					auto* comp = &container.PresentBuffer[mCurComponentIndices[compIndex] + index];

					if (comp->OwnerIndex != mCurEntityIndex)
						return nullptr;
					else
						return comp;
				}
			}

			template<typename T>
			T* Edit(size_t index = 0)
			{
				static_assert(AuthSet::contains<T>::value, "T must be one of the iterator's editable components.");
				int compIndex = RequiredSet::count + AuthSet::index_of<T>::value;

				if (mCurEntityIndex == kInvalidEntityIndex)
					throw std::runtime_error("invalid iterator");
				if (mOutdatedIndex)
					UpdateIndices();

				auto& container = std::get<ComponentContainer<T>>(mOwner->mComponents);
				return &container.FutureBuffer[mCurComponentIndices[compIndex] + index];
			}

			template<typename T>
			T* EditOptional(size_t index = 0)
			{
				static_assert(OptionSet::contains<T>::value, "T must be one of the iterator's optional components.");
				int compIndex = RequiredSet::count + AuthSet::count + OptionSet::count + OptionSet::index_of<T>::value;

				if (mCurEntityIndex == kInvalidEntityIndex)
					throw std::runtime_error("invalid iterator");
				if (mOutdatedIndex)
					UpdateIndices();

				auto& container = std::get<ComponentContainer<T>>(mOwner->mComponents);
				if ((mCurComponentIndices[compIndex] + index) >= container.FutureBuffer.size())
					return nullptr;
				else
				{
					auto* comp = &container.FutureBuffer[mCurComponentIndices[compIndex] + index];

					if (comp->OwnerIndex != mCurEntityIndex)
						return nullptr;
					else
						return comp;
				}
			}
		protected:
			template <typename TypeSeqContainer, typename ComponentType>
			inline void UpdateIndicesImpl(std::size_t offset, bool is_edit)
			{
				int compIndex = offset + TypeSeqContainer::index_of<ComponentType>::value;
				const auto& container = is_edit ? std::get<ComponentContainer<ComponentType>>(mOwner->mComponents).FutureBuffer :
					std::get<ComponentContainer<ComponentType>>(mOwner->mComponents).PresentBuffer;

				// Check if the next component's near the previous one
				// This provides a speedup since we don't have to resort to binary search for small deltas
				if (mEntitySkipCount < 5)
				{
					for (size_t n = 0; n <= mEntitySkipCount; n++)
					{
						size_t cidx = mCurComponentIndices[compIndex] + n;
						if (container.size() <= cidx)
							break;
						else
						{
							if (container[cidx].OwnerIndex == mCurEntityIndex)
							{
								mCurComponentIndices[compIndex] = cidx;
								return;
							}
						}
					}
				}

				auto it = mOwner->FindFirstComponentBelongingToEntity(container, mOwner->mEntities[mCurEntityIndex]);
				mCurComponentIndices[compIndex] = std::distance(container.begin(), it);
			}

			template <typename TypeSeqContainer, typename TypeSeq>
			inline typename std::enable_if<TypeSeq::is_last, void>::type DoIndicesUpdateImpl(std::size_t offset, bool is_edit)
			{
				UpdateIndicesImpl<TypeSeqContainer, TypeSeq::head>(offset, is_edit);
			}

			template <typename TypeSeqContainer, typename TypeSeq>
			inline typename std::enable_if<!TypeSeq::is_last, void>::type DoIndicesUpdateImpl(std::size_t offset, bool is_edit)
			{
				UpdateIndicesImpl<TypeSeqContainer, TypeSeq::head>(offset, is_edit);
				DoIndicesUpdateImpl<TypeSeqContainer, TypeSeq::tail>(offset, is_edit);
			}

			template <typename TypeSeqContainer>
			inline typename std::enable_if<TypeSeqContainer::count != 0, void>::type DoIndicesUpdate(std::size_t offset, bool is_edit)
			{
				DoIndicesUpdateImpl<TypeSeqContainer, TypeSeqContainer::as_type_seq>(offset, is_edit);
			}

			template <typename TypeSeqContainer>
			inline typename std::enable_if<TypeSeqContainer::count == 0, void>::type DoIndicesUpdate(std::size_t offset, bool is_edit)
			{
			}

			void UpdateIndices()
			{
				assert(mCurEntityIndex != kInvalidEntityIndex);

				DoIndicesUpdate<RequiredSet>(0, false);
				DoIndicesUpdate<AuthSet>(RequiredSet::count, true);

				if (HasAnyComponents<OptionSet, false>(mOwner->mEntities[mCurEntityIndex]))
					DoIndicesUpdate<OptionSet>(RequiredSet::count + AuthSet::count, false);
				if (HasAnyComponents<OptionSet, true>(mOwner->mEntities[mCurEntityIndex]))
					DoIndicesUpdate<OptionSet>(RequiredSet::count + AuthSet::count + OptionSet::count, true);

				mOutdatedIndex = false;
				mEntitySkipCount = 0;
			}
		};

		EntityRef PerformMigration(World<ComponentTypes...>* destination, EntityRef migrated_entity, std::vector<EntityRef>* inherited_migrations)
		{
			enforceRet(migrated_entity.IsValid(), EntityRef::Invalid);
			enforceRet(!mProcessing, EntityRef::Invalid);
			enforceRet(!destination->mProcessing, EntityRef::Invalid);

			// Migrate the entity
			entity_type& source_entity = mEntities[migrated_entity.Index];
			entity_type ent = source_entity;

			enforceRet(source_entity.Guid != kInvalidEntityGuid, EntityRef::Invalid);

			if (!destination->AvailableEntities.empty())
			{
				auto target_ent = destination->AvailableEntities.back();
				destination->AvailableEntities.pop_back();
				destination->mEntities[target_ent.Index] = ent;
				destination->mEntitySearchListValid = false;
			}
			else
			{
				ent.Index = destination->mEntities.size();
				memset(ent.ComponentCount, 0, sizeof(ent.ComponentCount));
				memset(ent.InternalComponentCount, 0, sizeof(ent.InternalComponentCount));
				destination->mEntities.push_back(ent);
				destination->mEntitySearchListValid = false;
			}

			source_entity.Guid = EntityRef::Invalid.Guid;

			// Migrate the components
			foreach_tuple_element(mComponents, QueueRemoval(this, source_entity, false));
			foreach_tuple_element(mComponents, ComponentMigrator(this, destination, source_entity, ent, inherited_migrations));

			// Queue the source entity removal
			memset(source_entity.ComponentCount, 0, sizeof(source_entity.ComponentCount));
			memset(source_entity.InternalComponentCount, 0, sizeof(source_entity.InternalComponentCount));

			AvailableEntities.push_back(source_entity);
			memset(&source_entity, 0, sizeof(entity_type));
			source_entity.Guid = kInvalidEntityGuid;
			source_entity.Index = kInvalidEntityIndex;

			Log::Trace(CODELOC, "Migrated entity ", ent.Guid, " from world ", (int) this, " to ", (int) destination);
			return{ ent.Guid, ent.Index, destination, ent.UserValue };
		}

		// Component iterators
		template<typename MaybeAuthority, typename MaybeOptional, typename... T>
		typename std::enable_if<is_type_tuple<MaybeAuthority>::value && is_type_tuple<MaybeOptional>::value,
			ComponentIterator<MaybeAuthority, MaybeOptional, ComponentSet<T...>>>::type GetComponentIterator(void* authority_source = nullptr)
		{
			if (!mProcessing)
				throw InvalidProcessStateException();

			static_assert((MaybeAuthority::count == 0) || MaybeAuthority::is_subset_of<T...>::value, "Authority components must be selected.");
			static_assert(!MaybeOptional::is_subset_of<T...>::value, "Optional components must not be selected.");
			static_assert(MaybeOptional::is_subset_of<ComponentTypes...>::value, "Optional components must all be present in the container.");
			static_assert(type_tuple<T...>::is_subset_of<ComponentTypes...>::value, "Selected components must all be present in the container.");

			MaybeAuthority::for_each(RequestAuthority(this, authority_source));
			return ComponentIterator<MaybeAuthority, MaybeOptional, ComponentSet<T...>>(this);
		}

		template<typename MaybeAuthority, typename MaybeOptional, typename... T>
		typename std::enable_if<is_type_tuple<MaybeAuthority>::value && is_type_tuple<MaybeOptional>::value,
			ComponentIterator<MaybeAuthority, MaybeOptional, ComponentSet<T...>>>::type GetComponentIterator(const std::initializer_list<void*>& authority_source)
		{
			if (!mProcessing)
				throw InvalidProcessStateException();

			static_assert((MaybeAuthority::count == 0) || MaybeAuthority::is_subset_of<T...>::value, "Authority components must be selected.");
			static_assert(!MaybeOptional::is_subset_of<T...>::value, "Optional components must not be selected.");
			static_assert(MaybeOptional::is_subset_of<ComponentTypes...>::value, "Optional components must all be present in the container.");
			static_assert(type_tuple<T...>::is_subset_of<ComponentTypes...>::value, "Selected components must all be present in the container.");
			static_assert((MaybeAuthority::size + MaybeOptional::size + sizeof...(T)) == authority_source.size(), "Authority source size must equal the number of components");
			//static_assert((MaybeAuthority::size) == authority_source.size(), "Authority source size must equal the number of components");
			// VS is being dumb and the above doesn't work because ???
			verify((MaybeAuthority::size) == authority_source.size());

			MaybeAuthority::for_each(RequestAuthority(this, authority_source));
			return ComponentIterator<MaybeAuthority, MaybeOptional, ComponentSet<T...>>(this);
		}

		template<typename MaybeAuthority, typename MaybeOptional, typename... T>
		typename std::enable_if<is_type_tuple<MaybeAuthority>::value && !is_type_tuple<MaybeOptional>::value,
			ComponentIterator<MaybeAuthority, OptionalSet<>, ComponentSet<MaybeOptional, T...>>>::type GetComponentIterator(void* authority_source = nullptr)
		{
			if (!mProcessing)
				throw InvalidProcessStateException();

			static_assert(MaybeAuthority::is_subset_of<MaybeOptional, T...>::value, "Authority components must be selected.");
			static_assert(type_tuple<MaybeOptional, T...>::is_subset_of<ComponentTypes...>::value, "Selected components must all be present in the container.");

			MaybeAuthority::for_each(RequestAuthority(this, authority_source));
			return ComponentIterator<MaybeAuthority, OptionalSet<>, ComponentSet<MaybeOptional, T...>>(this);
		}

		template<typename MaybeAuthority, typename MaybeOptional, typename... T>
		typename std::enable_if<is_type_tuple<MaybeAuthority>::value && !is_type_tuple<MaybeOptional>::value,
			ComponentIterator<MaybeAuthority, OptionalSet<>, ComponentSet<MaybeOptional, T...>>>::type GetComponentIterator(const std::initializer_list<void*>& authority_source)
		{
			if (!mProcessing)
				throw InvalidProcessStateException();

			static_assert(MaybeAuthority::is_subset_of<MaybeOptional, T...>::value, "Authority components must be selected.");
			static_assert(type_tuple<MaybeOptional, T...>::is_subset_of<ComponentTypes...>::value, "Selected components must all be present in the container.");
			//static_assert((MaybeAuthority::size) == authority_source.size(), "Authority source size must equal the number of components");
			// VS is being dumb and the above doesn't work because ???
			verify((MaybeAuthority::size) == authority_source.size());

			MaybeAuthority::for_each(RequestMultiAuthority(this, authority_source));
			return ComponentIterator<MaybeAuthority, OptionalSet<>, ComponentSet<MaybeOptional, T...>>(this);
		}

		template<typename MaybeOptional, typename... T>
		typename std::enable_if <is_type_tuple<MaybeOptional>::value,
			ComponentIterator<AuthoritySet<>, MaybeOptional, ComponentSet<T...>> > ::type GetReadComponentIterator()
		{
			static_assert(!MaybeOptional::is_subset_of<T...>::value, "Optional components must not be selected.");
			static_assert(type_tuple<T...>::is_subset_of<ComponentTypes...>::value, "Selected components must all be present in the container."); 
			static_assert(MaybeOptional::is_subset_of<ComponentTypes...>::value, "Optional components must all be present in the container.");

			return ComponentIterator<AuthoritySet<>, MaybeOptional, ComponentSet<T...>>(this);
		}

		template<typename MaybeOptional, typename... T>
		typename std::enable_if <!is_type_tuple<MaybeOptional>::value,
			ComponentIterator<AuthoritySet<>, OptionalSet<>, ComponentSet<MaybeOptional, T...>> > ::type GetReadComponentIterator()
		{
			static_assert(type_tuple<MaybeOptional, T...>::is_subset_of<ComponentTypes...>::value, "Selected components must all be present in the container.");

			return ComponentIterator<AuthoritySet<>, OptionalSet<>, ComponentSet<MaybeOptional, T...>>(this);
		}

		void* GetUserPointer() const override
		{
			return mUserPtr;
		}

		void SetUserPointer(void* ptr) override
		{
			mUserPtr = ptr;
		}
	private:
		// Workaround for an ICE
		template<typename T>
		inline void AddComponentImpl(size_t entityGuid, int entityIndex, int uservalue, size_t dist, T data)
		{
			for (auto& comp : mPendingComponentActions)
			{
				if (comp.data.which() == sizeof...(ComponentTypes))
				{
					if (comp.data.get<detail::RemovalAction>().id == T::Id())
					{
						if (dist <= comp.index)
							comp.index++;
					}
				}
				else if (comp.data.which() == ComponentsTypeTuple::index_of<T>::value)
				{
					if (dist <= comp.index)
						comp.index++;
				}
			}
		}

		template<typename T, typename ContainerType>
		T* GetComponentInContainer(EntityRef ent, ContainerType& container, unsigned char idx = 0)
		{
			auto* entp = FindEntityPtr(ent.Guid);
			if (!entp)
				return nullptr;

			if (idx >= entp->ComponentCount[ComponentsTypeTuple::index_of<T>::value])
			{
				return nullptr;
			}
			else
			{
				auto it = FindFirstComponentBelongingToEntity(container, *entp);

				if (it != container.end())
					return &*(it + idx);
				else
					return nullptr;
			}
		}

		template<typename T>
		void* GetRawComponentImpl(EntityRef ent, size_t componentId, unsigned char idx = 0)
		{
			if (T::Id() == componentId)
				return GetComponent<T>(ent, idx);
			else
				return nullptr;
		}

		template<typename T, typename U, typename... V>
		void* GetRawComponentImpl(EntityRef ent, size_t componentId, unsigned char idx = 0)
		{
			if (T::Id() == componentId)
				return GetComponent<T>(ent, idx);
			else
				return GetRawComponentImpl<U, V...>(ent, componentId, idx);
		}

		template<typename T>
		unsigned char CountRawComponentsImpl(EntityRef ent, size_t componentId) const
		{
			if (T::Id() == componentId)
				return CountComponents<T>(ent);
			else
				return 0;
		}

		template<typename T, typename U, typename... V>
		unsigned char CountRawComponentsImpl(EntityRef ent, size_t componentId) const
		{
			if (T::Id() == componentId)
				return CountComponents<T>(ent);
			else
				return CountRawComponentsImpl<U, V...>(ent, componentId);
		}

		template<typename T>
		void* GetRawFutureComponentImpl(EntityRef ent, size_t componentId, unsigned char idx = 0)
		{
			if (T::Id() == componentId)
				return GetFutureComponent<T>(ent, idx);
			else
				return nullptr;
		}

		template<typename T, typename U, typename... V>
		void* GetRawFutureComponentImpl(EntityRef ent, size_t componentId, unsigned char idx = 0)
		{
			if (T::Id() == componentId)
				return GetFutureComponent<T>(ent, idx);
			else
				return GetRawFutureComponentImpl<U, V...>(ent, componentId, idx);
		}

		template<typename T>
		unsigned char CountRawFutureComponentsImpl(EntityRef ent, size_t componentId) const
		{
			if (T::Id() == componentId)
				return CountInternalComponents<T>(ent);
			else
				return 0;
		}

		template<typename T, typename U, typename... V>
		unsigned char CountRawFutureComponentsImpl(EntityRef ent, size_t componentId) const
		{
			if (T::Id() == componentId)
				return CountInternalComponents<T>(ent);
			else
				return CountRawFutureComponentsImpl<U, V...>(ent, componentId);
		}

		/// Attempts to find an entity that has the specified GUID in the
		/// current entities vector by using binary search.
		const entity_type* FindEntityPtr(size_t guid) const
		{
			auto ent = FindFirstEntity(guid);

			if (ent == mEntitySearchList.end())
				return nullptr;
			else if (ent->Guid != guid)
				return nullptr;
			else
				return &mEntities[ent->Index];
		}

		/// Attempts to find an entity that has the specified GUID in the
		/// current entities vector by using binary search.
		entity_type* FindEntityPtr(size_t guid)
		{
			auto ent = FindFirstEntity(guid);

			if (ent == mEntitySearchList.end())
				return nullptr;
			else if (ent->Guid != guid)
				return nullptr;
			else
				return &mEntities[ent->Index];
		}

		/// Searches for an entity by using binary search to search the current entities vector. 
		/// If the entity's not found it then attempts to search on the pending additions.
		const entity_type* FindEntityPtrExt(size_t guid) const
		{
			if (const entity_type* ret = FindEntityPtr(guid))
			{
				return ret;
			}
			else
			{
				for (auto& entity : mPendingEntityAdditions)
				{
					if (entity.Guid == guid)
						return &entity;
				}

				return nullptr;
			}
		}

		/// Searches for an entity by using binary search to search the current entities vector. 
		/// If the entity's not found it then attempts to search on the pending additions.
		entity_type* FindEntityPtrExt(size_t guid)
		{
			if (entity_type* ret = FindEntityPtr(guid))
			{
				return ret;
			}
			else
			{
				for (auto& entity : mPendingEntityAdditions)
				{
					if (entity.Guid == guid)
						return &entity;
				}

				return nullptr;
			}
		}

		void ExecuteQueuedEntityActions()
		{
			for (auto&& remove : mPendingEntityRemovals)
			{
				entity_type* ent = FindEntityPtr(remove.Guid);

				if (ent)
				{
					for (auto it = mEntitySearchList.begin(); it != mEntitySearchList.end(); it++)
					{
						if (it->Guid == remove.Guid)
						{
							it = mEntitySearchList.erase(it);
							if (it == mEntitySearchList.end())
								break;
						}
					}

					foreach_tuple_element(mComponents, QueueRemoval(this, *ent));

					memset(ent->ComponentCount, 0, sizeof(ent->ComponentCount));
					memset(ent->InternalComponentCount, 0, sizeof(ent->InternalComponentCount));

					AvailableEntities.push_back(*ent);
					memset(ent, 0, sizeof(entity_type));
					ent->Guid = kInvalidEntityGuid;
					ent->Index = kInvalidEntityIndex;
				}
			}

			for (auto&& add : mPendingEntityAdditions)
			{
				if (add.Index == kInvalidEntityIndex)
				{
					add.Index = mEntities.size();
					mEntities.push_back(add);
				}
				else
				{
					mEntities[add.Index] = add;
				}
			}

			mPendingEntityRemovals.clear();
			mPendingEntityAdditions.clear();
			mEntitySearchListValid = false;
		}

		auto FindFirstEntity(size_t guid) const -> decltype(mEntitySearchList.begin())
		{
			if (!mEntitySearchListValid)
			{
				struct EntityGuidSortComparator {
					bool operator()(const entity_type& lhs, const entity_type& rhs) const
					{
						return lhs.Guid < rhs.Guid;
					}
				};

				mEntitySearchList.resize(mEntities.size());
				memcpy(mEntitySearchList.data(), mEntities.data(), mEntities.size() * sizeof(entity_type));
				std::sort(mEntitySearchList.begin(), mEntitySearchList.end(), EntityGuidSortComparator());
				mEntitySearchList.erase(std::remove_if(mEntitySearchList.begin(), mEntitySearchList.end(), [](const entity_type& ent)
				{
					return ent.Guid == kInvalidEntityGuid;
				}), mEntitySearchList.end());

				mEntitySearchListValid = true;
			}

			std::vector<entity_type>::iterator it;
			auto first = mEntitySearchList.begin();
			auto last = mEntitySearchList.end();
			size_t count, step;
			count = std::distance(first, last);

			while (count > 0)
			{
				it = first;
				step = count / 2;
				std::advance(it, step);
				if (it->Guid < guid)
				{
					first = ++it;
					count -= step + 1;
				}
				else count = step;
			}

			return first;
		}

		template<typename T>
		auto FindFirstComponentBelongingToEntity(T& container, const entity_type& value) const -> decltype(container.begin())
		{
			auto first = container.begin();
			auto last = container.end();
			auto it = first;

			size_t count, step;
			count = std::distance(first, last);

			while (count > 0)
			{
				it = first;
				step = count / 2;
				std::advance(it, step);

				if (it->OwnerIndex < value.Index)
				{
					first = ++it;
					count -= step + 1;
				}
				else
					count = step;
			}

			return first;
		}

		template<typename T>
		auto FindLastComponentBelongingToEntity(T& container, const entity_type& value) const -> decltype(container.begin())
		{
			auto first = container.begin();
			auto last = container.end();
			auto it = first;

			size_t count, step;
			count = std::distance(first, last);

			while (count > 0)
			{
				it = first;
				step = count / 2;
				std::advance(it, step);

				if (!(value.Index < it->OwnerIndex))
				{
					first = ++it;
					count -= step + 1;
				}
				else
					count = step;
			}

			return first;
		}

		// Tuple Iteration Functors
		class QueueRemoval {
		private:
			World* mOwner;
			entity_type mTargetEntity;
			bool mDestructive;
		public:
			QueueRemoval(World* ecs, entity_type entity, bool destructive = true)
				: mOwner(ecs), mTargetEntity(entity), mDestructive(destructive)
			{
			}

			template<typename T>
			void operator()(T&& v)
			{
				using CompTypeD = typename std::decay<T>::type::value_type;
				auto& srcBuff = v.PresentBuffer;

				auto start = mOwner->FindFirstComponentBelongingToEntity(srcBuff, mTargetEntity);

				if (start == srcBuff.end())
				{
				}
				else
				{
					auto end = mOwner->FindLastComponentBelongingToEntity(srcBuff, mTargetEntity);

					ComponentAction removalAction = {
						std::distance(srcBuff.begin(), start),
						std::distance(start, end),
						mTargetEntity,
						detail::RemovalAction{ CompTypeD::Id() },
						mDestructive,
					};

					mOwner->mComponentCountDelta[ComponentsTypeTuple::index_of<CompTypeD>::value] -= removalAction.removeLength;
					mOwner->mPendingComponentActions.push_back(removalAction);
				}
			}
		};

		class ComponentMigrator {
		private:
			World* mSource;
			World* mDestination;
			entity_type mSourceEntity;
			EntityRef mDestinationEntity;
			std::vector<EntityRef>* mInheritedMigrations;
		public:
			ComponentMigrator(World* source, World* destination, entity_type source_entity, entity_type destination_entity, std::vector<EntityRef>* inherit_migrations)
				: mSource(source), mDestination(destination), mSourceEntity(source_entity), mInheritedMigrations(inherit_migrations)
			{
				mDestinationEntity = EntityRef{ destination_entity.Guid, destination_entity.Index, destination, destination_entity.UserValue };
			}

			// Perform custom migration handling
			template<typename T>
			inline typename std::enable_if<T::HasCustomMigrationHandling == true>::type TriggerOnMigrate(T* component)
			{
				component->OnMigrate(mDestinationEntity, mInheritedMigrations);
			}

			template<typename T>
			inline typename std::enable_if<T::HasCustomMigrationHandling == false>::type TriggerOnMigrate(T* component)
			{
			}

			template<typename T>
			void operator()(T&& v)
			{
				using CompTypeD = typename std::decay<T>::type::value_type;
				auto& source_buffer = v.PresentBuffer;
				auto& destination_buffer = std::get<ComponentContainer<CompTypeD>>(mDestination->mComponents).PresentBuffer;

				auto start = mSource->FindFirstComponentBelongingToEntity(source_buffer, mSourceEntity);

				if (start != source_buffer.end())
				{
					auto end = mSource->FindLastComponentBelongingToEntity(source_buffer, mSourceEntity);

					for (auto it = start; it != end; it++)
					{
						TriggerOnMigrate<CompTypeD>(&(*it));
						if (!mDestination->AddComponent(mDestinationEntity, *it))
							Log::Error(CODELOC, "Could not migrate component component of type ", CompTypeD::Id(), " for entity ", mSourceEntity.Guid);
					}
				}
			}
		};
		
		class ComponentMigrationNotifier {
		private:
			World* mWorld;
			entity_type* mEntity;
		public:
			ComponentMigrationNotifier(World* world, entity_type* entity)
				: mWorld(world), mEntity(entity)
			{
			}

			// Perform custom migration handling
			template<typename T>
			inline typename std::enable_if<T::HasCustomMigrationHandling == true>::type TriggerOnMigrateComplete(T* component)
			{
				component->OnMigrateComplete(EntityRef{ mEntity->Guid, mEntity->Index, mWorld, mEntity->UserValue });
			}

			template<typename T>
			inline typename std::enable_if<T::HasCustomMigrationHandling == false>::type TriggerOnMigrateComplete(T* component)
			{
			}

			template<typename T>
			void operator()(T&& v)
			{
				using CompTypeD = typename std::decay<T>::type::value_type;
				auto& source_buffer = v.PresentBuffer;
				auto start = mWorld->FindFirstComponentBelongingToEntity(source_buffer, *mEntity);

				if (start != source_buffer.end())
				{
					auto end = mWorld->FindLastComponentBelongingToEntity(source_buffer, *mEntity);

					for (auto it = start; it != end; it++)
						TriggerOnMigrateComplete<CompTypeD>(&(*it));
				}
			}
		};
		class SwapBuffers {
		private:
			World* mOwner;
		public:
			SwapBuffers(World* owner) : mOwner(owner)
			{
			}

			template<typename T>
			void operator()(T&& v)
			{
				std::swap(v.PresentBuffer, v.FutureBuffer);
			}
		};

		class AddPendingComponents {
		private:
			World* mOwner;
		public:
			AddPendingComponents(World* owner) : mOwner(owner)
			{
			}

			template<typename T>
			void operator()(T&& v)
			{
				using CompTypeD = typename std::decay<T>::type::value_type;
				auto start_time = std::chrono::high_resolution_clock::now();
				auto& srcBuff = v.PresentBuffer;
				auto& targetBuff = v.FutureBuffer;
				auto& compMetrics = mOwner->mMetrics.ComponentMetrics[ComponentsTypeTuple::index_of<CompTypeD>::value];
				compMetrics.TypeId = CompTypeD::Id();

				targetBuff.clear();
				targetBuff.resize(srcBuff.size() + mOwner->mComponentCountDelta[ComponentsTypeTuple::index_of<CompTypeD>::value]);
				size_t copyOrigStart = 0;
				size_t copyDestStart = 0;

				for (auto& action : mOwner->mPendingComponentActions)
				{
					if (action.data.which() == sizeof...(ComponentTypes))
					{
						if (action.data.get<detail::RemovalAction>().id == CompTypeD::Id())
						{
							if (action.destructive)
							{
								for (size_t n = 0; n < action.removeLength; n++)
									srcBuff[action.index + n].Destroy();
							}

							size_t toCopy = action.index - copyOrigStart;

							if (toCopy > 0)
							{
								memcpy(&targetBuff.data()[copyDestStart], &srcBuff.data()[copyOrigStart], sizeof(CompTypeD) * toCopy);
								copyOrigStart += toCopy;
								copyDestStart += toCopy;
							}

							entity_type* owner = mOwner->FindEntityPtrExt(action.owner.Guid);
							if (owner)
								owner->InternalComponentCount[ComponentsTypeTuple::index_of<CompTypeD>::value] -= (unsigned char) action.removeLength;

							copyOrigStart += action.removeLength;
							compMetrics.DeleteOps++;
						}
						continue;
					}

					if (action.data.which() != ComponentsTypeTuple::index_of<CompTypeD>::value)
						continue;

					entity_type* owner = mOwner->FindEntityPtrExt(action.owner.Guid);
					if (owner)
					{
						if ((action.index - copyOrigStart) > 0)
						{
							size_t toCopy = action.index - copyOrigStart;
							memcpy(&targetBuff.data()[copyDestStart], &srcBuff.data()[copyOrigStart], sizeof(CompTypeD) * toCopy);
							try
							{
								auto& dst = targetBuff[copyDestStart + toCopy];
								dst = action.data.get<CompTypeD>();
								dst.OwnerIndex = owner->Index;
								owner->InternalComponentCount[ComponentsTypeTuple::index_of<CompTypeD>::value]++;
								copyDestStart++;
							}
							catch (const std::exception& ex)
							{
								printf(ex.what());
							}

							copyOrigStart += toCopy;
							copyDestStart += toCopy;
							compMetrics.AddOps++;
						}
						else
						{
							try
							{
								auto& dst = targetBuff[copyDestStart];
								dst = action.data.get<CompTypeD>();
								dst.OwnerIndex = owner->Index;

								owner->InternalComponentCount[ComponentsTypeTuple::index_of<CompTypeD>::value]++;
								copyDestStart++;
							}
							catch (const std::exception& ex)
							{
								printf(ex.what());
							}

							compMetrics.AddOps++;
						}
					}
				}

				if (copyOrigStart < srcBuff.size())
					memcpy(&targetBuff.data()[copyDestStart], &srcBuff.data()[copyOrigStart], sizeof(CompTypeD) * (srcBuff.size() - copyOrigStart));

				std::chrono::duration<double> delta = std::chrono::high_resolution_clock::now() - start_time;
				compMetrics.UpdateTime = delta.count();
			}
		};

		class RequestAuthority {
		private:
			World* mOwner;
			void* mAuthoritySource;
		public:
			RequestAuthority(World* owner) : mOwner(owner), mAuthoritySource(nullptr)
			{
			}

			RequestAuthority(World* owner, void* authoritySource) : mOwner(owner), mAuthoritySource(authoritySource)
			{
			}

			template<typename T>
			void operator()(T* v, std::size_t type_index)
			{
				auto& authdata = mOwner->mAuthorityExists[type_tuple<ComponentTypes...>::index_of<T>::value];

				if (authdata.Requested && ((mAuthoritySource == nullptr) || (authdata.RequestSource != mAuthoritySource)))
				{
					throw AuthorityException();
				}
				else
				{
					authdata.Requested = true;
					authdata.RequestSource = mAuthoritySource;
				}
			}
		};

		class RequestMultiAuthority {
		private:
			World* mOwner;
			std::initializer_list<void*> mAuthoritySource;
			unsigned int mAuthoritySourceIndex;
		public:
			RequestMultiAuthority(World* owner, std::initializer_list<void*> authoritySource) 
				: mOwner(owner), mAuthoritySource(authoritySource), mAuthoritySourceIndex(0)
			{
			}

			template<typename T>
			void operator()(T* v, std::size_t type_index)
			{
				auto& authdata = mOwner->mAuthorityExists[type_tuple<ComponentTypes...>::index_of<T>::value];

				// Should never happen since we check authority source size earlier
				if (mAuthoritySourceIndex >= mAuthoritySource.size())
					throw MissingAuthorityException();

				void* auth_source = mAuthoritySource.begin()[mAuthoritySourceIndex];

				if (authdata.Requested && ((auth_source == nullptr) || (authdata.RequestSource != auth_source)))
				{
					throw AuthorityException();
				}
				else
				{
					authdata.Requested = true;
					authdata.RequestSource = auth_source;
				}

				mAuthoritySourceIndex++;
			}
		};

		class DestroyComponents {
		public:
			template<typename T>
			void operator()(T&& v)
			{
				for (auto& comp : v.PresentBuffer)
					comp.Destroy();
			}
		};
	};
}