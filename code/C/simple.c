/*
simple: Forth-like in C (ideosyncratic, non-portable), clang, Win32(Windows 11)

The simplest thing that could be called a Forth (Mr. Moore will probably disagree with me). However, it provides a useful parser, stack and interface to the host system. Words in this implementation:

* **+** ( a b -- sum) Add two numbers on top of stack.
* **-** ( a b -- difference) Subtracts top number from next on stack.
* **.** ( n --) Pop top of stack and print it.
* **.s** ( --) Print contents of stack (nondestructive).
* **bye** ( --) Exit interpreter.
* **interpret** ( string --) Execute word on top of stack.
* **swap** ( a b -- b a) Swaps top two items on stack.
* **word** ( c -- string) Collect characters in input stream up to character c.
*/
#include "duffle.amd64.win32.h"

// Fixed command list (not able to define more)
typedef Enum_(U8, Word_Tag) {
#define Word_Tag_Entries() \
	X(Null,"__NULL__")       \
	X(bye ,     "bye")       \
	X(dot,      ".")         \
	X(dot_s,    ".s")        \
	X(add,      "+")         \
	X(swap,     "swap")      \
	X(sub,      "-")         \
	X(mul,      "*")         \
	X(div,      "/")         \
	X(mod,      "mod")       \
	X(add_of,   "+of")       \
	X(sub_of,   "-of")       \
	X(mul_of,   "*of")       \
	/*X(add_s,    "+s")*/    \
	/*X(sub_s,    "-s")*/    \
	/*X(mul_s,    "*s")*/    \
	/*X(div_s,    "/s")*/    \
	/*X(add_s_of, "+sof")*/  \
	/*X(sub_s_of, "-sof")*/  \
	/*X(mul_s_of, "*sof")*/  \
	X(signal,   "signal")    \
	X(u_of,     "u_of")      \
	X(s_of,     "s_of")      \
	/*X(Cast_U8,  "U8")*/ /*(only serialization)*/ \
	/*X(Cast_S8,  "S8")*/ /*(only serialization)*/ \
	/*X(Cast_F4,  "F4")*/ /*(only serialization)*/ \
	/*X(Cast_F8,  "F8")*/ /*(only serialization)*/ \
	X(DictionaryNum,"__Word_DictionaryNum__")  \
	X(word,"word")           \
	X(interpret,"interpret") \
	/*Non-dictionary*/       \
	X(Char,"Char")           \
	X(U8,  "U8")             \
	X(S8,  "S8")             \
	X(F4,  "F4")             \
	X(F8,  "F8")             \
	X(Str8,"Str8")
#define X(n,s) tmpl(Word,n),
	Word_Tag_Entries()
#undef X
	Word_Num,
};
Str8 str8_word_tag(Word_Tag tag) {
	LP_ Str8 tbl[] = {
	#define X(n,s) str8(s),
		Word_Tag_Entries()
	#undef X
	};
	return tbl[tag];
}
#undef Word_Tag_Entries
typedef Struct_(Word) { Word_Tag tag; union {
	Str8   str;
	UTF8   sym;
	U8     u8;
	S8     s8;
	F4     f4;
	F8     f8;
};};

typedef FStack_(StackArena_K64,  U1,   kilo(64));
typedef FStack_(Stack_Word,      Word, kilo(64) / S_(Word));

enum Signal {
	Bit_(Signal_U8_OF, 1),
	Bit_(Signal_S8_OF, 2),
};
typedef Struct_(ProcessMemory) {
	StackArena_K64 word_strs;
	Stack_Word     stack;
	U8             dictionary[Word_DictionaryNum];
	U1             buff[kilo(1)];
	U8             buff_cursor;
	B8             signal;
	MS_Handle      std_out;
	MS_Handle      std_in;
};
typedef Struct_(ThreadMemory) {
	StackArena_K64 scratch;
};
global     ProcessMemory pmem;
global LT_ ThreadMemory  thread;

#pragma region Grime
IA_ Slice scratch_push(U8 len) { return fstack_push_(thread.scratch, len); }

typedef Opt_(str8_from_u4) { U4 radix, min_digits, digit_group_separator; }; 
IA_ Str8 str8_from_u4_opt(U4 num, Opt_str8_from_u4 o) { if (o.radix == 0) {o.radix = 10;} 
/*gather info*/Info_str8_from_u4 info = str8_from_u4_info(num, o.radix, o.min_digits, o.digit_group_separator);
/*write buf  */return str8_from_u4_buf(scratch_push(128), num, o.radix, o.min_digits, o.digit_group_separator, info);
}
#define str8_from_u4(num, ...) str8_from_u4_opt(num, opt_(str8_from_u4, __VA_ARGS__))

