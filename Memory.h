// Single header Smart Handles("Pointers") implementation
// @2023-2024 (IHarzI) Zakhar Maslianka
#pragma once

#include <memory>
#include <utility>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>

#ifndef HARZ_MEMORY_HANDLE_STATIC_CUSTOM
#define STATIC static
#else
#define STATIC HARZ_MEMORY_HANDLE_STATIC_CUSTOM
#endif

#ifndef HARZ_MEMORY_HANDLE_INLINE_CUSTOM
#define INLINE inline
#else
#define INLINE HARZ_MEMORY_HANDLE_INLINE_CUSTOM
#endif // !HARZ_MEMORY_HANDLE_INLINE_CUSTOM

namespace harz
{
	namespace detailMemoryHandleImplementation
	{
		INLINE STATIC void zero_memory(void* block, unsigned long size);

		INLINE STATIC void set_memory(void* block, unsigned long size, int Value);
	};
}

#ifndef HARZ_NEW_IMPL
#define HARZ_NEW_IMPL
#define HARZ_NEW_IMPL_SmartPointer
template <typename T, typename ...Args>
[[nodiscard]] STATIC T* harz_new(Args&&... args);

template <typename T>
STATIC void harz_delete(T* address) noexcept;
#endif // !HARZ_NEW

#ifdef MEMORY_MULTITHREAD_SAFE
#include <mutex>
#define REFERENCE_INTERLOCK_GUARD std::lock_guard<std::mutex> InterlockGuard{harz::detailMemoryHandleImplementation::GetInterlockedMutex()};

#define HARZ_MEMORY_REF_INTRLK_GRD_DEC_U64(value) harz::detailMemoryHandleImplementation::ReferenceDecrementInterlocked(value)
#define HARZ_MEMORY_REF_INTRLK_GRD_INCR_U64(value) harz::detailMemoryHandleImplementation::ReferenceIncrementInterlocked(value)
#define HARZ_MEMORY_REF_INTRLK_GRD_GET_U64(value) harz::detailMemoryHandleImplementation::ReferenceGetValueInterlocked(value)
#define HARZ_MEMORY_REF_INTRLK_ERASE_REF(value) harz::detailMemoryHandleImplementation::RefMapEraseInterlocked(value, *harz::detailMemoryHandleImplementation::getRefMap())
#define HARZ_MEMORY_REFMAP_INTRLK_GRD_GETREF_U64(value) harz::detailMemoryHandleImplementation::RefMapGetValueInterlocked(value, *harz::detailMemoryHandleImplementation::getRefMap())
#endif

#define HARZ_MEMORY_REFMAP harz::detailMemoryHandleImplementation::getRefMap()

// define HARZ_MEMORY_REF_MAP_USE_CUSTOM with custom map type
#ifndef HARZ_MEMORY_REF_MAP_USE_CUSTOM
#include <unordered_map>
#define HARZ_MEMORY_REF_MAP_TYPE std::unordered_map
#else
#define HARZ_MEMORY_REF_MAP_TYPE HARZ_MEMORY_REF_MAP_USE_CUSTOM
#endif // !HARZ_MEMORY_REF_MAP_USE_CUSTOM

#ifndef HARZ_MEMORY_HANDLE_ASSERT_CUSTOM
#include <cassert>
#define ASSERT_MSG(cond, msg) assert(cond)
#define ASSERT(cond) assert(cond)
#define ERROR(msg) assert(false)
#else

#define ASSERT_MSG(cond, msg) HARZ_MEMORY_HANDLE_ASSERT_CUSTOM(cond, msg)
#define ASSERT(cond) HARZ_MEMORY_HANDLE_ASSERT_CUSTOM(cond)
#define ERROR(msg) ASSERT(false, msg)
#endif

#ifndef HARZ_MEMORY_HANDLE_ERROR_CUSTOM
#define ERROR(msg) assert(false)
#else
#define ERROR(msg) HARZ_MEMORY_HANDLE_ERROR_CUSTOM(msg)
#endif

// define for memory debug
#ifdef HARZ_MEMORY_DEBUG
#define HARZ_MEMORY_DEBUG_CHECK
#endif

template <typename T>
struct UniqueMemoryHandle;

