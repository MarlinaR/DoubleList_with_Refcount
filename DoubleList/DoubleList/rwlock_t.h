#pragma once
#ifndef RWLOCK_T_H
#define RWLOCK_T_H
#include <atomic>
#include <thread>
struct rwlock_t {
public:
	std::atomic<uint32_t> val = 0;
	uint32_t WRITE_BIT = (1 << 31);

	rwlock_t() {
		val = 0;
	}

	void rlock() {
		while (true) {
			uint32_t old = val;
			// If no write lock -> increase readers by 1
			if (!(old & WRITE_BIT) && val.compare_exchange_strong(old, old + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
				return;
			}
			std::this_thread::yield();
		}
	}

	void wlock() {
		while (true) {
			uint32_t old = val;
			if (!(old & WRITE_BIT) &&
				val.compare_exchange_strong(old, old | WRITE_BIT, std::memory_order_acquire, std::memory_order_relaxed)) {
				break;
			}
			std::this_thread::yield();
		}
		// Waiting for readers to stop
		while (true) {
			if (val == WRITE_BIT) 
				break;
			std::this_thread::yield();
		}
	}

	void unlock() {
		// If writer locked
		if (val == WRITE_BIT) {
			val = 0;
		}
		// If reader locked
		else {
			val--;
		}
	}
};

#endif //RWLOCK_T_H