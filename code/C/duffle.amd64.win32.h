/*
C DSL Duffle
ISA:      amd64
Sandbox:  Windows 11
Compiler: clang
Standard: c23
*/
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wswitch"
#pragma clang diagnostic ignored "-Wuninitialized"
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "Kernel32.lib")
#pragma comment(lib, "msvcrt.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ucrt.lib")
#pragma comment(lib, "vcruntime.lib")
#define WinAPI __attribute((__stdcall__)) __attribute__((__force_align_arg_pointer__)) // Win32 Syscall FFI

#pragma region DSL
#define m_expand(...)      __VA_ARGS__
#define glue_impl(A, B)    A ## B
#define glue(A, B)         glue_impl(A, B)
#define tmpl(prefix, type) prefix ## _ ## type

#define VA_Sel_1( _1, ... ) _1 // <-- Of all th args passed pick _1.
#define VA_Sel_2( _1, _2, ... ) _2 // <-- Of all the args passed pick _2.
#define VA_Sel_3( _1, _2, _3, ... ) _3 // etc..

#define global static // Mark global data
#define gknown        // Mark global data used in procedure

#define LT_      thread_local
#define LP_      static // static data within procedure scope
#define internal static // internal

#define asm           __asm__
#define align(value)  __attribute__(aligned (value))          // for easy alignment
#define C_(type,data) ((type)(data))                          // for enforced precedence
#define expect(x,y)   __builtin_expect(x, y)                  // so compiler knows the common path
#define I_            internal inline
#define IA_           I_       __attribute__((always_inline)) // inline always
#define N_            internal __attribute__((noinline))      // inline never
#define RO_           __attribute__((section(".rodata")))     // Read only data allocation
#define r             restrict                                // pointers are either restricted or volatile and nothing else 
#define v             volatile                                // pointers are either restricted or volatile and nothing else
#define T_            typeof
#define T_same(a,b)   _Generic((a), typeof((b)): 1, default: 0)

#define r_(ptr)        C_(T_(ptr[0])*r, ptr)
#define v_(ptr)        C_(T_(ptr[0])*v, ptr)
#define tr_(type, ptr) C_(type*r, ptr)
#define tv_(type, ptr) C_(type*v, ptr)

#define array_len(a)                  (U8)(sizeof(a) / sizeof(typeof((a)[0])))
#define array_decl(type, ...)         (type[]){__VA_ARGS__}
#define Array_sym(type,len)           type ## _ ## A ## len
#define Array_expand(type,len)        type Array_sym(type, len)[len];
#define Array_(type,len)              Array_expand(type,len)
#define Bit_(id,b)                    id = (1 << b), tmpl(id,pos) = b
#define Enum_(underlying_type,symbol) enum   symbol: underlying_type symbol; enum symbol: underlying_type
#define Struct_(symbol)               struct symbol   symbol;                struct symbol
#define Union_(symbol)                union  symbol   symbol;                union  symbol

#define Opt_(proc)                    Struct_(tmpl(Opt,proc))
#define opt_(symbol, ...)             (tmpl(Opt,symbol)){__VA_ARGS__}
#define Ret_(proc)                    Struct_(tmpl(Ret,proc))
#define ret_(proc)                    tmpl(Ret,proc) proc

// Generally unused, allows force inlining of procedures at the discretion of the call-site.
#if 0
#define IC_(name) inline_ ## name
#define I_proc(name, params, args) \
  IA_ void IC_(name) params; \
  I_  void     name  params { IC_(name)args; } \
  IA_ void IC_(name) params
#define I_proc_r(type_ret, name, params, args) \
  IA_ type_ret IC_(name) params; \
  I_  type_ret     name  params { return IC_(name)args; }
#endif

typedef __UINT8_TYPE__  U1; typedef __UINT16_TYPE__ U2; typedef __UINT32_TYPE__ U4; typedef __UINT64_TYPE__ U8; 
typedef __INT8_TYPE__   S1; typedef __INT16_TYPE__  S2; typedef __INT32_TYPE__  S4; typedef __INT64_TYPE__  S8;
typedef unsigned char   B1; typedef __UINT16_TYPE__ B2; typedef __UINT32_TYPE__ B4; typedef __UINT64_TYPE__ B8;
typedef float           F4; typedef double          F8; 
typedef float F4_2 __attribute__((vector_size(16)));

