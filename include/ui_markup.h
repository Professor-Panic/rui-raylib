#pragma once

#include "css_emulator.h"
#include "component_registry.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

// Declarative XML/JSX-like front end for the CSS UI runtime.

struct UIXObject;
struct UIXArray;
class UIXValue;

// Customization point for types UIXValue does not know. Specialize it (before
// the type is first bound) to make Field()/Ref()/Bind() work with it directly:
//
//   template <> struct UIXConvert<entt::entity> {
//       static UIXValue ToValue(const entt::entity& e);
//       static bool FromValue(const UIXValue& v, entt::entity& out);
//   };
//
// The primary template is empty on purpose; an enable_if'd second parameter
// lets one specialization cover a family of types.
template <typename T, typename Enable = void>
struct UIXConvert {};

template <typename T, typename Enable = void>
struct UIXHasConvert : std::false_type {};

template <typename T>
struct UIXHasConvert<T,std::void_t<
    decltype(UIXConvert<T>::ToValue(std::declval<const T&>())),
    decltype(UIXConvert<T>::FromValue(std::declval<const UIXValue&>(),std::declval<T&>()))
>> : std::true_type {};

class UIXValue {
public:
    using Object = std::shared_ptr<UIXObject>;
    using Array = std::shared_ptr<UIXArray>;
    using Storage = std::variant<
        std::monostate,
        bool,
        int,
        double,
        std::string,
        Vector2,
        Vector3,
        Vector4,
        Color,
        Rectangle,
        Texture2D,
        Object,
        Array
    >;

    UIXValue() = default;
    UIXValue(std::nullptr_t) {}
    UIXValue(bool value) : value_(value) {}
    UIXValue(int value) : value_(value) {}
    UIXValue(float value) : value_(static_cast<double>(value)) {}
    UIXValue(double value) : value_(value) {}
    UIXValue(const char* value) : value_(std::string(value != nullptr ? value : "")) {}
    UIXValue(std::string value) : value_(std::move(value)) {}
    UIXValue(Vector2 value) : value_(value) {}
    UIXValue(Vector3 value) : value_(value) {}
    UIXValue(Vector4 value) : value_(value) {}
    UIXValue(Color value) : value_(value) {}
    UIXValue(Rectangle value) : value_(value) {}
    UIXValue(Texture2D value) : value_(value) {}
    UIXValue(Object value) : value_(std::move(value)) {}
    UIXValue(Array value) : value_(std::move(value)) {}

    const Storage& StorageValue() const { return value_; }
    Storage& StorageValue() { return value_; }

    bool IsNull() const;
    bool IsObject() const;
    bool IsArray() const;
    bool Truthy() const;
    double Number(double fallback = 0.0) const;
    std::string String() const;
    std::size_t Size() const;

    const UIXObject* AsObject() const;
    UIXObject* AsObject();
    const UIXArray* AsArray() const;
    UIXArray* AsArray();

    template <typename T>
    static UIXValue From(const T& value) {
        using Value = std::decay_t<T>;
        if constexpr(std::is_same_v<Value,UIXValue>) {
            return value;
        }
        else if constexpr(UIXHasConvert<Value>::value) {
            return UIXConvert<Value>::ToValue(value);
        }
        else if constexpr(std::is_enum_v<Value>) {
            return FromInteger(static_cast<std::underlying_type_t<Value>>(value));
        }
        else if constexpr(std::is_floating_point_v<Value>) {
            return UIXValue(static_cast<double>(value));
        }
        else if constexpr(std::is_integral_v<Value> && !std::is_same_v<Value,bool>) {
            return FromInteger(value);
        }
        else {
            return UIXValue(value);
        }
    }

    // Values that fit an int stay ints; wider ones (uint32_t ids, entt handles
    // with a version in the high bits) are stored as double, which is exact up
    // to 2^53, instead of wrapping negative.
    template <typename TInteger>
    static UIXValue FromInteger(TInteger value) {
        if constexpr(std::is_signed_v<TInteger>) {
            const long long wide = static_cast<long long>(value);
            if(wide >= std::numeric_limits<int>::min() && wide <= std::numeric_limits<int>::max()) {
                return UIXValue(static_cast<int>(wide));
            }
        }
        else {
            if(static_cast<unsigned long long>(value) <=
               static_cast<unsigned long long>(std::numeric_limits<int>::max())) {
                return UIXValue(static_cast<int>(value));
            }
        }
        return UIXValue(static_cast<double>(value));
    }

    template <typename T>
    bool AssignTo(T& destination) const {
        using Value = std::decay_t<T>;
        if constexpr(std::is_same_v<Value,UIXValue>) {
            destination = *this;
            return true;
        }
        else if constexpr(UIXHasConvert<Value>::value) {
            return UIXConvert<Value>::FromValue(*this,destination);
        }
        else if constexpr(std::is_same_v<Value,std::string>) {
            destination = String();
            return true;
        }
        else if constexpr(std::is_same_v<Value,bool>) {
            destination = Truthy();
            return true;
        }
        else if constexpr(std::is_enum_v<Value>) {
            destination = static_cast<Value>(
                static_cast<std::underlying_type_t<Value>>(Number())
            );
            return true;
        }
        else if constexpr(std::is_integral_v<Value>) {
            destination = static_cast<Value>(Number());
            return true;
        }
        else if constexpr(std::is_floating_point_v<Value>) {
            destination = static_cast<Value>(Number());
            return true;
        }
        else if(const auto* exact = std::get_if<Value>(&value_)) {
            destination = *exact;
            return true;
        }
        else {
            return false;
        }
    }

private:
    Storage value_;
};

struct UIXRegistryState;
class UIXRegistry;

enum class UIXRegistrationKind {
    Binding,
    Function
};

// Non-owning registration token. Ignoring it keeps the registration alive.
class UIXBindingHandle {
public:
    UIXBindingHandle() = default;

    bool Unbind() const;
    bool Valid() const;
    explicit operator bool() const { return Valid(); }

