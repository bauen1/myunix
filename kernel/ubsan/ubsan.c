#include <stdarg.h>
#include <stdint.h>

#include <console.h>
#include <cpu.h>

struct ubsan_source_location {
	const char *filename;
	uint32_t line;
	uint32_t column;
};

struct ubsan_type_descriptor {
	uint16_t kind;
	uint16_t info;
	char name[];
};

struct ubsan_type_mismatch_data {
	struct ubsan_source_location location;
	struct ubsan_type_descriptor *type;
	// FIXME: not sure about the types of alignment and type_check_kind
	uintptr_t alignment;
	uint8_t type_check_kind;
};

struct ubsan_overflow {
	struct ubsan_source_location location;
	struct ubsan_type_descriptor *type;
};

struct ubsan_shift_out_of_bounds {
	struct ubsan_source_location location;
	struct ubsan_type_descriptor *left_type;
	struct ubsan_type_descriptor *right_type;
};

struct ubsan_out_of_bounds {
	struct ubsan_source_location location;
	struct ubsan_type_descriptor *left_type;
	struct ubsan_type_descriptor *right_type;
};

struct ubsan_unreachable {
	struct ubsan_source_location location;
};


struct ubsan_vla_bound_data {
	struct ubsan_source_location location;
	struct ubsan_type_descriptor *type;
};

struct ubsan_invalid_value_data {
	struct ubsan_source_location location;
	struct ubsan_type_descriptor *type;
};

struct ubsan_invalid_builtin_data {
	struct ubsan_source_location location;
	unsigned char kind;
};

struct ubsan_function_type_mismatch {
	struct ubsan_source_location location;
	struct ubsan_type_descriptor *type;
};

struct ubsan_non_null_return_data {
	struct ubsan_source_location location;
};

struct ubsan_non_null_arg_data {
	struct ubsan_source_location location;
	struct ubsan_source_location attr_location;
	int arg_index;
};

struct ubsan_pointer_overflow_data {
	struct ubsan_source_location location;
};


__attribute__((noreturn))
static void ubsan_panic(const struct ubsan_source_location *src, const char *violation_format, ...) {
	printf("[ubsan] %s:%d:%dc : violation: ", src->filename, src->line, src->column);

	va_list args;
	va_start(args, violation_format);
	vprintf(violation_format, args);
	va_end(args);
	putc('\n');

	halt();
}

#define IS_ALIGNED(value, alignment) !(value & (alignment - 1))

static const char *type_check_types[] = {
	"load of", "store to", "reference binding to", "member access within",
	"member call on", "constructor call on", "downcast of", "downcast of",
	"upcast of", "cast to virtual base of", "_Nonnull binding to"
};

__attribute__((noreturn))
void __ubsan_handle_type_mismatch(struct ubsan_type_mismatch_data *type_mismatch_data, uintptr_t ptr) {
	if (ptr == 0) {
		ubsan_panic(&type_mismatch_data->location, "%s NULL pointer of type %s",
			type_check_types[type_mismatch_data->type_check_kind],
			type_mismatch_data->type->name);
	} else if ((type_mismatch_data->alignment != 0) &&
		(IS_ALIGNED(ptr, type_mismatch_data->alignment))) {
		ubsan_panic(&type_mismatch_data->location,
			"%s misaligned address %p of type %s which requires %u byte alignment",
			type_check_types[type_mismatch_data->type_check_kind],
			ptr, type_mismatch_data->type->name,
			type_mismatch_data->alignment);
	} else  {
		ubsan_panic(&type_mismatch_data->location,
			"%s address %p with insufficient space for object of type %s\n",
			type_check_types[type_mismatch_data->type_check_kind],
			ptr, type_mismatch_data->type->name);
	}
	ubsan_panic(&type_mismatch_data->location, "%s type_mismatch_data: %p, ptr: %p", __func__, type_mismatch_data, ptr);
}

__attribute__((noreturn))
void __ubsan_handle_out_of_bounds(struct ubsan_out_of_bounds *out_of_bounds, uintptr_t index) {
	ubsan_panic(&out_of_bounds->location, "%s out_of_bounds: %p, index: %p", __func__, out_of_bounds, index);
}