#define u1_(value)  C_(U1, value)
#define u2_(value)  C_(U2, value)
#define u4_(value)  C_(U4, value)
#define u8_(value)  C_(U8, value)
#define s1_(value)  C_(S1, value)
#define s2_(value)  C_(S2, value)
#define s4_(value)  C_(S4, value)
#define s8_(value)  C_(S8, value)
#define f4_(value)  C_(F4, value)
#define f8_(value)  C_(F8, value)

#define u1_r(value) C_(U1*r, value)
#define u2_r(value) C_(U2*r, value)
#define u4_r(value) C_(U4*r, value)
#define u8_r(value) C_(U8*r, value)
#define u1_v(value) C_(U1*v, value)
#define u2_v(value) C_(U2*v, value)
#define u4_v(value) C_(U4*v, value)
#define u8_v(value) C_(U8*v, value)

#define kilo(n)         (C_(U8, n) << 10)
#define mega(n)         (C_(U8, n) << 20)
#define giga(n)         (C_(U8, n) << 30)
#define tera(n)         (C_(U8, n) << 40)
#define null             C_(U8,    0)
#define nullptr          C_(void*, 0)
#define O_(type,member)  C_(U8,__builtin_offsetof(type,member))
#define S_(data)         C_(U8, sizeof(data))

#define sop_1(op,a,b) C_(U1, s1_(a) op s1_(b))
#define sop_2(op,a,b) C_(U2, s2_(a) op s2_(b))
#define sop_4(op,a,b) C_(U4, s4_(a) op s4_(b))
#define sop_8(op,a,b) C_(U8, s8_(a) op s8_(b))

#undef def_signed_op
#define def_signed_op(id,op,width) IA_ U ## width id ## _s ## width(U ## width a, U ## width b) {return sop_ ## width(op, a, b); }
#define def_signed_ops(id,op)      def_signed_op(id, op, 1) def_signed_op(id, op, 2) def_signed_op(id, op, 4) def_signed_op(id, op, 8)
def_signed_ops(add, +)
def_signed_ops(sub, -)
def_signed_ops(mut, *)
def_signed_ops(div, /)
def_signed_ops(gt,  >)
def_signed_ops(lt,  <) 
def_signed_ops(ge, >=)
def_signed_ops(le, <=)
#undef def_signed_ops
#undef def_signed_op

#define def_generic_sop(op, a, ...) _Generic((a), U1:  op ## _s1, U2: op ## _s2, U4: op ## _s4, U8: op ## _s8) (a, __VA_ARGS__)
#define add_s(a,b) def_generic_sop(add,a,b)
#define sub_s(a,b) def_generic_sop(sub,a,b)
#define mut_s(a,b) def_generic_sop(mut,a,b)
#define gt_s(a,b)  def_generic_sop(gt, a,b)
#define lt_s(a,b)  def_generic_sop(lt, a,b)
#define ge_s(a,b)  def_generic_sop(ge, a,b)
#define le_s(a,b)  def_generic_sop(le, a,b)
#undef def_generic_sop
#pragma endregion DSL

#pragma region Thread Coherence
IA_ void barrier_compiler(void){asm volatile("::""memory");} // Compiler Barrier
IA_ void barrier_memory  (void){__builtin_ia32_mfence();}    // Memory   Barrier
IA_ void barrier_read    (void){__builtin_ia32_lfence();}    // Read     Barrier
IA_ void barrier_write   (void){__builtin_ia32_sfence();}    // Write    Barrier

IA_ U4 atm_add_u4 (U4*r addr, U4 value){asm volatile("lock xaddl %0,%1":"=r"(value),"=m"(addr[0]):"0"(value),"m"(addr[0]):"memory","cc");return value;}
IA_ U8 atm_add_u8 (U8*r addr, U8 value){asm volatile("lock xaddq %0,%1":"=r"(value),"=m"(addr[0]):"0"(value),"m"(addr[0]):"memory","cc");return value;}
IA_ U4 atm_swap_u4(U4*r addr, U4 value){asm volatile("lock xchgl %0,%1":"=r"(value),"=m"(addr[0]):"0"(value),"m"(addr[0]):"memory","cc");return value;}
IA_ U8 atm_swap_u8(U8*r addr, U8 value){asm volatile("lock xchgq %0,%1":"=r"(value),"=m"(addr[0]):"0"(value),"m"(addr[0]):"memory","cc");return value;}
#pragma endregion Thread Coherence