    const std::string& Name() const { return name_; }
    UIXRegistrationKind Kind() const { return kind_; }

private:
    friend class UIXRegistry;

    UIXBindingHandle(
        std::weak_ptr<UIXRegistryState> state,
        std::uint64_t id,
        std::string name,
        UIXRegistrationKind kind
    )
        : state_(std::move(state)),
          id_(id),
          name_(std::move(name)),
          kind_(kind) {}

    std::weak_ptr<UIXRegistryState> state_;
    std::uint64_t id_ = 0;
    std::string name_;
    UIXRegistrationKind kind_ = UIXRegistrationKind::Binding;
};

// Move-only RAII token. Destroying or resetting it unregisters the entry.
class UIXScopedBinding {
public:
    UIXScopedBinding() = default;
    explicit UIXScopedBinding(UIXBindingHandle handle)
        : handle_(std::move(handle)) {}
    ~UIXScopedBinding() { Reset(); }

    UIXScopedBinding(UIXScopedBinding&& other) noexcept
        : handle_(std::move(other.handle_)) {
        other.handle_ = {};
    }

    UIXScopedBinding& operator=(UIXScopedBinding&& other) noexcept {
        if(this == &other) return *this;
        Reset();
        handle_ = std::move(other.handle_);
        other.handle_ = {};
        return *this;
    }

    UIXScopedBinding(const UIXScopedBinding&) = delete;
    UIXScopedBinding& operator=(const UIXScopedBinding&) = delete;

    bool Reset() {
        const bool removed = handle_.Unbind();
        handle_ = {};
        return removed;
    }

    UIXBindingHandle Release() {
        UIXBindingHandle released = std::move(handle_);
        handle_ = {};
        return released;
    }

    bool Valid() const { return handle_.Valid(); }
    explicit operator bool() const { return Valid(); }
    const UIXBindingHandle& Handle() const { return handle_; }

private:
    UIXBindingHandle handle_;
};

// Optional lifetime signal for objects that may disappear independently.
// Put this inside the owning object and bind through BindTracked().
class UIXBindingLifetime {
public:
    UIXBindingLifetime()
        : token_(std::make_shared<int>(0)) {}

    UIXBindingLifetime(const UIXBindingLifetime&) = delete;
    UIXBindingLifetime& operator=(const UIXBindingLifetime&) = delete;
    UIXBindingLifetime(UIXBindingLifetime&&) noexcept = default;
    UIXBindingLifetime& operator=(UIXBindingLifetime&&) noexcept = default;

    std::weak_ptr<void> Token() const { return token_; }

    // Invalidates existing tracked bindings while allowing new ones.
    void Invalidate() { token_ = std::make_shared<int>(0); }

private:
    std::shared_ptr<void> token_;
};

class UIXBinding {
public:
    using Getter = std::function<UIXValue()>;
    using Setter = std::function<bool(const UIXValue&)>;
    using AlivePredicate = std::function<bool()>;

    UIXBinding() = default;
    UIXBinding(
        Getter getter,
        Setter setter = {},
        AlivePredicate alive = {}
    );

    UIXValue Get() const;
    bool Set(const UIXValue& value) const;
    bool Valid() const;
    bool Writable() const;
    bool Expired() const;

    // Optional hook for bindings that cache their source (BindList with a
    // source function). Registry::Invalidate(name) calls it.
    // Returns true if it actually invalidated something.
    using Invalidator = std::function<bool()>;
    UIXBinding& OnInvalidate(Invalidator invalidator) {
        invalidator_ = std::move(invalidator);
        return *this;
    }
    bool Invalidate() const {
        return invalidator_ && invalidator_();
    }

    static UIXBinding Constant(UIXValue value);
    static UIXBinding TrackedConstant(
        UIXValue value,
        std::weak_ptr<void> lifetime
    );

    template <typename T>
    static UIXBinding Reference(T& value) {
        return UIXBinding(
            [&value]() { return UIXValue::From(value); },
            [&value](const UIXValue& incoming) {
                return incoming.AssignTo(value);
            }
        );
    }

    template <typename T>
    static UIXBinding TrackedReference(
        T& value,
        std::weak_ptr<void> lifetime
    ) {
        return UIXBinding(
            [&value]() { return UIXValue::From(value); },
            [&value](const UIXValue& incoming) {
                return incoming.AssignTo(value);
            },
            [lifetime=std::move(lifetime)]() {
                return !lifetime.expired();
            }
        );
    }

private:
    Getter getter_;
    Setter setter_;
    AlivePredicate alive_;
    Invalidator invalidator_;
};

struct UIXObject {
    std::unordered_map<std::string,UIXBinding> fields;

    // Set by BindList: the C++ row this object was built from. Non-owning;
    // valid until the bound vector is modified (same rule as field bindings).
    void* native = nullptr;
    const std::type_info* native_type = nullptr;
};

struct UIXArray {
    std::vector<UIXBinding> items;
};

// Builds a binding from `T& accessor(Object&)`. Used by Ref().
template <typename TObject, typename TAccessor>
UIXBinding UIXRefBinding(TObject* object, TAccessor accessor) {
    using Result = std::invoke_result_t<TAccessor&,TObject&>;
    static_assert(
        std::is_lvalue_reference_v<Result>,
        "Ref accessor must return an lvalue reference (T&), e.g. [&](Row& r) -> T& { ... }"
    );
    auto getter = [object,accessor]() mutable {
        return UIXValue::From(accessor(*object));
    };
    if constexpr(std::is_const_v<std::remove_reference_t<Result>>) {
        return UIXBinding(std::move(getter));
    }
    else {
        return UIXBinding(
            std::move(getter),
            [object,accessor](const UIXValue& incoming) mutable {
                return incoming.AssignTo(accessor(*object));
            }
        );
    }
}

template <typename TObject>
class UIXObjectBuilder {
public:
    UIXObjectBuilder(UIXObject& output, TObject& object)
        : output_(&output), object_(&object) {}