__attribute__((noreturn))
void __ubsan_handle_add_overflow(struct ubsan_overflow *overflow, uintptr_t lhs, uintptr_t rhs) {
	ubsan_panic(&overflow->location, "%s overflow: %p, lhs: %p, rhs: %p", __func__, overflow, lhs, rhs);
}

__attribute__((noreturn))
void __ubsan_handle_sub_overflow(struct ubsan_overflow *overflow, uintptr_t lhs, uintptr_t rhs) {
	ubsan_panic(&overflow->location, "%s overflow: %p, lhs: %p, rhs: %p", __func__, overflow, lhs, rhs);
}

__attribute__((noreturn))
void __ubsan_handle_mul_overflow(struct ubsan_overflow *overflow, uintptr_t lhs, uintptr_t rhs) {
	ubsan_panic(&overflow->location, "%s overflow: %p, lhs: %p, rhs: %p", __func__, overflow, lhs, rhs);
}

__attribute__((noreturn))
void __ubsan_handle_divrem_overflow(struct ubsan_overflow *overflow, uintptr_t lhs, uintptr_t rhs) {
	ubsan_panic(&overflow->location, "%s overflow: %p, lhs: %p, rhs: %p", __func__, overflow, lhs, rhs);
}

__attribute__((noreturn))
void __ubsan_handle_negate_overflow(struct ubsan_overflow *overflow, uintptr_t old) {
	ubsan_panic(&overflow->location, "%s overflow: %p, old: %p", __func__, overflow, old);
}

__attribute__((noreturn))
void __ubsan_handle_builtin_unreachable(struct ubsan_unreachable *unreachable) {
	ubsan_panic(&unreachable->location, "called __builtin_unreachable");
}

__attribute__((noreturn))
void __ubsan_handle_shift_out_of_bounds(struct ubsan_shift_out_of_bounds *shift_out_of_bounds, uintptr_t lhs, uintptr_t rhs) {
	ubsan_panic(&shift_out_of_bounds->location, "%s shift_out_of_bounds: %p, lhs: %p, rhs: %p", __func__, shift_out_of_bounds, lhs, rhs);
}

__attribute__((noreturn))
void __ubsan_handle_load_invalid_value(struct ubsan_invalid_value_data *invalid_value_data, uintptr_t val) {
	ubsan_panic(&invalid_value_data->location, "%s invalid_value_data: %p val: %p", __func__, invalid_value_data, val);
}

__attribute__((noreturn))
void __ubsan_handle_invalid_builtin(struct ubsan_invalid_builtin_data *invalid_builtin_data) {
	ubsan_panic(&invalid_builtin_data->location, "%s invalid builtin (kind: '%c')", __func__, invalid_builtin_data->kind);
}

__attribute__((noreturn))
void __ubsan_handle_function_type_mismatch(struct ubsan_function_type_mismatch *function_type_mismatch, uintptr_t ptr) {
	ubsan_panic(&function_type_mismatch->location, "%s call to incorrect function type '%s' ptr: %p", __func__, function_type_mismatch->type->name, ptr);
}

//__attribute__((noreturn))
//void __ubsan_handle_nonnull_return_v1(

/*
TODO: implement
libsanitizer/ubsan/ubsan_handlers.cc:void __ubsan::__ubsan_handle_cfi_bad_icall(CFIBadIcallData *CallData,
libsanitizer/ubsan/ubsan_handlers.cc:void __ubsan::__ubsan_handle_cfi_check_fail(CFICheckFailData *Data,
libsanitizer/ubsan/ubsan_handlers.cc:void __ubsan::__ubsan_handle_float_cast_overflow(void *Data, ValueHandle From) {
libsanitizer/ubsan/ubsan_handlers.cc:void __ubsan::__ubsan_handle_missing_return(UnreachableData *Data) {
libsanitizer/ubsan/ubsan_handlers.cc:void __ubsan::__ubsan_handle_nonnull_arg(NonNullArgData *Data) {
libsanitizer/ubsan/ubsan_handlers.cc:void __ubsan::__ubsan_handle_nonnull_return(NonNullReturnData *Data) {
libsanitizer/ubsan/ubsan_handlers.cc:void __ubsan::__ubsan_handle_vla_bound_not_positive(VLABoundData *Data,
*/