#define str8_fmt_tbl(s,tbl)                     str8_fmt(str8(s), tbl)
#define print_fmt_tbl(s,tbl)              print(str8_fmt(str8(s), tbl))
#define print_(s)                         print(str8(s))
IA_ Str8 str8_fmt(Str8 fmt, KTL_Str8 tbl) { return str8_fmt_ktl_buf(scratch_push(kilo(4)), tbl, fmt); };
IA_ void print(Str8 s)                    { U4 written; ms_write_console(pmem.std_out, s.ptr, u4_(s.len), & written, 0); }

typedef Struct_(Defer_print_fmt) { KTL_Str8 tbl; U8 once; };
#define print_fmt_(fmt, ...) \
defer_info(Defer_print_fmt, print(str8_fmt(str8(fmt),info.tbl))) {   \
	KTL_Slot_Str8 tbl[] = __VA_ARGS__; /*aggregate entries to array*/  \
	info.tbl = ktl_str8_from_arr(tbl); /*reference array as slice  */  \
}
#pragma endregion Grime

#define  set_signal_(flag, value) set_signal(flag, tmpl(flag,pos), value)
IA_ void set_signal(U8 flag, U8 pos, U8 value) { pmem.signal = (pmem.signal & ~flag) | (value << pos); }

IA_ Word*r stack_get_r(U8 id) { U8 rel = pmem.stack.top - id; return & pmem.stack.arr[clamp_decrement(rel)]; };
IA_ Word   stack_get  (U8 id) { U8 rel = pmem.stack.top - id; return   pmem.stack.arr[clamp_decrement(rel)]; };
I_  Word   stack_pop() {
	U8   last = clamp_decrement(pmem.stack.top);
	Word w    = pmem.stack.arr[last]; 
	if (w.tag == Word_Str8){ pmem.word_strs.top -= w.str.len; }
	pmem.stack.top = last;
	if (pmem.stack.top == 0) {
		pmem.stack.arr[pmem.stack.top] = (Word){0};
	}
	return w;
}
IA_ void stack_push(Word w) { pmem.stack.arr[pmem.stack.top] = w; ++ pmem.stack.top; }

IA_ void stack_push_char(UTF8 sym) { stack_push((Word){Word_Char, .sym = sym}); }
IA_ void stack_push_u8  (U8   n)   { stack_push((Word){Word_U8,   .u8  = n}); }
IA_ void stack_push_str8(Str8 str) { 
	Str8 stored = fstack_push_array(pmem.word_strs, UTF8, str.len);
	slice_copy(stored, str);
	stack_push((Word){Word_Str8, .str = stored}); 
}

IA_ Str8 serialize_word(Word word){ switch(word.tag) {
case Word_Str8: return word.str;
case Word_F4:
case Word_F8:
case Word_S8:   // TOOD(Ed): Support more
case Word_U8:   return str8_from_u4(u4_(word.u8)); 
default: return (Str8){};
}}
I_ Str8 serialize_stack() {
	Str8Gen serial = { .ptr = C_(UTF8*, scratch_push(kilo(32)).ptr), .cap = kilo(32), };
	str8gen_append_str8_(& serial, "[ "); 
	if (pmem.stack.top > 0) {
		for (U4 id = pmem.stack.top - 1; id > 0; -- id) {
			str8gen_append_str8 (& serial, serialize_word(stack_get(id))); 
			str8gen_append_str8_(& serial, ", ");
		}
		str8gen_append_str8 (& serial, serialize_word(stack_get(0))); 
	}
	str8gen_append_str8_(& serial, " ]\n");
	return (Str8){serial.ptr, serial.len};
}
I_ void serialize_signal() {
	U4 u8_overflow = (pmem.signal & Signal_U8_OF) != 0;
	U4 s8_overflow = (pmem.signal & Signal_S8_OF) != 0;
	print_fmt_(
		"  U8_Overflow: <U8_OF>\n"
		"  S8_Overflow: <S8_OF>\n"
	,{
		{ktl_str8_key("U8_OF"),str8_from_u4(u8_overflow)},
		{ktl_str8_key("S8_OF"),str8_from_u4(s8_overflow)},
	});
}

IA_ U8 swap_sub_u8(U8 b, U8 a) { return b - a; }
IA_ U8 swap_div_u8(U8 b, U8 a) { return b / a; }
IA_ U8 swap_mod_u8(U8 b, U8 a) { return b % a; }