    UIXObjectBuilder(
        std::shared_ptr<UIXObject> output,
        TObject& object,
        UIXBindingHandle handle = {}
    )
        : output_owner_(std::move(output)),
          output_(output_owner_.get()),
          object_(&object),
          handle_(std::move(handle)) {}

    template <typename TField>
    UIXObjectBuilder& Field(
        std::string name,
        TField TObject::* member
    ) {
        output_->fields[std::move(name)] =
            UIXBinding::Reference(object_->*member);
        return *this;
    }

    template <typename TField>
    UIXObjectBuilder& TrackedField(
        std::string name,
        TField TObject::* member,
        const UIXBindingLifetime& lifetime
    ) {
        output_->fields[std::move(name)] =
            UIXBinding::TrackedReference(object_->*member,lifetime.Token());
        return *this;
    }

    template <typename TField, typename TGetter, typename TSetter>
    UIXObjectBuilder& Property(
        std::string name,
        TGetter getter,
        TSetter setter
    ) {
        TObject* object = object_;
        output_->fields[std::move(name)] = UIXBinding(
            [object,getter]() {
                return UIXValue::From(getter(*object));
            },
            [object,setter](const UIXValue& incoming) {
                TField converted{};
                if(!incoming.AssignTo(converted)) return false;
                setter(*object,std::move(converted));
                return true;
            }
        );
        return *this;
    }

    // One accessor instead of a getter/setter pair:
    //   b.Ref("name", [&](Row& r) -> std::string& { return reg.get<C>(r.id).name; });
    // A const reference result makes the field read-only.
    template <typename TAccessor>
    UIXObjectBuilder& Ref(std::string name, TAccessor accessor) {
        output_->fields[std::move(name)] = UIXRefBinding(object_,std::move(accessor));
        return *this;
    }

    template <typename TField, typename TConfigure>
    UIXObjectBuilder& Object(
        std::string name,
        TField TObject::* member,
        TConfigure configure
    ) {
        auto nested = std::make_shared<UIXObject>();
        UIXObjectBuilder<TField> builder(nested,object_->*member);
        configure(builder);
        output_->fields[std::move(name)] =
            UIXBinding::Constant(UIXValue(std::move(nested)));
        return *this;
    }

    UIXObjectBuilder& Value(std::string name, UIXValue value) {
        output_->fields[std::move(name)] =
            UIXBinding::Constant(std::move(value));
        return *this;
    }

    UIXBindingHandle Handle() const { return handle_; }
    UIXBindingHandle Done() const { return handle_; }
    UIXScopedBinding Scoped() const {
        return UIXScopedBinding(handle_);
    }

private:
    std::shared_ptr<UIXObject> output_owner_;
    UIXObject* output_ = nullptr;
    TObject* object_ = nullptr;
    UIXBindingHandle handle_;
};

template <typename TObject>
struct UIXListBuilderState {
    using Configure = std::function<void(UIXObject&,TObject&)>;
    std::vector<TObject>* values = nullptr;
    std::vector<Configure> configure;

    // Source-driven lists (BindList(name, source_fn)): `owned` is refilled from
    // `source` the next time the list is read after Invalidate().
    std::function<std::vector<TObject>()> source;
    std::vector<TObject> owned;
    bool dirty = true;
};

// Returned by UIXListBuilder::Via(). Each row is mapped to another object
// (e.g. the component an entity row points at), so fields can be declared
// with plain member pointers instead of accessor lambdas:
//
//   runtime.BindList("Items", source)
//       .Via([&](Row& r) -> Item& { return reg.get<Item>(r.id); })
//       .Ref("name", &Item::name);
//
// TBuilder is the UIXListBuilder<TRow> it forwards to. Copies are cheap (the
// builder shares its state), and the projection is copied into each binding,
// so capture by reference only what outlives the UI.
template <typename TBuilder, typename TRow, typename TProject>
class UIXProjectedListBuilder {
public:
    using Target = std::remove_cv_t<
        std::remove_reference_t<std::invoke_result_t<TProject&,TRow&>>
    >;

    UIXProjectedListBuilder(TBuilder builder, TProject project)
        : builder_(std::move(builder)), project_(std::move(project)) {}

    // Reference field by member pointer: Ref("name", &Item::name).
    // A const member makes the field read-only.
    template <typename TField>
    UIXProjectedListBuilder& Ref(std::string name, TField Target::* member) {
        builder_.Ref(
            std::move(name),
            [project=project_,member](TRow& row) mutable -> TField& {
                return project(row).*member;
            }
        );
        return *this;
    }

    // Same, for an accessor on the projected object:
    //   Ref("hp", [](Item& i) -> int& { return i.stats.hp; })
    template <typename TAccessor>
    UIXProjectedListBuilder& Ref(std::string name, TAccessor accessor) {
        builder_.Ref(
            std::move(name),
            [project=project_,accessor](TRow& row) mutable -> decltype(auto) {
                return accessor(project(row));
            }
        );
        return *this;
    }

    // Field() is an alias so it reads the same as the non-projected builder.
    template <typename TField>
    UIXProjectedListBuilder& Field(std::string name, TField Target::* member) {
        return Ref(std::move(name),member);
    }

    // Back to the row-level builder (to add Field("id",&Row::id) etc.).
    TBuilder& Base() { return builder_; }

    UIXBindingHandle Handle() const { return builder_.Handle(); }
    UIXBindingHandle Done() const { return builder_.Done(); }
    UIXScopedBinding Scoped() const { return builder_.Scoped(); }

private:
    TBuilder builder_;
    TProject project_;
};

template <typename TObject>
class UIXListBuilder {
public:
    using State = UIXListBuilderState<TObject>;

    UIXListBuilder() = default;
    UIXListBuilder(
        std::shared_ptr<State> state,
        UIXBindingHandle handle
    )
        : state_(std::move(state)), handle_(std::move(handle)) {}

    template <typename TField>
    UIXListBuilder& Field(
        std::string name,
        TField TObject::* member
    ) {
        state_->configure.push_back(
            [name=std::move(name),member](UIXObject& output,TObject& object) {
                output.fields[name] = UIXBinding::Reference(object.*member);
            }
        );
        return *this;
    }