#pragma region Debug
WinAPI void process_exit(U4 status) asm("exit");
#define debug_trap() __builtin_debugtrap()
#if BUILD_DEBUG
IA_ void assert(U8 cond) { if(cond){return;} else{debug_trap(); process_exit(1);} }
#else
#define assert(cond)
#endif
#pragma endregion Debug

#pragma region Memory
#define MEM_ALIGNMENT_DEFAULT  (2 * S_(void*))

#define assert_bounds(point, start, end) for(;0;){ \
	assert((start) <= (point)); \
	assert((point) <= (end));   \
} while(0)

IA_ U8 align_pow2(U8 x, U8 b) {
	assert(b != 0);
	assert((b & (b - 1)) == 0);  // Check power of 2
	return ((x + b - 1) & (~(b - 1)));
}

IA_ U8 mem_copy            (U8 dest, U8 src,   U8 len) { return (U8)(__builtin_memcpy ((void*)dest, (void const*)src,   len)); }
IA_ U8 mem_copy_overlapping(U8 dest, U8 src,   U8 len) { return (U8)(__builtin_memmove((void*)dest, (void const*)src,   len)); }
IA_ U8 mem_fill            (U8 dest, U8 value, U8 len) { return (U8)(__builtin_memset ((void*)dest, (int)        value, len)); }
IA_ B4 mem_zero            (U8 dest,           U8 len) { if(dest == 0){return false;} mem_fill(dest, 0, len); return true; }

typedef Struct_(Slice) { U8 ptr, len; }; // Untyped Slice
IA_ Slice slice_ut_(U8 ptr, U8 len) { return (Slice){ptr, len}; }

#define Slice_(type)        Struct_(tmpl(Slice,type)) { type* ptr; U8 len; }
typedef Slice_(B1);
#define slice_assert(s)    do { assert((s).ptr != 0); assert((s).len > 0); } while(0)
#define slice_end(slice)   ((slice).ptr + (slice).len)
#define S_slice(s)         ((s).len * S_((s).ptr[0]))

#define slice_ut(ptr,len)  slice_ut_(u8_(ptr), u8_(len))
#define slice_ut_arr(a)    slice_ut_(u8_(a),   S_(a))
#define slice_to_ut(s)     slice_ut_(u8_((s).ptr), S_slice(s))

#define slice_iter(container, iter)     (T_((container).ptr) iter = (container).ptr; iter != slice_end(container); ++ iter)
#define slice_arg_from_array(type, ...) & (tmpl(Slice,type)) { .ptr = array_decl(type,__VA_ARGS__), .len = array_len( array_decl(type,__VA_ARGS__)) }

IA_ void slice_zero_(Slice s) { slice_assert(s); mem_zero(s.ptr, s.len); }
#define  slice_zero(s)        slice_zero_(slice_to_ut(s))

IA_ void slice_copy_(Slice dest, Slice src) {
	assert(dest.len >= src.len);
	slice_assert(dest);
	slice_assert(src);
	mem_copy(dest.ptr, src.ptr, src.len);
}
#define slice_copy(dest, src) do {  \
	static_assert(T_same(dest, src)); \
	slice_copy_(slice_to_ut(dest), slice_to_ut(src)); \
} while(0)
#pragma endregion Memory

#pragma region Math
#define u8_max 0xffffffffffffffffull

#define min(A,B)       (((A) < (B)) ? (A) : (B))
#define max(A,B)       (((A) > (B)) ? (A) : (B))
#define clamp_bot(X,B) max(X, B) // Clamp "X" by "B"

#define clamp_decrement(X) (((X) > 0) ? ((X) - 1) : 0)

typedef Struct_(R1_U1){ U1 p0; U1 p1; };
typedef Struct_(R1_U2){ U2 p0; U2 p1; };
typedef Struct_(R1_U4){ U4 p0; U2 p4; };
typedef Struct_(R1_U8){ U8 p0; U8 p4; };