template <typename T>
struct SharedMemoryHandle;

template <typename T>
struct WeakMemoryHandle;

namespace harz
{
	namespace detailMemoryHandleImplementation {
			using RefCount = unsigned long;
			using ValuePtr = void*;
			using RefMapType = HARZ_MEMORY_REF_MAP_TYPE<void*, RefCount>;

			STATIC RefMapType* getRefMap()
			{
				STATIC RefMapType RefMap{};
				return &RefMap;
			}

#ifdef MEMORY_MULTITHREAD_SAFE
			STATIC INLINE std::mutex& GetInterlockedMutex() { STATIC std::mutex InterlockedMutex{}; return InterlockedMutex; }

			STATIC INLINE RefCount* RefMapGetValueInterlocked(void* address, HARZ_MEMORY_REF_MAP_TYPE<void*, RefCount>& RefMap) {
				REFERENCE_INTERLOCK_GUARD
					if (auto RefCounterToData = RefMap.find(address); RefCounterToData != RefMap.end())
					{
						return &RefCounterToData->second;
					}
				return nullptr;
			}

			STATIC INLINE bool RefMapEraseInterlocked(void* address, HARZ_MEMORY_REF_MAP_TYPE<void*, RefCount>& RefMap) {
				REFERENCE_INTERLOCK_GUARD
					RefMap.erase(address);
				return true;
			}

			STATIC INLINE void ReferenceIncrementInterlocked(RefCount& ref) {
				REFERENCE_INTERLOCK_GUARD
					ref++;
			}

			STATIC INLINE void ReferenceDecrementInterlocked(RefCount& ref) {
				REFERENCE_INTERLOCK_GUARD
					ref--;
			}

			STATIC INLINE RefCount ReferenceGetValueInterlocked(RefCount& ref) {
				REFERENCE_INTERLOCK_GUARD
					return ref;
			}
#endif

		class MallocAllocator
		{
		public:
			using pointer = void*;
			STATIC INLINE pointer Allocate(size_t allocationsize)
			{
				pointer allocation = malloc(allocationsize);
				zero_memory(allocation, allocationsize);
				return allocation;
			};

			STATIC INLINE bool Deallocate(void* allocation)
			{
				if (allocation)
				{
					free(allocation);
					return true;
				};
				return false;
			};
		};

		STATIC INLINE void* copyMemory(void* src, void* dst, size_t size) { memcpy(dst, src, size);	return dst; };
	}
};

#ifdef HARZ_NEW_IMPL_SmartPointer
// Call deconstructor for object and release occupied memory block
template <typename T>
STATIC void harz_delete(T* address) noexcept
{
	if (address)
	{
		address->~T();
		harz::detailMemoryHandleImplementation::MallocAllocator::Deallocate(address);
	};
}

// Allocate memory block of <T> object size and call <T> constructor with arguments in-place 
template <typename T, typename ...Args>
[[nodiscard]] STATIC T* harz_new(Args&&... args)
{
	void* ptr = harz::detailMemoryHandleImplementation::MallocAllocator::Allocate(sizeof(T));
	T* ResultNew = nullptr;
	if (ptr)
		ResultNew = (new(ptr) T(std::forward<Args>(args)...)); // create object in-place
	return ResultNew;
}
#endif

namespace harz
{
	namespace detailMemoryHandleImplementation
	{
		STATIC INLINE void zero_memory(void* block, unsigned long size) { memset(block, 0, size); };

		STATIC INLINE void set_memory(void* block, unsigned long size, int Value) { memset(block, Value, size); };
	}
};

template <typename T>
struct UniqueMemoryHandle
{
public:
	template <typename ...Args>
	STATIC UniqueMemoryHandle Create(Args&&... args)
	{
		return (T*)(harz_new<T>(args...));
	}

	UniqueMemoryHandle() : Data(nullptr) {};

	UniqueMemoryHandle(const UniqueMemoryHandle&) = delete;
	UniqueMemoryHandle& operator=(const UniqueMemoryHandle&) = delete;

	UniqueMemoryHandle(UniqueMemoryHandle& OtherHandle)
	{
		AcquireDataToHandle(OtherHandle.RetrieveResourse());
	}