    template <typename TField, typename TGetter, typename TSetter>
    UIXListBuilder& Property(
        std::string name,
        TGetter getter,
        TSetter setter
    ) {
        state_->configure.push_back(
            [name=std::move(name),getter,setter](UIXObject& output,TObject& object) {
                TObject* pointer = &object;
                output.fields[name] = UIXBinding(
                    [pointer,getter]() {
                        return UIXValue::From(getter(*pointer));
                    },
                    [pointer,setter](const UIXValue& incoming) {
                        TField converted{};
                        if(!incoming.AssignTo(converted)) return false;
                        setter(*pointer,std::move(converted));
                        return true;
                    }
                );
            }
        );
        return *this;
    }

    // One accessor instead of a getter/setter pair:
    //   list.Ref("name", [&](Row& r) -> std::string& { return reg.get<C>(r.id).name; });
    // A const reference result makes the field read-only.
    template <typename TAccessor>
    UIXListBuilder& Ref(std::string name, TAccessor accessor) {
        state_->configure.push_back(
            [name=std::move(name),accessor](UIXObject& output,TObject& object) {
                output.fields[name] = UIXRefBinding(&object,accessor);
            }
        );
        return *this;
    }

    template <typename TField, typename TConfigure>
    UIXListBuilder& Object(
        std::string name,
        TField TObject::* member,
        TConfigure configure
    ) {
        state_->configure.push_back(
            [name=std::move(name),member,configure](UIXObject& output,TObject& object) {
                auto nested = std::make_shared<UIXObject>();
                UIXObjectBuilder<TField> builder(nested,object.*member);
                configure(builder);
                output.fields[name] =
                    UIXBinding::Constant(UIXValue(std::move(nested)));
            }
        );
        return *this;
    }

    UIXListBuilder& Value(std::string name, UIXValue value) {
        state_->configure.push_back(
            [name=std::move(name),value=std::move(value)](UIXObject& output,TObject&) {
                output.fields[name] = UIXBinding::Constant(value);
            }
        );
        return *this;
    }

    // Maps each row to another object so Ref()/Field() can take member
    // pointers of it. `project` must return an lvalue reference.
    template <typename TProject>
    auto Via(TProject project) {
        static_assert(
            std::is_lvalue_reference_v<std::invoke_result_t<TProject&,TObject&>>,
            "Via projection must return an lvalue reference, e.g. [&](Row& r) -> C& { ... }"
        );
        return UIXProjectedListBuilder<UIXListBuilder,TObject,TProject>(
            *this,std::move(project)
        );
    }

    // Used by BindList(name, source_fn).
    template <typename TSource>
    UIXListBuilder& SetSource(TSource source) {
        state_->source = std::move(source);
        state_->dirty = true;
        return *this;
    }

    UIXBindingHandle Handle() const { return handle_; }
    UIXBindingHandle Done() const { return handle_; }
    UIXScopedBinding Scoped() const {
        return UIXScopedBinding(handle_);
    }

private:
    std::shared_ptr<State> state_;
    UIXBindingHandle handle_;
};

struct UIXEvent {
    std::string type;
    std::string id;
    UIXValue value;
    UIXValue previous_value;
    int index = -1;
    Rectangle bounds{};
    void* native_widget = nullptr;

    // When the handler runs inside a <for>, this is the row object of the
    // innermost loop and `index` is its loop index (unless the widget already
    // supplied one).
    std::shared_ptr<UIXObject> item;

    // The C++ row behind the enclosing <for> iteration, or nullptr if the
    // event is not inside a loop over a BindList of TItem.
    template <typename TItem>
    TItem* Item() const {
        if(!item || item->native == nullptr || item->native_type == nullptr) return nullptr;
        return *item->native_type == typeid(TItem)
            ? static_cast<TItem*>(item->native)
            : nullptr;
    }
};

using UIXArguments = std::vector<UIXValue>;
using UIXNativeFunction =
    std::function<UIXValue(const UIXArguments&,UIXEvent&)>;

class UIXBindingGroup;

class UIXRegistry {
public:
    using WarningHandler = std::function<void(const std::string&)>;

    UIXRegistry();
    ~UIXRegistry();
    UIXRegistry(UIXRegistry&&) noexcept;
    UIXRegistry& operator=(UIXRegistry&&) noexcept;
    UIXRegistry(const UIXRegistry&) = delete;
    UIXRegistry& operator=(const UIXRegistry&) = delete;

    UIXBindingHandle Bind(std::string name, UIXBinding binding);

    template <typename T>
    UIXBindingHandle Bind(std::string name, T& value) {
        return Bind(std::move(name),UIXBinding::Reference(value));
    }

    template <typename T>
    UIXBindingHandle BindTracked(
        std::string name,
        T& value,
        const UIXBindingLifetime& lifetime
    ) {
        return Bind(
            std::move(name),
            UIXBinding::TrackedReference(value,lifetime.Token())
        );
    }

    UIXScopedBinding BindScoped(std::string name, UIXBinding binding) {
        return UIXScopedBinding(Bind(std::move(name),std::move(binding)));
    }

    template <typename T>
    UIXScopedBinding BindScoped(std::string name, T& value) {
        return UIXScopedBinding(Bind(std::move(name),value));
    }

    UIXBindingHandle Value(std::string name, UIXValue value);
    UIXBindingHandle Constant(std::string name, UIXValue value) {
        return Value(std::move(name),std::move(value));
    }

    template <typename TObject>
    UIXObjectBuilder<TObject> BindObject(
        std::string name,
        TObject& object
    ) {
        auto fields = std::make_shared<UIXObject>();
        const UIXBindingHandle handle = Bind(
            std::move(name),
            UIXBinding::Constant(UIXValue(fields))
        );
        return UIXObjectBuilder<TObject>(fields,object,handle);
    }

