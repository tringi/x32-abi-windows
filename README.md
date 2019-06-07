# x32-abi-windows / x86-64X32

**Not actually ABI in any regular sense.** In this crude test I'm using this title only to convey that the purpose of this code is similar of the X32 ABI on Linux, which is: running 64-bit code with 32-bit pointers, for performance and footprint reasons.

Let's call it **x86-64X32**

## TL;DR

A pointer-heavy code **can** be 9 % faster if built as 64-bit with 32-bit pointers (instead just 32-bit), and if the memory layout allows for better cache utilization and locality due to shorter pointers. The **64X32** program will still use more memory unless special care of allocations isn't taken.

If the data structure doesn't lend itself to improved cache utilization there are little to no gains.

The rule of thumb seems to be: *If the performance of your code is better when compiled as 32-bit than 64-bit, then **64X32** mode can improve it further, otherwise not so much.*

## Test program

The program checks if its executable was built with **IMAGE_FILE_LARGE_ADDRESS_AWARE** flag. **Iff** the executable is **64-bit** and the flag was intentionally **cleared** (restricting the address space of the process to lowest 2 GBs (lower 31 bits)), then runs the test with **long** (on 64-bit Windows **long** type is still 32-bit) as a storage type for the pointers (I call it **short_ptr**). Otherwise it runs the same test with native pointer size.

The test consists of building a random tree and randomly walking through it. Obviously this is a synthetic test and the results will vary wildly when used with real-world code.

In [/results](https://github.com/tringi/x32-abi-windows/tree/master/results) you'll find prebuilt test programs.

* 6-9-64M ([x32-abi-windows-tight.cpp](https://github.com/tringi/x32-abi-windows/blob/master/x32-abi-windows-tight.cpp)) test builds a tree where every node has 6 pointers to next nodes (some left intentionally NULL). A tree of 9 levels is generated and then walked through 67108864 times.
* 3-15-16M ([x32-abi-windows.cpp](https://github.com/tringi/x32-abi-windows/blob/master/x32-abi-windows.cpp)) test builds a tree where every node has 3 pointers, some intentionally NULL, spaced by cache line-sized padding to break sharing. A tree of 15 levels is generated and then walked through 16777216 times.

## Changes since first release

* replaced **std::rand** with **std::mt19937** and **std::uniform_int_distribution** to get better random distribution
* nodes are preallocated and random-shuffled before being inserted into tree to break potential allocation locality; this has caused the results to change, for worse, in all cases, but interestingly the least for x86-64X32
* added test with additional cache line-sized padding between pointers to simulate some real-world data, to break sharing; these results show only 3% improvement which is nearing the margin of error

*I'm still working on other ways to make the test more conclusive*

## Test results
**x86-64 Ryzen 1800X 3.6 GHz, DDR4 @ 3333 MHz dual channel**

The results are very interesting, yet not entirely surprising if you've already read on this topic, i.e. why people around Linux found it worth experimenting with. I very much invite you to run the tests yourself.

High performance power scheme. The tables contain best result out of 6 or more runs.

### 6-9-64M
**pointers tightly packed** [x32-abi-windows-tight.cpp](https://github.com/tringi/x32-abi-windows/blob/master/x32-abi-windows-tight.cpp)

|  | x86-64 | x86-32 (WoW) | x86-64X32 |
| --- | ---: | ---: | ---: |
| Memory use | 1471.9 MB | 919.4 MB | 1139.0 MB |
| Tree size | 1201.2 MB | 600.6 MB | 600.6 MB |
| Build time | 2.81 s | 2.79 s | **2.87 s** |
| **Walk time** | **11.61 s** | **10.94 s** | **10.13 s** |

### 3-15-16M
**deeper tree, each pointer in different cache line** [x32-abi-windows.cpp](https://github.com/tringi/x32-abi-windows/blob/master/x32-abi-windows.cpp)

|  | x86-64 | x86-32 (WoW) | x86-64X32 |
| --- | ---: | ---: | ---: |
| Memory use | 1829.1.9 MB | 1643.4 MB | 1648.2 MB |
| Tree size | 1524.1 MB | 1439.4 MB | 1439.4 MB |
| Build time | 2.03 s | 2.12 s | **1.92 s** |
| **Walk time** | **5.74 s** | **6.38 s** | **5.56 s** |

## Possible improvements

* Using custom allocator could bring the memory requirements of X32 down to x86-32 ballpark.
* It would be interesting to see function call dispatch through 32-bit pointers in 64-bit code.
* This test only narrows the pointers you dereference explicitly. For anything further, it would require compiler support, e.g. to support short-ptr virtual tables.

## Random notes

* **No X32 *ABI* for ARM-64.** ARM Windows loader won't load native ARM64 binaries that have LARGE ADDRESS AWARE flag cleared (or ADDRESS SPACE RANDOMIZATION disabled for that matter). And the x86 to AArch64 translator can process only 32-bit code (for now I'm told).
* For the purposes of this X32 *ABI* it would be nice if the Windows supported restricting to 4 GB address space for 64-bit programs. It's the case of 32-bit executables that have the IMAGE_FILE_LARGE_ADDRESS_AWARE flag set.

## Generated binary analysis

The differences in generated instructions seem marginal. On **short_ptr** side there are less REX prefixes, but more conversion and move instructions. It seems like the optimizer doesn't see perfectly through what I'm trying to accomplish, but I've found no obvious pesimisations either. Although it won't use SSE to zero out the array of the *short* pointers.
