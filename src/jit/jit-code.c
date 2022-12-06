//
//  Copyright (C) 2022  Nick Gasson
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "util.h"
#include "cpustate.h"
#include "debug.h"
#include "hash.h"
#include "ident.h"
#include "jit/jit-priv.h"
#include "opt.h"
#include "thread.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef __MINGW32__
#include <sys/mman.h>
#endif

#ifdef HAVE_CAPSTONE
#include <capstone/capstone.h>
#endif

#define CODECACHE_ALIGN 4096
#define CODECACHE_SIZE  0x100000

typedef struct _code_span {
   code_span_t *next;
   ident_t      name;
   uint8_t     *base;
   size_t       size;
} code_span_t;

typedef struct _patch_list {
   patch_list_t    *next;
   uint8_t         *wptr;
   jit_label_t      label;
   code_patch_fn_t  fn;
} patch_list_t;

typedef struct _code_cache {
   nvc_lock_t   lock;
   uint8_t     *mem;
   size_t       freeptr;
   code_span_t *spans;
#ifdef HAVE_CAPSTONE
   csh          capstone;
#endif
} code_cache_t;

static void code_disassemble(code_cache_t *code, code_span_t *span,
                             uintptr_t mark, struct cpu_state *cpu);

static void code_cache_unwinder(uintptr_t addr, debug_frame_t *frame,
                                void *context)
{
   code_cache_t *code = context;

   const uint8_t *pc = (uint8_t *)addr;
   for (code_span_t *span = code->spans; span; span = span->next) {
      if (pc >= span->base && pc < span->base + span->size) {
         frame->kind = FRAME_VHDL;
         frame->disp = pc - span->base;
         frame->symbol = istr(span->name);
      }
   }
}

static void code_fault_handler(int sig, void *addr, struct cpu_state *cpu,
                               void *context)
{
   code_cache_t *code = context;

   const uint8_t *pc = (uint8_t *)cpu->pc;
   if (pc < code->mem || pc > code->mem + CODECACHE_SIZE)
      return;

   for (code_span_t *span = code->spans; span; span = span->next) {
      if (pc >= span->base && pc < span->base + span->size)
         code_disassemble(code, span, cpu->pc, cpu);
   }
}

code_cache_t *code_cache_new(void)
{
   code_cache_t *code = xcalloc(sizeof(code_cache_t));

   code->mem = map_huge_pages(CODECACHE_ALIGN, CODECACHE_SIZE);

   nvc_memprotect(code->mem, CODECACHE_SIZE, MEM_RWX);

#ifdef HAVE_CAPSTONE
#if defined ARCH_X86_64
   if (cs_open(CS_ARCH_X86, CS_MODE_64, &(code->capstone)) != CS_ERR_OK)
      fatal_trace("failed to init capstone for x86_64");
#elif defined ARCH_ARM64
   if (cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &(code->capstone)) != CS_ERR_OK)
      fatal_trace("failed to init capstone for Arm64");
#else
#error Cannot configure capstone for this architecture
#endif
#endif

   add_fault_handler(code_fault_handler, code);
   debug_add_unwinder(code->mem, CODECACHE_SIZE, code_cache_unwinder, code);

   return code;
}

void code_cache_free(code_cache_t *code)
{
   debug_remove_unwinder(code->mem);
   remove_fault_handler(code_fault_handler, code);

   nvc_munmap(code->mem, CODECACHE_SIZE);

   for (code_span_t *it = code->spans, *tmp; it; it = tmp) {
      tmp = it->next;
      free(it);
   }

#ifdef HAVE_CAPSTONE
   cs_close(&(code->capstone));
#endif

   free(code);
}

