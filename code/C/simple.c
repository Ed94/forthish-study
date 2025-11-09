/*
Simple Forth-like: C (ideosyncratic, non-portable), clang, Win32(Windows 11)
*/
#include "duffle.amd64.win32.h"

typedef Enum_(U8, Word_Tag) {
	Word_bye,
	Word_dot,
	Word_dot_s,
	Word_add,
	Word_swap,
	Word_sub,
	Word_word,
	Word_interpret,
	Word_DictionaryNum,
	// Non-dictionary
	Word_Char,
	Word_Number,
	Word_String,
	Word_Num,
};
Str8 str8_word_tag(Word_Tag tag) {
	LP_ Str8 tbl[] = {
		str8("bye"),
		str8("."),
		str8(".s"),
		str8("+"),
		str8("swap"),
		str8("-"),
		str8("word"),
		str8("interpret"),
		str8("__Word_DicationaryNum__"),
		str8("__Char__"),
		str8("__Number__"),
		str8("__String__"),
	};
	return tbl[tag];
}
typedef Struct_(Word) { Word_Tag tag; union {
	Str8   str;
	UTF8   sym;
	U8     num;
};};

typedef Struct_(Stack) { U8 ptr, len, id; };

typedef FStack_(StackArena_K128, U1,   kilo(128));
typedef FStack_(StackArena_M1,   U1,   mega(1));
typedef FStack_(Stack_Word,      Word, mega(1) / S_(Word));

typedef Struct_(ProcessMemory) {
	StackArena_M1 word_strs;
	Stack_Word    stack;
	U8            dictionary[Word_DictionaryNum];
	U1            buff[kilo(1)];
	U8            buff_cursor;
	MS_Handle     std_out;
	MS_Handle     std_in;
};
typedef Struct_(ThreadMemory) {
	StackArena_K128 scratch;
};
global     ProcessMemory pmem;
global LT_ ThreadMemory  thread;

IA_ Slice scratch_64k()        { return (Slice){u8_(thread.scratch.arr), S_(thread.scratch.arr)}; }
IA_ Slice scratch_push(U8 len) { return fstack_push_(thread.scratch, len); }

typedef Opt_(str8_from_u4) { U4 radix, min_digits, digit_group_separator; }; 
IA_ Str8 str8_from_u4_opt(U4 num, Opt_str8_from_u4 o) { if (o.radix == 0) {o.radix = 10;} 
	Info_str8_from_u4 info = str8_from_u4_info(num, o.radix, o.min_digits, o.digit_group_separator);
	return str8_from_u4_buf(scratch_push(128), num, o.radix, o.min_digits, o.digit_group_separator, info);
}
#define str8_from_u4(num, ...) str8_from_u4_opt(num, opt_(str8_from_u4, __VA_ARGS__))

#define str8_fmt_(s, tbl_arr)                   str8_fmt(str8(s), ktl_str8_from_arr(tbl_arr))
#define print_fmt_(s, tbl_arr)            print(str8_fmt(str8(s), ktl_str8_from_arr(tbl_arr)))
#define print_(s)                         print(str8(s))
IA_ Str8 str8_fmt(Str8 fmt, KTL_Str8 tbl) { return str8_fmt_ktl_buf(scratch_push(kilo(64)), tbl, fmt); };
IA_ void print(Str8 s)                    { U4 written; ms_write_console(pmem.std_out, s.ptr, u4_(s.len), & written, 0); }

IA_ Word*r stack_get_r(U8 id) { return & pmem.stack.arr[id]; };
IA_ Word   stack_get(U8 id)   { return   pmem.stack.arr[id]; };
IA_ Word   stack_pop()        { 
	Word w = pmem.stack.arr[pmem.stack.top]; 
	if (w.tag == Word_String){ pmem.word_strs.top -= w.str.len; }
	pmem.stack.arr[pmem.stack.top] = (Word){0}; 
	pmem.stack.top = clamp_bot(0, s8_(-- pmem.stack.top));
	return w; 
}
IA_ Word stack_push(Word w) { return pmem.stack.arr[pmem.stack.top] = w; ++ pmem.stack.top; }

