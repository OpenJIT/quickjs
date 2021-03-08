#include <stdlib.h>
#include <string.h>
#include "quickjs.hpp"

qjs::Value::Value(JSValue&& value, JSContext* context):
    m_context(context),
    m_value(value)
{}

qjs::Value::Value(const Value& val):
    m_context(val.m_context),
    m_value(JS_DupValue(m_context, val.m_value))
{}

qjs::Value::~Value() {
    JS_FreeValue(m_context, m_value);
}

qjs::Value& qjs::Value::operator =(const Value& value) {
    m_context = value.m_context;
    m_value = JS_DupValue(m_context, value.m_value);
    return *this;
}

std::optional<qjs::Value> qjs::Value::get_property(const std::string& prop) {
    if (!has_property(prop)) {
        return std::nullopt;
    }
    auto value = JS_GetPropertyStr(m_context, m_value, prop.c_str());
    return Value(std::move(value), m_context);
}

std::optional<qjs::Value> qjs::Value::get_property(uint32_t idx) {
    if (!has_property("length")) {
        return std::nullopt;
    }
    auto length = get_property("length");
    if (length) {
        uint32_t len;
        JS_ToUint32(m_context, &len, m_value);
        if (idx >= len) {
            return std::nullopt;
        }
    }
    auto value = JS_GetPropertyUint32(m_context, m_value, idx);
    return Value(std::move(value), m_context);
}

bool qjs::Value::set_property(const std::string& prop, const Value& val) {
    if (!is_extensible()) {
        return false;
    }
    JS_SetPropertyStr(m_context, m_value, prop.c_str(), val.m_value);
    return true;
}

bool qjs::Value::set_property(uint32_t idx, const Value& val ) {
    if (!is_extensible()) {
        return false;
    }
    JS_SetPropertyUint32(m_context, m_value, idx, val.m_value);
    return true;
}

bool qjs::Value::has_property(const std::string& prop) {
    auto atom = JS_NewAtomLen(m_context, prop.c_str(), prop.size());
    bool result = JS_HasProperty(m_context, m_value, atom);
    JS_FreeAtom(m_context, atom);
    return result;
}

bool qjs::Value::is_extensible() {
    return JS_IsExtensible(m_context, m_value);
}

void qjs::Value::prevent_extensions() {
    JS_PreventExtensions(m_context, m_value);
}

bool qjs::Value::delete_property(const std::string& prop) {
    auto atom = JS_NewAtomLen(m_context, prop.c_str(), prop.size());
    int result = JS_DeleteProperty(m_context, m_value, atom, 0);
    JS_FreeAtom(m_context, atom);
    return result == 0;
}

bool qjs::Value::set_prototype(const Value& proto) {
    return JS_SetPrototype(m_context, m_value, proto.m_value) == 0;
}

qjs::Value qjs::Value::get_prototype() {
    return Value(JS_GetPrototype(m_context, m_value), m_context);
}

qjs::Value qjs::Value::call(const Value& this_obj, const std::vector<Value>& args) {
    std::vector<JSValueConst> c_args;
    for (const auto& value : args) {
        c_args.push_back(value.m_value);
    }
    int c_argc = c_args.size();
    auto value = JS_Call(m_context, m_value, this_obj.m_value, c_argc, c_args.data());
    return Value(std::move(value), m_context);
}

qjs::Value qjs::Value::invoke(const std::string& prop, const std::vector<Value>& args) {
    std::vector<JSValueConst> c_args;
    for (const auto& value : args) {
        c_args.push_back(value.m_value);
    }
    int c_argc = c_args.size();
    auto atom = JS_NewAtomLen(m_context, prop.c_str(), prop.size());
    auto value = JS_Invoke(m_context, m_value, atom, c_argc, c_args.data());
    JS_FreeAtom(m_context, atom);
    return Value(std::move(value), m_context);
}

qjs::Exception::Exception(Value value): m_value(value) {}

qjs::Exception::operator Value&() {
    return m_value;
}

qjs::Value* qjs::Exception::operator ->() {
    return &m_value;
}

qjs::Runtime_Ref::Runtime_Ref(JSRuntime* runtime): m_runtime(runtime) {}

qjs::Runtime_Ref::Runtime_Ref(const Runtime& runtime): Runtime_Ref(runtime.m_ref) {}

void qjs::Runtime_Ref::set_runtime_info(const std::string& info) {
    JS_SetRuntimeInfo(m_runtime, info.c_str());
}