    template <typename TObject, typename TConfigure>
    UIXObjectBuilder<TObject> BindObject(
        std::string name,
        TObject& object,
        TConfigure configure
    ) {
        auto builder = BindObject(std::move(name),object);
        configure(builder);
        return builder;
    }

    template <typename TObject>
    UIXObjectBuilder<TObject> BindObjectTracked(
        std::string name,
        TObject& object,
        const UIXBindingLifetime& lifetime
    ) {
        auto fields = std::make_shared<UIXObject>();
        const UIXBindingHandle handle = Bind(
            std::move(name),
            UIXBinding::TrackedConstant(UIXValue(fields),lifetime.Token())
        );
        return UIXObjectBuilder<TObject>(fields,object,handle);
    }

    template <typename TObject>
    UIXListBuilder<TObject> BindList(
        std::string name,
        std::vector<TObject>& values
    ) {
        return BindListImpl<TObject>(std::move(name),&values);
    }

    // Shared by the vector and source overloads. `values` may be null for a
    // source-driven list; the source fills it on first read.
    template <typename TObject>
    UIXListBuilder<TObject> BindListImpl(
        std::string name,
        std::vector<TObject>* values
    ) {
        auto state = std::make_shared<UIXListBuilderState<TObject>>();
        state->values = values;

        UIXBinding binding(
            [state]() {
                if(state->source && state->dirty) {
                    state->owned = state->source();
                    state->values = &state->owned;
                    state->dirty = false;
                }
                auto array = std::make_shared<UIXArray>();
                if(state->values == nullptr) return UIXValue(std::move(array));
                array->items.reserve(state->values->size());

                for(TObject& value : *state->values) {
                    if(state->configure.empty()) {
                        if constexpr(
                            std::is_constructible_v<UIXValue,TObject> ||
                            std::is_enum_v<TObject> ||
                            std::is_arithmetic_v<TObject>
                        ) {
                            array->items.push_back(UIXBinding::Reference(value));
                        }
                        else {
                            auto fields = std::make_shared<UIXObject>();
                            fields->native = &value;
                            fields->native_type = &typeid(TObject);
                            array->items.push_back(
                                UIXBinding::Constant(UIXValue(std::move(fields)))
                            );
                        }
                        continue;
                    }

                    auto fields = std::make_shared<UIXObject>();
                    fields->native = &value;
                    fields->native_type = &typeid(TObject);
                    for(const auto& configure : state->configure) {
                        configure(*fields,value);
                    }
                    array->items.push_back(
                        UIXBinding::Constant(UIXValue(std::move(fields)))
                    );
                }
                return UIXValue(std::move(array));
            }
        );

        binding.OnInvalidate([state]() {
            if(!state->source) return false;   // vector-backed lists are always live
            state->dirty = true;
            return true;
        });
        const UIXBindingHandle handle = Bind(
            std::move(name),
            std::move(binding)
        );
        return UIXListBuilder<TObject>(std::move(state),handle);
    }

    // Source-driven list: rows come from a function instead of a vector you
    // keep in sync. The function runs on first read and again on the first
    // read after Invalidate(name):
    //
    //   runtime.BindList("GameComponents", [&]{ return BuildRows(); });
    //   ...
    //   registry.on_construct<GameComponent>().connect<&Invalidate>();  // -> runtime.Invalidate("GameComponents")
    //
    // Rows are owned by the list and stay put until the next Invalidate().
    template <
        typename TSource,
        typename = std::enable_if_t<std::is_invocable_v<TSource&>>
    >
    auto BindList(std::string name, TSource source) {
        using TObject = typename std::invoke_result_t<TSource&>::value_type;
        auto builder = BindListImpl<TObject>(std::move(name),nullptr);
        builder.SetSource(std::move(source));
        return builder;
    }

    template <
        typename TSource, typename TConfigure,
        typename = std::enable_if_t<std::is_invocable_v<TSource&>>
    >
    auto BindList(std::string name, TSource source, TConfigure configure) {
        auto builder = BindList(std::move(name),std::move(source));
        configure(builder);
        return builder;
    }

    // Marks a cached BindList source as stale. Returns false if `name` is not
    // a source-driven list. Nothing is rebuilt until the UI is rebuilt; use
    // UIXRuntime::Invalidate to also request that.
    bool Invalidate(const std::string& name);

    template <typename TObject, typename TConfigure>
    UIXListBuilder<TObject> BindList(
        std::string name,
        std::vector<TObject>& values,
        TConfigure configure
    ) {
        auto builder = BindList(std::move(name),values);
        configure(builder);
        return builder;
    }

    UIXBindingHandle Function(
        std::string name,
        UIXNativeFunction function
    );
    UIXBindingHandle Function(
        std::string name,
        std::function<void()> function
    );
    UIXBindingHandle Function(
        std::string name,
        std::function<void(const UIXEvent&)> function
    );

    // Registers a function that receives the C++ row of the enclosing <for>
    // (a BindList<TItem> element) instead of making you look it up:
    //   ItemFunction<Row>("select", [](Row& row) { ... });
    //   ItemFunction<Row>("select", [](Row& row, const UIXEvent& e) { ... });
    //   ItemFunction<Row>("select", [](Row& row, const UIXArguments& a, UIXEvent& e) { ... });
    // If the call is not inside a loop over TItem, the callback is skipped.
    template <typename TItem, typename TFunction>
    UIXBindingHandle ItemFunction(std::string name, TFunction function) {
        return Function(
            std::move(name),
            UIXNativeFunction(
                [function=std::move(function)](
                    const UIXArguments& arguments,UIXEvent& event
                ) mutable -> UIXValue {
                    TItem* item = event.Item<TItem>();
                    if(item == nullptr) return UIXValue{};
                    if constexpr(std::is_invocable_v<TFunction&,TItem&,const UIXArguments&,UIXEvent&>) {
                        function(*item,arguments,event);
                    }
                    else if constexpr(std::is_invocable_v<TFunction&,TItem&,const UIXEvent&>) {
                        function(*item,static_cast<const UIXEvent&>(event));
                    }
                    else {
                        function(*item);
                    }
                    return UIXValue{};
                }
            )
        );
    }