IA_ void stack_push_char(UTF8 sym) { stack_push((Word){Word_Char,   .sym = sym}); }
IA_ void stack_push_str8(Str8 str) { 
	Str8 stored = fstack_push_array(pmem.word_strs, UTF8, str.len);
	slice_copy(stored, str);
	stack_push((Word){Word_String, .str = stored}); 
}
IA_ Str8 serialize_word(Word word){ switch(word.tag) {
case Word_String: return word.str;
case Word_Number: return str8_from_u4(u4_(word.num));
default: return (Str8){};
}}
I_ Str8 serialize_stack(){
	Str8Gen serial = { .ptr = C_(UTF8*, scratch_push(kilo(64)).ptr), .cap = kilo(64), };
	str8gen_append_str8_(& serial, "[ "); 
	for (U4 id = pmem.stack.top; s4_(pmem.stack.top) >= 0; -- id) {
		// \t<word>,\n
		str8gen_append_str8_(& serial, "\t");
		str8gen_append_str8 (& serial, serialize_word(stack_get(id))); 
		str8gen_append_str8_(& serial, ",\n");
	}
	str8gen_append_str8_(& serial, " ]");
	return (Str8){serial.ptr, serial.len};
}

IA_ void xbye()  { process_exit(0); }
IA_ void xdot()  { Str8 str = str8_from_u4(u4_(stack_pop().num)); print(str); }
IA_ void xdots() { defer_rewind(thread.scratch.top) { print(serialize_stack()); } }
IA_ void xadd()  { stack_push((Word){Word_Number, .num = stack_pop().num + stack_pop().num}); }
IA_ void xswap() { U8 x = stack_get(-1).num; stack_get_r(-1)->num = stack_get(-2).num; stack_get_r(-2)->num = x; }
IA_ void xsub()  { xswap(); stack_push((Word){Word_Number, .num = stack_pop().num - stack_pop().num}); }

I_ void xword() {
	Word  want      = stack_pop();
	U8    found     = pmem.buff_cursor;
	U8    found_len = 0;
	while (pmem.buff_cursor < S_(pmem.buff)) {
		U8 x = pmem.buff[pmem.buff_cursor];
		++ pmem.buff_cursor;
		if (want.sym == x || '\n' == x || '\r' == x) {
			break;
		}
		else {
			++ found_len;
		}
	}
	U8 hash = hash64_fnv1a_ret((Slice){u8_(pmem.buff) + found, found_len}, 0);
	for (U8 id = 0; id < Word_DictionaryNum; ++ id)
		if (hash == pmem.dictionary[id]) {
			stack_push((Word){id, {}});
			return; // Identified a dictionary word, not a string
		}
	stack_push_str8((Str8){C_(UTF8*, pmem.buff + found), found_len});
}
I_ void xinterpret() {
	Word word = stack_pop();
	if (word.tag > Word_Num) {
		return;
	}
	if (word.tag < Word_DictionaryNum) switch(word.tag) { // lookup table
	case Word_bye:   xbye();  return;
	case Word_dot:   xdot();  return;
	case Word_dot_s: xdots(); return;
	case Word_add:   xadd();  return;
	case Word_swap:  xswap(); return;
	case Word_sub:   xsub();  return;
	}
	else {
		KTL_Slot_Str8 tbl_arr[] = { {ktl_str8_key("word"), serialize_word(word)}, };
		print_fmt_("<word>?\n", tbl_arr);
	}
}
I_ void ok(){ while(1) {
	fstack_reset(thread.scratch);
	print_("OK ");
	U4 len, read_ok = ms_read_console(pmem.std_in, pmem.buff, S_(pmem.buff), & len, null);
	if (read_ok == false || len == 0) { continue; }
	pmem.buff_cursor = 0;
	while (pmem.buff_cursor < len) {
		stack_push_char(' ');
		xword();
		xinterpret();
		while (len > 0 && (pmem.buff[pmem.buff_cursor - 1] == '\n' || pmem.buff[pmem.buff_cursor - 1] == '\r')) { -- len; }
	}
}}

int main(void)
{
	/*prep*/ {
		pmem.std_out = ms_get_std_handle(MS_STD_OUTPUT);
		pmem.std_in  = ms_get_std_handle(MS_STD_INPUT);
		for (U8 id = Word_bye; id < Word_DictionaryNum; ++ id) {
			hash64_fnv1a(& pmem.dictionary[id], slice_to_ut(str8_word_tag(id)), 0);
		}
	}
	ok();
	return 0;
}

// Artifacts I don't want to delete..
#if 0 /*Prefer to just hash on-site*/
#define      ktl_str8_make_(values) ktl_str8_make((Slice_Str8_A2){values, array_len(values)})
IA_ KTL_Str8 ktl_str8_make(Slice_Str8_A2 values) {
	KTL_Str8 tbl = fstack_push_array(thread.scratch, KTL_Slot_Str8, 1); 
	ktl_populate_slice_a2_str8(& tbl, values);
	return tbl;
}
#endif