	UniqueMemoryHandle(UniqueMemoryHandle&& OtherHandle) noexcept
	{
		AcquireDataToHandle(OtherHandle.RetrieveResourse());
	}

	UniqueMemoryHandle& operator=(UniqueMemoryHandle&& OtherHandle) {
		if (this != &OtherHandle) {
			if (IsValid())
			{
				Release();
			}
			AcquireDataToHandle(OtherHandle.RetrieveResourse());
		}
		return *this;
	}

	UniqueMemoryHandle(T* DataToHandle)
	{
		AcquireDataToHandle(DataToHandle);
	}

	~UniqueMemoryHandle()
	{
		if (IsValid())
		{
			Release();
		};
	}

	bool IsValid() const
	{
		return Data != nullptr;
	}

	T* Get() const { return Data; };
	T* Get() { return Data; };

	const T& GetReference() const { ASSERT(IsValid()); return *Data; };
	T& GetReference() { ASSERT(IsValid()); return *Data; };

	void Release() {
		ReleaseResourseChecked();
	};

	// Deletes contaiend data(if valid) and creates new in place
	template <typename ...Args>
	void ResetNew(Args&&... args)
	{
		AcquireDataToHandle((T*)(harz_new<T>(args...)));
	}

	void Reset(T* DataToHandle)
	{
		if (Data == DataToHandle)
		{
			return;
		}

		if (!DataToHandle)
		{
			Release();
			return;
		}
		AcquireDataToHandle(DataToHandle);
	}

	void Reset(UniqueMemoryHandle<T> OtherHandle)
	{
		if (Data == OtherHandle.Get())
		{
			// Two same unique handles??
			ERROR("Resourse ownership violation");
			return;
		}

		if (!OtherHandle.IsValid())
		{
			Release();
			return;
		}
		AcquireDataToHandle(OtherHandle.RetrieveResourse());
	}

	T* RetrieveResourse()
	{
		if (!IsValid())
		{
			return nullptr;
		};

		T* ResourseToReturn = Data;

		Data = nullptr;

		return ResourseToReturn;
	};

	explicit operator bool() const {
		return IsValid();
	}

	bool operator==(T* DataPtr)
	{
		return Data == DataPtr;
	}

	T& operator*() const {
		return GetReference();
	}

	T* operator->() const {
		return Get();
	}

	T& operator*() {
		return GetReference();
	}

	T* operator->() {
		return Get();
	}

private:
	T* Data = nullptr;

	void AcquireDataToHandle(T* DataToHandle)
	{
		if (!DataToHandle)
		{
			return;
		}

		if (Data == DataToHandle)
		{
			return;
		}

		Release();
		Data = DataToHandle;
	}

	void ReleaseResourseChecked()
	{
		if (!Data)
		{
			return;
		}

		harz_delete(Data);
		Data = nullptr;
	}
};


template <typename T>
struct SharedMemoryHandle
{
public:
	template <typename ...Args>
	STATIC SharedMemoryHandle Create(Args&&... args)
	{
		return (T*)(harz_new<T>(args...));
	}

	SharedMemoryHandle() : Data(nullptr) {};

	SharedMemoryHandle(const SharedMemoryHandle& OtherHandle)
	{
		AcquireDataToHandle(OtherHandle.Data);
	}

	SharedMemoryHandle& operator=(const SharedMemoryHandle& OtherHandle)
	{
		if (this != &OtherHandle) {
			if (IsValid())
			{
				Release();
			}
			AcquireDataToHandle(OtherHandle.Get());
		}
		return *this;
	}

	SharedMemoryHandle(SharedMemoryHandle& OtherHandle)
	{
		AcquireDataToHandle(OtherHandle.Data);
	}

	SharedMemoryHandle(UniqueMemoryHandle<T>&& OtherUniqueHandle) noexcept
	{
		AcquireDataToHandle(OtherUniqueHandle.RetrieveResourse());
	}

	SharedMemoryHandle(SharedMemoryHandle&& OtherHandle) noexcept
	{
		AcquireDataToHandle(OtherHandle.Get());
		OtherHandle.Release();
	}