    bool Unbind(const std::string& name);
    bool Unbind(const UIXBindingHandle& handle);
    bool UnbindFunction(const std::string& name);
    bool UnbindFunction(const UIXBindingHandle& handle);
    void ClearBindings();
    void ClearFunctions();
    void Clear();
    std::size_t CleanupExpired();

    bool HasBinding(const std::string& name) const;
    bool HasFunction(const std::string& name) const;
    std::size_t BindingCount() const;
    std::size_t FunctionCount() const;

    void SetWarningHandler(WarningHandler handler);

    UIXBinding ResolveBinding(const std::string& path) const;
    UIXValue Resolve(const std::string& path) const;
    bool Assign(const std::string& path, const UIXValue& value) const;
    UIXValue Invoke(
        const std::string& name,
        const UIXArguments& arguments,
        UIXEvent& event
    ) const;

    UIXBindingGroup BindGroup();

private:
    friend class UIXBindingHandle;
    std::shared_ptr<UIXRegistryState> state_;
};

// Owns multiple registrations and removes all of them on destruction.
// Store this as the last member of an object to unbind before its data dies.
class UIXBindingGroup {
public:
    UIXBindingGroup() = default;
    explicit UIXBindingGroup(UIXRegistry& registry)
        : registry_(&registry) {}
    ~UIXBindingGroup() { Reset(); }

    UIXBindingGroup(UIXBindingGroup&& other) noexcept
        : registry_(other.registry_), handles_(std::move(other.handles_)) {
        other.registry_ = nullptr;
        other.handles_.clear();
    }

    UIXBindingGroup& operator=(UIXBindingGroup&& other) noexcept {
        if(this == &other) return *this;
        Reset();
        registry_ = other.registry_;
        handles_ = std::move(other.handles_);
        other.registry_ = nullptr;
        other.handles_.clear();
        return *this;
    }

    UIXBindingGroup(const UIXBindingGroup&) = delete;
    UIXBindingGroup& operator=(const UIXBindingGroup&) = delete;

    UIXBindingHandle Add(UIXBindingHandle handle) {
        if(handle.Valid()) handles_.push_back(handle);
        return handle;
    }

    template <typename T>
    UIXBindingHandle Bind(std::string name, T& value) {
        return Add(Registry().Bind(std::move(name),value));
    }

    UIXBindingHandle Bind(std::string name, UIXBinding binding) {
        return Add(Registry().Bind(std::move(name),std::move(binding)));
    }

    template <typename T>
    UIXBindingHandle BindTracked(
        std::string name,
        T& value,
        const UIXBindingLifetime& lifetime
    ) {
        return Add(Registry().BindTracked(
            std::move(name),value,lifetime
        ));
    }

    template <typename TObject>
    UIXObjectBuilder<TObject> BindObject(
        std::string name,
        TObject& object
    ) {
        auto builder = Registry().BindObject(std::move(name),object);
        Add(builder.Handle());
        return builder;
    }

    template <typename TObject, typename TConfigure>
    UIXObjectBuilder<TObject> BindObject(
        std::string name,
        TObject& object,
        TConfigure configure
    ) {
        auto builder = BindObject(std::move(name),object);
        configure(builder);
        return builder;
    }

    template <typename TObject>
    UIXObjectBuilder<TObject> BindObjectTracked(
        std::string name,
        TObject& object,
        const UIXBindingLifetime& lifetime
    ) {
        auto builder = Registry().BindObjectTracked(
            std::move(name),object,lifetime
        );
        Add(builder.Handle());
        return builder;
    }

    template <typename TObject>
    UIXListBuilder<TObject> BindList(
        std::string name,
        std::vector<TObject>& values
    ) {
        auto builder = Registry().BindList(std::move(name),values);
        Add(builder.Handle());
        return builder;
    }

    template <typename TObject, typename TConfigure>
    UIXListBuilder<TObject> BindList(
        std::string name,
        std::vector<TObject>& values,
        TConfigure configure
    ) {
        auto builder = BindList(std::move(name),values);
        configure(builder);
        return builder;
    }

    template <
        typename TSource,
        typename = std::enable_if_t<std::is_invocable_v<TSource&>>
    >
    auto BindList(std::string name, TSource source) {
        auto builder = Registry().BindList(std::move(name),std::move(source));
        Add(builder.Handle());
        return builder;
    }

    template <
        typename TSource, typename TConfigure,
        typename = std::enable_if_t<std::is_invocable_v<TSource&>>
    >
    auto BindList(std::string name, TSource source, TConfigure configure) {
        auto builder = BindList(std::move(name),std::move(source));
        configure(builder);
        return builder;
    }

    template <typename TFunction>
    UIXBindingHandle Function(
        std::string name,
        TFunction&& function
    ) {
        return Add(Registry().Function(
            std::move(name),
            std::forward<TFunction>(function)
        ));
    }

    template <typename TItem, typename TFunction>
    UIXBindingHandle ItemFunction(
        std::string name,
        TFunction&& function
    ) {
        return Add(Registry().template ItemFunction<TItem>(
            std::move(name),
            std::forward<TFunction>(function)
        ));
    }

    UIXBindingHandle Constant(std::string name, UIXValue value) {
        return Add(Registry().Constant(std::move(name),std::move(value)));
    }

    void Reset() {
        for(auto iterator = handles_.rbegin(); iterator != handles_.rend(); ++iterator) {
            iterator->Unbind();
        }
        handles_.clear();
    }

    std::vector<UIXBindingHandle> Release() {
        return std::exchange(handles_,{});
    }

    std::size_t Size() const { return handles_.size(); }
    bool Empty() const { return handles_.empty(); }

private:
    UIXRegistry& Registry() const { return *registry_; }

    UIXRegistry* registry_ = nullptr;
    std::vector<UIXBindingHandle> handles_;
};

