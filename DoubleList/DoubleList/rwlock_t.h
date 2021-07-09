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
			if (!(old & WRITE_BIT) && val.compare_exchange_strong(old, old + 1)) {
				return;
			}
			std::this_thread::yield();
		}
	}

	void wlock() {
		while (true) {
			uint32_t old = val;
			if (!(old & WRITE_BIT) &&
				val.compare_exchange_strong(old, old | WRITE_BIT)) {
				break;
			}
			std::this_thread::yield();
		}
		while (true) {
			if (val == WRITE_BIT) {
				break;
			}
			std::this_thread::yield();
		}
	}

	void unlock() {
		if (val == WRITE_BIT) {
			val = 0;
		}
		else {
			--val;
		}
	}
};

#endif //RWLOCK_T_H