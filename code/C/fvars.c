/*
fvars: Forth-like in C (ideosyncratic, non-portable), clang, Win32(Windows 11)

Now we're getting into memory manipulations with variables and such. This introduces some new stuff in the code field -- a constant or variable is just like any other Forth word, except the code retrieves the values (or address of the values, for variables).

* **!** ( v a --) Store value at address a.
* **,** ( v --) Store v as next value in dictionary.
* **@** ( a -- v) Fetch value from address a.
* **constant** ( name | v --) Create constant with value v.
* **create** ( name | --) Create word name in dictionary.
* **dump** ( start n --) Dump n values starting from RAM address a.
* **variable** ( name | v --) Create variable name, with initial value v.
*/
#include "duffle.amd64.win32.h"

// Just the encoding format for SWord data
typedef Enum_(U8, SWord_Tag) {
#define Word_Tag_Entries() \
	X(Char,"Char")           \
	X(Code,"Code")           \
	X(U8,  "U8")             \
	X(S8,  "S8")             \
	X(F4,  "F4")             \
	X(F8,  "F8")             \
	X(Str8,"Str8")
#define X(n,s) tmpl(SWord,n),
	Word_Tag_Entries()
#undef X
	SWord_Num,
};
Str8 str8_word_tag(SWord_Tag tag) {
	LP_ Str8 tbl[] = {
	#define X(n,s) str8(s),
		Word_Tag_Entries()
	#undef X
	#undef Word_Tag_Entries
	};
	return tbl[tag];
}
typedef Struct_(SWord) { SWord_Tag tag; union {
	Str8      str;
	UTF8      sym;
	U8        u8;
	S8        s8;
	F4        f4;
	F8        f8;
};};

enum {
	Word_Str_Cap = 128 - S_(U1) - S_(U8), // 128 - len - code
	Word_Cap     = 4096,
	Dict_Cap     = Word_Cap * 2,
	Stack_Cap    = 512,
};
typedef void WordProc(); // Will be used to execute words.
typedef Struct_(Word) {
	U1 len, ptr[Word_Str_Cap];
	U8 code;
};
typedef Struct_(WordEntry) {
	U8 hash;
	U8 slot;
};
enum { 
	Bit_(WordSlot_Occupied, 63),
	WordSlot_Invalid = Word_Cap,
};
typedef FStack_(WordRam,         Word, Word_Cap);
typedef FStack_(WordStack,      SWord, Stack_Cap); 

