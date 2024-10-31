// main.cpp
#include <iostream>
#include <python3.12/Python.h>
#include <stdexcept>
#include <thread>
#include <vector>

class PythonThreading {
private:
  PyThread_type_lock globalLock;

public:
  PythonThreading() : globalLock(nullptr) {
    // Initialize the threading system
    PyThread_init_thread(); // void return type, always succeeds in modern
                            // Python

    // Create a global lock
    globalLock = PyThread_allocate_lock();
    if (globalLock == nullptr) {
      throw std::runtime_error("Failed to allocate global lock");
    }
  }

  ~PythonThreading() {
    if (globalLock) {
      PyThread_free_lock(globalLock);
    }
  }

  bool acquireLock(int waitflag = 1) {
    if (!globalLock) {
      throw std::runtime_error("Threading system not properly initialized");
    }
    return PyThread_acquire_lock(globalLock, waitflag) != 0;
  }

  void releaseLock() {
    if (!globalLock) {
      throw std::runtime_error("Threading system not properly initialized");
    }
    PyThread_release_lock(globalLock);
  }
};

class PythonInterpreter {
private:
  PyThreadState *mainThreadState;
  PythonThreading threading;

public:
  PythonInterpreter() : mainThreadState(nullptr) {
    // Initialize Python
    Py_Initialize();

    // In Python 3.7+, the GIL is automatically initialized
    // Store the main thread state
    mainThreadState = PyThreadState_Get();

    // Release the GIL so other threads can use it
    PyEval_SaveThread();
  }

  ~PythonInterpreter() {
    // Reacquire GIL for cleanup
    PyEval_RestoreThread(mainThreadState);
    Py_Finalize();
  }
};

void executePythonInThread(PythonThreading &threading, const std::string &code,
                           int threadId) {
  // Acquire the custom lock
  if (!threading.acquireLock()) {
    std::cerr << "Thread " << threadId << " failed to acquire lock\n";
    return;
  }

  // Ensure GIL state
  PyGILState_STATE gstate = PyGILState_Ensure();

  try {
    std::cout << "Thread " << threadId << " executing Python code\n";

    PyObject *mainModule = PyImport_AddModule("__main__");
    if (!mainModule) {
      throw std::runtime_error("Failed to get main module");
    }

    PyObject *globalDict = PyModule_GetDict(mainModule);
    if (!globalDict) {
      throw std::runtime_error("Failed to get global dictionary");
    }

    PyObject *result =
        PyRun_String(code.c_str(), Py_file_input, globalDict, globalDict);

    if (result == nullptr) {
      PyErr_Print();
      std::cerr << "Thread " << threadId << " execution failed\n";
    } else {
      Py_DECREF(result);
      std::cout << "Thread " << threadId << " execution successful\n";
    }
  } catch (const std::exception &e) {
    std::cerr << "Error in thread " << threadId << ": " << e.what() << "\n";
  }

  // Release the GIL
  PyGILState_Release(gstate);

  // Release our custom lock
  threading.releaseLock();
}

int main() {
  try {
    PythonInterpreter interpreter;

    std::string pythonCode = R"(
import numpy as np
import threading

current_thread = threading.current_thread()
thread_id = current_thread.ident
print(f'Python thread ID: {thread_id}')
arr = np.array([1, 2, 3, 4, 5])
print(f'Processing array: {arr}')
        )";

    // Create multiple threads
    std::vector<std::thread> threads;
    const int NUM_THREADS = 4;

    PythonThreading threading;
    for (int i = 0; i < NUM_THREADS; i++) {
      threads.emplace_back(executePythonInThread, std::ref(threading),
                           pythonCode, i);
    }

    // Wait for all threads to complete
    for (auto &thread : threads) {
      thread.join();
    }

    std::cout << "All threads completed successfully\n";
  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
