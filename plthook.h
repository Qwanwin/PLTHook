#ifndef PLTHOOK_H
#define PLTHOOK_H

#include <string>

struct QwanPLT;

QwanPLT* qwanplt_open(const std::string& library_name);
int qwanplt_replace(QwanPLT* hook, const std::string& func_name, void* hook_func, void** old_func);
void qwanplt_close(QwanPLT* hook);

#endif // PLTHOOK_H