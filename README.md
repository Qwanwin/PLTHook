# PLTHook – ELF PLT Hooking Framework for Android

PLTHook is an ELF PLT (Procedure Linkage Table) based hooking library for Android, allowing developers to dynamically replace function implementations within shared libraries.

---

## Table of Contents

* [Features](#features)
* [File Structure](#file-structure)
* [How it Works](#how-it-works)
* [API](#api)
* [Usage Example](#usage-example)
* [Compatibility](#compatibility)
* [Caveats](#caveats)
* [When to Hook](#when-to-hook)
* [Additional Notes for Developers](#additional-notes-for-developers)
* [Contact](#contact)


---

## Features

* Hook functions within shared libraries (`.so`) via the PLT.
* Supports arm32 and arm64 architectures.
* Automatic ELF32/ELF64 detection.
* Allows restoration of original functions (via old function pointers).
* Uses `mprotect` for safe modification of GOT entries.
* Uses `dl_iterate_phdr` for `.dynamic` segment discovery.


---

## File Structure

* **plthook.h:** Main header file containing function and structure declarations.
* **plthook.cpp:** Hooking implementation, handles processing of `.dynamic`, `.plt.got`, and `.rela.plt` segments.


---

## How it Works

1. Loads the target library using `dlopen()`.
2. Locates the `.dynamic` segment using `dl_iterate_phdr`.
3. Retrieves:
    * `.dynsym`: Symbol table
    * `.dynstr`: Symbol strings
    * `.rela.plt`: PLT relocation table
    * `.got.plt`: PLT entries to be replaced
4. Finds the target function name, then:
    * Applies `mprotect` to make the GOT writable.
    * Overwrites the GOT entry with the address of `hook_func`.
    * Restores the original memory protection.


---

## API

* **`QwanPLT* qwanplt_open(const std::string& library_name);`**
    * Opens and prepares the PLT structure for hooking.
    * `library_name`: Can be empty for the main `libc.so` (NULL).
    * Returns: A pointer to the `QwanPLT` structure.

* **`int qwanplt_replace(QwanPLT* hook, const std::string& func_name, void* hook_func, void** old_func);`**
    * Replaces the `func_name` function with `hook_func`.
    * `old_func`: Output pointer to the original implementation (optional).
    * Returns: 0 on success, throws an exception on failure.

* **`void qwanplt_close(QwanPLT* hook);`**
    * Closes and cleans up all allocated resources.
    * Includes calling `dlclose()` on the library handle.


---

## Usage Example

```c++
#include <iostream>
#include <dlfcn.h>
#include "plthook.h"

// Hook function
extern "C" void* my_malloc(size_t size) {
    std::cout << "[HOOK] malloc called with size: " << size << std::endl;
    static void* (*real_malloc)(size_t) = nullptr;
    if (!real_malloc) {
        real_malloc = (void* (*)(size_t))dlsym(RTLD_NEXT, "malloc");
    }
    return real_malloc(size);
}

int main() {
    try {
        QwanPLT* hook = qwanplt_open("libc.so"); 
        void* old_func = nullptr;

        qwanplt_replace(hook, "malloc", (void*)my_malloc, &old_func);
        std::cout << "PLT hook success!" << std::endl;

        void* ptr = malloc(100); 
        free(ptr);                

        qwanplt_close(hook);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
    }
    return 0;
}
```

To hook multiple functions: Repeat `qwanplt_replace()` for each function name.


**Calling the hook from `JNI_OnLoad`:**

```c++
extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    try {
        QwanPLT* hook = qwanplt_open("libtarget.so");
        qwanplt_replace(hook, "target_function", (void*)my_hook, nullptr);
    } catch (...) {
        // Handle error
    }
    return JNI_VERSION_1_6;
}
```


---

## Compatibility

* Tested on: Android API 21+, ARMv7 (armeabi-v7a), ARM64 (arm64-v8a)
* NDK 21 and above (using LLVM)


---

## Caveats

* Modifying the GOT requires `mprotect`, which might not work if SELinux is strictly enforced.
* Does not work if the function is not resolved through the PLT (e.g., static internal calls or TLS).
* Cannot hook inline functions.
* Cannot hook static functions.
* Cannot hook prelinked functions.
* Android 7+ (Full RELRO) can have a permanently read-only GOT.  Workarounds include inline hooking, tramp hooks, or patching the original code.


---

## When to Hook

Hooking must be performed after the target library is loaded (either via `dlopen()` or automatically by the loader).


---

## Additional Notes for Developers

* Main library / statically linked binary: If `library_name` is empty, `dlpi_name` might be empty or NULL.  Check for this condition.


---

## Contact

* Telegram: [@Qwanwin](https://t.me/Qwanwin)
* Channel: [@Codex4444](https://t.me/Codex4444)


---