	SharedMemoryHandle& operator=(SharedMemoryHandle&& OtherHandle) noexcept
	{
		if (this != &OtherHandle) {
			if (IsValid())
			{
				Release();
			}
			AcquireDataToHandle(OtherHandle.Get());
			OtherHandle.Release();
		}
		return *this;
	}

	SharedMemoryHandle(T* DataToHandle)
	{
		AcquireDataToHandle(DataToHandle);
	}

	~SharedMemoryHandle()
	{
		if (IsValid())
		{
			Release();
		};
	}

	bool IsValid() const
	{
#ifdef MEMORY_MULTITHREAD_SAFE
		//
		REFERENCE_INTERLOCK_GUARD;
#endif
		return Data != nullptr && HARZ_MEMORY_REFMAP && HARZ_MEMORY_REFMAP->find(Data) != HARZ_MEMORY_REFMAP->end();
	}

	T* Get() const { return Data; };
	T* Get() { return Data; };

	const T& GetReference() const { ASSERT(IsValid()); return *Data; };
	T& GetReference() { ASSERT(IsValid()); return *Data; };

	WeakMemoryHandle<T> GetWeak() {
		return { Data };
	};

	void Release() { ReleaseResourseChecked(); };

	void Reset(T* DataToHandle)
	{
		if (Data == DataToHandle)
		{
			return;
		}

		if (IsValid())
		{
			Release();
		}
		AcquireDataToHandle(DataToHandle);
	}

	void Reset(SharedMemoryHandle<T> OtherHandle)
	{
		if (Data == OtherHandle.Get())
		{
			return;
		}

		if (IsValid())
		{
			Release();
		}
		AcquireDataToHandle(OtherHandle.Get());
	}

	explicit operator bool() const {
		return IsValid();
	}

	bool operator==(T* DataPtr)
	{
		return Data == DataPtr;
	}

	bool operator==(SharedMemoryHandle<T>& OtherHandle)
	{
		return Data == OtherHandle.Data;
	}

	bool operator!=(T* DataPtr)
	{
		return !(Data == DataPtr);
	}

	bool operator!=(SharedMemoryHandle<T>& OtherHandle)
	{
		return !(Data == OtherHandle.Data_);
	}

	T& operator*() const {
		return GetReference();
	}

	T* operator->() const {
		return Get();
	}

	T& operator*() {
		return GetReference();
	}

	T* operator->() {
		return Get();
	}

private:

	void AcquireDataToHandle(T* DataToHandle)
	{
		if (!DataToHandle)
		{
			return;
		}

		if (Data == DataToHandle)
		{
			return;
		}

		InitCheckedRefMap();

#ifdef MEMORY_MULTITHREAD_SAFE
		if (unsigned long* RefCount = HARZ_MEMORY_REFMAP_INTRLK_GRD_GETREF_U64((void*)DataToHandle))
		{
			HARZ_MEMORY_REF_INTRLK_GRD_INCR_U64(*RefCount);
		}
		else
		{
			REFERENCE_INTERLOCK_GUARD
				HARZ_MEMORY_REFMAP->insert({ (void*)DataToHandle, 1 });
		}
#else
		auto RefCount = HARZ_MEMORY_REFMAP->find((void*)DataToHandle);
		if (RefCount != HARZ_MEMORY_REFMAP->end())
		{
			RefCount->second += 1;
		}
		else
		{
			HARZ_MEMORY_REFMAP->insert({ (void*)DataToHandle, 1 });
		}
#endif
		Data = DataToHandle;
	}

	void InitCheckedRefMap()
	{
		if (!HARZ_MEMORY_REFMAP)
		{
			//
			ASSERT(HARZ_MEMORY_REFMAP);
		}
	}

	void ReleaseRefMapIfEmpty()
	{
		//
	}

