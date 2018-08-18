/* Copyright (c) 2014, Artak Khnkoyan <artak.khnkoyan@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <Windows.h>

#define mcontext_t CONTEXT

typedef struct ucontext_t {
    mcontext_t uc;
    char* stack;
    size_t  stack_size;
} ucontext_t;

__declspec(noinline) int __stdcall getcontext(ucontext_t* ctx)
{
    RtlCaptureContext(&ctx->uc);
    return 0;
}

__declspec(noinline) void __stdcall setcontext(ucontext_t* ctx)
{
#if defined (_WIN64)
	RtlRestoreContext((PCONTEXT)&ctx->uc, 0);
#else
    SetThreadContext(GetCurrentThread(), &ctx->uc);
#endif
}

void makecontext(ucontext_t* ctx, void(*callback)(), int argc, ...)
{
    va_list arguments;
    size_t* sp = (size_t*)(ctx->stack + ctx->stack_size);
    size_t arg;
    size_t task;
    int n;

    va_start(arguments, argc);

    for (n = 0; n < argc; ++n)
    {
        arg = va_arg(arguments, size_t);
        *(sp - n - 1) = arg;

        if (n == 0)
            task = arg;
    }

    va_end(arguments);

#if defined (_WIN64)
    ctx->uc.Rip = (size_t)callback;
    ctx->uc.Rsp = (size_t)(sp - argc - 1);
    ctx->uc.Rcx = task;
#else
    ctx->uc.Eip = (size_t)callback;
    ctx->uc.Esp = (size_t)(sp - argc - 1);
#endif
}

#if defined (_WIN64)

__declspec(noinline) void __stdcall fix_and_swapcontext(
	ucontext_t* from,
	ucontext_t* to,
	DWORD64 ip,
	DWORD64 sp)
{
	getcontext(from);

	from->uc.Rip = ip;
	from->uc.Rsp = sp;

	setcontext(to);
}

extern void __stdcall swapcontext(ucontext_t* from, const ucontext_t* to);

#else

const int offset_eip = (int)(&((CONTEXT*)0)->Eip);
const int offset_esp = (int)(&((CONTEXT*)0)->Esp);

int __stdcall swapcontext(ucontext_t* oucp, ucontext_t* ucp)
{
    __asm {

        push [oucp]
        call getcontext

        // correct eip
        mov eax, [oucp]
        add eax, offset_eip
        mov edx, offset done
        mov [eax], edx

        // correct esp
        mov eax, [oucp]
        add eax, offset_esp
        mov [eax], esp

        push [ucp]
        call setcontext
done:
    }
}

#endif