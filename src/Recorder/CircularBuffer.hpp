#ifndef _CIRCULARBUFFER_HPP
#define _CIRCULARBUFFER_HPP

#include <mutex>
#include <vector>

/**
 * @brief Thread-safe circular buffer template for recorder module
 *
 * Unlike RingBuffer which blocks when full, CircularBuffer overwrites
 * oldest data when capacity is reached, making it ideal for continuous
 * recording with pre-trigger capture.
 *
 * @tparam T Element type to store
 */
template <typename T>
class CircularBuffer
{
   public:
	/**
	 * @brief Construct circular buffer with specified capacity
	 * @param capacity Maximum number of elements to store
	 */
	explicit CircularBuffer(size_t capacity = 0)
		: capacity_(capacity), writeIndex_(0), sampleCount_(0), triggerIndex_(0), triggered_(false)
	{
		if (capacity_ > 0)
			buffer_.resize(capacity_);
	}

	/**
	 * @brief Resize the buffer (clears existing data)
	 * @param newCapacity New buffer capacity
	 */
	void resize(size_t newCapacity)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		capacity_ = newCapacity;
		buffer_.resize(capacity_);
		writeIndex_ = 0;
		sampleCount_ = 0;
		triggered_ = false;
	}

	/**
	 * @brief Push element to buffer (overwrites oldest if full)
	 * @param item Element to add
	 * @return Index where element was written
	 */
	size_t push(const T& item)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (capacity_ == 0)
			return 0;

		size_t index = writeIndex_;
		buffer_[writeIndex_] = item;
		writeIndex_ = (writeIndex_ + 1) % capacity_;

		if (sampleCount_ < capacity_)
			sampleCount_++;

		return index;
	}

	/**
	 * @brief Mark current position as trigger point
	 */
	void markTrigger()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		// Trigger is at the last written position
		triggerIndex_ = (writeIndex_ == 0) ? capacity_ - 1 : writeIndex_ - 1;
		triggered_ = true;
	}

	/**
	 * @brief Set trigger index explicitly
	 * @param index Index to mark as trigger point
	 */
	void setTriggerIndex(size_t index)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		triggerIndex_ = index;
		triggered_ = true;
	}

	/**
	 * @brief Check if trigger has been marked
	 */
	bool isTriggered() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return triggered_;
	}

	/**
	 * @brief Get number of samples currently in buffer
	 */
	size_t size() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return sampleCount_;
	}

	/**
	 * @brief Get buffer capacity
	 */
	size_t capacity() const
	{
		return capacity_;
	}

	/**
	 * @brief Check if buffer is empty
	 */
	bool empty() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return sampleCount_ == 0;
	}

	/**
	 * @brief Check if buffer is full
	 */
	bool isFull() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return sampleCount_ == capacity_;
	}

	/**
	 * @brief Get element at index (relative to oldest sample)
	 * @param index Index from 0 to size()-1
	 * @return Element at index
	 */
	const T& operator[](size_t index) const
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (sampleCount_ < capacity_)
		{
			// Buffer not full yet, direct indexing
			return buffer_[index];
		}
		else
		{
			// Buffer full, calculate actual index from oldest sample
			size_t actualIndex = (writeIndex_ + index) % capacity_;
			return buffer_[actualIndex];
		}
	}

	/**
	 * @brief Get element at index (relative to oldest sample) - non-const version
	 * @param index Index from 0 to size()-1
	 * @return Element at index
	 */
	T& operator[](size_t index)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (sampleCount_ < capacity_)
		{
			// Buffer not full yet, direct indexing
			return buffer_[index];
		}
		else
		{
			// Buffer full, calculate actual index from oldest sample
			size_t actualIndex = (writeIndex_ + index) % capacity_;
			return buffer_[actualIndex];
		}
	}

	/**
	 * @brief Extract all samples in chronological order
	 * @return Vector containing all samples from oldest to newest
	 */
	std::vector<T> extractAll() const
	{
		std::lock_guard<std::mutex> lock(mutex_);

		std::vector<T> result;
		result.reserve(sampleCount_);

		if (sampleCount_ == 0)
			return result;

		if (sampleCount_ < capacity_)
		{
			// Buffer not full yet, copy directly
			for (size_t i = 0; i < sampleCount_; i++)
				result.push_back(buffer_[i]);
		}
		else
		{
			// Buffer full, start from oldest sample
			for (size_t i = 0; i < capacity_; i++)
			{
				size_t actualIndex = (writeIndex_ + i) % capacity_;
				result.push_back(buffer_[actualIndex]);
			}
		}

		return result;
	}

	/**
	 * @brief Get pre-trigger samples
	 * @param percent Percentage of buffer to retrieve before trigger (0-100)
	 * @return Vector containing pre-trigger samples
	 */
	std::vector<T> getPreTrigger(uint8_t percent) const
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (!triggered_ || sampleCount_ == 0)
			return {};

		size_t preTriggerSamples = (sampleCount_ * percent) / 100;
		std::vector<T> result;
		result.reserve(preTriggerSamples);

		// Find the position of trigger in extracted samples
		size_t triggerPos = calculateTriggerPosition();

		// Extract samples before trigger
		size_t startPos = (triggerPos >= preTriggerSamples) ? (triggerPos - preTriggerSamples) : 0;
		size_t endPos = triggerPos;

		for (size_t i = startPos; i < endPos; i++)
		{
			if (sampleCount_ < capacity_)
				result.push_back(buffer_[i]);
			else
			{
				size_t actualIndex = (writeIndex_ + i) % capacity_;
				result.push_back(buffer_[actualIndex]);
			}
		}

		return result;
	}

	/**
	 * @brief Get post-trigger samples
	 * @param percent Percentage of buffer to retrieve after trigger (0-100)
	 * @return Vector containing post-trigger samples
	 */
	std::vector<T> getPostTrigger(uint8_t percent) const
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (!triggered_ || sampleCount_ == 0)
			return {};

		size_t postTriggerSamples = (sampleCount_ * percent) / 100;
		std::vector<T> result;
		result.reserve(postTriggerSamples);

		// Find the position of trigger in extracted samples
		size_t triggerPos = calculateTriggerPosition();

		// Extract samples after trigger
		size_t startPos = triggerPos + 1;
		size_t endPos = (startPos + postTriggerSamples > sampleCount_) ? sampleCount_ : startPos + postTriggerSamples;

		for (size_t i = startPos; i < endPos; i++)
		{
			if (sampleCount_ < capacity_)
				result.push_back(buffer_[i]);
			else
			{
				size_t actualIndex = (writeIndex_ + i) % capacity_;
				result.push_back(buffer_[actualIndex]);
			}
		}

		return result;
	}

	/**
	 * @brief Get trigger index in extracted samples
	 * @return Position of trigger in extracted data (0 to size()-1)
	 */
	size_t getTriggerPosition() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return calculateTriggerPosition();
	}

	/**
	 * @brief Clear all data and reset state
	 */
	void clear()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		writeIndex_ = 0;
		sampleCount_ = 0;
		triggerIndex_ = 0;
		triggered_ = false;
		// Don't clear buffer_ vector, just reset indices for efficiency
	}

   private:
	/**
	 * @brief Calculate trigger position in extracted samples (internal, no lock)
	 */
	size_t calculateTriggerPosition() const
	{
		if (!triggered_)
			return 0;

		if (sampleCount_ < capacity_)
		{
			// Buffer not full, trigger index is direct
			return triggerIndex_;
		}
		else
		{
			// Buffer wrapped, calculate relative position
			if (triggerIndex_ >= writeIndex_)
				return triggerIndex_ - writeIndex_;
			else
				return capacity_ - writeIndex_ + triggerIndex_;
		}
	}

	std::vector<T> buffer_;
	size_t capacity_;
	size_t writeIndex_;
	size_t sampleCount_;
	size_t triggerIndex_;
	bool triggered_;

	mutable std::mutex mutex_;
};

#endif // _CIRCULARBUFFER_HPP
