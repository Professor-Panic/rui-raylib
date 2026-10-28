#pragma once

#include "css_parser.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// RUI 0.9 component metadata/runtime layer.
//
// 0.8 established registry-driven component construction. 0.9 makes that
// metadata operational: UIComponent owns common lifecycle mechanics,
// capabilities drive interaction, constraints/validators participate in the
// assignment path, custom converters are registry-owned, serializable
// properties round-trip generically, child policies are enforceable, aliases
// are lifetime-stable, and schema output is richer.

inline std::string NormalizeUIComponentName(std::string value) {
    value.erase(
        std::remove_if(value.begin(),value.end(),[](unsigned char c) {
            return std::isspace(c) != 0 || c == '-' || c == '_';
        }),
        value.end()
    );
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

inline std::string UIComponentSyntheticClass(std::string_view name) {
    return "__rui-component-"+NormalizeUIComponentName(std::string(name));
}

enum class UIPropertyType {
    String,
    Bool,
    Int,
    Float,
    Double,
    Color,
    Vector2,
    Vector3,
    Vector4,
    Rectangle,
    Enum,
    Custom
};

struct UICustomPropertyValue {
    std::type_index type = typeid(void);
    std::shared_ptr<void> storage;

    template <typename T>
    const T* Get() const {
        if(type != std::type_index(typeid(T)) || !storage) return nullptr;
        return static_cast<const T*>(storage.get());
    }

    friend bool operator==(const UICustomPropertyValue& a,const UICustomPropertyValue& b) {
        return a.type == b.type && a.storage == b.storage;
    }
};

template <typename T>
inline UICustomPropertyValue MakeUICustomPropertyValue(T value) {
    return {std::type_index(typeid(T)),std::make_shared<T>(std::move(value))};
}

using UIPropertyValue = std::variant<
    std::monostate,
    std::string,
    bool,
    int,
    float,
    double,
    Color,
    Vector2,
    Vector3,
    Vector4,
    Rectangle,
    UICustomPropertyValue
>;

enum class UIPropertyFlag : std::uint32_t {
    None         = 0,
    Bindable     = 1u << 0,
    Serializable = 1u << 1,
    Inspectable  = 1u << 2,
    ReadOnly     = 1u << 3,
    Required     = 1u << 4,
    RuntimeOnly  = 1u << 5,
    MarkupOnly   = 1u << 6,
    Deprecated   = 1u << 7
};

inline UIPropertyFlag operator|(UIPropertyFlag a,UIPropertyFlag b) {
    return static_cast<UIPropertyFlag>(
        static_cast<std::uint32_t>(a)|static_cast<std::uint32_t>(b)
    );
}
inline UIPropertyFlag& operator|=(UIPropertyFlag& a,UIPropertyFlag b) {
    a = a|b;
    return a;
}
inline bool HasUIPropertyFlag(UIPropertyFlag value,UIPropertyFlag flag) {
    return (static_cast<std::uint32_t>(value)&static_cast<std::uint32_t>(flag)) != 0;
}

struct UIVersionMetadata {
    std::string since = "0.9.0";
    std::string deprecated_since;
    std::string removed_in;
    std::string replacement;
};

enum class UIPropertyValidationPolicy {
    Reject,
    Clamp
};

struct UIPropertyConstraints {
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::vector<std::string> enum_values;
    UIPropertyValidationPolicy numeric_policy = UIPropertyValidationPolicy::Reject;
};

inline bool UIPropertyValueMatchesType(
    UIPropertyType type,
    const UIPropertyValue& value,
    std::type_index custom_type = typeid(void)
) {
    switch(type) {
        case UIPropertyType::String: return std::holds_alternative<std::string>(value);
        case UIPropertyType::Bool: return std::holds_alternative<bool>(value);
        case UIPropertyType::Int: return std::holds_alternative<int>(value);
        case UIPropertyType::Float: return std::holds_alternative<float>(value);
        case UIPropertyType::Double: return std::holds_alternative<double>(value);
        case UIPropertyType::Color: return std::holds_alternative<Color>(value);
        case UIPropertyType::Vector2: return std::holds_alternative<Vector2>(value);
        case UIPropertyType::Vector3: return std::holds_alternative<Vector3>(value);
        case UIPropertyType::Vector4: return std::holds_alternative<Vector4>(value);
        case UIPropertyType::Rectangle: return std::holds_alternative<Rectangle>(value);
        case UIPropertyType::Enum: return std::holds_alternative<std::string>(value);
        case UIPropertyType::Custom: {
            const auto* custom = std::get_if<UICustomPropertyValue>(&value);
            return custom && custom->type == custom_type;
        }
    }
    return false;
}

inline std::optional<double> UIPropertyNumericValue(const UIPropertyValue& value) {
    if(const int* v=std::get_if<int>(&value)) return static_cast<double>(*v);
    if(const float* v=std::get_if<float>(&value)) return static_cast<double>(*v);
    if(const double* v=std::get_if<double>(&value)) return *v;
    return std::nullopt;
}

inline void UIPropertyAssignNumeric(UIPropertyValue& value,double number) {
    if(std::holds_alternative<int>(value)) value=static_cast<int>(std::llround(number));
    else if(std::holds_alternative<float>(value)) value=static_cast<float>(number);
    else if(std::holds_alternative<double>(value)) value=number;
}

struct UIPropertyDescriptor {
    std::string name;
    std::string markup_name;
    UIPropertyType type = UIPropertyType::Custom;
    std::type_index custom_cpp_type = typeid(void);
    std::string custom_type_name;
    UIPropertyValue default_value;
    UIPropertyFlag flags =
        UIPropertyFlag::Bindable|
        UIPropertyFlag::Serializable|
        UIPropertyFlag::Inspectable;
    UIPropertyConstraints constraints;
    std::vector<std::string> aliases;
    std::string documentation;
    UIVersionMetadata version;

    // Optional RCSS exposure. Empty style_name means this remains an
    // RXML/runtime property only. Exposed values use the same transition/
    // keyframe engine as built-in RCSS properties.
    std::string style_name;

    std::function<bool(const UIPropertyValue&,std::string&)> validator;
    std::function<bool(UIElement&,const UIPropertyValue&,std::string&)> setter;
    std::function<UIPropertyValue(const UIElement&)> getter;
    // Direct custom-component access avoids manufacturing a temporary
    // UIElement variant when RCSS applies a visual override during Draw().
    std::function<bool(IUICustomElement&,const UIPropertyValue&,std::string&)> component_setter;
    std::function<UIPropertyValue(const IUICustomElement&)> component_getter;
    std::function<void(UIElement&)> invalidator;

    bool ValidateValue(
        const UIPropertyValue& input,
        UIPropertyValue* normalized,
        std::string* error = nullptr
    ) const {
        if(!UIPropertyValueMatchesType(type,input,custom_cpp_type)) {
            if(error) *error = "property '"+name+"' received the wrong value type";
            return false;
        }

        UIPropertyValue candidate=input;
        if(auto numeric=UIPropertyNumericValue(candidate)) {
            double value=*numeric;
            bool outside=false;
            if(constraints.minimum && value < *constraints.minimum) {
                outside=true;
                if(constraints.numeric_policy==UIPropertyValidationPolicy::Clamp) value=*constraints.minimum;
            }
            if(constraints.maximum && value > *constraints.maximum) {
                outside=true;
                if(constraints.numeric_policy==UIPropertyValidationPolicy::Clamp) value=*constraints.maximum;
            }
            if(outside && constraints.numeric_policy==UIPropertyValidationPolicy::Reject) {
                if(error) {
                    std::ostringstream message;
                    message << "property '" << name << "' is outside its allowed range";
                    *error=message.str();
                }
                return false;
            }
            if(outside) UIPropertyAssignNumeric(candidate,value);
        }

        if(type==UIPropertyType::Enum && !constraints.enum_values.empty()) {
            const std::string& text=std::get<std::string>(candidate);
            const std::string normalized_text=NormalizeUIComponentName(text);
            const bool valid=std::any_of(
                constraints.enum_values.begin(),constraints.enum_values.end(),
                [&](const std::string& item) {
                    return NormalizeUIComponentName(item)==normalized_text;
                }
            );
            if(!valid) {
                if(error) *error="invalid enum value '"+text+"' for property '"+name+"'";
                return false;
            }
        }

        if(validator) {
            std::string local;
            if(!validator(candidate,local)) {
                if(error) *error=local.empty()
                    ? "custom validation failed for property '"+name+"'"
                    : std::move(local);
                return false;
            }
        }

        if(normalized) *normalized=std::move(candidate);
        return true;
    }

    bool Set(UIElement& element,const UIPropertyValue& value,std::string* error = nullptr) const {
        if(HasUIPropertyFlag(flags,UIPropertyFlag::ReadOnly)) {
            if(error) *error = "property is read-only";
            return false;
        }
        if(!setter) {
            if(error) *error = "property has no setter";
            return false;
        }
        UIPropertyValue normalized;
        std::string local;
        if(!ValidateValue(value,&normalized,&local)) {
            if(error) *error=std::move(local);
            return false;
        }
        if(!setter(element,normalized,local)) {
            if(error) *error=std::move(local);
            return false;
        }
        if(invalidator) invalidator(element);
        if(error) error->clear();
        return true;
    }

    UIPropertyValue Get(const UIElement& element) const {
        return getter ? getter(element) : UIPropertyValue{};
    }
};

struct UIEventDescriptor {
    std::string name;
    std::string markup_name;
    std::string payload_type;
    std::string documentation;
    std::vector<std::string> aliases;
    UIVersionMetadata version;
};

enum class UIComponentCapability : std::uint32_t {
    None          = 0,
    Focusable     = 1u << 0,
    Clickable     = 1u << 1,
    Draggable     = 1u << 2,
    Scrollable    = 1u << 3,
    Selectable    = 1u << 4,
    KeyboardInput = 1u << 5,
    TextEditable  = 1u << 6,
    PopupOwner    = 1u << 7
};
inline UIComponentCapability operator|(UIComponentCapability a,UIComponentCapability b) {
    return static_cast<UIComponentCapability>(
        static_cast<std::uint32_t>(a)|static_cast<std::uint32_t>(b)
    );
}
inline UIComponentCapability& operator|=(UIComponentCapability& a,UIComponentCapability b) {
    a=a|b; return a;
}
inline bool HasUIComponentCapability(UIComponentCapability value,UIComponentCapability flag) {
    return (static_cast<std::uint32_t>(value)&static_cast<std::uint32_t>(flag)) != 0;
}

enum class UIChildPolicyKind {
    None,
    Any,
    SpecificTypes,
    ConfigurationOnly,
    Custom
};

struct UIChildPolicy {
    UIChildPolicyKind kind = UIChildPolicyKind::None;
    std::vector<std::string> allowed_types;
};

enum class UIComponentKind {
    Visual,
    Configuration
};

using UIConfigurationValues=std::unordered_map<std::string,UIPropertyValue>;

enum class UIRegistrationConflictPolicy {
    RejectDuplicate,
    Replace,
    Alias
};

struct UIComponentDescriptor {
    std::string name;
    std::string parent;
    std::vector<std::string> aliases;
    UIComponentKind kind = UIComponentKind::Visual;
    std::function<UIElement()> factory;
    SelectorType selector = SelectorType::Any;
    std::vector<UIPropertyDescriptor> properties;
    std::vector<UIEventDescriptor> events;
    UIComponentCapability capabilities = UIComponentCapability::None;
    UIChildPolicy children;
    std::string documentation;
    UIVersionMetadata version;
    bool generic_markup = true;
    bool builtin = false;

    // Configuration descriptors receive parsed/typed attributes without
    // becoming visual nodes. Custom child policies receive the same controlled
    // metadata packet and decide how it affects the parent.
    std::function<bool(UIElement&,const UIConfigurationValues&,std::string&)> configure_parent;
    std::function<bool(UIElement&,std::string_view,const UIConfigurationValues&,std::string&)> child_processor;

    const UIPropertyDescriptor* FindOwnProperty(std::string_view raw_name) const {
        const std::string normalized=NormalizeUIComponentName(std::string(raw_name));
        for(const UIPropertyDescriptor& property:properties) {
            if(NormalizeUIComponentName(property.name)==normalized ||
               NormalizeUIComponentName(property.markup_name)==normalized) return &property;
            for(const std::string& alias:property.aliases) {
                if(NormalizeUIComponentName(alias)==normalized) return &property;
            }
        }
        return nullptr;
    }

    const UIPropertyDescriptor* FindOwnStyleProperty(std::string_view raw_name) const {
        const std::string normalized=NormalizeUIComponentName(std::string(raw_name));
        for(const UIPropertyDescriptor& property:properties) {
            if(property.style_name.empty()) continue;
            if(NormalizeUIComponentName(property.style_name)==normalized) return &property;
        }
        return nullptr;
    }

    const UIEventDescriptor* FindOwnEvent(std::string_view raw_name) const {
        const std::string normalized=NormalizeUIComponentName(std::string(raw_name));
        for(const UIEventDescriptor& event:events) {
            if(NormalizeUIComponentName(event.name)==normalized ||
               NormalizeUIComponentName(event.markup_name)==normalized) return &event;
            for(const std::string& alias:event.aliases) {
                if(NormalizeUIComponentName(alias)==normalized) return &event;
            }
        }
        return nullptr;
    }
};

class UIPropertyConverterRegistry {
public:
    struct Entry {
        std::string type_name;
        std::function<std::optional<UICustomPropertyValue>(std::string_view,std::string&)> parse;
        std::function<std::optional<std::string>(const UICustomPropertyValue&,std::string&)> format;
    };

    template <typename T,typename Parser>
    bool Register(
        Parser parser,
        std::function<std::string(const T&)> formatter = {},
        std::string type_name = {},
        std::string* error = nullptr
    ) {
        if(frozen_) {
            if(error) *error="property converter registry is frozen";
            return false;
        }
        const std::type_index key=typeid(T);
        if(entries_.count(key)!=0) {
            if(error) *error="duplicate custom property converter";
            return false;
        }
        Entry entry;
        entry.type_name=type_name.empty()?typeid(T).name():std::move(type_name);
        entry.parse=[parser=std::move(parser)](std::string_view text,std::string& local)
            -> std::optional<UICustomPropertyValue> {
            std::optional<T> parsed;
            if constexpr(std::is_invocable_r_v<std::optional<T>,Parser,std::string_view,std::string&>) {
                parsed=parser(text,local);
            }
            else {
                static_assert(
                    std::is_invocable_r_v<std::optional<T>,Parser,std::string_view>,
                    "custom property parser must return std::optional<T>"
                );
                parsed=parser(text);
            }
            if(!parsed.has_value()) {
                if(local.empty()) local="custom property conversion failed";
                return std::nullopt;
            }
            return MakeUICustomPropertyValue<T>(std::move(*parsed));
        };
        if(formatter) {
            entry.format=[formatter=std::move(formatter)](
                const UICustomPropertyValue& value,std::string& local
            ) -> std::optional<std::string> {
                const T* typed=value.Get<T>();
                if(!typed) {
                    local="custom property formatter received wrong type";
                    return std::nullopt;
                }
                return formatter(*typed);
            };
        }
        entries_.emplace(key,std::move(entry));
        return true;
    }

    const Entry* Find(std::type_index type) const {
        auto found=entries_.find(type);
        return found==entries_.end()?nullptr:&found->second;
    }

    std::optional<UIPropertyValue> Convert(
        std::type_index type,std::string_view text,std::string* error=nullptr
    ) const {
        const Entry* entry=Find(type);
        if(!entry || !entry->parse) {
            if(error) *error="no converter registered for custom property type";
            return std::nullopt;
        }
        std::string local;
        auto parsed=entry->parse(text,local);
        if(!parsed) {
            if(error) *error=std::move(local);
            return std::nullopt;
        }
        if(error) error->clear();
        return UIPropertyValue(std::move(*parsed));
    }

    std::optional<std::string> Format(
        const UICustomPropertyValue& value,std::string* error=nullptr
    ) const {
        const Entry* entry=Find(value.type);
        if(!entry || !entry->format) {
            if(error) *error="no formatter registered for custom property type";
            return std::nullopt;
        }
        std::string local;
        auto formatted=entry->format(value,local);
        if(error) *error=std::move(local);
        return formatted;
    }

    std::vector<std::string> TypeNames() const {
        std::vector<std::string> result;
        for(const auto& pair:entries_) result.push_back(pair.second.type_name);
        std::sort(result.begin(),result.end());
        return result;
    }

    void Freeze() { frozen_=true; }
    bool Frozen() const { return frozen_; }

private:
    std::map<std::type_index,Entry> entries_;
    bool frozen_=false;
};

class UIComponentRegistry {
public:
    bool Register(
        UIComponentDescriptor descriptor,
        UIRegistrationConflictPolicy policy=UIRegistrationConflictPolicy::RejectDuplicate,
        std::string* error=nullptr
    ) {
        if(frozen_) {
            if(error) *error="component registry is frozen";
            return false;
        }
        if(descriptor.name.empty()) {
            if(error) *error="component name cannot be empty";
            return false;
        }
        const std::string key=NormalizeUIComponentName(descriptor.name);
        auto existing=components_.find(key);
        if(existing!=components_.end() && policy==UIRegistrationConflictPolicy::RejectDuplicate) {
            if(error) *error="duplicate component registration: "+descriptor.name;
            return false;
        }
        if(existing!=components_.end() && policy==UIRegistrationConflictPolicy::Alias) {
            if(error) *error="Alias policy requires RegisterAlias";
            return false;
        }
        if(descriptor.version.since.empty()) descriptor.version.since="0.9.0";
        components_[key]=std::move(descriptor);
        RebuildDescriptorAliases();
        return true;
    }

    bool RegisterAlias(std::string alias,std::string target,std::string* error=nullptr) {
        if(frozen_) {
            if(error) *error="component registry is frozen";
            return false;
        }
        const std::string alias_key=NormalizeUIComponentName(alias);
        if(alias_key.empty()) {
            if(error) *error="component alias must not be empty";
            return false;
        }
        if(components_.count(alias_key)!=0 || descriptor_aliases_.count(alias_key)!=0 ||
           explicit_aliases_.count(alias_key)!=0) {
            if(error) *error="duplicate component alias: "+alias;
            return false;
        }
        const UIComponentDescriptor* resolved=Find(target);
        if(!resolved) {
            if(error) *error="component alias target does not exist: "+target;
            return false;
        }
        explicit_aliases_[alias_key]=NormalizeUIComponentName(resolved->name);
        return true;
    }

    const UIComponentDescriptor* Find(std::string_view raw_name) const {
        std::string key=NormalizeUIComponentName(std::string(raw_name));
        if(auto alias=explicit_aliases_.find(key);alias!=explicit_aliases_.end()) key=alias->second;
        else if(auto alias=descriptor_aliases_.find(key);alias!=descriptor_aliases_.end()) key=alias->second;
        auto found=components_.find(key);
        return found==components_.end()?nullptr:&found->second;
    }

    UIComponentDescriptor* FindMutable(std::string_view raw_name) {
        if(frozen_) return nullptr;
        std::string key=NormalizeUIComponentName(std::string(raw_name));
        if(auto alias=explicit_aliases_.find(key);alias!=explicit_aliases_.end()) key=alias->second;
        else if(auto alias=descriptor_aliases_.find(key);alias!=descriptor_aliases_.end()) key=alias->second;
        auto found=components_.find(key);
        return found==components_.end()?nullptr:&found->second;
    }

    bool Contains(std::string_view name) const { return Find(name)!=nullptr; }

    UIComponentCapability GetCapabilities(const UIComponentDescriptor& descriptor) const {
        UIComponentCapability result=descriptor.capabilities;
        if(!descriptor.parent.empty()) {
            if(const UIComponentDescriptor* base=Find(descriptor.parent)) result|=GetCapabilities(*base);
        }
        return result;
    }

    std::optional<UIElement> Create(std::string_view name,std::string* error=nullptr) const {
        const UIComponentDescriptor* descriptor=Find(name);
        if(!descriptor) {
            if(error) *error="unknown component: "+std::string(name);
            return std::nullopt;
        }
        if(descriptor->kind!=UIComponentKind::Visual) {
            if(error) *error="configuration element is not a visual component: "+descriptor->name;
            return std::nullopt;
        }
        if(!descriptor->factory) {
            if(error) *error="component has no factory: "+descriptor->name;
            return std::nullopt;
        }
        UIElement element=descriptor->factory();
        if(UIComponent* component=GetUIWidget<UIComponent>(element)) {
            const UIComponentCapability capabilities=GetCapabilities(*descriptor);
            const bool visual_children=
                descriptor->children.kind==UIChildPolicyKind::Any ||
                descriptor->children.kind==UIChildPolicyKind::SpecificTypes ||
                descriptor->children.kind==UIChildPolicyKind::Custom;
            component->AssociateDescriptor(
                descriptor,this,descriptor->name,descriptor->selector,
                static_cast<std::uint32_t>(capabilities),visual_children
            );
        }
        return element;
    }

    const UIPropertyDescriptor* FindProperty(
        const UIComponentDescriptor& descriptor,std::string_view name
    ) const {
        if(const UIPropertyDescriptor* own=descriptor.FindOwnProperty(name)) return own;
        if(!descriptor.parent.empty()) {
            if(const UIComponentDescriptor* base=Find(descriptor.parent)) return FindProperty(*base,name);
        }
        return nullptr;
    }

    const UIPropertyDescriptor* FindStyleProperty(
        const UIComponentDescriptor& descriptor,std::string_view name
    ) const {
        if(const UIPropertyDescriptor* own=descriptor.FindOwnStyleProperty(name)) return own;
        if(!descriptor.parent.empty()) {
            if(const UIComponentDescriptor* base=Find(descriptor.parent)) return FindStyleProperty(*base,name);
        }
        return nullptr;
    }

    const UIEventDescriptor* FindEvent(
        const UIComponentDescriptor& descriptor,std::string_view name
    ) const {
        if(const UIEventDescriptor* own=descriptor.FindOwnEvent(name)) return own;
        if(!descriptor.parent.empty()) {
            if(const UIComponentDescriptor* base=Find(descriptor.parent)) return FindEvent(*base,name);
        }
        return nullptr;
    }

    std::vector<const UIPropertyDescriptor*> GetProperties(const UIComponentDescriptor& descriptor) const {
        std::vector<const UIPropertyDescriptor*> result;
        if(!descriptor.parent.empty()) {
            if(const UIComponentDescriptor* base=Find(descriptor.parent)) result=GetProperties(*base);
        }
        for(const UIPropertyDescriptor& property:descriptor.properties) {
            auto same=std::find_if(result.begin(),result.end(),[&](const UIPropertyDescriptor* existing) {
                return NormalizeUIComponentName(existing->name)==NormalizeUIComponentName(property.name);
            });
            if(same!=result.end()) *same=&property;
            else result.push_back(&property);
        }
        return result;
    }

    std::vector<const UIEventDescriptor*> GetEvents(const UIComponentDescriptor& descriptor) const {
        std::vector<const UIEventDescriptor*> result;
        if(!descriptor.parent.empty()) {
            if(const UIComponentDescriptor* base=Find(descriptor.parent)) result=GetEvents(*base);
        }
        for(const UIEventDescriptor& event:descriptor.events) {
            auto same=std::find_if(result.begin(),result.end(),[&](const UIEventDescriptor* existing) {
                return NormalizeUIComponentName(existing->name)==NormalizeUIComponentName(event.name);
            });
            if(same!=result.end()) *same=&event;
            else result.push_back(&event);
        }
        return result;
    }

    std::vector<const UIComponentDescriptor*> GetDescriptors() const {
        std::vector<const UIComponentDescriptor*> result;
        result.reserve(components_.size());
        for(const auto& pair:components_) result.push_back(&pair.second);
        std::sort(result.begin(),result.end(),[](const auto* a,const auto* b){return a->name<b->name;});
        return result;
    }

    const std::unordered_map<std::string,std::string>& ExplicitAliases() const { return explicit_aliases_; }
    const std::unordered_map<std::string,std::string>& DescriptorAliases() const { return descriptor_aliases_; }

    UIPropertyConverterRegistry& Converters() { return converters_; }
    const UIPropertyConverterRegistry& Converters() const { return converters_; }

    bool Validate(std::vector<std::string>* diagnostics=nullptr) const {
        std::vector<std::string> local;
        auto report=[&](std::string message){ local.push_back(std::move(message)); };

        std::unordered_map<std::string,std::string> seen_aliases;
        for(const auto& pair:components_) {
            const UIComponentDescriptor& descriptor=pair.second;
            if(!descriptor.parent.empty() && Find(descriptor.parent)==nullptr) {
                report(descriptor.name+": unknown parent component '"+descriptor.parent+"'");
            }
            if(descriptor.kind==UIComponentKind::Visual && !descriptor.factory) {
                report(descriptor.name+": visual component has no factory");
            }
            if(descriptor.kind==UIComponentKind::Configuration && !descriptor.configure_parent) {
                report(descriptor.name+": configuration element has no parent configuration handler");
            }

            std::set<std::string> property_names_seen;
            std::set<std::string> style_names_seen;
            for(const UIPropertyDescriptor& property:descriptor.properties) {
                const std::string key=NormalizeUIComponentName(property.name);
                if(key.empty()) report(descriptor.name+": property has an empty name");
                else if(!property_names_seen.insert(key).second) report(descriptor.name+": duplicate property '"+property.name+"'");
                for(const std::string& alias:property.aliases) {
                    const std::string alias_key=NormalizeUIComponentName(alias);
                    if(!property_names_seen.insert(alias_key).second) report(descriptor.name+": duplicate property alias '"+alias+"'");
                }
                if(!property.style_name.empty()) {
                    const std::string style_key=NormalizeUIComponentName(property.style_name);
                    if(style_key.empty()) report(descriptor.name+"."+property.name+": RCSS style property has an empty name");
                    else if(!style_names_seen.insert(style_key).second) report(descriptor.name+": duplicate RCSS style property '"+property.style_name+"'");
                }
                if(descriptor.kind==UIComponentKind::Visual && !property.setter && !HasUIPropertyFlag(property.flags,UIPropertyFlag::ReadOnly)) {
                    report(descriptor.name+"."+property.name+": writable property has no setter");
                }
                if(property.constraints.minimum && property.constraints.maximum && *property.constraints.minimum>*property.constraints.maximum) {
                    report(descriptor.name+"."+property.name+": minimum is greater than maximum");
                }
                if(!std::holds_alternative<std::monostate>(property.default_value)) {
                    std::string default_error;
                    if(!property.ValidateValue(property.default_value,nullptr,&default_error)) {
                        report(descriptor.name+"."+property.name+": invalid default value: "+default_error);
                    }
                }
            }

            std::set<std::string> event_names_seen;
            for(const UIEventDescriptor& event:descriptor.events) {
                const std::string key=NormalizeUIComponentName(event.name);
                if(key.empty()) report(descriptor.name+": event has an empty name");
                else if(!event_names_seen.insert(key).second) report(descriptor.name+": duplicate event '"+event.name+"'");
                const std::string markup=NormalizeUIComponentName(event.markup_name);
                if(!markup.empty() && !event_names_seen.insert(markup).second && markup!=key) {
                    report(descriptor.name+": duplicate event markup name '"+event.markup_name+"'");
                }
            }

            for(const std::string& alias:descriptor.aliases) {
                const std::string key=NormalizeUIComponentName(alias);
                if(key.empty()) report(descriptor.name+": empty component alias");
                else if(components_.count(key)!=0 && key!=pair.first) report(descriptor.name+": alias conflicts with component name '"+alias+"'");
                else if(auto found=seen_aliases.find(key);found!=seen_aliases.end() && found->second!=pair.first) {
                    report(descriptor.name+": duplicate component alias '"+alias+"'");
                }
                else seen_aliases[key]=pair.first;
            }

            if(descriptor.children.kind==UIChildPolicyKind::SpecificTypes) {
                for(const std::string& type:descriptor.children.allowed_types) {
                    if(Find(type)==nullptr) report(descriptor.name+": unknown allowed child type '"+type+"'");
                }
            }
        }

        for(const auto& pair:explicit_aliases_) {
            if(components_.count(pair.first)!=0 || descriptor_aliases_.count(pair.first)!=0) {
                report("explicit alias conflicts with registered component/descriptor alias: "+pair.first);
            }
            if(components_.count(pair.second)==0) report("explicit alias has unknown target: "+pair.first);
        }

        for(const auto& pair:components_) {
            std::set<std::string> path;
            const UIComponentDescriptor* current=&pair.second;
            while(current) {
                const std::string key=NormalizeUIComponentName(current->name);
                if(!path.insert(key).second) {
                    report(pair.second.name+": component inheritance cycle detected");
                    break;
                }
                if(current->parent.empty()) break;
                current=Find(current->parent);
            }
        }

        if(diagnostics) *diagnostics=local;
        return local.empty();
    }

    bool Freeze(std::vector<std::string>* diagnostics=nullptr) {
        if(frozen_) return true;
        FinalizeCapabilityEvents();
        RebuildDescriptorAliases();
        if(!Validate(diagnostics)) return false;
        converters_.Freeze();
        frozen_=true;
        return true;
    }

    bool Frozen() const { return frozen_; }

private:
    void RebuildDescriptorAliases() {
        descriptor_aliases_.clear();
        for(const auto& pair:components_) {
            for(const std::string& alias:pair.second.aliases) {
                const std::string key=NormalizeUIComponentName(alias);
                if(!key.empty() && descriptor_aliases_.count(key)==0) descriptor_aliases_[key]=pair.first;
            }
        }
        // Explicit aliases intentionally remain untouched. This is the 0.9
        // fix for aliases disappearing when later components are registered.
    }

    void EnsureEvent(UIComponentDescriptor& descriptor,std::string name,std::string markup,std::string docs) {
        if(descriptor.FindOwnEvent(name) || descriptor.FindOwnEvent(markup)) return;
        UIEventDescriptor event;
        event.name=std::move(name);
        event.markup_name=std::move(markup);
        event.payload_type="UIXEvent";
        event.documentation=std::move(docs);
        descriptor.events.push_back(std::move(event));
    }

    void FinalizeCapabilityEvents() {
        for(auto& pair:components_) {
            UIComponentDescriptor& descriptor=pair.second;
            const UIComponentCapability capabilities=GetCapabilities(descriptor);
            if(HasUIComponentCapability(capabilities,UIComponentCapability::Clickable)) {
                EnsureEvent(descriptor,"click","onClick","Emitted after a standard clickable activation.");
            }
            if(HasUIComponentCapability(capabilities,UIComponentCapability::Draggable)) {
                EnsureEvent(descriptor,"dragstart","onDragStart","Emitted when generic dragging begins.");
                EnsureEvent(descriptor,"drag","onDrag","Emitted while generic dragging continues.");
                EnsureEvent(descriptor,"dragend","onDragEnd","Emitted when generic dragging ends.");
            }
        }
    }

    std::map<std::string,UIComponentDescriptor> components_;
    std::unordered_map<std::string,std::string> descriptor_aliases_;
    std::unordered_map<std::string,std::string> explicit_aliases_;
    UIPropertyConverterRegistry converters_;
    bool frozen_=false;
};

namespace rui_component_detail {

template <typename T> struct PropertyTypeTraits { static constexpr UIPropertyType type=UIPropertyType::Custom; };
template <> struct PropertyTypeTraits<std::string> { static constexpr UIPropertyType type=UIPropertyType::String; };
template <> struct PropertyTypeTraits<bool>        { static constexpr UIPropertyType type=UIPropertyType::Bool; };
template <> struct PropertyTypeTraits<int>         { static constexpr UIPropertyType type=UIPropertyType::Int; };
template <> struct PropertyTypeTraits<float>       { static constexpr UIPropertyType type=UIPropertyType::Float; };
template <> struct PropertyTypeTraits<double>      { static constexpr UIPropertyType type=UIPropertyType::Double; };
template <> struct PropertyTypeTraits<Color>       { static constexpr UIPropertyType type=UIPropertyType::Color; };
template <> struct PropertyTypeTraits<Vector2>     { static constexpr UIPropertyType type=UIPropertyType::Vector2; };
template <> struct PropertyTypeTraits<Vector3>     { static constexpr UIPropertyType type=UIPropertyType::Vector3; };
template <> struct PropertyTypeTraits<Vector4>     { static constexpr UIPropertyType type=UIPropertyType::Vector4; };
template <> struct PropertyTypeTraits<Rectangle>   { static constexpr UIPropertyType type=UIPropertyType::Rectangle; };

template <typename T>
UIPropertyValue ToValue(const T& value) {
    if constexpr(PropertyTypeTraits<T>::type==UIPropertyType::Custom) {
        return MakeUICustomPropertyValue<T>(value);
    }
    else return UIPropertyValue(value);
}

inline std::string JsonEscape(std::string_view value) {
    std::string output;
    for(char c:value) {
        switch(c) {
            case '\\': output+="\\\\"; break;
            case '"': output+="\\\""; break;
            case '\n': output+="\\n"; break;
            case '\r': output+="\\r"; break;
            case '\t': output+="\\t"; break;
            default: output.push_back(c); break;
        }
    }
    return output;
}

inline const char* PropertyTypeName(UIPropertyType type) {
    switch(type) {
        case UIPropertyType::String:return "string";
        case UIPropertyType::Bool:return "bool";
        case UIPropertyType::Int:return "int";
        case UIPropertyType::Float:return "float";
        case UIPropertyType::Double:return "double";
        case UIPropertyType::Color:return "color";
        case UIPropertyType::Vector2:return "vector2";
        case UIPropertyType::Vector3:return "vector3";
        case UIPropertyType::Vector4:return "vector4";
        case UIPropertyType::Rectangle:return "rectangle";
        case UIPropertyType::Enum:return "enum";
        default:return "custom";
    }
}

inline const char* ChildPolicyName(UIChildPolicyKind kind) {
    switch(kind) {
        case UIChildPolicyKind::Any:return "any";
        case UIChildPolicyKind::SpecificTypes:return "specific-types";
        case UIChildPolicyKind::ConfigurationOnly:return "configuration-only";
        case UIChildPolicyKind::Custom:return "custom";
        default:return "none";
    }
}

inline const char* ComponentKindName(UIComponentKind kind) {
    return kind==UIComponentKind::Configuration?"configuration":"visual";
}

inline const char* ValidationPolicyName(UIPropertyValidationPolicy policy) {
    return policy==UIPropertyValidationPolicy::Clamp?"clamp":"reject";
}

inline std::vector<std::string> CapabilityNames(UIComponentCapability capabilities) {
    std::vector<std::string> result;
    auto add=[&](UIComponentCapability flag,const char* name){if(HasUIComponentCapability(capabilities,flag)) result.push_back(name);};
    add(UIComponentCapability::Focusable,"Focusable");
    add(UIComponentCapability::Clickable,"Clickable");
    add(UIComponentCapability::Draggable,"Draggable");
    add(UIComponentCapability::Scrollable,"Scrollable");
    add(UIComponentCapability::Selectable,"Selectable");
    add(UIComponentCapability::KeyboardInput,"KeyboardInput");
    add(UIComponentCapability::TextEditable,"TextEditable");
    add(UIComponentCapability::PopupOwner,"PopupOwner");
    return result;
}

inline std::string CamelToKebab(std::string_view value) {
    std::string out;
    out.reserve(value.size()+4);
    for(char c:value) {
        if(std::isupper(static_cast<unsigned char>(c))) {
            if(!out.empty() && out.back()!='-') out.push_back('-');
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        else if(c=='_') out.push_back('-');
        else out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

inline std::string NumericList(std::initializer_list<double> values) {
    std::ostringstream out;
    bool first=true;
    for(double value:values){if(!first)out<<',';first=false;out<<value;}
    return out.str();
}

} // namespace rui_component_detail

template <typename Widget,typename T>
UIPropertyDescriptor MakeUIProperty(
    std::string name,
    T Widget::* member,
    T default_value=T{},
    UIPropertyFlag flags=UIPropertyFlag::Bindable|UIPropertyFlag::Serializable|UIPropertyFlag::Inspectable,
    std::string documentation={}
) {
    UIPropertyDescriptor descriptor;
    descriptor.name=name;
    descriptor.markup_name=name;
    descriptor.type=rui_component_detail::PropertyTypeTraits<T>::type;
    descriptor.default_value=rui_component_detail::ToValue(default_value);
    descriptor.flags=flags;
    descriptor.documentation=std::move(documentation);
    if constexpr(rui_component_detail::PropertyTypeTraits<T>::type==UIPropertyType::Custom) {
        descriptor.custom_cpp_type=typeid(T);
        descriptor.custom_type_name=typeid(T).name();
    }
    descriptor.setter=[member,name](UIElement& element,const UIPropertyValue& raw,std::string& error) {
        Widget* widget=GetUIWidget<Widget>(element);
        if(!widget) { error="property '"+name+"' applied to the wrong component type"; return false; }
        if constexpr(rui_component_detail::PropertyTypeTraits<T>::type==UIPropertyType::Custom) {
            const auto* custom=std::get_if<UICustomPropertyValue>(&raw);
            const T* value=custom?custom->Get<T>():nullptr;
            if(!value) { error="property '"+name+"' received the wrong custom value type"; return false; }
            widget->*member=*value;
        }
        else {
            const T* value=std::get_if<T>(&raw);
            if(!value) { error="property '"+name+"' received the wrong value type"; return false; }
            widget->*member=*value;
        }
        return true;
    };
    descriptor.getter=[member](const UIElement& const_element)->UIPropertyValue {
        UIElement& element=const_cast<UIElement&>(const_element);
        Widget* widget=GetUIWidget<Widget>(element);
        if(!widget) return {};
        return rui_component_detail::ToValue(widget->*member);
    };
    if constexpr(std::is_base_of_v<IUICustomElement,Widget>) {
        descriptor.component_setter=[member,name](IUICustomElement& base,const UIPropertyValue& raw,std::string& error) {
            Widget* widget=dynamic_cast<Widget*>(&base);
            if(!widget) { error="property '"+name+"' applied to the wrong component type"; return false; }
            if constexpr(rui_component_detail::PropertyTypeTraits<T>::type==UIPropertyType::Custom) {
                const auto* custom=std::get_if<UICustomPropertyValue>(&raw);
                const T* value=custom?custom->Get<T>():nullptr;
                if(!value) { error="property '"+name+"' received the wrong custom value type"; return false; }
                widget->*member=*value;
            }
            else {
                const T* value=std::get_if<T>(&raw);
                if(!value) { error="property '"+name+"' received the wrong value type"; return false; }
                widget->*member=*value;
            }
            return true;
        };
        descriptor.component_getter=[member](const IUICustomElement& base)->UIPropertyValue {
            const Widget* widget=dynamic_cast<const Widget*>(&base);
            if(!widget) return {};
            return rui_component_detail::ToValue(widget->*member);
        };
    }
    return descriptor;
}

template <typename Widget,typename Enum>
UIPropertyDescriptor MakeUIEnumProperty(
    std::string name,
    Enum Widget::* member,
    std::vector<std::pair<std::string,Enum>> values,
    Enum default_value,
    UIPropertyFlag flags=UIPropertyFlag::Bindable|UIPropertyFlag::Serializable|UIPropertyFlag::Inspectable,
    std::string documentation={}
) {
    static_assert(std::is_enum_v<Enum>,"MakeUIEnumProperty requires an enum type");
    UIPropertyDescriptor descriptor;
    descriptor.name=name; descriptor.markup_name=name; descriptor.type=UIPropertyType::Enum;
    descriptor.flags=flags; descriptor.documentation=std::move(documentation);
    std::string default_name;
    for(const auto& pair:values){descriptor.constraints.enum_values.push_back(pair.first);if(pair.second==default_value)default_name=pair.first;}
    descriptor.default_value=default_name;
    descriptor.setter=[member,name,values](UIElement& element,const UIPropertyValue& raw,std::string& error) {
        Widget* widget=GetUIWidget<Widget>(element);
        if(!widget){error="property '"+name+"' applied to the wrong component type";return false;}
        const std::string* text=std::get_if<std::string>(&raw);
        if(!text){error="enum property '"+name+"' expects a symbolic string";return false;}
        const std::string normalized=NormalizeUIComponentName(*text);
        for(const auto& pair:values){if(NormalizeUIComponentName(pair.first)==normalized){widget->*member=pair.second;return true;}}
        error="invalid enum value '"+*text+"' for property '"+name+"'";return false;
    };
    descriptor.getter=[member,values](const UIElement& const_element)->UIPropertyValue {
        UIElement& element=const_cast<UIElement&>(const_element);
        Widget* widget=GetUIWidget<Widget>(element);
        if(!widget)return {};
        for(const auto& pair:values)if(pair.second==widget->*member)return pair.first;
        return {};
    };
    if constexpr(std::is_base_of_v<IUICustomElement,Widget>) {
        descriptor.component_setter=[member,name,values](IUICustomElement& base,const UIPropertyValue& raw,std::string& error) {
            Widget* widget=dynamic_cast<Widget*>(&base);
            if(!widget){error="property '"+name+"' applied to the wrong component type";return false;}
            const std::string* text=std::get_if<std::string>(&raw);
            if(!text){error="enum property '"+name+"' expects a symbolic string";return false;}
            const std::string normalized=NormalizeUIComponentName(*text);
            for(const auto& pair:values){if(NormalizeUIComponentName(pair.first)==normalized){widget->*member=pair.second;return true;}}
            error="invalid enum value '"+*text+"' for property '"+name+"'";return false;
        };
        descriptor.component_getter=[member,values](const IUICustomElement& base)->UIPropertyValue {
            const Widget* widget=dynamic_cast<const Widget*>(&base);
            if(!widget)return {};
            for(const auto& pair:values)if(pair.second==widget->*member)return pair.first;
            return {};
        };
    }
    return descriptor;
}


template <typename T>
UIPropertyDescriptor MakeUIConfigurationProperty(
    std::string name,
    T default_value=T{},
    UIPropertyFlag flags=UIPropertyFlag::Inspectable,
    std::string documentation={}
) {
    UIPropertyDescriptor descriptor;
    descriptor.name=name;
    descriptor.markup_name=name;
    descriptor.type=rui_component_detail::PropertyTypeTraits<T>::type;
    descriptor.default_value=rui_component_detail::ToValue(default_value);
    descriptor.flags=flags;
    descriptor.documentation=std::move(documentation);
    if constexpr(rui_component_detail::PropertyTypeTraits<T>::type==UIPropertyType::Custom) {
        descriptor.custom_cpp_type=typeid(T);
        descriptor.custom_type_name=typeid(T).name();
    }
    return descriptor;
}

template <typename T,typename Validator>
void SetUIPropertyValidator(UIPropertyDescriptor& descriptor,Validator validator) {
    descriptor.validator=[validator=std::move(validator)](
        const UIPropertyValue& raw,std::string& error
    ) mutable -> bool {
        const T* value=nullptr;
        if constexpr(rui_component_detail::PropertyTypeTraits<T>::type==UIPropertyType::Custom) {
            if(const auto* custom=std::get_if<UICustomPropertyValue>(&raw)) value=custom->Get<T>();
        }
        else value=std::get_if<T>(&raw);
        if(!value) {
            error="property validator received the wrong value type";
            return false;
        }
        if constexpr(std::is_invocable_r_v<bool,Validator,const T&,std::string&>) {
            return validator(*value,error);
        }
        else {
            static_assert(
                std::is_invocable_r_v<bool,Validator,const T&>,
                "validator must return bool and accept const T& or const T&, std::string&"
            );
            const bool valid=validator(*value);
            if(!valid && error.empty()) error="custom property validation failed";
            return valid;
        }
    };
}

inline UIPropertyDescriptor& SetUIPropertyRange(
    UIPropertyDescriptor& descriptor,
    double minimum,
    double maximum,
    UIPropertyValidationPolicy policy=UIPropertyValidationPolicy::Reject
) {
    descriptor.constraints.minimum=minimum;
    descriptor.constraints.maximum=maximum;
    descriptor.constraints.numeric_policy=policy;
    return descriptor;
}

template <typename Custom>
UIComponentDescriptor MakeUICustomComponentDescriptor(std::string name) {
    static_assert(std::is_base_of_v<IUICustomElement,Custom>,"Custom RUI components derive from UIComponent or IUICustomElement");
    UIComponentDescriptor descriptor;
    descriptor.name=std::move(name);
    descriptor.selector=SelectorType::Any;
    descriptor.generic_markup=true;
    descriptor.factory=[]()->UIElement {
        return std::static_pointer_cast<IUICustomElement>(std::make_shared<Custom>());
    };
    return descriptor;
}

inline UIComponentDescriptor MakeUIConfigurationDescriptor(std::string name) {
    UIComponentDescriptor descriptor;
    descriptor.name=std::move(name);
    descriptor.kind=UIComponentKind::Configuration;
    descriptor.generic_markup=true;
    return descriptor;
}

template <typename Custom>
class UIComponentBuilder {
public:
    explicit UIComponentBuilder(std::string name)
        : descriptor_(MakeUICustomComponentDescriptor<Custom>(std::move(name))) {}

    UIComponentBuilder& Documentation(std::string text) {
        descriptor_.documentation=std::move(text); return *this;
    }
    UIComponentBuilder& Parent(std::string name) {
        descriptor_.parent=std::move(name); return *this;
    }
    UIComponentBuilder& Alias(std::string alias) {
        descriptor_.aliases.push_back(std::move(alias)); return *this;
    }
    UIComponentBuilder& Capabilities(UIComponentCapability value) {
        descriptor_.capabilities=value; return *this;
    }
    UIComponentBuilder& Children(
        UIChildPolicyKind kind,std::vector<std::string> allowed={}
    ) {
        descriptor_.children.kind=kind;
        descriptor_.children.allowed_types=std::move(allowed);
        return *this;
    }
    template <typename T>
    UIComponentBuilder& Property(
        std::string name,
        T Custom::* member,
        T default_value=T{},
        UIPropertyFlag flags=UIPropertyFlag::Bindable|UIPropertyFlag::Serializable|UIPropertyFlag::Inspectable,
        std::string documentation={}
    ) {
        descriptor_.properties.push_back(
            MakeUIProperty<Custom,T>(
                std::move(name),member,std::move(default_value),flags,std::move(documentation)
            )
        );
        return *this;
    }
    template <typename T>
    UIComponentBuilder& RangeProperty(
        std::string name,
        T Custom::* member,
        T default_value,
        double minimum,
        double maximum,
        UIPropertyValidationPolicy policy=UIPropertyValidationPolicy::Reject,
        UIPropertyFlag flags=UIPropertyFlag::Bindable|UIPropertyFlag::Serializable|UIPropertyFlag::Inspectable,
        std::string documentation={}
    ) {
        UIPropertyDescriptor property=MakeUIProperty<Custom,T>(
            std::move(name),member,std::move(default_value),flags,std::move(documentation)
        );
        SetUIPropertyRange(property,minimum,maximum,policy);
        descriptor_.properties.push_back(std::move(property));
        return *this;
    }
    template <typename Enum>
    UIComponentBuilder& EnumProperty(
        std::string name,
        Enum Custom::* member,
        std::vector<std::pair<std::string,Enum>> values,
        Enum default_value,
        UIPropertyFlag flags=UIPropertyFlag::Bindable|UIPropertyFlag::Serializable|UIPropertyFlag::Inspectable,
        std::string documentation={}
    ) {
        descriptor_.properties.push_back(MakeUIEnumProperty<Custom,Enum>(
            std::move(name),member,std::move(values),default_value,flags,std::move(documentation)
        ));
        return *this;
    }
    template <typename T,typename Validator>
    UIComponentBuilder& ValidateProperty(
        std::string_view name,
        Validator validator
    ) {
        const std::string normalized=NormalizeUIComponentName(std::string(name));
        for(UIPropertyDescriptor& property:descriptor_.properties) {
            if(NormalizeUIComponentName(property.name)==normalized ||
               NormalizeUIComponentName(property.markup_name)==normalized) {
                SetUIPropertyValidator<T>(property,std::move(validator));
                break;
            }
        }
        return *this;
    }

    UIComponentBuilder& StyleProperty(
        std::string_view property_name,
        std::string rcss_name = {}
    ) {
        const std::string normalized=NormalizeUIComponentName(std::string(property_name));
        for(UIPropertyDescriptor& property:descriptor_.properties) {
            if(NormalizeUIComponentName(property.name)==normalized ||
               NormalizeUIComponentName(property.markup_name)==normalized) {
                property.style_name = rcss_name.empty()
                    ? rui_component_detail::CamelToKebab(property.name)
                    : std::move(rcss_name);
                break;
            }
        }
        return *this;
    }

    UIComponentBuilder& Event(
        std::string name,std::string markup_name,
        std::string payload_type="UIXEvent",std::string documentation={}
    ) {
        UIEventDescriptor event;
        event.name=std::move(name);
        event.markup_name=std::move(markup_name);
        event.payload_type=std::move(payload_type);
        event.documentation=std::move(documentation);
        descriptor_.events.push_back(std::move(event));
        return *this;
    }
    UIComponentBuilder& CustomChildren(
        std::function<bool(UIElement&,std::string_view,const UIConfigurationValues&,std::string&)> processor
    ) {
        descriptor_.children.kind=UIChildPolicyKind::Custom;
        descriptor_.child_processor=std::move(processor);
        return *this;
    }

    UIComponentDescriptor Build() { return std::move(descriptor_); }
    UIComponentDescriptor& Descriptor() { return descriptor_; }

private:
    UIComponentDescriptor descriptor_;
};

template <typename Custom>
UIComponentBuilder<Custom> DescribeUIComponent(std::string name) {
    return UIComponentBuilder<Custom>(std::move(name));
}

inline void RegisterBuiltinComponents(UIComponentRegistry& registry) {
    auto legacy = [&](UIComponentDescriptor descriptor) {
        descriptor.generic_markup = false;
        descriptor.builtin = true;
        descriptor.version.since = "0.8.0";
        std::string ignored;
        registry.Register(std::move(descriptor),UIRegistrationConflictPolicy::RejectDuplicate,&ignored);
    };

    {
        UIComponentDescriptor d;
        d.name = "Button"; d.aliases = {"button"}; d.selector = SelectorType::Button;
        d.factory = [](){ return UIElement(UIButton{}); };
        d.capabilities = UIComponentCapability::Focusable|UIComponentCapability::Clickable;
        d.documentation = "Clickable retained-mode button.";
        d.properties.push_back(MakeUIProperty<UIButton,std::string>("text",&UIButton::text));
        d.properties.push_back(MakeUIProperty<UIButton,bool>("disabled",&UIButton::disabled,false));
        d.events.push_back({"click","onClick","UIXEvent","Emitted when the button is activated."});
        legacy(std::move(d));
    }
    {
        UIComponentDescriptor d;
        d.name = "Label"; d.aliases = {"label","text"}; d.selector = SelectorType::Label;
        d.factory = [](){ return UIElement(UILabel{}); };
        d.documentation = "Text label.";
        d.properties.push_back(MakeUIProperty<UILabel,std::string>("text",&UILabel::text));
        legacy(std::move(d));
    }
    {
        UIComponentDescriptor d;
        d.name = "Container"; d.aliases = {"container","row","column"}; d.selector = SelectorType::Container;
        d.factory = [](){ return UIElement(std::make_shared<UIContainer>()); };
        d.children.kind = UIChildPolicyKind::Any;
        d.capabilities = UIComponentCapability::Scrollable;
        d.documentation = "Flex/grid container for child UI elements.";
        legacy(std::move(d));
    }
    {
        UIComponentDescriptor d;
        d.name = "FloatInput"; d.aliases = {"input","floatinput"}; d.selector = SelectorType::Input;
        d.factory = [](){ return UIElement(UIInput<float>{}); };
        d.capabilities = UIComponentCapability::Focusable|UIComponentCapability::KeyboardInput|UIComponentCapability::TextEditable;
        d.properties.push_back(MakeUIProperty<UIInput<float>,float>("value",&UIInput<float>::value,0.0f));
        d.properties.push_back(MakeUIProperty<UIInput<float>,float>("min",&UIInput<float>::min,0.0f));
        d.properties.push_back(MakeUIProperty<UIInput<float>,float>("max",&UIInput<float>::max,1.0f));
        d.properties.push_back(MakeUIProperty<UIInput<float>,float>("step",&UIInput<float>::step,0.1f));
        d.properties.push_back(MakeUIProperty<UIInput<float>,bool>("disabled",&UIInput<float>::disabled,false));
        d.events.push_back({"change","onChange","UIXEvent","Emitted when the value changes."});
        d.events.push_back({"submit","onSubmit","UIXEvent","Emitted when editing is committed."});
        legacy(std::move(d));
    }
    {
        UIComponentDescriptor d;
        d.name = "IntInput"; d.aliases = {"intinput"}; d.selector = SelectorType::Input;
        d.factory = [](){ return UIElement(UIInput<int>{}); };
        d.capabilities = UIComponentCapability::Focusable|UIComponentCapability::KeyboardInput|UIComponentCapability::TextEditable;
        d.properties.push_back(MakeUIProperty<UIInput<int>,int>("value",&UIInput<int>::value,0));
        d.properties.push_back(MakeUIProperty<UIInput<int>,int>("min",&UIInput<int>::min,0));
        d.properties.push_back(MakeUIProperty<UIInput<int>,int>("max",&UIInput<int>::max,100));
        d.properties.push_back(MakeUIProperty<UIInput<int>,int>("step",&UIInput<int>::step,1));
        d.properties.push_back(MakeUIProperty<UIInput<int>,bool>("disabled",&UIInput<int>::disabled,false));
        d.events.push_back({"change","onChange","UIXEvent","Emitted when the value changes."});
        d.events.push_back({"submit","onSubmit","UIXEvent","Emitted when editing is committed."});
        legacy(std::move(d));
    }
    {
        UIComponentDescriptor d;
        d.name = "BoolInput"; d.aliases = {"boolinput"}; d.selector = SelectorType::Input;
        d.factory = [](){ return UIElement(UIInput<bool>{}); };
        d.capabilities = UIComponentCapability::Focusable|UIComponentCapability::KeyboardInput;
        d.properties.push_back(MakeUIProperty<UIInput<bool>,bool>("value",&UIInput<bool>::value,false));
        d.properties.push_back(MakeUIProperty<UIInput<bool>,bool>("disabled",&UIInput<bool>::disabled,false));
        d.events.push_back({"change","onChange","UIXEvent","Emitted when the value changes."});
        legacy(std::move(d));
    }
    {
        UIComponentDescriptor d;
        d.name = "StringInput"; d.aliases = {"stringinput","textinput"}; d.selector = SelectorType::Input;
        d.factory = [](){ return UIElement(UIInput<std::string>{}); };
        d.capabilities = UIComponentCapability::Focusable|UIComponentCapability::KeyboardInput|UIComponentCapability::TextEditable;
        d.properties.push_back(MakeUIProperty<UIInput<std::string>,std::string>("value",&UIInput<std::string>::value));
        d.properties.push_back(MakeUIProperty<UIInput<std::string>,std::string>("placeholder",&UIInput<std::string>::placeholder));
        d.properties.push_back(MakeUIProperty<UIInput<std::string>,bool>("disabled",&UIInput<std::string>::disabled,false));
        d.events.push_back({"change","onChange","UIXEvent","Emitted when the value changes."});
        d.events.push_back({"submit","onSubmit","UIXEvent","Emitted when editing is committed."});
        legacy(std::move(d));
    }

    auto register_simple = [&](
        std::string name,std::vector<std::string> aliases,SelectorType selector,
        std::string docs,std::function<UIElement()> factory,
        UIChildPolicyKind children=UIChildPolicyKind::None
    ) {
        UIComponentDescriptor d;
        d.name=std::move(name); d.aliases=std::move(aliases); d.selector=selector;
        d.documentation=std::move(docs); d.factory=std::move(factory); d.children.kind=children;
        legacy(std::move(d));
    };
    register_simple("Vector2Input",{"vector2input"},SelectorType::Input,"Two-component numeric input.",[](){return UIElement(UIInput<Vector2>{});});
    register_simple("Vector3Input",{"vector3input"},SelectorType::Input,"Three-component numeric input.",[](){return UIElement(UIInput<Vector3>{});});
    register_simple("Vector4Input",{"vector4input"},SelectorType::Input,"Four-component numeric input.",[](){return UIElement(UIInput<Vector4>{});});
    register_simple("ColorInput",{"colorinput"},SelectorType::Input,"Color input.",[](){return UIElement(UIInput<Color>{});});
    register_simple("Selectable",{"selectable"},SelectorType::Selectable,"Selectable/toggle control.",[](){return UIElement(UISelectable{});});
    register_simple("ComboBox",{"combobox","combo","select"},SelectorType::ComboBox,"Drop-down selection control.",[](){return UIElement(UIComboBox{});});
    register_simple("RangeInput",{"rangeinput","floatrange","intrange"},SelectorType::RangeInput,"Two-handle range input.",[](){return UIElement(UIRangeInput<float>{});});
    register_simple("Canvas",{"canvas"},SelectorType::Canvas,"Custom immediate drawing surface.",[](){return UIElement(UICanvas{});});
    register_simple("Image",{"image","uiimage"},SelectorType::Image,"Texture/image element.",[](){return UIElement(UIImage{});});
    register_simple("Splitter",{"splitter"},SelectorType::Splitter,"Resizable pane splitter.",[](){return UIElement(UISplitter{});});
    register_simple("Collapsible",{"collapsible"},SelectorType::Collapsible,"Collapsible child container.",[](){return UIElement(std::make_shared<UICollapsible>());},UIChildPolicyKind::Any);
    register_simple("TreeNode",{"treenode","tree"},SelectorType::TreeNode,"Tree hierarchy node.",[](){return UIElement(std::make_shared<UITreeNode>());},UIChildPolicyKind::Any);
    register_simple("TabBar",{"tabs","tabbar"},SelectorType::TabBar,"Tabbed content container.",[](){return UIElement(std::make_shared<UITabBar>());},UIChildPolicyKind::ConfigurationOnly);
    register_simple("Menu",{"menu"},SelectorType::Menu,"Menu control.",[](){return UIElement(std::make_shared<UIMenu>());},UIChildPolicyKind::Any);
    register_simple("Popup",{"popup","modal"},SelectorType::Popup,"Popup/modal surface.",[](){return UIElement(std::make_shared<UIPopup>());},UIChildPolicyKind::Any);
    register_simple("Window",{"window"},SelectorType::Window,"Floating window.",[](){return UIElement(std::make_shared<UIWindow>());},UIChildPolicyKind::Any);
    register_simple("TextBox",{"textbox","richtext","wrappedtext"},SelectorType::TextBox,"Wrapped/editable text box.",[](){return UIElement(std::shared_ptr<IUICustomElement>(std::make_shared<UITextBox>()));});
    register_simple("Table",{"table","datagrid"},SelectorType::Table,"Virtualized table/data-grid.",[](){return UIElement(std::shared_ptr<IUICustomElement>(std::make_shared<UITable>()));},UIChildPolicyKind::ConfigurationOnly);
    register_simple("VirtualList",{"virtuallist"},SelectorType::VirtualList,"Virtualized repeated-list surface.",[](){return UIElement(std::shared_ptr<IUICustomElement>(std::make_shared<UIVirtualList>()));},UIChildPolicyKind::ConfigurationOnly);
    register_simple("DockSpace",{"dockspace","docking"},SelectorType::DockSpace,"Docking workspace.",[](){return UIElement(std::shared_ptr<IUICustomElement>(std::make_shared<UIDockSpace>()));},UIChildPolicyKind::ConfigurationOnly);
    register_simple("Viewport",{"viewport"},SelectorType::Viewport,"Transformed retained viewport.",[](){return UIElement(std::shared_ptr<IUICustomElement>(std::make_shared<UIViewport>()));},UIChildPolicyKind::Any);
    register_simple("ViewportItem",{"viewportitem"},SelectorType::ViewportItem,"World-space viewport child.",[](){return UIElement(std::shared_ptr<IUICustomElement>(std::make_shared<UIViewportItem>()));},UIChildPolicyKind::Any);
}


inline std::shared_ptr<UIComponentRegistry> MakeDefaultComponentRegistry() {
    auto registry=std::make_shared<UIComponentRegistry>();
    RegisterBuiltinComponents(*registry);
    return registry;
}

inline std::optional<UIPropertyValue> ConvertUIPropertyText(
    std::string_view raw,
    const UIPropertyDescriptor& property,
    const UIComponentRegistry& registry,
    std::string* error=nullptr
) {
    const std::string text(raw);
    try {
        switch(property.type) {
            case UIPropertyType::String:
            case UIPropertyType::Enum:
                return UIPropertyValue(text);
            case UIPropertyType::Bool: {
                const std::string normalized=NormalizeUIComponentName(text);
                if(normalized=="true" || normalized=="1" || normalized=="yes" || normalized=="on") return UIPropertyValue(true);
                if(normalized=="false" || normalized=="0" || normalized=="no" || normalized=="off") return UIPropertyValue(false);
                break;
            }
            case UIPropertyType::Int: {
                std::size_t used=0; int value=std::stoi(text,&used); if(used==text.size()) return UIPropertyValue(value); break;
            }
            case UIPropertyType::Float: {
                std::size_t used=0; float value=std::stof(text,&used); if(used==text.size()) return UIPropertyValue(value); break;
            }
            case UIPropertyType::Double: {
                std::size_t used=0; double value=std::stod(text,&used); if(used==text.size()) return UIPropertyValue(value); break;
            }
            case UIPropertyType::Color: {
                if(auto value=ParseCSSColor(text)) return UIPropertyValue(*value);
                break;
            }
            case UIPropertyType::Vector2:
            case UIPropertyType::Vector3:
            case UIPropertyType::Vector4:
            case UIPropertyType::Rectangle: {
                std::string normalized=text;
                std::replace(normalized.begin(),normalized.end(),',',' ');
                std::istringstream in(normalized);
                if(property.type==UIPropertyType::Vector2){Vector2 v{};if(in>>v.x>>v.y)return UIPropertyValue(v);}
                if(property.type==UIPropertyType::Vector3){Vector3 v{};if(in>>v.x>>v.y>>v.z)return UIPropertyValue(v);}
                if(property.type==UIPropertyType::Vector4){Vector4 v{};if(in>>v.x>>v.y>>v.z>>v.w)return UIPropertyValue(v);}
                if(property.type==UIPropertyType::Rectangle){Rectangle v{};if(in>>v.x>>v.y>>v.width>>v.height)return UIPropertyValue(v);}
                break;
            }
            case UIPropertyType::Custom:
                return registry.Converters().Convert(property.custom_cpp_type,raw,error);
        }
    }
    catch(...) {}
    if(error && error->empty()) *error="could not convert '"+text+"' for property '"+property.name+"'";
    return std::nullopt;
}

inline std::optional<std::string> FormatUIPropertyValue(
    const UIPropertyValue& value,
    const UIComponentRegistry& registry,
    std::string* error=nullptr
) {
    return std::visit([&](const auto& stored)->std::optional<std::string> {
        using T=std::decay_t<decltype(stored)>;
        std::ostringstream out;
        out<<std::setprecision(std::numeric_limits<double>::max_digits10);
        if constexpr(std::is_same_v<T,std::monostate>) return std::string{};
        else if constexpr(std::is_same_v<T,std::string>) return stored;
        else if constexpr(std::is_same_v<T,bool>) return stored?std::string("true"):std::string("false");
        else if constexpr(std::is_same_v<T,int> || std::is_same_v<T,float> || std::is_same_v<T,double>) {out<<stored;return out.str();}
        else if constexpr(std::is_same_v<T,Color>) {
            std::ostringstream color;
            color << '#' << std::hex << std::setfill('0') << std::setw(2) << int(stored.r)
                  << std::setw(2) << int(stored.g) << std::setw(2) << int(stored.b)
                  << std::setw(2) << int(stored.a);
            return color.str();
        }
        else if constexpr(std::is_same_v<T,Vector2>) {out<<stored.x<<','<<stored.y;return out.str();}
        else if constexpr(std::is_same_v<T,Vector3>) {out<<stored.x<<','<<stored.y<<','<<stored.z;return out.str();}
        else if constexpr(std::is_same_v<T,Vector4>) {out<<stored.x<<','<<stored.y<<','<<stored.z<<','<<stored.w;return out.str();}
        else if constexpr(std::is_same_v<T,Rectangle>) {out<<stored.x<<','<<stored.y<<','<<stored.width<<','<<stored.height;return out.str();}
        else if constexpr(std::is_same_v<T,UICustomPropertyValue>) return registry.Converters().Format(stored,error);
        else return std::nullopt;
    },value);
}

struct UISerializedComponent {
    std::string component;
    std::map<std::string,std::string> properties;
};

inline std::optional<UISerializedComponent> SerializeRegisteredComponent(
    const UIComponentRegistry& registry,
    const UIComponentDescriptor& descriptor,
    const UIElement& element,
    std::vector<std::string>* diagnostics=nullptr
) {
    UISerializedComponent output;
    output.component=descriptor.name;
    std::vector<std::string> local;
    for(const UIPropertyDescriptor* property:registry.GetProperties(descriptor)) {
        if(!HasUIPropertyFlag(property->flags,UIPropertyFlag::Serializable) ||
           HasUIPropertyFlag(property->flags,UIPropertyFlag::RuntimeOnly)) continue;
        if(!property->getter) continue;
        std::string error;
        auto text=FormatUIPropertyValue(property->Get(element),registry,&error);
        if(!text) local.push_back(descriptor.name+"."+property->name+": "+error);
        else output.properties[property->name]=std::move(*text);
    }
    if(diagnostics) *diagnostics=local;
    if(!local.empty()) return std::nullopt;
    return output;
}

inline bool DeserializeRegisteredComponent(
    const UIComponentRegistry& registry,
    const UIComponentDescriptor& descriptor,
    UIElement& element,
    const UISerializedComponent& input,
    std::vector<std::string>* diagnostics=nullptr
) {
    std::vector<std::string> local;
    for(const auto& pair:input.properties) {
        const UIPropertyDescriptor* property=registry.FindProperty(descriptor,pair.first);
        if(!property) {local.push_back(descriptor.name+": unknown serialized property '"+pair.first+"'");continue;}
        if(!HasUIPropertyFlag(property->flags,UIPropertyFlag::Serializable) || HasUIPropertyFlag(property->flags,UIPropertyFlag::RuntimeOnly)) continue;
        std::string error;
        auto value=ConvertUIPropertyText(pair.second,*property,registry,&error);
        if(!value || !property->Set(element,*value,&error)) local.push_back(descriptor.name+"."+property->name+": "+error);
    }
    if(diagnostics) *diagnostics=local;
    return local.empty();
}

namespace rui_component_detail {

inline std::vector<std::string> DiscoverRcssAllowedValues(PropertyType property) {
    // Candidate vocabulary is generic. Validity is still decided by the real
    // RCSS parser through ParsePropertyValue(), so the schema does not maintain
    // a second property-specific grammar.
    static const char* candidates[]={
        "none","auto","block","inline","flex","grid","row","column","row-reverse","column-reverse",
        "nowrap","wrap","wrap-reverse","flex-start","flex-end","center","space-between","space-around","space-evenly",
        "stretch","baseline","start","end","visible","hidden","scroll","absolute","relative","static","fixed",
        "left","right","top","bottom","both","horizontal","vertical","solid","dashed","dotted","true","false",
        "cover","contain","fill","repeat","no-repeat","normal","multiply","screen","overlay","darken","lighten",
        "ease","linear","ease-in","ease-out","ease-in-out","forwards","backwards","alternate","reverse","dense",
        "pointer","text","move","default","nearest","bilinear","additive","alpha","open","closed"
    };
    std::vector<std::string> accepted;
    for(const char* token:candidates) if(ParsePropertyValue(property,token).has_value()) accepted.emplace_back(token);
    // String-like properties accept nearly everything. In that case the probe
    // vocabulary is not a real enum, so omit it rather than misleading tooling.
    if(accepted.size()>24) accepted.clear();
    return accepted;
}

inline std::string DefaultValueString(
    const UIPropertyDescriptor& property,const UIComponentRegistry& registry
) {
    std::string error;
    auto value=FormatUIPropertyValue(property.default_value,registry,&error);
    return value.value_or(std::string{});
}

} // namespace rui_component_detail

inline std::string GenerateRuiSchemaJson(const UIComponentRegistry& registry) {
    using namespace rui_component_detail;
    std::ostringstream out;
    out << "{\n  \"schemaVersion\": \"0.9.1\",\n  \"components\": [\n";
    const auto descriptors=registry.GetDescriptors();
    for(std::size_t i=0;i<descriptors.size();++i) {
        const UIComponentDescriptor& d=*descriptors[i];
        out << "    {\n      \"name\": \""<<JsonEscape(d.name)<<"\",\n";
        out << "      \"kind\": \""<<ComponentKindName(d.kind)<<"\",\n";
        out << "      \"parent\": \""<<JsonEscape(d.parent)<<"\",\n";
        out << "      \"since\": \""<<JsonEscape(d.version.since)<<"\",\n";
        out << "      \"deprecatedSince\": \""<<JsonEscape(d.version.deprecated_since)<<"\",\n";
        out << "      \"replacement\": \""<<JsonEscape(d.version.replacement)<<"\",\n";
        out << "      \"documentation\": \""<<JsonEscape(d.documentation)<<"\",\n";
        out << "      \"childPolicy\": \""<<ChildPolicyName(d.children.kind)<<"\",\n";
        out << "      \"allowedChildTypes\": [";
        for(std::size_t a=0;a<d.children.allowed_types.size();++a){if(a)out<<", ";out<<"\""<<JsonEscape(d.children.allowed_types[a])<<"\"";}
        out << "],\n      \"aliases\": [";
        for(std::size_t a=0;a<d.aliases.size();++a){if(a)out<<", ";out<<"\""<<JsonEscape(d.aliases[a])<<"\"";}
        out << "],\n      \"capabilities\": [";
        const auto capabilities=CapabilityNames(registry.GetCapabilities(d));
        for(std::size_t c=0;c<capabilities.size();++c){if(c)out<<", ";out<<"\""<<capabilities[c]<<"\"";}
        out << "],\n      \"properties\": [";
        const auto properties=registry.GetProperties(d);
        for(std::size_t p=0;p<properties.size();++p) {
            const UIPropertyDescriptor& property=*properties[p];
            if(p)out<<',';
            out << "\n        {\"name\": \""<<JsonEscape(property.name)
                <<"\", \"markupName\": \""<<JsonEscape(property.markup_name)
                <<"\", \"type\": \""<<(property.type==UIPropertyType::Custom?JsonEscape(property.custom_type_name):PropertyTypeName(property.type))
                <<"\", \"default\": \""<<JsonEscape(DefaultValueString(property,registry))
                <<"\", \"bindable\": "<<(HasUIPropertyFlag(property.flags,UIPropertyFlag::Bindable)?"true":"false")
                <<", \"serializable\": "<<(HasUIPropertyFlag(property.flags,UIPropertyFlag::Serializable)?"true":"false")
                <<", \"inspectable\": "<<(HasUIPropertyFlag(property.flags,UIPropertyFlag::Inspectable)?"true":"false")
                <<", \"rcssName\": \""<<JsonEscape(property.style_name)<<"\""
                <<", \"animatable\": "<<(!property.style_name.empty()?"true":"false")
                <<", \"validationPolicy\": \""<<ValidationPolicyName(property.constraints.numeric_policy)<<"\""
                <<", \"documentation\": \""<<JsonEscape(property.documentation)<<"\"";
            if(property.constraints.minimum)out<<", \"minimum\": "<<*property.constraints.minimum;
            if(property.constraints.maximum)out<<", \"maximum\": "<<*property.constraints.maximum;
            if(!property.constraints.enum_values.empty()) {
                out<<", \"enum\": [";
                for(std::size_t e=0;e<property.constraints.enum_values.size();++e){if(e)out<<", ";out<<"\""<<JsonEscape(property.constraints.enum_values[e])<<"\"";}
                out<<']';
            }
            out<<'}';
        }
        if(!properties.empty())out<<"\n      ";
        out << "],\n      \"events\": [";
        const auto events=registry.GetEvents(d);
        for(std::size_t e=0;e<events.size();++e){if(e)out<<", ";out<<"{\"name\": \""<<JsonEscape(events[e]->name)<<"\", \"markupName\": \""<<JsonEscape(events[e]->markup_name)<<"\", \"payloadType\": \""<<JsonEscape(events[e]->payload_type)<<"\"}";}
        out << "]\n    }"<<(i+1<descriptors.size()?",":"")<<"\n";
    }
    out << "  ],\n  \"rcss\": {\n"
        << "    \"pseudoStates\": [\"hover\", \"active\", \"focus\", \"disabled\", \"open\", \"selected\", \"dragging\"],\n"
        << "    \"typeSelectorsAreRegistryDriven\": true,\n"
        << "    \"properties\": [\n";
    for(std::size_t i=0;i<property_names.size();++i) {
        const PropertyType property=static_cast<PropertyType>(i);
        const std::string name=property_names[i];
        const bool length=IsLengthProperty(property);
        const auto allowed=DiscoverRcssAllowedValues(property);
        out << "      {\"name\": \""<<JsonEscape(name)<<"\", \"type\": \""<<(length?"length":"value")<<"\"";
        if(length)out<<", \"units\": [\"px\", \"%\", \"vw\", \"vh\", \"em\", \"rem\"]";
        if(!allowed.empty()) {
            out<<", \"allowedValues\": [";
            for(std::size_t a=0;a<allowed.size();++a){if(a)out<<", ";out<<"\""<<JsonEscape(allowed[a])<<"\"";}
            out<<']';
        }
        out<<'}'<<(i+1<property_names.size()?",":"")<<"\n";
    }
    out << "    ]\n  }\n}\n";
    return out.str();
}

inline std::string GenerateRuiProjectSchemaJson(const UIComponentRegistry& registry) {
    using namespace rui_component_detail;
    std::ostringstream out;
    out<<"{\n  \"schemaVersion\": \"0.9.1\",\n  \"customComponents\": [";
    bool first=true;
    for(const UIComponentDescriptor* d:registry.GetDescriptors()) {
        if(d->builtin)continue;
        if(!first)out<<',';
        first=false;
        out<<"\n    {\"name\": \""<<JsonEscape(d->name)
           <<"\", \"kind\": \""<<ComponentKindName(d->kind)
           <<"\", \"styleProperties\": [";
        bool first_style=true;
        for(const UIPropertyDescriptor* property:registry.GetProperties(*d)) {
            if(property->style_name.empty()) continue;
            if(!first_style) out<<", ";
            first_style=false;
            out<<"{\"name\": \""<<JsonEscape(property->style_name)
               <<"\", \"property\": \""<<JsonEscape(property->name)
               <<"\", \"type\": \""
               <<(property->type==UIPropertyType::Custom?JsonEscape(property->custom_type_name):PropertyTypeName(property->type))
               <<"\", \"animatable\": true}";
        }
        out<<"]}";
    }
    if(!first)out<<'\n';
    out<<"  ],\n  \"customConverters\": [";
    const auto converters=registry.Converters().TypeNames();
    for(std::size_t i=0;i<converters.size();++i){if(i)out<<", ";out<<"\""<<JsonEscape(converters[i])<<"\"";}
    out<<"],\n  \"bindings\": [],\n  \"functions\": []\n}\n";
    return out.str();
}

inline std::string GenerateRuiComponentMarkdown(const UIComponentRegistry& registry) {
    using namespace rui_component_detail;
    std::ostringstream out;
    out<<"# RUI Component Reference\n\nGenerated from the runtime component registry.\n\n";
    for(const UIComponentDescriptor* d:registry.GetDescriptors()) {
        out<<"## "<<d->name<<"\n\n";
        if(!d->documentation.empty())out<<d->documentation<<"\n\n";
        out<<"- Kind: `"<<ComponentKindName(d->kind)<<"`\n";
        out<<"- Since: `"<<d->version.since<<"`\n";
        out<<"- Child policy: `"<<ChildPolicyName(d->children.kind)<<"`\n";
        const auto capabilities=CapabilityNames(registry.GetCapabilities(*d));
        if(!capabilities.empty()){out<<"- Capabilities: ";for(std::size_t i=0;i<capabilities.size();++i){if(i)out<<", ";out<<'`'<<capabilities[i]<<'`';}out<<"\n";}
        if(!d->aliases.empty()){out<<"- Aliases: ";for(std::size_t i=0;i<d->aliases.size();++i){if(i)out<<", ";out<<'`'<<d->aliases[i]<<'`';}out<<"\n";}
        const auto properties=registry.GetProperties(*d);
        if(!properties.empty()) {
            out<<"\n### Properties\n\n| Property | Type | Bindable | Serializable | Constraints | Description |\n|---|---|---:|---:|---|---|\n";
            for(const UIPropertyDescriptor* p:properties) {
                std::string constraints;
                if(p->constraints.minimum)constraints+="min="+std::to_string(*p->constraints.minimum)+" ";
                if(p->constraints.maximum)constraints+="max="+std::to_string(*p->constraints.maximum)+" ";
                out<<"| `"<<p->name<<"` | `"<<(p->type==UIPropertyType::Custom?p->custom_type_name:PropertyTypeName(p->type))<<"` | "
                   <<(HasUIPropertyFlag(p->flags,UIPropertyFlag::Bindable)?"yes":"no")<<" | "
                   <<(HasUIPropertyFlag(p->flags,UIPropertyFlag::Serializable)?"yes":"no")<<" | "<<constraints<<" | "<<p->documentation<<" |\n";
            }
        }
        const auto events=registry.GetEvents(*d);
        if(!events.empty()){out<<"\n### Events\n\n";for(const UIEventDescriptor* e:events)out<<"- `"<<e->markup_name<<"` — "<<e->documentation<<"\n";}
        out<<"\n";
    }
    return out.str();
}
