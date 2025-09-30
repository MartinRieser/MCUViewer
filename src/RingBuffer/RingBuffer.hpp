/**
 * @file RingBuffer.hpp
 * @brief Thread-safe circular buffer for inter-thread communication
 *
 * Provides a fixed-size ring buffer with mutex-protected push/pop operations.
 * Used for passing trace data between acquisition and processing threads.
 */

#ifndef __RIGNBUFFER_HPP
#define __RIGNBUFFER_HPP

#include <array>
#include <iostream>
#include <mutex>
#include <optional>

/**
 * @class RingBuffer
 * @brief Thread-safe FIFO circular buffer
 * @tparam T Element type
 * @tparam capacity Maximum buffer capacity
 *
 * Implements a fixed-size ring buffer with:
 * - Thread-safe push/pop operations using mutex
 * - Non-blocking: returns std::nullopt when empty, false when full
 * - Constant-time operations (O(1))
 * - Zero memory allocation after construction
 */
template <typename T, size_t capacity>
class RingBuffer
{
   public:
	explicit RingBuffer() : read_idx(0), write_idx(0), size_(0) {}

	/** @brief Add item to buffer
	 *  @param item Item to add
	 *  @return true if added, false if buffer full */
	bool push(const T& item)
	{
		std::unique_lock<std::mutex> lock(mutex);
		if (size_ == capacity)
			return false;

		buffer[write_idx] = item;
		write_idx = (write_idx + 1) % capacity;
		size_++;
		return true;
	}

	/** @brief Remove and return next item
	 *  @return std::optional<T> Item if available, std::nullopt if empty */
	std::optional<T> pop()
	{
		std::unique_lock<std::mutex> lock(mutex);

		if (size_ == 0)
			return std::nullopt;

		T item = buffer[read_idx];
		read_idx = (read_idx + 1) % capacity;
		size_--;
		return item;
	}

	/** @brief Get current number of elements
	 *  @return size_t Number of elements in buffer */
	size_t size()
	{
		std::unique_lock<std::mutex> lock(mutex);
		return size_;
	}

	/** @brief Remove all elements */
	void clear()
	{
		while (size_)
			pop();
	}

   private:
	std::array<T, capacity> buffer;  ///< Fixed-size storage array
	size_t read_idx;   ///< Next read position
	size_t write_idx;  ///< Next write position
	size_t size_;      ///< Current element count
	std::mutex mutex;  ///< Protects all operations
};

#endif