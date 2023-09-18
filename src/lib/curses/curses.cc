#include <v8.h>
#include "macros.h"

#include <ncurses.h>
#include <unistd.h>

namespace {
JS_METHOD(_curses) {
	ASSERT_CONSTRUCTOR;
	if (args.Length()) {
		JS_TYPE_ERROR("Bad argument count. Use 'new Curses();'");
		return;
	}
	initscr();
}

JS_METHOD(_raw) {
	if (args.Length()) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.raw();'");
		return;
	}
	raw();
	args.GetReturnValue().SetUndefined();
}

JS_METHOD(_cbreak) {
	if (args.Length()) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.cbreak();'");
		return;
	}
	cbreak();
	args.GetReturnValue().SetUndefined();
}

JS_METHOD(_echo) {
	if (args.Length()) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.echo();'");
		return;
	}
	echo();
	args.GetReturnValue().SetUndefined();
}

JS_METHOD(_noecho) {
	if (args.Length()) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.noecho();'");
		return;
	}
	noecho();
	args.GetReturnValue().SetUndefined();
}

JS_METHOD(_keypad) {
	if (args.Length()) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.keypad();'");
		return;
	}
	keypad(stdscr, true);
	args.GetReturnValue().SetUndefined();
}

JS_METHOD(_halfdelay) {
	if (args.Length() != 1) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.halfdelay(delay);'");
		return;
	}
	int delay = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	halfdelay(delay);
	args.GetReturnValue().SetUndefined();
}

JS_METHOD(_refresh) {
	if (args.Length()) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.refresh();'");
		return;
	}
	refresh();
	args.GetReturnValue().SetUndefined();
}

JS_METHOD(_printw) {
	if (args.Length() != 1) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.addstr(str);'");
		return;
	}
	v8::String::Utf8Value str(JS_ISOLATE, args[0]);
	printw(*str);
	args.GetReturnValue().SetUndefined();
}

JS_METHOD(_mvprintw) {
	if (args.Length() != 3) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.mvaddstr(y, x, str);'");
		return;
	}
	int y = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int x = args[1]->Int32Value(JS_CONTEXT).ToChecked();

	v8::String::Utf8Value str(JS_ISOLATE, args[2]);

	mvprintw(y, x, *str);

	args.GetReturnValue().SetUndefined();
}

JS_METHOD(_move) {
	if (args.Length() != 2) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.move(y, x);'");
		return;
	}
	int y = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int x = args[1]->Int32Value(JS_CONTEXT).ToChecked();

	move(y, x);

	args.GetReturnValue().SetUndefined();
}

JS_METHOD(_get_row_size) {
	if (args.Length()) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.getRowSize();'");
		return;
	}
	int y, x;
	getmaxyx(stdscr, y, x);
	args.GetReturnValue().Set(JS_INT(y));
}

JS_METHOD(_get_column_size) {
	if (args.Length()) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.getColumnSize();'");
		return;
	}
	int x, y;
	getmaxyx(stdscr, y, x);
	args.GetReturnValue().Set(JS_INT(x));
}

JS_METHOD(_get_cur_y) {
	if (args.Length()) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.getCurY();'");
		return;
	}
	int y, x;
	getyx(stdscr, y, x);
	args.GetReturnValue().Set(JS_INT(y));
}

JS_METHOD(_get_cur_x) {
	if (args.Length()) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.getCurX();'");
		return;
	}
	int x, y;
	getyx(stdscr, y, x);
	args.GetReturnValue().Set(JS_INT(x));
}

JS_METHOD(_usleep) {
	if (args.Length() != 1) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.getCurX();'");
		return;
	}
	int time = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	usleep(time);
	args.GetReturnValue().SetUndefined();
}

JS_METHOD(_getch) {
	if (args.Length()) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.getCurX();'");
		return;
	}
	args.GetReturnValue().Set(JS_INT(getch()));
}

JS_METHOD(_end) {
	if (args.Length()) {
		JS_TYPE_ERROR("Bad argument count. Use 'curses.end();'");
		return;
	}
	endwin();
	args.GetReturnValue().SetUndefined();
}


}

SHARED_INIT() {
	v8::Local<v8::FunctionTemplate> ft = v8::FunctionTemplate::New(JS_ISOLATE, _curses);
	ft->SetClassName(JS_STR("Curses"));

	v8::Local<v8::ObjectTemplate> it = ft->InstanceTemplate();
	it->SetInternalFieldCount(1);

	v8::Local<v8::ObjectTemplate> pt = ft->PrototypeTemplate();

	/**
	 * Prototype methods (new Curses().*)
	 */
	pt->Set(JS_ISOLATE, "Raw",           v8::FunctionTemplate::New(JS_ISOLATE, _raw));
	pt->Set(JS_ISOLATE, "CBreak",        v8::FunctionTemplate::New(JS_ISOLATE, _cbreak));
	pt->Set(JS_ISOLATE, "Echo",          v8::FunctionTemplate::New(JS_ISOLATE, _echo));
	pt->Set(JS_ISOLATE, "Noecho",        v8::FunctionTemplate::New(JS_ISOLATE, _noecho));
	pt->Set(JS_ISOLATE, "Keypad",        v8::FunctionTemplate::New(JS_ISOLATE, _keypad));
	pt->Set(JS_ISOLATE, "Halfdelay",     v8::FunctionTemplate::New(JS_ISOLATE, _halfdelay));
	pt->Set(JS_ISOLATE, "Refresh",       v8::FunctionTemplate::New(JS_ISOLATE, _refresh));
	pt->Set(JS_ISOLATE, "Printw",        v8::FunctionTemplate::New(JS_ISOLATE, _printw));
	pt->Set(JS_ISOLATE, "MVPrintw",      v8::FunctionTemplate::New(JS_ISOLATE, _mvprintw));
	pt->Set(JS_ISOLATE, "Move",          v8::FunctionTemplate::New(JS_ISOLATE, _move));
	pt->Set(JS_ISOLATE, "GetRowSize",    v8::FunctionTemplate::New(JS_ISOLATE, _get_row_size));
	pt->Set(JS_ISOLATE, "GetColumnSize", v8::FunctionTemplate::New(JS_ISOLATE, _get_column_size));
	pt->Set(JS_ISOLATE, "GetCurY",       v8::FunctionTemplate::New(JS_ISOLATE, _get_cur_y));
	pt->Set(JS_ISOLATE, "GetCurX",       v8::FunctionTemplate::New(JS_ISOLATE, _get_cur_x));
	pt->Set(JS_ISOLATE, "Usleep",        v8::FunctionTemplate::New(JS_ISOLATE, _usleep));
	pt->Set(JS_ISOLATE, "Getch",         v8::FunctionTemplate::New(JS_ISOLATE, _getch));
	pt->Set(JS_ISOLATE, "End",           v8::FunctionTemplate::New(JS_ISOLATE, _end));

	(void)exports->Set(JS_CONTEXT, JS_STR("Curses"), ft->GetFunction(JS_CONTEXT).ToLocalChecked());
}
