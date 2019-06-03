# x32-abi-windows

**Not actually ABI in any regular sense.** In this crude test I'm using that title only to convey that the purpose of this code is similar of the X32 ABI on Linux, which is: running 64-bit code with 32-bit pointers, for performance and footprint reasons.

## Test program

Checks if the executable was build with **IMAGE_FILE_LARGE_ADDRESS_AWARE** flag. **Iff** the executable is **64-bit** and the flag was intentionally **cleared** (restricting the memory of the process to lowest 2 GBs (lower 31 bits)), then runs the test with 32-bit long (on 64-bit Windows **long** type is 32-bit) acting as storage for the pointer (I call it **short_ptr**). Otherwise it runs the same test with native pointer size.

In [/results/6-9-64M](https://github.com/tringi/x32-abi-windows/tree/master/results/6-9-64M) you'll find prebuilt test programs, where each node of the tree carries 6 pointers, the tree of 9 levels is generated, totalling 19754205 nodes, and then walked 67108864 times.

## Results (6-9-64M)

Note that this is syntetic test. The results of this technique will vary wildly (in all and every direction) when used with real code. Also the methods of measurement are very rough and the whole thing might be flawed in more than one way, but anyway...

The results are very interesting, yet not entirely surprising, at least to me as I've read quite a lot on this topic. I very much invite you to run them yourself.

### x86-64 Ryzen 1800X 3.6 GHz, DDR4 @ 3333 MHz dual channel

High performace power scheme. The table contains best result out of 6 (or so) runs.

|  | x86-64 | x86-32 (WoW) | x86-X32 |
| --- | ---: | ---: | ---: |
| Memory use | 1253.6 MB | 783.9 MB | 968.7 MB |
| Tree size | 1055.0 MB | 527.5 MB | 527.5 MB |
| Build time | 2.01 s | 2.05 s | **2.28 s** |
| **Walk time** | **7.21 s** | **6.93 s** | **6.50 s** |

### AArch64 Snapdragon 835 2.2 GHz, LPDDR4 @ 1866 MHz on board
*This is really for reference only, see notes.*

|  | AArch-64 | x86-32 (WoW) |
| --- | ---: | ---: |
| Memory use | 1226.2 MB | 787.9 MB |
| Tree size | 1055.0 MB | 527.5 MB |
| Build time | 8.81 s | 6.91 s |
| **Walk time** | 16.47 s | 20.33 s |

## TL;DR

A pointer-heavy code could be 6% faster (or more, or less) if built as 64-bit with 32-bit pointers instead just simply 32-bit, but it still will take more memory if special care of allocation granularity isn't taken.

## Possible improvements

* It would be interesting to see function call dispatch through 32-bit pointers in 64-bit code.
* This test only narrows the pointers you dereference explicitly. For anything further it would require compiler support, e.g. virtual tables.

## Random notes

* **No X32 *ABI* for ARM-64.** ARM Windows loader won't load native ARM64 binaries that have LARGE ADDRESS AWARE flag cleared (or ADDRESS SPACE RANDOMIZATION disabled for that matter). And the x86 to AArch64 translator can process only 32-bit code (for now I'm told).
* The program uses **std::rand** to produce the same random tree on all platforms.
* For the purposes of this X32 *ABI* it would be nice if the Windows supported restricting to 4 GB address space for 64-bit programs. It's the case of 32-bit executables that have the IMAGE_FILE_LARGE_ADDRESS_AWARE flag set.

## Generated binary analysis

The differences in generated instructions seem marginal. On **short_ptr** side there are less REX prefixes, but more conversion and move instructions. It seems like the optimizer doesn't see perfectly through what I'm trying to acomplish, but I've found no obvious pesimisations either. Although it won't use SSE to zero out the array of the *short* pointers.