void qjs::Runtime_Ref::set_memory_limit(size_t limit) {
    JS_SetMemoryLimit(m_runtime, limit);
}

void qjs::Runtime_Ref::set_gc_threshold(size_t gc_threshold) {
    JS_SetGCThreshold(m_runtime, gc_threshold);
}

void qjs::Runtime_Ref::set_max_stack_size(size_t stack_size) {
    JS_SetMaxStackSize(m_runtime, stack_size);
}

void qjs::Runtime_Ref::mark_value(const Value& value, Mark_Func* func) {
    JS_MarkValue(m_runtime, value.m_value, func);
}

void qjs::Runtime_Ref::run_gc() {
    JS_RunGC(m_runtime);
}

bool qjs::Runtime_Ref::is_live_object(const Value& obj) {
    return JS_IsLiveObject(m_runtime, obj.m_value);
}

qjs::Runtime::Runtime(): m_ref(JS_NewRuntime()) {}

qjs::Runtime::~Runtime() {
    JS_FreeRuntime(m_ref.m_runtime);
}

qjs::Runtime::operator Runtime_Ref&() {
    return m_ref;
}

qjs::Runtime_Ref* qjs::Runtime::operator ->() {
    return &m_ref;
}

thread_local qjs::Class_Id qjs::Context::closure_class = 0;

qjs::Context::Context(JSContext* ctx):
    m_context(ctx)
{
    if (!closure_class) {
        JS_NewClassID(&closure_class);
        const char* class_name = "Std_Closure_Class";
        JSClassFinalizer* finalizer = [](JSRuntime*, JSValue val) {
            void* opaque = JS_GetOpaque(val, closure_class);
            auto ptr = static_cast<Function*>(opaque);
            delete ptr;
        };
        JSClassGCMark* gc_mark = [](JSRuntime*, JSValueConst, JS_MarkFunc*) {};
        JSClassDef class_def{
            class_name,
            finalizer,
            gc_mark,
            nullptr,
            nullptr,
        };
        JS_NewClass(JS_GetRuntime(m_context), closure_class, &class_def);
    }
}

qjs::Context::Context(Runtime_Ref& runtime): Context(JS_NewContext(runtime.m_runtime)) {}

qjs::Exception qjs::Context::get_exception() {
    auto except = JS_GetException(m_context);
    return Value(std::move(except), m_context);
}

qjs::Context::Context(const Context& ctx): Context(JS_DupContext(ctx.m_context)) {}

qjs::Context::~Context() {
    if (m_context) {
        JS_FreeContext(m_context);
    }
}

qjs::Runtime_Ref qjs::Context::get_runtime() {
    return Runtime_Ref(JS_GetRuntime(m_context));
}

void qjs::Context::set_class_proto(Class_Id id, const Value& obj) {
    JS_SetClassProto(m_context, id, obj.m_value);
}

qjs::Value qjs::Context::get_class_proto(Class_Id id) {
    return Value(JS_GetClassProto(m_context, id), m_context);
}

qjs::Value qjs::Context::new_c_function(const Function& fn, const std::string& name, int length, int magic) {
    auto fn_ = new Function(fn);
    auto data = JS_NewObjectClass(m_context, closure_class);
    auto datav = static_cast<JSValue*>(malloc(sizeof(JSValue)));
    memcpy(static_cast<void*>(datav), static_cast<void*>(&data), sizeof(JSValue));
    JS_SetOpaque(data, static_cast<void*>(fn_));
    auto value = JS_NewCFunctionData(m_context,
                                     [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValue* data) -> JSValue {
                                         Context ctx_(JS_DupContext(ctx));
                                         const Value this_val_(JS_DupValue(ctx_.m_context, this_val), ctx_.m_context);
                                         std::vector<Value> argv_;
                                         for (int i = 0; i < argc; i++) {
                                             argv_.push_back(Value(JS_DupValue(ctx_.m_context, argv[i]), ctx_.m_context));
                                         }
                                         auto opaque = JS_GetOpaque(data[0], closure_class);
                                         auto fn = static_cast<Function*>(opaque);
                                         auto value = (*fn)(ctx_, this_val_, argv_, magic);
                                         return JS_DupValue(ctx_.m_context, value.m_value);
                                     },
                                     length,
                                     magic,
                                     1,
                                     datav);
    return Value(std::move(value), m_context);
}

qjs::Value qjs::Context::new_bool(bool val) {
    auto value = JS_NewBool(m_context, val);
    return Value(std::move(value), m_context);
}

