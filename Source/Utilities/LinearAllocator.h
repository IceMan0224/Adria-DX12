#pragma once


namespace adria
{
	template<Uint64 BlockSize>
	class LinearAllocator
	{
	private:
		struct MemoryBlock 
		{
			Uint8* buffer;
			Uint64 used;
			Uint64 capacity;
			MemoryBlock* next;

			MemoryBlock(Uint64 size)
				: buffer(new Uint8[size]), used(0), capacity(size), next(nullptr) 
			{}

			~MemoryBlock() 
			{
				delete[] buffer;
			}
		};
		MemoryBlock* current_block;
		MemoryBlock* first_block;
		Uint64 block_size;

	public:
		LinearAllocator()
			: current_block(nullptr), first_block(nullptr), block_size(BlockSize) {
			first_block = new MemoryBlock(block_size);
			current_block = first_block;
		}

		~LinearAllocator() 
		{
			Free();
		}
		ADRIA_NONCOPYABLE(LinearAllocator)
		LinearAllocator(LinearAllocator&& other) noexcept
			: current_block(other.current_block),
			first_block(other.first_block),
			block_size(other.block_size) 
		{
			other.current_block = nullptr;
			other.first_block = nullptr;
		}

		void* Allocate(Uint64 size, Uint64 align)
		{
			Uint64 alignment = std::max(align, Uint64(1));

			auto compute_padding = [&]()
			{
				Uintptr current = reinterpret_cast<Uintptr>(current_block->buffer + current_block->used);
				Uint64 misalignment = current % alignment;
				return misalignment == 0 ? Uint64(0) : alignment - misalignment;
			};

			Uint64 padding = compute_padding();
			if (current_block->used + padding + size > current_block->capacity)
			{
				Uint64 new_block_size = std::max(block_size, size + alignment);
				MemoryBlock* new_block = new MemoryBlock(new_block_size);
				current_block->next = new_block;
				current_block = new_block;
				padding = compute_padding();
			}

			void* result = current_block->buffer + current_block->used + padding;
			current_block->used += padding + size;
			return result;
		}

		template<typename T>
		T* Allocate(Uint64 n)
		{
			return static_cast<T*>(Allocate(n * sizeof(T), alignof(T)));
		}

		void Deallocate(void* p, Uint64 n) noexcept 
		{
		}

		void Reset() 
		{
			MemoryBlock* block = first_block;
			while (block) 
			{
				block->used = 0;
				block = block->next;
			}
			current_block = first_block;
		}
		void Free()
		{
			MemoryBlock* block = first_block;
			while (block)
			{
				MemoryBlock* next = block->next;
				delete block;
				block = next;
			}
			first_block = nullptr;
			current_block = nullptr;
		}

		template<typename U, typename... Args>
		void Construct(U* p, Args&&... args) 
		{
			new (p) U(std::forward<Args>(args)...);
		}
		template<typename U>
		void Destroy(U* p) 
		{
			p->~U();
		}
	};
}