	void ReleaseResourseChecked()
	{
		ASSERT(!Data || Data && HARZ_MEMORY_REFMAP);
		if (Data && HARZ_MEMORY_REFMAP)
		{
			unsigned long* Ref = nullptr;
			{
#ifdef MEMORY_MULTITHREAD_SAFE
				REFERENCE_INTERLOCK_GUARD;
#endif
				auto RefCounterToData = HARZ_MEMORY_REFMAP->find(Data);
				ASSERT(RefCounterToData != HARZ_MEMORY_REFMAP->end());
				if (RefCounterToData != HARZ_MEMORY_REFMAP->end())
					Ref = &RefCounterToData->second;
			};

			if (Ref)
			{
#ifdef MEMORY_MULTITHREAD_SAFE
				HARZ_MEMORY_REF_INTRLK_GRD_DEC_U64(*Ref);
				if (HARZ_MEMORY_REF_INTRLK_GRD_GET_U64(*Ref) < 1)
				{
					HARZ_MEMORY_REF_INTRLK_ERASE_REF(Data);
					harz_delete(Data);
					ReleaseRefMapIfEmpty();
				}
#else
				*Ref -= 1;
				if (*Ref < 1)
				{
					HARZ_MEMORY_REFMAP->erase(Data);
					harz_delete(Data);
					ReleaseRefMapIfEmpty();
				};
#endif
			};
		};
		Data = nullptr;
	}

	T* Data = nullptr;
};


template <typename T>
struct WeakMemoryHandle
{
	WeakMemoryHandle() : Data(nullptr) {};

	WeakMemoryHandle(const WeakMemoryHandle& OtherHandle)
	{
		Data = OtherHandle.Data;
	}

	WeakMemoryHandle(const SharedMemoryHandle<T>& SharedHandle)
	{
		Data = SharedHandle.Get();
	}

	WeakMemoryHandle& operator=(const WeakMemoryHandle& OtherHandle)
	{
		Data = OtherHandle.Data;
		return *this;
	}

	WeakMemoryHandle(WeakMemoryHandle& OtherHandle)
	{
		Data = OtherHandle.Data;
	}

	WeakMemoryHandle& operator=(WeakMemoryHandle&& OtherHandle)
	{
		Data = OtherHandle.Data;
		return *this;
	}

	~WeakMemoryHandle()
	{}

	bool IsValid() const { return Data != nullptr && CheckRefCount(Data); }

	T* Get() const { return Data; };

	T& GetReference() { ASSERT(IsValid()); return *Data; };

	void Reset(SharedMemoryHandle<T>& SharedHandle)
	{
		Data = SharedHandle.Get();
	}

	explicit operator bool() const {
		return IsValid();
	}

	bool operator==(T* DataPtr)
	{
		return Data == DataPtr;
	}

	bool operator==(WeakMemoryHandle<T>& OtherHandle)
	{
		return Data == OtherHandle.Data;
	}

	bool operator==(SharedMemoryHandle<T>& OtherHandle)
	{
		return Data == OtherHandle.Get();
	}

	bool operator!=(T* DataPtr)
	{
		return !(Data == DataPtr);
	}

	bool operator!=(WeakMemoryHandle<T>& OtherHandle)
	{
		return !(Data == OtherHandle.Data);
	}

	bool operator!=(SharedMemoryHandle<T>& OtherHandle)
	{
		return !(Data == OtherHandle.Data);
	}

	T& operator*() const {
		return GetReference();
	}

	T* operator->() const {
		return Get();
	}

	T& operator*() {
		return GetReference();
	}

	T* operator->() {
		return Get();
	}

private:

	bool CheckRefCount(T* Ptr) const
	{
#ifdef MEMORY_MULTITHREAD_SAFE
		REFERENCE_INTERLOCK_GUARD;
#endif // MEMORY_MULTITHREAD_SAFE

		if (HARZ_MEMORY_REFMAP &&
			HARZ_MEMORY_REFMAP->find(Ptr) != HARZ_MEMORY_REFMAP->end()
			&& HARZ_MEMORY_REFMAP->find(Ptr)->second > 0)
		{
			return true;
		}

		return false;
	}

	T* Data = nullptr;
	using ValuePtr = T*;
};

template<typename T, typename ...Args>
STATIC SharedMemoryHandle<T> MakeSharedHandle(Args&&... args)
{
	return std::move(SharedMemoryHandle<T>::Create(args...));
}

template<typename T, typename ...Args>
STATIC UniqueMemoryHandle<T> MakeUniqueHandle(Args&&... args)
{
	return std::move(UniqueMemoryHandle<T>::Create(args...));
}