inline UIXBindingGroup UIXRegistry::BindGroup() {
    return UIXBindingGroup(*this);
}

enum class UIXNodeType {
    Element,
    Text
};
struct UIXPreset {
    std::unordered_map<std::string, std::string> values;
};

struct UIXSourceLocation {
    std::string file;
    int line = 1;
    int column = 1;
};

struct UIXAttribute {
    std::string name;
    std::string value;
    UIXSourceLocation location;
};

struct UIXNode {
    UIXNodeType type = UIXNodeType::Element;
    std::string name;
    std::string text;
    std::vector<UIXAttribute> attributes;
    std::vector<UIXNode> children;
    UIXSourceLocation location;
};

struct UIXFunction {
    std::string name;
    std::vector<std::string> parameters;
    UIXNode declaration;
};

struct UIXComponent {
    std::string name;
    UIXNode declaration;
};

struct UIXDiagnostic {
    UIXSourceLocation location;
    std::string message;
    bool error = true;
};

struct UIXDocument {
    std::vector<UIXNode> roots;
    std::unordered_map<std::string,UIXFunction> functions;
    std::unordered_map<std::string,UIXComponent> components;
    std::vector<UIXDiagnostic> diagnostics;
    std::unordered_map<std::string, UIXPreset> presets;

    bool Valid() const;
};

UIXDocument ParseUIX(
    const std::string& source,
    std::string source_name = "<memory>"
);

struct UIXBuildOptions {
    bool warn_unknown_attributes = true;
    bool warn_unknown_elements = true;
    bool preserve_runtime_layout = true;
};

// Lightweight non-owning control wrapper for state and animation commands.
// It becomes invalid when the corresponding UI tree element is rebuilt away.
class UIXControl {
public:
    UIXControl() = default;
    explicit UIXControl(UIElementBase* element) : element_(element) {}

    bool Valid() const { return element_ != nullptr; }
    explicit operator bool() const { return Valid(); }

    bool SetState(const std::string& name, bool active = true) {
        if(element_ == nullptr) return false;
        element_->pseudo_states.Set(name,active);
        return true;
    }
    bool DefineState(
        std::string name,
        bool initial_value = false,
        std::function<void(bool&)> update = {}
    ) {
        if(element_ == nullptr) return false;
        element_->pseudo_states.Define(
            std::move(name),initial_value,std::move(update)
        );
        return true;
    }
    bool DefineStateTracked(
        std::string name,
        const UILifetimeToken& lifetime,
        bool initial_value = false,
        std::function<void(bool&)> update = {}
    ) {
        if(element_ == nullptr) return false;
        element_->pseudo_states.DefineTracked(
            std::move(name),initial_value,lifetime.Token(),std::move(update)
        );
        return true;
    }
    bool ToggleState(const std::string& name) {
        if(element_ == nullptr) return false;
        element_->pseudo_states.Toggle(name);
        return true;
    }
    bool RemoveState(const std::string& name) {
        return element_ != nullptr && element_->pseudo_states.Remove(name);
    }
    bool HasState(const std::string& name) const {
        return element_ != nullptr && element_->pseudo_states.Active(name);
    }
    bool Play(const std::string& name, bool restart = true) {
        return element_ != nullptr && PlayUIAnimation(*element_,name,restart);
    }
    bool Pause(const std::string& name) {
        return element_ != nullptr && PauseUIAnimation(*element_,name);
    }
    bool Resume(const std::string& name) {
        return element_ != nullptr && ResumeUIAnimation(*element_,name);
    }
    bool Stop(const std::string& name) {
        return element_ != nullptr && StopUIAnimation(*element_,name);
    }
    bool Restart(const std::string& name) {
        return element_ != nullptr && RestartUIAnimation(*element_,name);
    }
    bool Reverse(const std::string& name) {
        return element_ != nullptr && ReverseUIAnimation(*element_,name);
    }
    bool IsPlaying(const std::string& name) const {
        return element_ != nullptr && IsUIAnimationPlaying(*element_,name);
    }
    UIElementBase* Native() const { return element_; }

private:
    UIElementBase* element_ = nullptr;
};

class UIXRuntime {
public:
    struct Impl;

    UIXRuntime();
    explicit UIXRuntime(UIXRegistry& registry);
    ~UIXRuntime();
    UIXRuntime(UIXRuntime&&) noexcept;
    UIXRuntime& operator=(UIXRuntime&&) noexcept;
    UIXRuntime(const UIXRuntime&) = delete;
    UIXRuntime& operator=(const UIXRuntime&) = delete;

    UIXRegistry& Registry();
    const UIXRegistry& Registry() const;

    // RUI 0.9 component metadata/registration registry. The default registry
    // contains the built-in components but stays mutable until the first
    // rebuild so applications can register custom components explicitly.
    UIComponentRegistry& Components();
    const UIComponentRegistry& Components() const;
    void SetComponentRegistry(std::shared_ptr<UIComponentRegistry> registry);
    bool FreezeComponents(std::vector<std::string>* diagnostics = nullptr);
    std::string ComponentSchemaJson() const;
    std::string ProjectSchemaJson() const;
    std::string ComponentReferenceMarkdown() const;

    // Common registry operations are forwarded so most users never need Registry().
    UIXBindingHandle Bind(std::string name, UIXBinding binding) {
        return Registry().Bind(std::move(name),std::move(binding));
    }

    template <typename T>
    UIXBindingHandle Bind(std::string name, T& value) {
        return Registry().Bind(std::move(name),value);
    }

    template <typename T>
    UIXBindingHandle BindTracked(
        std::string name,
        T& value,
        const UIXBindingLifetime& lifetime
    ) {
        return Registry().BindTracked(std::move(name),value,lifetime);
    }

    UIXScopedBinding BindScoped(std::string name, UIXBinding binding) {
        return Registry().BindScoped(std::move(name),std::move(binding));
    }

    template <typename T>
    UIXScopedBinding BindScoped(std::string name, T& value) {
        return Registry().BindScoped(std::move(name),value);
    }

