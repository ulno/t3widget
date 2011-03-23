/* Copyright (C) 2010 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "dialogs/finddialog.h"
#include "findcontext.h"
#include "log.h"
#include "main.h"

using namespace std;

namespace t3_widget {

#define FIND_DIALOG_WIDTH 50
#define FIND_DIALOG_HEIGHT 10

#warning FIXME: keep (limited) history

find_dialog_t::find_dialog_t(bool _replace) :
	dialog_t(FIND_DIALOG_HEIGHT + 2 * _replace, FIND_DIALOG_WIDTH, _replace ? "Replace" : "Find"),
	state(find_flags_t::ICASE | find_flags_t::WRAP), replace(_replace)
{
	smart_label_t *find_label,
		*replace_label,
		*whole_word_label,
		*match_case_label,
		*regex_label,
		*wrap_label,
		*transform_backslash_label,
		*reverse_direction_label;
	button_t *find_button, *cancel_button, *in_selection_button, *replace_all_button;

	find_label = new smart_label_t(this, "S_earch for;eE", true);
	find_label->set_position(1, 2);
	find_line = new text_field_t(this);
	find_line->set_anchor(find_label, T3_PARENT(T3_ANCHOR_TOPRIGHT) | T3_CHILD(T3_ANCHOR_TOPLEFT));
	find_line->set_size(None, FIND_DIALOG_WIDTH - find_label->get_width() - 5);
	find_line->set_position(0, 1);
	find_line->set_label(find_label);
	find_line->connect_activate(sigc::mem_fun0(this, &find_dialog_t::find_activated));

	if (replace) {
		replace_label = new smart_label_t(this, "Re_place with;pP", true);
		replace_label->set_position(2, 2);
		replace_line = new text_field_t(this);
		replace_line->set_anchor(replace_label, T3_PARENT(T3_ANCHOR_TOPRIGHT) | T3_CHILD(T3_ANCHOR_TOPLEFT));
		replace_line->set_size(None, FIND_DIALOG_WIDTH - replace_label->get_width() - 5);
		replace_line->set_position(0, 1);
		replace_line->set_label(replace_label);
		replace_line->connect_activate(sigc::mem_fun0(this, &find_dialog_t::find_activated));
	}

	whole_word_checkbox = new checkbox_t(this);
	whole_word_checkbox->set_position(2 + replace, 2);
	whole_word_label = new smart_label_t(this, "Match whole word _only;oO");
	whole_word_label->set_anchor(whole_word_checkbox, T3_PARENT(T3_ANCHOR_TOPRIGHT) | T3_CHILD(T3_ANCHOR_TOPLEFT));
	whole_word_label->set_position(0, 1);
	whole_word_checkbox->set_label(whole_word_label);
	whole_word_checkbox->connect_toggled(sigc::mem_fun(this, &find_dialog_t::whole_word_toggled));
	whole_word_checkbox->connect_move_focus_up(sigc::mem_fun(this, &find_dialog_t::focus_previous));
	whole_word_checkbox->connect_move_focus_down(sigc::mem_fun(this, &find_dialog_t::focus_next));
	whole_word_checkbox->connect_activate(sigc::mem_fun0(this, &find_dialog_t::find_activated));
	match_case_checkbox = new checkbox_t(this);
	match_case_checkbox->set_position(3 + replace, 2);
	match_case_label = new smart_label_t(this, "_Match case;mM");
	match_case_label->set_anchor(match_case_checkbox, T3_PARENT(T3_ANCHOR_TOPRIGHT) | T3_CHILD(T3_ANCHOR_TOPLEFT));
	match_case_label->set_position(0, 1);
	match_case_checkbox->set_label(match_case_label);
	match_case_checkbox->connect_toggled(sigc::mem_fun(this, &find_dialog_t::icase_toggled));
	match_case_checkbox->connect_move_focus_up(sigc::mem_fun(this, &find_dialog_t::focus_previous));
	match_case_checkbox->connect_move_focus_down(sigc::mem_fun(this, &find_dialog_t::focus_next));
	match_case_checkbox->connect_activate(sigc::mem_fun0(this, &find_dialog_t::find_activated));
	regex_checkbox = new checkbox_t(this);
	regex_checkbox->set_position(4 + replace, 2);
	regex_label = new smart_label_t(this, "Regular e_xpression;xX");
	regex_label->set_anchor(regex_checkbox, T3_PARENT(T3_ANCHOR_TOPRIGHT) | T3_CHILD(T3_ANCHOR_TOPLEFT));
	regex_label->set_position(0, 1);
	regex_checkbox->set_label(regex_label);
	regex_checkbox->connect_toggled(sigc::mem_fun(this, &find_dialog_t::regex_toggled));
	regex_checkbox->connect_move_focus_up(sigc::mem_fun(this, &find_dialog_t::focus_previous));
	regex_checkbox->connect_move_focus_down(sigc::mem_fun(this, &find_dialog_t::focus_next));
	regex_checkbox->connect_activate(sigc::mem_fun0(this, &find_dialog_t::find_activated));
	wrap_checkbox = new checkbox_t(this);
	wrap_checkbox->set_position(5 + replace, 2);
	wrap_label = new smart_label_t(this, "_Wrap;wW");
	wrap_label->set_anchor(wrap_checkbox, T3_PARENT(T3_ANCHOR_TOPRIGHT) | T3_CHILD(T3_ANCHOR_TOPLEFT));
	wrap_label->set_position(0, 1);
	wrap_checkbox->set_label(wrap_label);
	wrap_checkbox->connect_toggled(sigc::mem_fun(this, &find_dialog_t::wrap_toggled));
	wrap_checkbox->connect_move_focus_up(sigc::mem_fun(this, &find_dialog_t::focus_previous));
	wrap_checkbox->connect_move_focus_down(sigc::mem_fun(this, &find_dialog_t::focus_next));
	wrap_checkbox->connect_activate(sigc::mem_fun0(this, &find_dialog_t::find_activated));
	transform_backslash_checkbox = new checkbox_t(this);
	transform_backslash_checkbox->set_position(6 + replace, 2);
	transform_backslash_label = new smart_label_t(this, "_Transform backslash expressions;tT");
	transform_backslash_label->set_anchor(transform_backslash_checkbox, T3_PARENT(T3_ANCHOR_TOPRIGHT) | T3_CHILD(T3_ANCHOR_TOPLEFT));
	transform_backslash_label->set_position(0, 1);
	transform_backslash_checkbox->set_label(transform_backslash_label);
	transform_backslash_checkbox->connect_toggled(sigc::mem_fun(this, &find_dialog_t::transform_backslash_toggled));
	transform_backslash_checkbox->connect_move_focus_up(sigc::mem_fun(this, &find_dialog_t::focus_previous));
	transform_backslash_checkbox->connect_move_focus_down(sigc::mem_fun(this, &find_dialog_t::focus_next));
	transform_backslash_checkbox->connect_activate(sigc::mem_fun0(this, &find_dialog_t::find_activated));
	reverse_direction_checkbox = new checkbox_t(this);
	reverse_direction_checkbox->set_position(7 + replace, 2);
	reverse_direction_label = new smart_label_t(this, "Re_verse direction;vV");
	reverse_direction_label->set_anchor(reverse_direction_checkbox, T3_PARENT(T3_ANCHOR_TOPRIGHT) | T3_CHILD(T3_ANCHOR_TOPLEFT));
	reverse_direction_label->set_position(0, 1);
	reverse_direction_checkbox->set_label(reverse_direction_label);
	reverse_direction_checkbox->connect_toggled(sigc::mem_fun(this, &find_dialog_t::backward_toggled));
	reverse_direction_checkbox->connect_move_focus_up(sigc::mem_fun(this, &find_dialog_t::focus_previous));
	reverse_direction_checkbox->connect_move_focus_down(sigc::mem_fun(this, &find_dialog_t::focus_next));
	reverse_direction_checkbox->connect_activate(sigc::mem_fun0(this, &find_dialog_t::find_activated));

	cancel_button = new button_t(this, "_Cancel;cC");
	cancel_button->set_anchor(this, T3_PARENT(T3_ANCHOR_BOTTOMRIGHT) | T3_CHILD(T3_ANCHOR_BOTTOMRIGHT));
	cancel_button->set_position(-1, -2);
	cancel_button->connect_activate(sigc::mem_fun(this, &find_dialog_t::hide));
	cancel_button->connect_move_focus_up(sigc::mem_fun(this, &find_dialog_t::focus_previous));
	cancel_button->connect_move_focus_up(sigc::mem_fun(this, &find_dialog_t::focus_previous));
	cancel_button->connect_move_focus_left(sigc::mem_fun(this, &find_dialog_t::focus_previous));
	find_button = new button_t(this, "_Find;fF", true);
	find_button->set_anchor(cancel_button, T3_PARENT(T3_ANCHOR_TOPLEFT) | T3_CHILD(T3_ANCHOR_TOPRIGHT));
	find_button->set_position(0, -2);
	find_button->connect_activate(sigc::mem_fun0(this, &find_dialog_t::find_activated));
	find_button->connect_move_focus_right(sigc::mem_fun(this, &find_dialog_t::focus_next));
	find_button->connect_move_focus_up(sigc::mem_fun(this, &find_dialog_t::focus_previous));
	if (replace) {
		find_button->connect_move_focus_up(sigc::mem_fun(this, &find_dialog_t::focus_previous));

		replace_all_button = new button_t(this, "_All;aA");
		replace_all_button->set_anchor(find_button, T3_PARENT(T3_ANCHOR_TOPLEFT) | T3_CHILD(T3_ANCHOR_TOPRIGHT));
		replace_all_button->set_position(0, -2);
		replace_all_button->connect_activate(sigc::bind(sigc::mem_fun1(this, &find_dialog_t::find_activated), find_action_t::REPLACE_ALL));
		replace_all_button->connect_move_focus_up(sigc::mem_fun(this, &find_dialog_t::focus_previous));
		replace_all_button->connect_move_focus_up(sigc::mem_fun(this, &find_dialog_t::focus_previous));
		replace_all_button->connect_move_focus_down(sigc::mem_fun(this, &find_dialog_t::focus_next));
		replace_all_button->connect_move_focus_down(sigc::mem_fun(this, &find_dialog_t::focus_next));
		replace_all_button->connect_move_focus_left(sigc::mem_fun(this, &find_dialog_t::focus_next));
		replace_all_button->connect_move_focus_right(sigc::mem_fun(this, &find_dialog_t::focus_previous));

		in_selection_button = new button_t(this, "In _Selection;sS");
		in_selection_button->set_anchor(this, T3_PARENT(T3_ANCHOR_BOTTOMRIGHT) | T3_CHILD(T3_ANCHOR_BOTTOMRIGHT));
		in_selection_button->set_position(-2, -2);
		in_selection_button->connect_activate(sigc::bind(sigc::mem_fun1(this, &find_dialog_t::find_activated), find_action_t::REPLACE_IN_SELECTION));
		in_selection_button->connect_move_focus_up(sigc::mem_fun(this, &find_dialog_t::focus_previous));
		in_selection_button->connect_move_focus_down(sigc::mem_fun(this, &find_dialog_t::focus_next));
		in_selection_button->connect_move_focus_down(sigc::mem_fun(this, &find_dialog_t::focus_next));
		in_selection_button->connect_move_focus_right(sigc::mem_fun(this, &find_dialog_t::focus_previous));
	}

	widgets.push_back(find_label);
	widgets.push_back(find_line);
	if (replace) {
		widgets.push_back(replace_label);
		widgets.push_back(replace_line);
	}
	widgets.push_back(whole_word_checkbox);
	widgets.push_back(whole_word_label);
	widgets.push_back(match_case_label);
	widgets.push_back(match_case_checkbox);
	widgets.push_back(regex_label);
	widgets.push_back(regex_checkbox);
	widgets.push_back(wrap_label);
	widgets.push_back(wrap_checkbox);
	widgets.push_back(transform_backslash_label);
	widgets.push_back(transform_backslash_checkbox);
	widgets.push_back(reverse_direction_label);
	widgets.push_back(reverse_direction_checkbox);
	if (replace) {
		widgets.push_back(in_selection_button);
		widgets.push_back(replace_all_button);
	}
	widgets.push_back(find_button);
	widgets.push_back(cancel_button);
}

bool find_dialog_t::set_size(optint height, optint width) {
	(void) height;
	(void) width;
	return true;
}

void find_dialog_t::set_text(const string *str) {
	find_line->set_text(str);
}

#define TOGGLED_CALLBACK(name, flag_name) void find_dialog_t::name##_toggled(void) { state ^= find_flags_t::flag_name; }
TOGGLED_CALLBACK(backward, BACKWARD)
TOGGLED_CALLBACK(icase, ICASE)
TOGGLED_CALLBACK(regex, REGEX)
TOGGLED_CALLBACK(wrap, WRAP)
TOGGLED_CALLBACK(transform_backslash, TRANSFROM_BACKSLASH)
TOGGLED_CALLBACK(whole_word, WHOLE_WORD)
#undef TOGGLED_CALLBACK

void find_dialog_t::find_activated(void) {
	find_activated(find_action_t::FIND);
}

void find_dialog_t::find_activated(find_action_t action) {
	try {
		finder_t context(find_line->get_text(), state, replace ? replace_line->get_text() : NULL);
		hide();
		activate(action, &context);
	} catch (const char *message) {
		message_dialog.set_message(message);
		message_dialog.center_over(this);
		message_dialog.show();
	}
}

//============= replace_buttons_dialog_t ===============

replace_buttons_dialog_t::replace_buttons_dialog_t(void) : dialog_t(3, 60, "Replace") {
	button_t *find_button, *cancel_button, *replace_all_button, *replace_button;
	int dialog_width;

	replace_all_button = new button_t(this, "_All;aA");
	replace_all_button->set_position(1, 2);
	replace_all_button->connect_activate(sigc::bind(activate.make_slot(), find_action_t::REPLACE_ALL));
	replace_all_button->connect_move_focus_right(sigc::mem_fun(this, &replace_buttons_dialog_t::focus_next));

	replace_button = new button_t(this, "_Replace;rR");
	replace_button->set_anchor(replace_all_button, T3_PARENT(T3_ANCHOR_TOPRIGHT) | T3_CHILD(T3_ANCHOR_TOPLEFT));
	replace_button->set_position(0, 2);
	replace_button->connect_activate(sigc::bind(activate.make_slot(), find_action_t::REPLACE));
	replace_button->connect_move_focus_left(sigc::mem_fun(this, &replace_buttons_dialog_t::focus_previous));
	replace_button->connect_move_focus_right(sigc::mem_fun(this, &replace_buttons_dialog_t::focus_next));

	find_button = new button_t(this, "_Find;fF");
	find_button->set_anchor(replace_button, T3_PARENT(T3_ANCHOR_TOPRIGHT) | T3_CHILD(T3_ANCHOR_TOPLEFT));
	find_button->set_position(0, 2);
	find_button->connect_activate(sigc::bind(activate.make_slot(), find_action_t::FIND));
	find_button->connect_move_focus_left(sigc::mem_fun(this, &replace_buttons_dialog_t::focus_previous));
	find_button->connect_move_focus_right(sigc::mem_fun(this, &replace_buttons_dialog_t::focus_next));

	cancel_button = new button_t(this, "_Cancel;cC");
	cancel_button->set_anchor(find_button, T3_PARENT(T3_ANCHOR_TOPRIGHT) | T3_CHILD(T3_ANCHOR_TOPLEFT));
	cancel_button->set_position(0, 2);
	cancel_button->connect_activate(sigc::mem_fun(this, &find_dialog_t::hide));
	cancel_button->connect_move_focus_left(sigc::mem_fun(this, &replace_buttons_dialog_t::focus_previous));

	widgets.push_back(replace_all_button);
	widgets.push_back(replace_button);
	widgets.push_back(find_button);
	widgets.push_back(cancel_button);

	dialog_width = replace_all_button->get_width() + replace_button->get_width() +
		find_button->get_width() + cancel_button->get_width() + 10;
	dialog_t::set_size(None, dialog_width);
}

}; // namespace