typedef Struct_(V2_U1){ U1 x; U1 y;};

IA_ B8 add_of  (U8 a, U8 b, U8*r res) { return __builtin_uaddll_overflow(a, b, res); }
IA_ B8 sub_of  (U8 a, U8 b, U8*r res) { return __builtin_usubll_overflow(a, b, res); }
IA_ B8 mul_of  (U8 a, U8 b, U8*r res) { return __builtin_umulll_overflow(a, b, res); }
IA_ B8 add_s_of(S8 a, S8 b, S8*r res) { return __builtin_saddll_overflow(a, b, res); }
IA_ B8 sub_s_of(S8 a, S8 b, S8*r res) { return __builtin_ssubll_overflow(a, b, res); }
IA_ B8 mul_s_of(S8 a, S8 b, S8*r res) { return __builtin_smulll_overflow(a, b, res); }
#pragma endregion Math

#pragma region Control Flow & Iteration
#define each_iter(type, iter, end)             (type iter = 0; iter < end; ++ iter)
#define index_iter(type, iter, begin, op, end) (type iter = begin; iter op end; (begin < end ? ++ iter : -- iter))
#define range_iter(iter,op,range)              (T_((range).p0) iter = (range).p0; iter op (range).p1; ((range).p0 < (range).p1 ? ++ iter : -- iter))

#define defer(expr)                for(U4         once= 1;                  once!=1;++     once,(expr))    // Basic do something after body
#define scope(begin,end)           for(U4         once=(1,(begin));         once!=1;++     once,(end ))    // Do things before or after a scope
#define defer_rewind(cursor)       for(T_(cursor) sp=cursor,once=0;         once!=1;++     once,cursor=sp) // Used with arenas/stacks
#define defer_info(type,expr, ...) for(type       info= {__VA_ARGS__}; info.once!=1;++info.once,(expr))    // Defer with tracked state

#define do_while(cond) for (U8 once=0; once!=1 || (cond); ++once)
#pragma endregion Control Flow & Iteration

#pragma region FArena
typedef Opt_(farena)    { U8 alignment, type_width; };
typedef Struct_(FArena) { U8 start, capacity, used; };
IA_ void farena_init(FArena*r arena, Slice mem) {  assert(arena != nullptr);
	arena->start    = mem.ptr;
	arena->capacity = mem.len;
	arena->used     = 0;
}
IA_ FArena farena_make(Slice mem) { FArena a; farena_init(& a, mem); return a; }
I_ Slice farena_push(FArena*r arena, U8 amount, Opt_farena o) {
	if (amount == 0) { return (Slice){}; }
	U8 desired   = amount * (o.type_width == 0 ? 1 : o.type_width);
	U8 to_commit = align_pow2(desired, o.alignment ?  o.alignment : MEM_ALIGNMENT_DEFAULT);
	U8 unused    = arena->capacity - arena->used; assert(to_commit <= unused);
	U8 ptr       = arena->start    + arena->used;
	arena->used += to_commit;
	return (Slice){ptr, desired};
}
IA_ void farena_reset(FArena*r arena) { arena->used = 0; }
IA_ void farena_rewind(FArena*r arena, U8 save_point) {
	U8 end       = arena->start + arena->used; assert_bounds(save_point, arena->start, end);
	arena->used -= save_point - arena->start;
}
IA_ U8 farena_save(FArena arena) { return arena.used; }
#define farena_push_(arena, amount, ...)                                          farena_push((arena), (amount), opt_(farena, __VA_ARGS__))
#define farena_push_type(arena, type, ...)                              C_(type*, farena_push((arena), 1,        opt_(farena, .type_width=S_(type), __VA_ARGS__)).ptr)
#define farena_push_array(arena, type, amount, ...) (tmpl(Slice,type)){ C_(type*, farena_push((arena), (amount), opt_(farena, .type_width=S_(type), __VA_ARGS__)).ptr), (amount) }
#pragma endregion FArena

#pragma region FStack
#define FStack_(name, type, width) Struct_(name) { U8 top; type arr[width]; }

IA_ Slice fstack_push(Slice mem, U8* top, U8 amount, Opt_farena o) { 
	FArena a = { mem.ptr, mem.len, top[0] }; Slice s = farena_push(& a, amount, o); 
	top[0] = a.used; return s;
};