typedef FStack_(StackArena_K16,    U1, kilo(16));
enum Signal {
	Bit_(Signal_U8_OF, 1),
	Bit_(Signal_S8_OF, 2),
};
typedef Struct_(ProcessMemory) {
	StackArena_K16 word_strs;
	WordRam        ram;
	WordEntry      dict[Dict_Cap];
	WordStack      stack;
	U1             buff[kilo(1)];
	U8             buff_cursor;
	B8             signal;
	MS_Handle      std_out;
	MS_Handle      std_in;
};
typedef FStack_(StackArena_K128, U1, kilo(128));
typedef Struct_(ThreadMemory) {	
	StackArena_K128 scratch;
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

IA_ SWord*r stack_get_r(U8 id) { U8 rel = pmem.stack.top - id; return & pmem.stack.arr[clamp_decrement(rel)]; };
IA_ SWord   stack_get  (U8 id) { U8 rel = pmem.stack.top - id; return   pmem.stack.arr[clamp_decrement(rel)]; };
I_  SWord   stack_pop() {
	U8    last = clamp_decrement(pmem.stack.top);
	SWord w    = pmem.stack.arr[last]; 
	if (w.tag == SWord_Str8){ pmem.word_strs.top -= w.str.len; }
	pmem.stack.top = last;
	if (pmem.stack.top == 0) {
		pmem.stack.arr[pmem.stack.top] = (SWord){0};
	}
	return w;
}
IA_ void stack_push(SWord w) { pmem.stack.arr[pmem.stack.top] = w; ++ pmem.stack.top; }

IA_ void stack_push_char(UTF8 sym)  { stack_push((SWord){SWord_Char, .sym = sym }); }
IA_ void stack_push_u8  (U8   n)    { stack_push((SWord){SWord_U8,   .u8  = n   }); }
IA_ void stack_push_code(U8   slot) { stack_push((SWord){SWord_Code, .u8  = slot}); }
IA_ void stack_push_str8(Str8 str) { 
	Str8 stored = fstack_push_array(pmem.word_strs, UTF8, str.len);
	slice_copy(stored, str);
	stack_push((SWord){SWord_Str8, .str = stored}); 
}

IA_ Str8 serialize_wordentry(WordEntry entry) {
	Word word = pmem.ram.arr[entry.slot];
	print_fmt_("<name> - Slot: <slot>, Code: <code>\n", {
		{ktl_str8_key("name"), str8_comp(word.ptr, word.len)},
		{ktl_str8_key("slot"), str8_from_u4(entry.slot)},
		{ktl_str8_key("code"), str8_from_u4(word.code)},
	});
	return (Str8){};
}
I_ Str8 serialize_dict() {
	Str8Gen serial = { .ptr = C_(UTF8*, scratch_push(kilo(32)).ptr), .cap = kilo(32), };
	str8gen_append_str8_(& serial, "[ "); 
	if (pmem.ram.top > 0) {
		for (U8 id = pmem.ram.top - 1; id > 0; ++ id) {
			str8gen_append_str8 (& serial, serialize_wordentry(pmem.dict[id])); 
			str8gen_append_str8_(& serial, ", ");
		}
		str8gen_append_str8 (& serial, serialize_wordentry(pmem.dict[0])); 
	}
	str8gen_append_str8_(& serial, " ]\n"); 
	return str8_comp(serial.ptr, serial.len);
}
IA_ Str8 serialize_sword(SWord word){ switch(word.tag) {
case SWord_Str8: return word.str;
case SWord_F4:
case SWord_F8:
case SWord_S8:   // TOOD(Ed): Support more
case SWord_U8:   return str8_from_u4(u4_(word.u8)); 
default: return (Str8){};
}}
I_ Str8 serialize_stack() {
	Str8Gen serial = { .ptr = C_(UTF8*, scratch_push(kilo(32)).ptr), .cap = kilo(32), };
	str8gen_append_str8_(& serial, "[ "); 
	if (pmem.stack.top > 0) {
		for (U4 id = pmem.stack.top - 1; id > 0; -- id) {
			str8gen_append_str8 (& serial, serialize_sword(stack_get(id))); 
			str8gen_append_str8_(& serial, ", ");
		}
		str8gen_append_str8 (& serial, serialize_sword(stack_get(0))); 
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

IA_ U8 dict_index(U8 hash) {
	U8 start = hash % Dict_Cap;
	U8 id    = start;
	while (pmem.dict[id].slot & WordSlot_Occupied) {
		if (pmem.dict[id].hash == hash) { return id; }
		id = (id + 1) % Dict_Cap;
		if (id == start) return Dict_Cap; // Table Full
	}
	return id;
}
I_ B8 dict_insert(U8 key, U8 slot) {
	U8 id = dict_index(key);
	if (id >= Dict_Cap) return false;
	pmem.dict[id].hash = key;
	pmem.dict[id].slot = slot | WordSlot_Occupied;
	return true;
}
I_ U8 dict_find(U8 key) {
	U8 id    = dict_index(key);
	WordEntry* entry = pmem.dict + id;
	U8  found = id < Dict_Cap &&  (entry->slot & WordSlot_Occupied) && entry->hash == key;
	if (found) { return entry->slot & ~WordSlot_Occupied; }
	return WordSlot_Invalid;
}

#define code_(name, proc) code(str8(name), u8_(& proc))
I_ void code(Str8 name, U8 code) {
	if (pmem.ram.top >= Word_Cap    ) { print_("Ram full!\n");      return; }
	if (name.len     >  Word_Str_Cap) { print_("Name too long!\n"); return; }
	// Fill out ram entry
	U8 slot = u8_(pmem.ram.arr) + pmem.ram.top * 128;
	u1_r(    slot + O_(Word,len ))[0] = name.len; 
	u8_r(    slot + O_(Word,code))[0] = code;
	mem_copy(slot + O_(Word,ptr), u8_(name.ptr), name.len);
	// Insert into dictonary
	U8 hash = hash64_fnv1a_ret(slice_to_ut(name), 0);
	dict_insert(hash, pmem.ram.top);
	++ pmem.ram.top;
}

I_ void xtick() {
	SWord name = stack_get(0);
	U8    x    = pmem.stack.top;
	U8    key  = hash64_fnv1a_ret(slice_to_ut(name.str), 0);
	U8    slot = dict_find(key);
	if (slot != WordSlot_Invalid) {
		stack_pop(); stack_push_code(slot); return;
	}
}
IA_ void xwords()   { defer_rewind(thread.scratch.top) { print(serialize_dict()); } }
IA_ void xexecute() { Word* slot = pmem.ram.arr + stack_pop().u8; C_(WordProc*,slot->code)(); }

IA_ U8 swap_sub_u8(U8 b, U8 a) { return b - a; }
IA_ U8 swap_div_u8(U8 b, U8 a) { return b / a; }
IA_ U8 swap_mod_u8(U8 b, U8 a) { return b % a; }

// Exercise Original
IA_ void xbye()  { process_exit(0); }
IA_ void xdot()  { print_fmt_("<num>\n", { {ktl_str8_key("num"), serialize_sword(stack_pop())}, }); }
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
	SWord  want = stack_pop();
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
	U1*r digit = pmem.buff + found;
	Str8 str   = str8_comp(pmem.buff + found, found_len);
	if (char_is_digit(digit[0], 10)) {
		stack_push_u8(u8_from_str8(str, 10)); return;
	}
	stack_push_str8(str); // Its a word
}
I_ void xinterpret() {
	SWord word = stack_get(0);
	if (word.tag > SWord_Num) {
		return;
	}
	xtick();
	word = stack_get(0);
	switch(word.tag) {
	case SWord_U8:               return; // Do nothing its handled by xword.
	case SWord_Code: xexecute(); return; // Were going to execute a code word.
	}
	stack_pop();
	// Unknown word
	print_fmt_("<word>?\n", { {ktl_str8_key("word"), serialize_sword(word)}, });
	stack_pop();
}
I_ void ok(){ while(1) {
	print_("OK ");
	U4 len, read_ok = ms_read_console(pmem.std_in, pmem.buff, S_(pmem.buff), & len, null);
	if (read_ok == false || len == 0) { continue; }
	pmem.buff_cursor = 0;
	while (pmem.buff_cursor < len) {
		fstack_reset(thread.scratch);
		stack_push_char(' ');
		xword();
		xinterpret();
		if (len > 0) switch(pmem.buff[pmem.buff_cursor - 1]) { case'\r': len -= 1; case'\n': len -= 1; }
	}
}}
int main(void)
{
	/*prep*/ {
		pmem.std_out = ms_get_std_handle(MS_STD_OUTPUT);
		pmem.std_in  = ms_get_std_handle(MS_STD_INPUT);
		code_("'",         xtick);
		code_("words",     xwords);
		code_("execute",   xexecute);
		code_("bye",       xbye);
		code_(".",         xdot);
		code_(".s",        xdots);
		code_("swap",      xswap);
		code_("+",         xadd);
		code_("-",         xsub);
		code_("*",         xmul);
		code_("/",         xdiv);
		code_("mod",       xmod);
		code_("+of",       xadd_of);
		code_("-of",       xsub_of);
		code_("*of",       xmul_of);
		code_("u_of",      xu_of);
		code_("s_of",      xs_of);
		code_("signal",    xsignal);
		code_("word",      xword);
		code_("interpret", xinterpret);
	}
	ok();
	return 0;
}
