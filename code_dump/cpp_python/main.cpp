// main.cpp
#include <chrono>
#include <iostream>
#include <python3.12/Python.h>
#include <stdexcept>
#include <thread>
#include <vector>

class PythonInterpreter {
private:
  PyThreadState *mainThreadState;

public:
  PythonInterpreter() : mainThreadState(nullptr) {
    Py_Initialize();
    PyThread_init_thread();
    mainThreadState = PyThreadState_Get();
    PyEval_SaveThread(); // Release the GIL for other threads
  }

  ~PythonInterpreter() {
    PyEval_RestoreThread(mainThreadState);
    Py_Finalize();
  }
};

void executePythonInThread(const std::string &code, int threadId) {
  // Acquire GIL for Python operations
  PyGILState_STATE gstate = PyGILState_Ensure();

  try {
    // Create a new dictionary for this thread
    PyObject *threadDict = PyDict_New();
    if (!threadDict) {
      throw std::runtime_error("Failed to create thread dictionary");
    }

    // Import required modules in this thread's context
    PyObject *mainModule = PyImport_AddModule("__main__");
    if (!mainModule) {
      Py_DECREF(threadDict);
      throw std::runtime_error("Failed to get main module");
    }

    PyObject *globalDict = PyModule_GetDict(mainModule);

    // Run the Python code
    PyObject *result = PyRun_String(code.c_str(), Py_file_input, globalDict,
                                    threadDict // Use thread-local dictionary
    );

    if (result == nullptr) {
      PyErr_Print();
      std::cerr << "Thread " << threadId << " execution failed\n";
    } else {
      Py_DECREF(result);
    }

    Py_DECREF(threadDict);
  } catch (const std::exception &e) {
    std::cerr << "Error in thread " << threadId << ": " << e.what() << "\n";
  }

  // Release the GIL
  PyGILState_Release(gstate);
}

int main() {
  try {
    PythonInterpreter interpreter;

    // Python code that simulates independent work
    std::string pythonCode = R"(
import numpy as np
import time
import threading
from random import uniform

# Get current thread info
thread = threading.current_thread()
thread_id = thread.ident

# Simulate some independent work
arr_size = np.random.randint(1000000, 2000000)
arr = np.random.rand(arr_size)

# Simulate processing with random sleep
sleep_time = uniform(0.1, 0.5)
time.sleep(sleep_time)

# Do some computation
result = np.mean(arr)
std_dev = np.std(arr)

print(f'Thread {thread_id}:')
print(f'  Processed array of size: {arr_size}')
print(f'  Mean: {result:.4f}')
print(f'  Std Dev: {std_dev:.4f}')
print(f'  Processing time: {sleep_time:.2f}s')
)";

    // Create multiple threads
    std::vector<std::thread> threads;
    const int NUM_THREADS = 4;

    // Record start time
    auto start_time = std::chrono::high_resolution_clock::now();

    // Launch threads
    for (int i = 0; i < NUM_THREADS; i++) {
      threads.emplace_back(executePythonInThread, pythonCode, i);
    }

    // Wait for all threads to complete
    for (auto &thread : threads) {
      thread.join();
    }

    // Calculate total execution time
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    std::cout << "\nAll threads completed successfully\n";
    std::cout << "Total execution time: " << duration.count() << "ms\n";
  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
