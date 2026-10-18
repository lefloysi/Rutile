#include "runtime.hpp"

struct RuntimeTest {
	RuntimeMutex* mutex;
	RuntimeCondition* condition;
	RuntimeAtomic* state;
};

int runtime_test_worker(void* context) {
	auto& test = *static_cast<RuntimeTest*>(context);
	runtime_mutex_lock(test.mutex);
	runtime_atomic_store(test.state, 1);
	runtime_condition_notify(test.condition);
	while (runtime_atomic_load(test.state) != 2) {
		runtime_condition_wait(test.condition, test.mutex, 100);
	}
	runtime_mutex_unlock(test.mutex);
	return 0;
}

int main() {
	RuntimeTest test{ runtime_mutex_create(), runtime_condition_create(), runtime_atomic_create(0) };
	if (!test.mutex || !test.condition || !test.state) {
		return 1;
	}
	auto* thread = runtime_thread_start(runtime_test_worker, &test);
	if (!thread) {
		return 2;
	}
	runtime_mutex_lock(test.mutex);
	while (!runtime_atomic_load(test.state)) {
		runtime_condition_wait(test.condition, test.mutex, 100);
	}
	const auto previous = runtime_atomic_fetch_add(test.state, 1);
	runtime_condition_notify(test.condition);
	runtime_mutex_unlock(test.mutex);
	runtime_thread_join(thread);
	const auto final = runtime_atomic_load(test.state);
	runtime_atomic_destroy(test.state);
	runtime_condition_destroy(test.condition);
	runtime_mutex_destroy(test.mutex);
	return previous == 1 && final == 2 ? 0 : 3;
}
