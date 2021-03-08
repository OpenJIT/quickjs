#ifndef QUICKJS_HPP
#define QUICKJS_HPP 1

#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "quickjs.h"

namespace qjs {
    using Mark_Func = JS_MarkFunc;
    using Class_Id = JSClassID;

    class Value {
        private:
        JSContext *m_context;
        JSValue m_value;

        private:
        Value(JSValue&& value, JSContext *context);

        public:
        Value(const Value&);
        ~Value();

        Value& operator =(const Value&);

        std::optional<Value> get_property(const std::string& prop);
        std::optional<Value> get_property(uint32_t idx);
        bool set_property(const std::string& prop, const Value& val);
        bool set_property(uint32_t idx, const Value& val );
        bool has_property(const std::string& prop);
        bool is_extensible();
        void prevent_extensions();
        bool delete_property(const std::string& prop);
        bool set_prototype(const Value& proto);
        Value get_prototype();

        Value call(const Value& this_obj, const std::vector<Value>& args);
        Value invoke(const std::string& prop, const std::vector<Value>& args);

        friend class Runtime_Ref;
        friend class Context;
    };

    class Exception {
        private:
        Value m_value;

        public:
        Exception(Value value);
        Exception(const Exception&) = default;
        ~Exception() = default;

        operator Value&();
        Value* operator ->();
    };

    template<typename T>
    using Result = std::variant<T, Exception>;

    class Runtime;
    
    class Runtime_Ref {
        private:
        JSRuntime* m_runtime;
        
        explicit Runtime_Ref(JSRuntime* runtime);

        public:
        Runtime_Ref(const Runtime&);
        Runtime_Ref(const Runtime_Ref&) = default;
        ~Runtime_Ref() = default;

        template<typename T>
        void set_opaque(T* ptr) {
            JS_SetRuntimeOpaque(m_runtime, static_cast<void*>(ptr));
        }

        template<typename T>
        T* get_opaque() {
            return JS_GetRuntimeOpaque(m_runtime);
        }

        void set_runtime_info(const std::string& info);
        void set_memory_limit(size_t limit);
        void set_gc_threshold(size_t gc_threshold);
        void set_max_stack_size(size_t stack_size);
        void mark_value(const Value& value, Mark_Func* func);
        void run_gc();
        bool is_live_object(const Value& obj);

        friend class Runtime;
        friend class Context;
    };
    
    class Runtime {
        private:
        Runtime_Ref m_ref;

        public:
        Runtime();
        Runtime(Runtime&&) = default;
        ~Runtime();

        operator Runtime_Ref&();
        Runtime_Ref* operator ->();

        friend class Runtime_Ref;
        friend class std::vector<Value>;
    };

    class Context;
    
    using Function = std::function<Value(Context&, const Value&, const std::vector<Value>&, int)>;

    class Context {
        private:
        static thread_local Class_Id closure_class;

        JSContext* m_context;

        explicit Context(JSContext*);

        Exception get_exception();
            
        public:
        explicit Context(Runtime_Ref& runtime);
        Context(const Context&);
        ~Context();

        template<typename T>
        void set_opaque(T* ptr) {
            JS_SetContextOpaque(m_context, static_cast<void*>(ptr));
        }

        template<typename T>
        T* get_opaque() {
            return JS_GetContextOpaque(m_context);
        }

        Runtime_Ref get_runtime();
        void set_class_proto(Class_Id id, const Value& obj);
        Value get_class_proto(Class_Id id);

        Value new_c_function(const Function& fn, const std::string& name, int length, int magic);
        Value new_bool(bool val);
        Value new_int32(int32_t val);
        Value new_int64(int64_t val);
        Value new_uint32(uint32_t val);
        Value new_float64(double val);
        Result<bool> to_bool(const Value& val);
        Result<int32_t> to_int32(const Value& val);
        Result<uint32_t> to_uint32(const Value& val);
        Result<int64_t> to_int64(const Value& val);
        Result<uint64_t> to_index(const Value& val);
        Result<double> to_float64(const Value& val);
        Result<int64_t> to_bigint64(const Value& val);
        Result<int64_t> to_int64_ext(const Value& val);
        Value new_string(const std::string& str);
        Value new_atom_string(const std::string& str);
        Value to_string(const Value& val);
        Value to_property_key(const Value& val);
        std::string to_std_string(const Value& val);
        Value new_object_proto(const Value& val);
        Value new_object();
        Value new_array();

        Value eval(const std::string& input, const std::string& filename, int eval_flags);

        friend class Runtime;
    };
}

#endif /* QUICKJS_HPP */
