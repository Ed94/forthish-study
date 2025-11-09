/*
Simple Forth-like: C (ideosyncratic, non-portable), clang, Win32(Windows 11)
*/
#include "duffle.amd64.win32.h"

typedef Struct_(Stack) { U8 ptr, len, id; };
#define FStack_(name, type, width) Struct_(name) { type arr[width]; U8 top; }

typedef void WordX();

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
	UTF8   sym;
	U8     num;
	Str8   str;
};};

typedef FStack_(Stack_Word, Word, mega(1) / S_(Word));
typedef Struct_(ProcessMemory) {
	FArena     word_store; U1 scratch_word_store[mega(1)];
	Stack_Word stack;
	U8         dictionary[Word_DictionaryNum];
	U1         buff[kilo(1)];
	U8         buff_cursor;
	MS_Handle  std_out;
	MS_Handle  std_in;
};
typedef Struct_(ThreadMemory) {
	FArena scratch; U1 scratch_64k[kilo(128)];
};
global     ProcessMemory pmem;
global LT_ ThreadMemory  thread;

IA_ Slice scratch_64k()        { return (Slice){u8_(thread.scratch_64k), S_(thread.scratch_64k)}; }
IA_ Slice scratch_push(U8 len) { return farena_push_(& thread.scratch, len); }

#define ktl_str8_make_(values) ktl_str8_make((Slice_Str8_A2){values, array_len(values)})
IA_ KTL_Str8 ktl_str8_make(Slice_Str8_A2 values) {
	KTL_Str8 tbl = farena_push_array(& thread.scratch, KTL_Slot_Str8, 1); 
	ktl_populate_slice_a2_str8(& tbl, values);
	return tbl;
}

#define str8_from_u4(num, ...) str8_from_u4_opt(num, opt_(str8_from_u4, __VA_ARGS__))
typedef Opt_(str8_from_u4) { U4 radix, min_digits, digit_group_separator; }; 
IA_ Str8 str8_from_u4_opt(U4 num, Opt_str8_from_u4 o) { 
	if (o.radix == 0) {o.radix = 10;} 
	Info_str8_from_u4 info = str8_from_u4_info(num, o.radix, o.min_digits, o.digit_group_separator);
	return str8_from_u4_buf(scratch_push(128), num, o.radix, o.min_digits, o.digit_group_separator, info);
}
IA_ Str8 str8_fmt(Str8 fmt, KTL_Str8 tbl) { return str8_fmt_ktl_buf(scratch_push(kilo(64)), tbl, fmt); };
IA_ void print(Str8 s)                    { U4 written; ms_write_console(pmem.std_out, s.ptr, u4_(s.len), & written, 0); }

IA_ Word*r stack_get_r(U8 id) { return & pmem.stack.arr[id]; };
IA_ Word   stack_get(U8 id)   { return pmem.stack.arr[id]; };
IA_ Word   stack_pop()        { return pmem.stack.arr[pmem.stack.top]; }
IA_ Word   stack_push(Word w) { ++ pmem.stack.top; return pmem.stack.arr[pmem.stack.top] = w; }

IA_ void stack_push_char(UTF8 sym) { stack_push((Word){Word_Char,   .sym = sym}); }
IA_ void stack_push_str8(Str8 str) { stack_push((Word){Word_String, .str = farena_push_array(& pmem.word_store, UTF8, str.len)}); }
Str8 serialize_word(Word word){ switch(word.tag) {
case Word_String: return word.str;
case Word_Number: return str8_from_u4(u4_(word.num));
default: return (Str8){};
}}
I_ Str8 serialize_stack(){
	Str8Gen serial = { .ptr = C_(UTF8*, scratch_push(kilo(64)).ptr), .cap = kilo(64), };
	str8gen_append_str8(& serial, str8("[ ")); 
	for (U4 id = pmem.stack.top; s4_(pmem.stack.top) >= 0; -- id) {
		// \t<word>,\n
		str8gen_append_str8(& serial, str8("\t"));
		str8gen_append_str8(& serial, serialize_word(stack_get(id))); 
		str8gen_append_str8(& serial, str8(",\n"));
	}
	str8gen_append_str8(& serial, str8(" ]"));
	return (Str8){serial.ptr, serial.len};
}

