/**
 * @file RingBufferBlocking.hpp
 * @brief Thread-safe blocking circular buffer implementation
 *
 * Provides a fixed-size circular buffer with blocking push/pop operations
 * using condition variables for producer-consumer synchronization.
 */

#pragma once
#include <array>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>

using std::chrono::operator""ms;

/**
 * @class RingBufferBlocking
 * @brief Thread-safe circular buffer with blocking operations
 *
 * A fixed-capacity ring buffer that blocks when:
 * - Push operations wait if buffer is full (with timeout)
 * - Pop operations wait if buffer is empty (indefinitely)
 *
 * Features:
 * - Thread-safe using mutex and condition variables
 * - Fixed compile-time capacity
 * - Blocking push with 100ms timeout
 * - Blocking pop with no timeout
 * - Clear operation to drain buffer
 *
 * @tparam T Type of elements stored in buffer
 * @tparam capacity Maximum number of elements buffer can hold
 *
 * @note Used for inter-thread data transfer in trace acquisition
 * @note Push timeout prevents deadlock in producer threads
 */
template <typename T, size_t capacity>
class RingBufferBlocking
{
   public:
	/**
	 * @brief Constructs an empty ring buffer
	 */
	explicit RingBufferBlocking() : read_idx(0), write_idx(0), size_(0) {}

	/**
	 * @brief Pushes an item into the buffer (blocking with timeout)
	 *
	 * Waits up to 100ms for space to become available. Returns false if
	 * buffer remains full after timeout.
	 *
	 * @param item Item to push into buffer
	 * @return true if item was pushed, false if timeout occurred
	 * @note Thread-safe, blocks producer if buffer is full
	 */
	bool push(const T& item)
	{
		std::unique_lock<std::mutex> lock(mutex);
		if (!cond_full.wait_for(lock, 100ms, [this]()
								{ return size_ < capacity; }))
			return false;

		buffer[write_idx] = item;
		write_idx = (write_idx + 1) % capacity;
		size_++;

		cond_empty.notify_one();
		return true;
	}

	/**
	 * @brief Pops an item from the buffer (blocking indefinitely)
	 *
	 * Waits indefinitely for an item to become available if buffer is empty.
	 *
	 * @return Item removed from buffer
	 * @note Thread-safe, blocks consumer if buffer is empty
	 * @note No timeout - will wait forever for data
	 */
	T pop()
	{
		std::unique_lock<std::mutex> lock(mutex);
		cond_empty.wait(lock, [this]()
						{ return size_ > 0; });

		T item = buffer[read_idx];
		read_idx = (read_idx + 1) % capacity;
		size_--;

		cond_full.notify_one();
		return item;
	}

	/**
	 * @brief Returns current number of elements in buffer
	 *
	 * @return Current buffer size
	 * @note Thread-safe
	 */
	size_t size()
	{
		std::unique_lock<std::mutex> lock(mutex);
		return size_;
	}

	/**
	 * @brief Clears all elements from buffer
	 *
	 * Repeatedly pops until buffer is empty. This will block if
	 * buffer is already empty.
	 *
	 * @note Thread-safe
	 * @warning Will block if called on empty buffer
	 */
	void clear()
	{
		while (size_)
			pop();
	}

   private:
	/** @brief Fixed-size array for storage */
	std::array<T, capacity> buffer;

	/** @brief Read index (consumer position) */
	size_t read_idx;

	/** @brief Write index (producer position) */
	size_t write_idx;

	/** @brief Current number of elements in buffer */
	size_t size_;

	/** @brief Mutex for thread synchronization */
	std::mutex mutex;

	/** @brief Condition variable for empty buffer (consumer waits) */
	std::condition_variable cond_empty;

	/** @brief Condition variable for full buffer (producer waits) */
	std::condition_variable cond_full;
};
