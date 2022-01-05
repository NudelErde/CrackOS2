#pragma once

#include <LanguageFeatures/SmartPointer.hpp>
#include <Memory/memory.hpp>
#include <stdint.h>

class Process;

class Mapping {
public:
    struct Flags {
        bool writable : 1;
        bool executable : 1;
    };

private:
    struct Entry {
        uint64_t virtAddr;
        uint64_t physAddr;
        uint64_t size;
        Flags flags;
        unique_ptr<Entry> next;
    };

    unique_ptr<Entry> head;

public:
    Mapping();
    Mapping(const Mapping&) = delete;
    Mapping(Mapping&&) = delete;
    Mapping& operator=(const Mapping&) = delete;
    Mapping& operator=(Mapping&&) = delete;

    void map(uint64_t virtAddr, uint64_t physAddr, uint64_t size, Flags flags);
    void unmap(uint64_t virtAddr, uint64_t size);
    void load();
    void unload();
    void compact();
};

class Thread {
private:
    shared_ptr<Process> process;
    Mapping threadLocalMemory;// includes thread local and stack

public:
    Thread(shared_ptr<Process> process);
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;
    Thread(Thread&&) = default;
    Thread& operator=(Thread&&) = default;
    inline shared_ptr<Process> getProcess() { return process; }

    Mapping& getThreadMemory() { return threadLocalMemory; }
};

class Process : public enable_shared_from_this<Process> {
private:
    Mapping processMemory;// includes program code, global data, heap, etc.
public:
    Process();
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    Process(Process&&) = default;
    Process& operator=(Process&&) = default;

    unique_ptr<Thread> spawnThread(uint64_t entryPoint);

    Mapping& getProcessMemory() { return processMemory; }
};