// This is here more for annotation than anything else.
#define fstack_save(stack)       stack.top
#define fstack_rewind(stack, sp) do{stack.top = sp;}while(0)
#define fstack_reset(stack)      do{stack.top = 0; }while(0)

#define fstack_slice(stack) slice_ut_arr((stack).arr)
#define fstack_push_(stk, amount, ...) fstack_push(fstack_slice(stk), & (stk).top, (amount), opt_(farena, __VA_ARGS__))
#define fstack_push_array(stk, type, amount, ...) \
(tmpl(Slice,type)){ C_(type*, fstack_push(fstack_slice(stk), & (stk).top, (amount), opt_(farena, .type_width=S_(type), __VA_ARGS__)).ptr), (amount) }
#pragma endregion FStack

#pragma region Text
typedef unsigned char UTF8;
typedef Struct_(Str8) { UTF8* ptr; U8 len; }; 
typedef Str8 Slice_UTF8;
typedef Slice_(Str8);
#define str8_comp(ptr, len) ((Str8){(UTF8*)ptr, len})
#define str8(literal)       ((Str8){(UTF8*)literal, S_(literal) - 1})
#pragma endregion Text

#pragma region Hashing
IA_ void hash64_fnv1a(U8*r hash, Slice data, U8 seed) {
	LP_ U8 const default_seed = 0xcbf29ce484222325; 
	if (seed == 0) seed = default_seed;
	hash[0] = seed; for (U8 elem = data.ptr; elem != slice_end(data); elem += 1) {
		hash[0] ^= u1_r(elem)[0];
		hash[0] *= 0x100000001b3;
	}
}
IA_ U8 hash64_fnv1a_ret(Slice data, U8 seed) { U8 h = 0; hash64_fnv1a(& h, data, seed); return h; }
#pragma endregion Hashing

#pragma region IO
#define MS_STD_INPUT  u4_(-10)
#define MS_STD_OUTPUT u4_(-11)
typedef Struct_(MS_Handle){U8 id;};
WinAPI MS_Handle ms_get_std_handle(U4 handle_type) asm("GetStdHandle");
WinAPI B4        ms_read_console(MS_Handle handle, UTF8*r buffer, U4 to_read, U4*r num_read, U8 reserved_input_control) asm("ReadConsoleA");
WinAPI B4        ms_write_console(MS_Handle handle, UTF8 const*r buffer, U4 chars_to_write, U4*v chars_written, U8 reserved) asm("WriteConsoleA");
#pragma endregion IO

#pragma region Key Table Linear (KTL)
enum { KT_SLot_value = S_(U8), };
#define KTL_Slot_(type) Struct_(tmpl(KTL_Slot,type)) { \
	U8   key;   \
	type value; \
}
#define KTL_(type) Slice_(tmpl(KTL_Slot,type)); \
	typedef tmpl(Slice_KTL_Slot,type) tmpl(KTL,type)
typedef Slice KTL_Byte;
typedef Struct_(KTL_Meta) {
	U8   slot_size;
	U8   type_width;
};

typedef Array_(Str8, 2);
typedef Slice_(Str8_A2);
typedef KTL_Slot_(Str8);
typedef KTL_(Str8);
IA_ void ktl_populate_slice_a2_str8(KTL_Str8* kt, Slice_Str8_A2 values) {
	assert(kt != null); slice_assert(* kt);
	if (values.len == 0) return;
	assert(kt->len == values.len);
	for index_iter(U4, id, 0, <, values.len) { 
		hash64_fnv1a(& kt->ptr[id].key, slice_to_ut(values.ptr[id][0]), 0);
		mem_copy(u8_(& kt->ptr[id].value), u8_(& values.ptr[id][1]), S_(Str8));
	}
}
#define ktl_str8_key(str)      hash64_fnv1a_ret(slice_to_ut(str8(str)), 0)
#define ktl_str8_from_arr(arr) (KTL_Str8){arr, array_len(arr)}
#pragma endregion KTL

#pragma region Text Ops
// NOTE(rjf): Includes reverses for uppercase and lowercase hex.
RO_ global U8 integer_symbol_reverse[128] = {
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
};

