Copyright Szabo Cristina-Andreea 2023-2024

# Memory Allocator

## Overview

This project implements a custom memory allocator in **C** that simulates the behavior of standard memory allocation functions like `malloc`, `calloc`, `realloc`, and `free`. The allocator uses a **block-based** approach to manage memory, providing fine-grained control over memory blocks. It supports memory allocation, deallocation, and reallocation while optimizing memory usage through splitting, coalescing, and merging adjacent free blocks.

The allocator uses `sbrk()` and `mmap()` system calls to allocate memory, ensuring efficient memory management. It includes a variety of features, such as block splitting for smaller allocations, coalescing adjacent free blocks, and tracking memory status.

## Features

- **Memory Allocation**: Implements `os_malloc` for allocating memory blocks.
- **Zero Initialization**: Implements `os_calloc` for allocating memory and initializing it to zero.
- **Memory Reallocation**: Implements `os_realloc` to resize allocated memory blocks.
- **Memory Deallocation**: Implements `os_free` to release memory blocks back to the pool.
- **Memory Block Management**:
  - **Splitting**: Splits larger free blocks into smaller ones for more efficient memory usage.
  - **Coalescing**: Merges adjacent free blocks to reduce fragmentation.
  - **Unification**: Unifies contiguous free blocks to optimize memory management.