// Exercise Original
IA_ void xbye()  { process_exit(0); }
IA_ void xdot()  { print_fmt_("<num>\n", { {ktl_str8_key("num"), serialize_word(stack_pop())}, }); }
IA_ void xdots() { defer_rewind(thread.scratch.top) { print(serialize_stack()); } }
IA_ void xswap() { U8 x = stack_get(0).u8; stack_get_r(0)->u8 = stack_get(1).u8; stack_get_r(1)->u8 = x; }
IA_ void xadd()  { stack_push_u8(stack_pop().u8 + stack_pop().u8); }
IA_ void xsub()  { U8 res = swap_sub_u8(stack_pop().u8,  stack_pop().u8); stack_push_u8(res); }
// Challenge additions
IA_ void xmul()  { stack_push_u8(stack_pop().u8 * stack_pop().u8);  }
IA_ void xdiv()  { U8 res = swap_div_u8(stack_pop().u8,  stack_pop().u8); stack_push_u8(res); }
IA_ void xmod()  { U8 res = swap_mod_u8(stack_pop().u8,  stack_pop().u8); stack_push_u8(res); }
// Bonus
IA_ void xadd_of() { U8 res, of = add_of(stack_pop().u8, stack_pop().u8, & res); stack_push_u8(res); set_signal_(Signal_U8_OF, of); }
IA_ void xsub_of() { U8 res, of = sub_of(stack_pop().u8, stack_pop().u8, & res); stack_push_u8(res); set_signal_(Signal_U8_OF, of); }
IA_ void xmul_of() { U8 res, of = mul_of(stack_pop().u8, stack_pop().u8, & res); stack_push_u8(res); set_signal_(Signal_U8_OF, of); }
IA_ void xu_of()   { stack_push_u8((pmem.signal & Signal_U8_OF) != 0); }
IA_ void xs_of()   { stack_push_u8((pmem.signal & Signal_S8_OF) != 0); }
IA_ void xsignal() { serialize_signal(); }

I_ void xword()
{
	Word  want = stack_pop();
try_again:
	U8    found     = pmem.buff_cursor;
	U8    found_len = 0;
	while (pmem.buff_cursor < S_(pmem.buff)) {
		U8 x = pmem.buff[pmem.buff_cursor]; ++ pmem.buff_cursor;
		switch(x) {
		case '\r': case '\n': goto check_word; // Nothing left to parse, a word should resolve.
		case '(': while(x != ')') { x = pmem.buff[pmem.buff_cursor]; pmem.buff_cursor += 1; };
			++ pmem.buff_cursor;
			switch(pmem.buff[pmem.buff_cursor]) { case '\r': case '\n': return; } // Nothing left to parse. No word resolved.
			goto try_again; // We've resolved a comment and still have input, attempt another scan for word
		}
		if (want.sym == x) goto check_word;
		++ found_len; // Stil resolving a word (no word end of line or end of word delimiter reached)
	}
check_word: if (found_len == 0) return; /*If No word resolved, return*/
	U8 hash = hash64_fnv1a_ret(slice_ut(u8_(pmem.buff) + found, found_len), 0);
	for (U8 id = 0; id < Word_DictionaryNum; ++ id)
		if (hash == pmem.dictionary[id]) {
			stack_push((Word){id, {}}); return; /*Identified a dictionary word*/ 
		}
	U1*r digit = pmem.buff + found;
	Str8 str   = str8_comp(pmem.buff + found, found_len);
	if (char_is_digit(digit[0], 10)) {
		stack_push_u8(u8_from_str8(str, 10)); return;
	}
	stack_push_str8(str); // Unknown string
}
I_ void xinterpret() {
	Word word = stack_get(0);
	if (word.tag > Word_Num) {
		return;
	}
	if (word.tag < Word_DictionaryNum) { stack_pop(); }
	switch(word.tag) { // lookup table
	default: break;

	case Word_Null:              return; // Do nothing.
	case Word_F4:
	case Word_F8:
	case Word_S8:
	case Word_U8:                return; // Do nothing we pushed it earlier.
	case Word_bye:    xbye();    return;
	case Word_dot:    xdot();    return;
	case Word_dot_s:  xdots();   return;
	case Word_add:    xadd();    return;
	case Word_swap:   xswap();   return;
	case Word_sub:    xsub();    return;

	case Word_mul:    xmul();    return;
	case Word_div:    xdiv();    return;
	case Word_mod:    xmod();    return;

	case Word_add_of: xadd_of(); return;
	case Word_sub_of: xsub_of(); return;
	case Word_mul_of: xmul_of(); return;
	case Word_u_of:   xu_of();   return;
	case Word_s_of:   xs_of();   return;
	case Word_signal: xsignal(); return;
	}
	// Unknown word
	print_fmt_("<word>?\n", { {ktl_str8_key("word"), serialize_word(word)}, });
	stack_pop();
}
I_ void ok(){ while(1) {
	fstack_reset(thread.scratch); print_("OK ");
	U4 len, read_ok = ms_read_console(pmem.std_in, pmem.buff, S_(pmem.buff), & len, null);
	if (read_ok == false || len == 0) { continue; }
	pmem.buff_cursor = 0;
	while (pmem.buff_cursor < len) {
		stack_push_char(' ');
		xword(); xinterpret();
		if (len > 0) switch(pmem.buff[pmem.buff_cursor - 1]) { case'\r': len -= 1; case'\n': len -= 1; }
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