IA_ B4   char_is_upper(UTF8 c) { return('A' <= c && c <= 'Z'); }
IA_ UTF8 char_to_lower(UTF8 c) { if (char_is_upper(c)) { c += ('a' - 'A'); } return(c); }
IA_ B4   char_is_digit(UTF8 c, U4 base) {
  B4 result = 0; if (0 < base && base <= 16) {
    if (integer_symbol_reverse[c] < base) result = 1;
  }
  return result;
}
IA_ UTF8 integer_symbols(UTF8 value) {
	LP_ UTF8 lookup_table[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F', }; 
	return lookup_table[C_(UTF8, value)]; 
}
IA_ U8 u8_from_str8(Str8 str, U4 radix) {
  U8 x = 0; if(1 < radix && radix <= 16) {
    for each_iter(U8, cursor, str.len) { 
      x *= radix;
      x += integer_symbol_reverse[str.ptr[cursor] & 0x7F];
    }
  }
  return x;
}

typedef Struct_(Info_str8_from_u4) {
	Str8 prefix;
	U4   digit_group_size;
	U4   needed_leading_zeros;
	U4   size_required;
};
I_ Info_str8_from_u4 str8_from_u4_info(U4 num, U4 radix, U4 min_digits, U4 digit_group_separator)
{
	Info_str8_from_u4 info = {0};
	LP_ Str8 tbl_prefix[] = { str8("0x"), str8("0o"), str8("0b") };
	switch (radix) {
	case 16: { info.prefix = tbl_prefix[0]; } break;
	case 8:  { info.prefix = tbl_prefix[1]; } break;
	case 2:  { info.prefix = tbl_prefix[2]; } break;
	}
	info.digit_group_size = 3;
	switch (radix) {
	default: break;
	case 2:
	case 8:
	case 16: {
		info.digit_group_size = 4;
	}
	break;
	}
	info.needed_leading_zeros = 0;
	{
		U4 needed_digits = 1;
		{
			U4 u32_reduce = num;
			for(;;)
			{
				u32_reduce /= radix;
				if (u32_reduce == 0) {
					break;
				}
				needed_digits += 1;
			}
		}
		info.needed_leading_zeros = (min_digits > needed_digits) ? min_digits - needed_digits : 0;
		U4 needed_separators       = 0;
		if (digit_group_separator != 0)
		{
			needed_separators = (needed_digits + info.needed_leading_zeros) / info.digit_group_size;
			if (needed_separators > 0 && (needed_digits + info.needed_leading_zeros) % info.digit_group_size == 0) {
				needed_separators -= 1;
			}
		}
		info.size_required = info.prefix.len + info.needed_leading_zeros + needed_separators + needed_digits;
	}
	return info;
}
I_ Str8 str8_from_u4_buf(Slice buf, U4 num, U4 radix, U4 min_digits, U4 digit_group_separator, Info_str8_from_u4 info)
{
	assert(buf.len >= info.size_required);
	Str8 result = { C_(UTF8*, buf.ptr), info.size_required };
	/*Fill Content*/ {
		U4 num_reduce             = num;
		U4 digits_until_separator = info.digit_group_size;
		for (U8 idx = 0; idx < result.len; idx += 1)
		{
			U8 separator_pos = result.len - idx - 1;
			if (digits_until_separator == 0 && digit_group_separator != 0) {
				result.ptr[separator_pos] = u1_(digit_group_separator);
				digits_until_separator    = info.digit_group_size + 1;
			}
			else {
				result.ptr[separator_pos] = (U1) char_to_lower(integer_symbols(u1_(num_reduce % radix)));
				num_reduce /= radix;
			}
			digits_until_separator -= 1;
			if (num_reduce == 0) break;
		}
		for (U8 leading_0_idx = 0; leading_0_idx < info.needed_leading_zeros; leading_0_idx += 1) {
			result.ptr[info.prefix.len + leading_0_idx] = '0';
		}
	}
	/*Fill Prefix*/ if (info.prefix.len > 0) { slice_copy(result, info.prefix); }
	return result;
}
I_ Str8 str8_fmt_ktl_buf(Slice buffer, KTL_Str8 table, Str8 fmt_template)
{
	slice_assert(buffer);
	slice_assert(table);
	slice_assert(fmt_template);
	UTF8*r cursor_buffer    = C_(UTF8*r, buffer.ptr);
	U8     buffer_remaining = buffer.len;
	UTF8*r cursor_fmt       = fmt_template.ptr;
	U8     left_fmt         = fmt_template.len;
	while (left_fmt && buffer_remaining)
	{
		// Forward until we hit the delimiter '<' or the template's contents are exhausted.
		U8 copy_offset = 0;
		if (cursor_fmt[0] == '<')
		{
			UTF8*r potential_token_cursor = cursor_fmt + 1; // Skip '<'
			U8     potential_token_len    = 0;
			B4     fmt_overflow           = false;
			while(true) {
				UTF8*r cursor       = potential_token_cursor + potential_token_len;
				fmt_overflow        = cursor >= slice_end(fmt_template);
				B4 found_terminator = potential_token_cursor[potential_token_len] == '>';
				if (fmt_overflow || found_terminator) { break; }
				++ potential_token_len;
			}
			if (fmt_overflow) { 
				// Failed to find a subst and we're at end of fmt, just copy segment.
				copy_offset = 1 + potential_token_len; // '<' + token
				goto write_to_buffer; 
			}
			// Hashing the potential token and cross checking it with our token table
			U8 key = hash64_fnv1a_ret(slice_ut(u8_(potential_token_cursor), potential_token_len), 0);
			Str8*r value = nullptr; for slice_iter(table, token) {
				// We do a linear iteration instead of a hash table lookup because the user should never subst with more than 100 unqiue tokens..
				if (token->key == key) { value = & token->value; break; }
			}
			if (value)
			{
				// We're going to appending the string, make sure we have enough space in our buffer.
				// NOTE(Ed): this version doesn't support growing the buffer (No Allocator Interface)
				assert((buffer_remaining - potential_token_len) > 0);
				copy_offset = min(buffer_remaining, value->len); // Prevent Buffer overflow.
				mem_copy(u8_(cursor_buffer), u8_(value->ptr), buffer_remaining);
				// Sync cursor format to after the processed token
				cursor_buffer    += copy_offset;
				buffer_remaining -= copy_offset;
				cursor_fmt        = potential_token_cursor + 1 + potential_token_len; // '<' + token
				left_fmt         -= potential_token_len    + 2; // The 2 here are the '<' & '>' delimiters being omitted.
				continue;
			}
			// If not a subsitution, we copy the segment and continue.
			copy_offset = 1 + potential_token_len; // '<' + token
			goto write_to_buffer;
		}
		else do {
			++ copy_offset;
		} 
		while ( (cursor_fmt[copy_offset] != '<' && (cursor_fmt + copy_offset) < slice_end(fmt_template)) );
	write_to_buffer:
		assert((buffer_remaining - copy_offset) > 0);
		copy_offset = min(buffer_remaining, copy_offset); // Prevent buffer overflow.
		mem_copy(u8_(cursor_buffer), u8_(cursor_fmt), copy_offset);
		buffer_remaining -= copy_offset;
		left_fmt         -= copy_offset;
		cursor_buffer    += copy_offset;
		cursor_fmt       += copy_offset;
	}
	return (Str8){C_(UTF8*, buffer.ptr), buffer.len - buffer_remaining};
}

typedef Struct_(Str8Gen) { UTF8* ptr; U8 cap, len; };
IA_ Slice str8gen_buf(Str8Gen*r gen) { return (Slice){u8_(gen->ptr) + gen->len, gen->cap - gen->len}; }

IA_ void str8gen_append_str8(Str8Gen*r gen, Str8 str) { assert(gen != nullptr);
	Str8 mem = farena_push_array(C_(FArena*, gen), UTF8, str.len, .alignment = 1);
	slice_copy(mem, str);
}
IA_ void str8gen_append_fmt(Str8Gen*r gen, Str8 fmt, KTL_Str8 tbl) {
	Str8 result = str8_fmt_ktl_buf(str8gen_buf(gen), tbl, fmt);
	gen->len += result.len;
}
#define str8gen_append_str8_(gen, s) str8gen_append_str8(gen, str8(s))
#pragma endregion Text Ops
