#include <Windows.h>
#include <PSApi.h>

#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <random>

extern "C" IMAGE_DOS_HEADER __ImageBase;

#if defined (_M_ARM64)
#define ARCHITECTURE "arm-64"
#elif defined(_M_ARM)
#define ARCHITECTURE "arm-32"
#elif defined(_M_X64)
#define ARCHITECTURE "x86-64"
#else
#define ARCHITECTURE "x86-32"
#define NATIVE_X86_32 1
#endif

BOOL (WINAPI * ptrIsWow64Process2) (HANDLE, USHORT *, USHORT *) = NULL;
VOID (WINAPI * ptrGetSystemTimeAsFileTime) (LPFILETIME) = NULL; // GetSystemTimePreciseAsFileTime

template <typename P>
bool symbol (HMODULE h, P & pointer, const char * name) {
    if (P p = reinterpret_cast <P> (GetProcAddress (h, name))) {
        pointer = p;
        return true;
    } else
        return false;
}

const char * platform () {
    if (ptrIsWow64Process2) {
        USHORT emulated = 0;
        USHORT native = 0;

        if (ptrIsWow64Process2 (GetCurrentProcess (), &emulated, &native)) {
            if (emulated) {
                switch (native) {
                    case IMAGE_FILE_MACHINE_IA64: return "itanium";
                    case IMAGE_FILE_MACHINE_AMD64: return "x86-64";
                    case IMAGE_FILE_MACHINE_ARM64: return "arm-64";

                    default:
                        return "unexpected";
                }
            } else
                return "native";
        }
    } else {
        BOOL wow = FALSE;
        if (IsWow64Process (GetCurrentProcess (), &wow)) {
            if (wow)
                return "x86-64";
            else
                return "native";
        }
    }
    return "error";
}

std::mt19937                         random_generator;
std::uniform_int_distribution <long> random_distribution;

template <typename T>
struct short_ptr {
    long pointer;

public:
    short_ptr () noexcept
        : pointer (0) {};
    short_ptr (T * native) noexcept
        : pointer (long (reinterpret_cast <std::intptr_t> (native))) {}
    short_ptr (short_ptr && other) noexcept
        : pointer (other.pointer) {}
    short_ptr (const short_ptr & other) noexcept
        : pointer (other.pointer) {}

    short_ptr & operator = (short_ptr && other) noexcept {
        this->pointer = other.pointer;
        return *this;
    }
    short_ptr & operator = (const short_ptr & other) noexcept {
        this->pointer = other.pointer;
        return *this;
    }

    T * operator -> () {
        return reinterpret_cast <T *> (std::intptr_t (this->pointer));
    }
    const T * operator -> () const {
        return reinterpret_cast <const T *> (std::intptr_t (this->pointer));
    }
    operator T * () {
        return reinterpret_cast <T *> (std::intptr_t (this->pointer));
    }
    operator const T * () const {
        return reinterpret_cast <const T *> (std::intptr_t (this->pointer));
    }
};

static constexpr auto N = 6u;

struct short_ptr_node {
    typedef short_ptr <short_ptr_node> ptr_type;

    ptr_type pointers [N] = {};
    long data = random_distribution (random_generator);
};

struct naked_ptr_node {
    typedef naked_ptr_node * ptr_type;

    ptr_type pointers [N] = {};
    long data = random_distribution (random_generator);
};

template <typename T>
T * walk (T * p, long x = 0) {
    
    while (auto next = p->pointers [(p->data ^ x) % N]) {
        p = next;
        x >>= 3; // keep roughly number of bits of N
    }
    return p;
}

std::size_t allocated = 0;

template <typename T>
void build (T * parent, std::size_t depth) {

    for (auto & p : parent->pointers) {
        if (random_distribution (random_generator) % 8) { // leave some NULL
            p = new T;
            ++allocated;
        }
    }
    if (depth--) {
        for (auto & p : parent->pointers) {
            if (p) {
                build (( T *) p, depth);
            }
        }
    }
}

struct time {
    union {
        FILETIME ft;
        long long ll = 0;
    };

    time & update () {
        ptrGetSystemTimeAsFileTime (&this->ft);
        return *this;
    }
    time localize () {
        time localized;
        FileTimeToLocalFileTime (&this->ft, &localized.ft);
        return localized;
    }
    SYSTEMTIME decode () {
        SYSTEMTIME st;
        FileTimeToSystemTime (&this->ft, &st);
        return st;
    }
    double difference (const time & earlier) const {
        return (this->ll - earlier.ll) / 10'000'000.0; // convert to seconds
    }
};

time t;
void log (const char * format, ...) {
    time tt;
    auto st = tt.update ().localize ().decode ();

    va_list args;                                                           \
    va_start (args, format);

    std::printf ("%02u:%02u:%02u.%03u  [%.2f]  ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, tt.difference (t));
    std::vprintf (format, args);
    std::printf ("\n");

    va_end (args);
    t.update ();
}

template <typename T>
void test () {
    static constexpr auto DEPTH = 9u;
    static constexpr auto WALKS = 1024*65536u;

    T root;
    try {
        build (&root, DEPTH);
        log ("built tree of %zu nodes in %u levels", allocated, DEPTH);

        PROCESS_MEMORY_COUNTERS memory;
        memory.cb = sizeof memory;
        GetProcessMemoryInfo (GetCurrentProcess (), &memory, sizeof memory);

        log ("using %.1f MB to store %.1f MB of data (%.2f%% overhead)",
             memory.PagefileUsage / 1048576.0,
             (long long) allocated * sizeof (T) / 1048576.0,

             100.0 * memory.PagefileUsage / (long long) (allocated * sizeof (T)) - 100.0
             );
        ;

        std::uintmax_t sum = 0;
        for (auto x = 0u; x != WALKS; ++x) {
            sum += walk (&root, x)->data;
        }
        log ("walked tree %u times, sum of data is %zu", WALKS, sum);

    } catch (const std::bad_alloc &) {
        log ("bad alloc");
    }
}


int main () {
    // some APIs

    if (auto hKernel32 = GetModuleHandle (L"KERNEL32")) {
        symbol (hKernel32, ptrIsWow64Process2, "IsWow64Process2");
        symbol (hKernel32, ptrGetSystemTimeAsFileTime, "GetSystemTimePreciseAsFileTime");
    }
    if (ptrGetSystemTimeAsFileTime == NULL) {
        ptrGetSystemTimeAsFileTime = GetSystemTimeAsFileTime;
    }

    // make more sure we crash in case of issue

    HeapSetInformation (GetProcessHeap (), HeapEnableTerminationOnCorruption, NULL, 0);

    // platform info

    auto header = reinterpret_cast <const IMAGE_NT_HEADERS *> (reinterpret_cast <const char *> (&__ImageBase) + __ImageBase.e_lfanew);
    bool laa = header->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE;

    std::printf ("X32-ABI on WINDOWS test; " ARCHITECTURE " on %s, large addresses %s\n\n", platform (), laa ? "enabled" : "DISABLED");

    // start

    t.update ();

#ifdef NATIVE_X86_32
    log ("native...");
    test <naked_ptr_node> ();
#else
    if (laa) {
        log ("naked_ptr_node...");
        test <naked_ptr_node> ();
    } else {
        log ("short_ptr_node ...");
        test <short_ptr_node> ();
    }
#endif

    log ("done.");
    return 0;
}
