//
//  Copyright (C) 2022-2023  Nick Gasson
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

#include "test_util.h"
#include "ident.h"
#include "jit/jit.h"
#include "option.h"
#include "phase.h"
#include "rt/model.h"
#include "rt/structs.h"
#include "scan.h"
#include "type.h"

START_TEST(test_basic1)
{
   input_from_file(TESTDIR "/model/basic1.vhd");

   tree_t top = run_elab();
   fail_if(top == NULL);

   lower_unit(top, NULL);

   jit_t *j = jit_new();
   jit_enable_runtime(j, true);

   rt_model_t *m = model_new(top, j);

   tree_t b0 = tree_stmt(top, 0);
   tree_t x = tree_decl(b0, 1);
   fail_unless(tree_kind(x) == T_SIGNAL_DECL);
   tree_t y = tree_decl(b0, 2);
   fail_unless(tree_kind(y) == T_SIGNAL_DECL);

   rt_scope_t *root = find_scope(m, b0);
   fail_if(root == NULL);

   model_reset(m);

   rt_signal_t *xs = find_signal(root, x);
   fail_if(xs == NULL);

   ck_assert_int_eq(xs->n_nexus, 1);
   ck_assert_int_eq(xs->nexus.width, 1);
   ck_assert_int_eq(xs->shared.size, 4);

   const int32_t *xp = signal_value(xs);
   ck_assert_int_eq(*xp, 42);

   rt_signal_t *ys = find_signal(root, y);
   fail_if(ys == NULL);

   const int8_t *yp = signal_value(ys);
   ck_assert_int_eq(*yp, 1);

   model_free(m);
   jit_free(j);

   fail_if_errors();
}
END_TEST

START_TEST(test_index1)
{
   input_from_file(TESTDIR "/model/index1.vhd");

   tree_t top = run_elab();
   fail_if(top == NULL);

   lower_unit(top, NULL);

   jit_t *j = jit_new();
   jit_enable_runtime(j, true);

   rt_model_t *m = model_new(top, j);

   tree_t b0 = tree_stmt(top, 0);
   tree_t s1 = search_decls(b0, ident_new("S1"), 0);
   fail_if(s1 == NULL);
   tree_t s2 = search_decls(b0, ident_new("S2"), 0);
   fail_if(s2 == NULL);

   rt_scope_t *root = find_scope(m, b0);
   fail_if(root == NULL);

   model_reset(m);

   rt_signal_t *ss1 = find_signal(root, s1);
   fail_if(ss1 == NULL);

   rt_signal_t *ss2 = find_signal(root, s2);
   fail_if(ss2 == NULL);

   model_run(m, UINT64_MAX);

   ck_assert_int_eq(ss1->n_nexus, 21);
   ck_assert_int_eq(ss1->nexus.width, 8);
   ck_assert_ptr_nonnull(ss1->index);
   ck_assert_int_eq(ss1->index->how, 3);
   ck_assert_ptr_eq(ss1->index->nexus[0], &(ss1->nexus));
   ck_assert_ptr_nonnull(ss1->index->nexus[20]);
   ck_assert_ptr_null(ss1->index->nexus[21]);

   ck_assert_int_eq(ss2->n_nexus, 41);
   ck_assert_int_eq(ss2->nexus.width, 10);
   ck_assert_ptr_nonnull(ss2->index);
   ck_assert_int_eq(ss2->index->how, -10);
   ck_assert_ptr_eq(ss2->index->nexus[0], &(ss2->nexus));
   ck_assert_ptr_nonnull(ss2->index->nexus[40]);
   ck_assert_ptr_null(ss2->index->nexus[41]);

   model_free(m);
   jit_free(j);

   fail_if_errors();
}
END_TEST

START_TEST(test_alias1)
{
   input_from_file(TESTDIR "/model/alias1.vhd");

   tree_t top = run_elab();
   fail_if(top == NULL);

   lower_unit(top, NULL);

   jit_t *j = jit_new();
   jit_enable_runtime(j, true);

   rt_model_t *m = model_new(top, j);
   model_reset(m);

   tree_t b0 = tree_stmt(top, 0);

   rt_scope_t *root = find_scope(m, b0);
   fail_if(root == NULL);

   model_run(m, UINT64_MAX);

   model_free(m);
   jit_free(j);

   fail_if_errors();
}
END_TEST

START_TEST(test_fast1)
{
   input_from_file(TESTDIR "/model/fast1.vhd");

   tree_t top = run_elab();
   fail_if(top == NULL);

   lower_unit(top, NULL);

   jit_t *j = jit_new();
   jit_enable_runtime(j, true);

   rt_model_t *m = model_new(top, j);
   model_reset(m);

   tree_t b0 = tree_stmt(top, 0);

   rt_scope_t *root = find_scope(m, b0);
   fail_if(root == NULL);

   tree_t x = search_decls(b0, ident_new("X"), 0);
   fail_if(x == NULL);

   rt_signal_t *sx = find_signal(root, x);
   fail_if(sx == NULL);
   fail_unless(sx->n_nexus == 1);
   fail_unless(sx->nexus.flags & NET_F_FAST_DRIVER);

   fail_if(model_step(m));

   fail_unless(sx->nexus.flags & NET_F_FAST_DRIVER);
   fail_unless(sx->nexus.sources.fastqueued);
   ck_assert_int_eq(*(int32_t *)sx->shared.data, 0);

   fail_if(model_step(m));

   unsigned deltas;
   ck_assert_int_eq(model_now(m, &deltas), 0);
   ck_assert_int_eq(deltas, 1);

   fail_if(sx->nexus.flags & NET_F_FAST_DRIVER);
   fail_if(sx->nexus.sources.fastqueued);
   ck_assert_int_eq(*(int32_t *)sx->shared.data, 1);

   fail_unless(model_step(m));

   ck_assert_int_eq(model_now(m, &deltas), 1000000);
   ck_assert_int_eq(deltas, 0);

   model_free(m);
   jit_free(j);

   fail_if_errors();
}
END_TEST

START_TEST(test_stateless1)
{
   input_from_file(TESTDIR "/model/stateless1.vhd");

   tree_t top = run_elab();
   fail_if(top == NULL);

   lower_unit(top, NULL);

   jit_t *j = jit_new();
   jit_enable_runtime(j, true);

   rt_model_t *m = model_new(top, j);
   model_reset(m);

   tree_t b0 = tree_stmt(top, 0);
   ck_assert_int_eq(tree_stmts(b0), 2);

   rt_scope_t *root = find_scope(m, b0);
   fail_if(root == NULL);

   rt_proc_t *p1 = find_proc(root, tree_stmt(b0, 0));
   ck_assert_ptr_nonnull(p1);
   ck_assert_str_eq(istr(p1->name), ":stateless1:p1");
   ck_assert_ptr_null(*mptr_get(p1->privdata));

   rt_proc_t *p2 = find_proc(root, tree_stmt(b0, 1));
   ck_assert_ptr_nonnull(p2);
   ck_assert_str_eq(istr(p2->name), ":stateless1:p2");
   ck_assert_ptr_nonnull(*mptr_get(p2->privdata));

   model_free(m);
   jit_free(j);

   fail_if_errors();
}
END_TEST

Suite *get_model_tests(void)
{
   Suite *s = suite_create("model");

   TCase *tc = nvc_unit_test();
   tcase_add_test(tc, test_basic1);
   tcase_add_test(tc, test_index1);
   tcase_add_test(tc, test_alias1);
   tcase_add_test(tc, test_fast1);
   tcase_add_test(tc, test_stateless1);
   suite_add_tcase(s, tc);

   return s;
}
