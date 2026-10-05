// SPDX-License-Identifier: Apache-2.0
#pragma once
class Fl_Table;
// FLTK 1.4's Fl_Table needs the same workarounds for every table we build, so they
// live in one place instead of being rediscovered per panel.
void table_prepare(Fl_Table *t);
// Fl_Table 1.4 does not translate wheel events itself.
void table_wheel(Fl_Table *t, int rows);