    template <typename TObject>
    UIXObjectBuilder<TObject> BindObject(
        std::string name,
        TObject& object
    ) {
        return Registry().BindObject(std::move(name),object);
    }

    template <typename TObject, typename TConfigure>
    UIXObjectBuilder<TObject> BindObject(
        std::string name,
        TObject& object,
        TConfigure configure
    ) {
        return Registry().BindObject(
            std::move(name),object,std::move(configure)
        );
    }

    template <typename TObject>
    UIXObjectBuilder<TObject> BindObjectTracked(
        std::string name,
        TObject& object,
        const UIXBindingLifetime& lifetime
    ) {
        return Registry().BindObjectTracked(
            std::move(name),object,lifetime
        );
    }

    template <typename TObject>
    UIXListBuilder<TObject> BindList(
        std::string name,
        std::vector<TObject>& values
    ) {
        return Registry().BindList(std::move(name),values);
    }

    template <typename TObject, typename TConfigure>
    UIXListBuilder<TObject> BindList(
        std::string name,
        std::vector<TObject>& values,
        TConfigure configure
    ) {
        return Registry().BindList(
            std::move(name),values,std::move(configure)
        );
    }

    template <
        typename TSource,
        typename = std::enable_if_t<std::is_invocable_v<TSource&>>
    >
    auto BindList(std::string name, TSource source) {
        return Registry().BindList(std::move(name),std::move(source));
    }

    template <
        typename TSource, typename TConfigure,
        typename = std::enable_if_t<std::is_invocable_v<TSource&>>
    >
    auto BindList(std::string name, TSource source, TConfigure configure) {
        return Registry().BindList(
            std::move(name),std::move(source),std::move(configure)
        );
    }

    // Marks a source-driven list (BindList(name, fn)) stale and requests a
    // rebuild, so the next rui::Context::Update re-reads it. Safe to call from
    // entt signals / destructors: nothing is rebuilt inside the call.
    bool Invalidate(const std::string& name) {
        const bool found = Registry().Invalidate(name);
        if(found) RequestRebuild();
        return found;
    }

    template <typename TFunction>
    UIXBindingHandle Function(
        std::string name,
        TFunction&& function
    ) {
        return Registry().Function(
            std::move(name),
            std::forward<TFunction>(function)
        );
    }

    template <typename TItem, typename TFunction>
    UIXBindingHandle ItemFunction(
        std::string name,
        TFunction&& function
    ) {
        return Registry().template ItemFunction<TItem>(
            std::move(name),
            std::forward<TFunction>(function)
        );
    }

    UIXBindingHandle Constant(std::string name, UIXValue value) {
        return Registry().Constant(std::move(name),std::move(value));
    }

    bool Unbind(const std::string& name) {
        return Registry().Unbind(name);
    }
    bool Unbind(const UIXBindingHandle& handle) {
        return Registry().Unbind(handle);
    }
    bool UnbindFunction(const std::string& name) {
        return Registry().UnbindFunction(name);
    }
    bool UnbindFunction(const UIXBindingHandle& handle) {
        return Registry().UnbindFunction(handle);
    }
    void ClearBindings() { Registry().ClearBindings(); }
    void ClearFunctions() { Registry().ClearFunctions(); }
    void ClearRegistry() { Registry().Clear(); }
    void SetWarningHandler(UIXRegistry::WarningHandler handler) {
        Registry().SetWarningHandler(std::move(handler));
    }
    UIXBindingGroup BindGroup() { return Registry().BindGroup(); }

    bool LoadString(
        const std::string& source,
        std::string source_name = "<memory>"
    );
    // Loads one RML file and recursively resolves its <Import src="..."/> tags.
    bool LoadFile(const std::string& path);
    bool Mount(
        const std::string& path,
        UIContainer& destination,
        const UIXBuildOptions& options = {}
    );
    void SetDocument(UIXDocument document);

    const UIXDocument& Document() const;
    // Combined contents of inline <Style> blocks and linked <Style src="..."/>
    // files in the currently loaded document.
    const std::string& EmbeddedStyleSource() const;
    const std::vector<UIXDiagnostic>& Diagnostics() const;
    void ClearRuntimeDiagnostics();

    void Rebuild(
        UIContainer& destination,
        const UIXBuildOptions& options = {}
    );

    // Set when scope-local `state="..."` changed value (or RequestRebuild()
    // was called). Markup `if`/`for` are evaluated at build time, so the host
    // must call Rebuild() afterwards; rui::Context::Update does this itself.
    // Rebuild() clears the flag.
    bool RebuildRequested() const;
    void RequestRebuild();

    // Synchronizes two-way state without rebuilding the tree.
    void Update(UIContainer& destination);
    void Sync(UIContainer& destination) { Update(destination); }

    UIXControl Control(const std::string& key);
    UIXControl Control(UIContainer& root, const std::string& key);
    bool SetPseudoState(const std::string& key, const std::string& state, bool active = true);
    bool TogglePseudoState(const std::string& key, const std::string& state);
    bool RemovePseudoState(const std::string& key, const std::string& state);
    bool HasPseudoState(const std::string& key, const std::string& state);
    bool PlayAnimation(const std::string& key, const std::string& animation, bool restart = true);
    bool PauseAnimation(const std::string& key, const std::string& animation);
    bool ResumeAnimation(const std::string& key, const std::string& animation);
    bool StopAnimation(const std::string& key, const std::string& animation);
    bool RestartAnimation(const std::string& key, const std::string& animation);
    bool ReverseAnimation(const std::string& key, const std::string& animation);
    bool IsAnimationPlaying(const std::string& key, const std::string& animation);

    UIUndoStack& UndoStack();
    const UIUndoStack& UndoStack() const;
    bool Undo();
    bool Redo();

    UIXValue Evaluate(const std::string& expression);
    bool Execute(const std::string& statements);
    UIXValue Invoke(
        const std::string& function,
        const UIXArguments& arguments = {},
        UIXEvent event = {}
    );

private:
    std::unique_ptr<Impl> impl_;
};