static void code_disassemble(code_cache_t *code, code_span_t *span,
                             uintptr_t mark, struct cpu_state *cpu)
{
#ifdef HAVE_CAPSTONE
   cs_insn *insn;
   size_t count = cs_disasm(code->capstone,
                            (const uint8_t *)span->base,
                            span->size, (uintptr_t)span->base, 0, &insn);
   if (count > 0) {
      printf("--");

      const int namelen = ident_len(span->name);
      for (int i = 0; i < 72 - namelen; i++)
         fputc('-', stdout);

      printf(" %s ----\n", istr(span->name));

      size_t j;
      for (j = 0; j < count; j++) {
         char hex1[33], *p = hex1;
         for (size_t k = 0; k < insn[j].size; k++)
            p += checked_sprintf(p, hex1 + sizeof(hex1) - p, "%02x",
                                 insn[j].bytes[k]);

         int col = printf("%-12" PRIx64 " %-16.16s %s %s", insn[j].address,
                          hex1, insn[j].mnemonic, insn[j].op_str);

         if (strlen(hex1) > 16)
            col = printf("\n%15s -%-16s", "", hex1 + 16) - 1;

         if (j + 1 < count && insn[j + 1].address == mark) {
            for (; col < 66; col++)
               fputc(' ', stdout);
            printf("<=============\n");
#ifdef ARCH_X86_64
            const char *names[] = {
               "RAX", "RCX", "RDX", "RBX", "RSP", "RBP", "RSI", "RDI",
               "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15"
            };
            for (int i = 0; i < ARRAY_LEN(names); i++)
               printf("\t%s\t%"PRIx64"\n", names[i], cpu->regs[i]);
#else
            for (int i = 0; i < 32; i++)
               printf("\tR%d\t%"PRIx64"\n", i, cpu->regs[i]);
#endif
         }
         else
            printf("\n");
      }

      cs_free(insn, count);

      for (int i = 0; i < 80; i++)
         fputc('-', stdout);
      printf("\n");
   }
   else
      fatal_trace("disassembly of %s failed", istr(span->name));
#endif
}

code_blob_t *code_blob_new(code_cache_t *code, ident_t name, jit_func_t *f)
{
   code_span_t *span = xcalloc(sizeof(code_span_t));
   span->name = name;
   span->next = code->spans;
   span->base = code->mem + code->freeptr;
   span->size = 0x10000;

   code->spans = span;

   code_blob_t *blob = xcalloc(sizeof(code_blob_t));
   blob->owner = code;
   blob->span  = span;
   blob->func  = f;
   blob->wptr  = span->base;

   assert(f == NULL || name == f->name);

   code->freeptr += span->size;

   return blob;
}

void code_blob_finalise(code_blob_t *blob, jit_entry_fn_t *entry)
{
   code_span_t *span = blob->span;
   span->size = blob->wptr - span->base;

   if (opt_get_verbose(OPT_ASM_VERBOSE, istr(span->name))) {
      color_printf("\n$bold$$blue$");
      code_disassemble(blob->owner, span, 0, NULL);
      color_printf("$$\n");
   }

   __builtin___clear_cache((char *)span->base, (char *)blob->wptr);

   store_release(entry, (jit_entry_fn_t)span->base);

   if (blob->patches != NULL)
      fatal_trace("not all labels in %s were patched", istr(span->name));

   ihash_free(blob->labels);
   free(blob);
}

void code_blob_emit(code_blob_t *blob, const uint8_t *bytes, size_t len)
{
   if (blob->wptr + len >= blob->span->base + blob->span->size)
      fatal_trace("JIT code buffer too small %zu", blob->span->size);

   for (size_t i = 0; i < len; i++)
      *(blob->wptr++) = bytes[i];
}

void code_blob_mark(code_blob_t *blob, jit_label_t label)
{
   if (blob->labels == NULL)
      blob->labels = ihash_new(256);

   ihash_put(blob->labels, label, blob->wptr);

   for (patch_list_t **p = &(blob->patches); *p; ) {
      if ((*p)->label == label) {
         patch_list_t *next = (*p)->next;
         (*(*p)->fn)(blob, label, (*p)->wptr, blob->wptr);
         free(*p);
         *p = next;
      }
      else
         p = &((*p)->next);
   }
}

void code_blob_patch(code_blob_t *blob, jit_label_t label, code_patch_fn_t fn)
{
   void *ptr = NULL;
   if (blob->labels != NULL && (ptr = ihash_get(blob->labels, label)))
      (*fn)(blob, label, blob->wptr, ptr);
   else {
      patch_list_t *new = xmalloc(sizeof(patch_list_t));
      new->next  = blob->patches;
      new->fn    = fn;
      new->label = label;
      new->wptr  = blob->wptr;

      blob->patches = new;
   }
}