IA_ void xbye()  { process_exit(0); }
IA_ void xdot()  { Str8 str = str8_from_u4(u4_(stack_pop().num)); print(str); }
IA_ void xdots() { U8 sp = farena_save(thread.scratch); print(serialize_stack()); farena_rewind(& thread.scratch, sp); }
IA_ void xadd()  { stack_push((Word){Word_Number, .num = stack_pop().num + stack_pop().num}); }
IA_ void xswap() { U8 x = stack_get(-1).num; stack_get_r(-1)->num = stack_get(-2).num; stack_get_r(-2)->num = x; }
IA_ void xsub()  { xswap(); stack_push((Word){Word_Number, .num = stack_pop().num - stack_pop().num}); }

void xword() {
	Str8  want      = stack_pop().str;
	U8    found     = pmem.buff_cursor;
	U8    found_len = 0;
	while (pmem.buff_cursor < S_(pmem.buff)) {
		U8 x = pmem.buff[pmem.buff_cursor];
		++ pmem.buff_cursor;
		if (want.ptr[0] == x) {
			break;
		}
		else {
			++ found_len;
		}
	}
	U8 hash = hash64_fnv1a_ret((Slice){found, found_len}, 0);
	for (U8 id = 0; id < Word_DictionaryNum; ++ id)
		if (hash == pmem.dictionary[id]) {
			stack_push((Word){id, {}});
			return; // Identified a dictionary word, not a string
		}
	stack_push_str8((Str8){C_(UTF8*, found), found_len});
}

void xinterpret() {
	Word word = stack_pop();
	if (word.tag > Word_Num) {
		return;
	}
	if (word.tag < Word_DictionaryNum) switch(word.tag) {
	// vtable lookup
	case Word_bye:   xbye();  return;
	case Word_dot:   xdot();  return;
	case Word_dot_s: xdots(); return;
	case Word_add:   xadd();  return;
	case Word_swap:  xswap(); return;
	case Word_sub:   xsub();  return;
	}
	else {
		Str8_A2 tbl_arr[] = { {str8("word"), serialize_word(word)}, }; 
		KTL_Str8 tbl = ktl_str8_make_(tbl_arr);
		print(str8_fmt(str8("<word>?"), tbl));
	}
}

void ok(){ while(1) {
	farena_reset(& thread.scratch);
	print(str8("OK "));
	U4 len, read_ok = ms_read_console(pmem.std_in, pmem.buff, S_(pmem.buff), & len, null);
	if    (read_ok == false || len == 0) { continue; }
	while (len > 0 && (pmem.buff[len - 1] == '\n' || pmem.buff[len - 1] == '\r')) { -- len; }
	pmem.buff_cursor = 0;
	while (pmem.buff_cursor < S_(pmem.buff)) {
		stack_push_char(' ');
		xword();
		xinterpret();
	}
}}

int main(void)
{
	/*prep*/ {
		pmem.std_out = ms_get_std_handle(MS_STD_OUTPUT);
		pmem.std_in  = ms_get_std_handle(MS_STD_INPUT);
		farena_init(& pmem.word_store, slice_raw(pmem.scratch_word_store, array_len(pmem.scratch_word_store)));
		farena_init(& thread.scratch,  slice_raw(thread.scratch_64k,      array_len(thread.scratch_64k)));
		for (U8 id = Word_bye; id < Word_DictionaryNum; ++ id) {
			hash64_fnv1a(& pmem.dictionary[id], slice_to_raw(str8_word_tag(id)), 0);
		}
	}
	Str8 buff = {pmem.buff, S_(pmem.buff)};
	ok();
	return 0;
}