qjs::Value qjs::Context::new_int32(int32_t val) {
    auto value = JS_NewInt32(m_context, val);
    return Value(std::move(value), m_context);
}

qjs::Value qjs::Context::new_int64(int64_t val) {
    auto value = JS_NewInt64(m_context, val);
    return Value(std::move(value), m_context);
}

qjs::Value qjs::Context::new_uint32(uint32_t val) {
    auto value = JS_NewUint32(m_context, val);
    return Value(std::move(value), m_context);
}

qjs::Value qjs::Context::new_float64(double val) {
    auto value = JS_NewFloat64(m_context, val);
    return Value(std::move(value), m_context);
}

qjs::Result<bool> qjs::Context::to_bool(const Value& val) {
    auto result = JS_ToBool(m_context, val.m_value);
    if (result == -1) {
        return get_exception();
    } else {
        return result != 0;
    }
}

qjs::Result<int32_t> qjs::Context::to_int32(const Value& val) {
    int32_t value;
    auto result = JS_ToInt32(m_context, &value, val.m_value);
    if (result == -1) {
        return get_exception();
    } else {
        return value;
    }
}

qjs::Result<uint32_t> qjs::Context::to_uint32(const Value& val) {
    uint32_t value;
    auto result = JS_ToUint32(m_context, &value, val.m_value);
    if (result == -1) {
        return get_exception();
    } else {
        return value;
    }
}

qjs::Result<int64_t> qjs::Context::to_int64(const Value& val) {
    int64_t value;
    auto result = JS_ToInt64(m_context, &value, val.m_value);
    if (result == -1) {
        return get_exception();
    } else {
        return value;
    }
}

qjs::Result<uint64_t> qjs::Context::to_index(const Value& val) {
    uint64_t value;
    auto result = JS_ToIndex(m_context, &value, val.m_value);
    if (result == -1) {
        return get_exception();
    } else {
        return value;
    }
}

qjs::Result<double> qjs::Context::to_float64(const Value& val) {
    double value;
    auto result = JS_ToFloat64(m_context, &value, val.m_value);
    if (result == -1) {
        return get_exception();
    } else {
        return value;
    }
}

qjs::Result<int64_t> qjs::Context::to_bigint64(const Value& val) {
    int64_t value;
    auto result = JS_ToBigInt64(m_context, &value, val.m_value);
    if (result == -1) {
        return get_exception();
    } else {
        return value;
    }
}

qjs::Result<int64_t> qjs::Context::to_int64_ext(const Value& val) {
    int64_t value;
    auto result = JS_ToInt64Ext(m_context, &value, val.m_value);
    if (result == -1) {
        return get_exception();
    } else {
        return value;
    }
}

qjs::Value qjs::Context::new_string(const std::string& str) {
    auto value = JS_NewStringLen(m_context, str.c_str(), str.size());
    return Value(std::move(value), m_context);
}

qjs::Value qjs::Context::new_atom_string(const std::string& str) {
    auto value = JS_NewAtomString(m_context, str.c_str());
    return Value(std::move(value), m_context);
}

qjs::Value qjs::Context::to_string(const Value& val) {
    auto value = JS_ToString(m_context, val.m_value);
    return Value(std::move(value), m_context);
}

qjs::Value qjs::Context::to_property_key(const Value& val) {
    auto value = JS_ToPropertyKey(m_context, val.m_value);
    return Value(std::move(value), m_context);
}

std::string qjs::Context::to_std_string(const Value& val) {
    size_t len;
    auto chars = JS_ToCStringLen(m_context, &len, val.m_value);
    std::string str(chars, len);
    JS_FreeCString(m_context, chars);
    return str;
}

qjs::Value qjs::Context::new_object_proto(const Value& val) {
    auto value = JS_NewObjectProto(m_context, val.m_value);
    return Value(std::move(value), m_context);
}

qjs::Value qjs::Context::new_object() {
    auto value = JS_NewObject(m_context);
    return Value(std::move(value), m_context);
}

qjs::Value qjs::Context::new_array() {
    auto value = JS_NewArray(m_context);
    return Value(std::move(value), m_context);
}

qjs::Value qjs::Context::eval(const std::string& input, const std::string& filename, int eval_flags) {
    auto value = JS_Eval(m_context, input.c_str(), input.size(), filename.c_str(), eval_flags);
    return Value(std::move(value), m_context);
}

qjs::Value qjs::Context::get_global_object() {
    auto value = JS_GetGlobalObject(m_context);
    return Value(std::move(value), m_context);
}
