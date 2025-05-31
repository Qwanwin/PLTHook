#include <elf.h>
#include <link.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <stdexcept>
#include <iostream>

#if __SIZEOF_POINTER__ == 8
    #define ELF_R_SYM ELF64_R_SYM
    #define ElfW(type) Elf64_##type
#else
    #define ELF_R_SYM ELF32_R_SYM
    #define ElfW(type) Elf32_##type
#endif

struct QwanPLT {
    void* handle;
    ElfW(Dyn)* dynamic;
    ElfW(Sym)* symtab;
    const char* strtab;
    ElfW(Rela)* plt_rela;
    ElfW(Addr)* plt_got;
    size_t plt_rela_size;
};

QwanPLT* qwanplt_open(const std::string& library_name) {
    QwanPLT* hook = new QwanPLT();
    hook->handle = nullptr;
    hook->dynamic = nullptr;
    hook->symtab = nullptr;
    hook->strtab = nullptr;
    hook->plt_rela = nullptr;
    hook->plt_got = nullptr;
    hook->plt_rela_size = 0;

    hook->handle = dlopen(library_name.empty() ? nullptr : library_name.c_str(), RTLD_LAZY);
    if (!hook->handle) {
        delete hook;
        throw std::runtime_error("dlopen failed: " + std::string(dlerror()));
    }

    auto dl_iterate_phdr_fn = (int (*)(int (*)(struct dl_phdr_info*, size_t, void*), void*)) dlsym(RTLD_DEFAULT, "dl_iterate_phdr");
    if (!dl_iterate_phdr_fn) {
        dlclose(hook->handle);
        delete hook;
        throw std::runtime_error("dl_iterate_phdr not found");
    }

    bool found = false;
    dl_iterate_phdr_fn([](struct dl_phdr_info* info, size_t size, void* data) {
        QwanPLT* hook = static_cast<QwanPLT*>(data);
        for (int i = 0; i < info->dlpi_phnum; i++) {
            if (info->dlpi_phdr[i].p_type == PT_DYNAMIC) {
                hook->dynamic = (ElfW(Dyn)*)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
                return 1; 
            }
        }
        return 0;
    }, hook);

    if (!hook->dynamic) {
        dlclose(hook->handle);
        delete hook;
        throw std::runtime_error("Failed to find PT_DYNAMIC");
    }

    for (ElfW(Dyn)* dyn = hook->dynamic; dyn->d_tag != DT_NULL; ++dyn) {
        switch (dyn->d_tag) {
            case DT_SYMTAB:
                hook->symtab = (ElfW(Sym)*)dyn->d_un.d_ptr;
                break;
            case DT_STRTAB:
                hook->strtab = (const char*)dyn->d_un.d_ptr;
                break;
            case DT_JMPREL:
                hook->plt_rela = (ElfW(Rela)*)dyn->d_un.d_ptr;
                break;
            case DT_PLTRELSZ:
                hook->plt_rela_size = dyn->d_un.d_val / sizeof(ElfW(Rela));
                break;
            case DT_PLTGOT:
                hook->plt_got = (ElfW(Addr)*)dyn->d_un.d_ptr;
                break;
        }
    }

    if (!hook->symtab || !hook->strtab || !hook->plt_rela || !hook->plt_got) {
        dlclose(hook->handle);
        delete hook;
        throw std::runtime_error("Failed to find required ELF sections");
    }

    return hook;
}

int qwanplt_replace(QwanPLT* hook, const std::string& func_name, void* hook_func, void** old_func) {
    for (size_t i = 0; i < hook->plt_rela_size; ++i) {
        ElfW(Rela)* rela = &hook->plt_rela[i];
        size_t sym_index = ELF_R_SYM(rela->r_info);
        ElfW(Sym)* sym = &hook->symtab[sym_index];

        if (strcmp(func_name.c_str(), hook->strtab + sym->st_name) == 0) {
            ElfW(Addr)* got_entry = (ElfW(Addr)*)(hook->plt_got + (rela->r_offset / sizeof(ElfW(Addr))));

            if (old_func) {
                *old_func = (void*)*got_entry;
            }

            size_t page_size = sysconf(_SC_PAGESIZE);
            void* page_start = (void*)((uintptr_t)got_entry & ~(page_size - 1));
            if (mprotect(page_start, page_size, PROT_READ | PROT_WRITE) != 0) {
                throw std::runtime_error("mprotect failed");
            }

            *got_entry = (ElfW(Addr))hook_func;

            if (mprotect(page_start, page_size, PROT_READ) != 0) {
                throw std::runtime_error("mprotect restore failed");
            }

            return 0;
        }
    }

    throw std::runtime_error("Function " + func_name + " not found in PLT");
}

void qwanplt_close(QwanPLT* hook) {
    if (hook->handle) {
        dlclose(hook->handle);
    }
    delete hook;
}