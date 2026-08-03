#include "ui_markup.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <set>
#include <sstream>
#include <unordered_set>

namespace {

std::string Trim(std::string value) {
    auto whitespace = [](unsigned char character) {
        return std::isspace(character) != 0;
    };
    value.erase(
        value.begin(),
        std::find_if(value.begin(),value.end(),[&](char character) {
            return !whitespace(static_cast<unsigned char>(character));
        })
    );
    value.erase(
        std::find_if(value.rbegin(),value.rend(),[&](char character) {
            return !whitespace(static_cast<unsigned char>(character));
        }).base(),
        value.end()
    );
    return value;
}

std::string Lower(std::string value) {
    std::transform(
        value.begin(),value.end(),value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );
    return value;
}

std::string NormalName(std::string value) {
    value = Lower(std::move(value));
    value.erase(
        std::remove_if(value.begin(),value.end(),[](char character) {
            return character == '-' || character == '_';
        }),
        value.end()
    );
    return value;
}

std::vector<std::string> SplitWords(const std::string& value) {
    std::vector<std::string> result;
    std::string current;
    for(char character : value) {
        if(std::isspace(static_cast<unsigned char>(character)) || character == ',') {
            if(!current.empty()) {
                result.push_back(std::move(current));
                current.clear();
            }
        }
        else {
            current.push_back(character);
        }
    }
    if(!current.empty()) result.push_back(std::move(current));
    return result;
}

std::string DecodeEntities(std::string value) {
    const std::pair<const char*,const char*> replacements[] = {
        {"&lt;","<"},
        {"&gt;",">"},
        {"&amp;","&"},
        {"&quot;","\""},
        {"&apos;","'"}
    };
    for(const auto& replacement : replacements) {
        std::size_t position = 0;
        while((position = value.find(replacement.first,position)) != std::string::npos) {
            value.replace(position,std::strlen(replacement.first),replacement.second);
            position += std::strlen(replacement.second);
        }
    }
    return value;
}

std::vector<std::string> PathParts(const std::string& raw_path) {
    std::vector<std::string> parts;
    std::string current;
    bool bracket = false;
    for(char character : Trim(raw_path)) {
        if(character == '.' && !bracket) {
            if(!current.empty()) {
                parts.push_back(std::move(current));
                current.clear();
            }
        }
        else if(character == '[') {
            if(!current.empty()) {
                parts.push_back(std::move(current));
                current.clear();
            }
            bracket = true;
        }
        else if(character == ']') {
            if(!current.empty()) {
                if(
                    current.size() >= 2 &&
                    ((current.front() == '\'' && current.back() == '\'') ||
                     (current.front() == '"' && current.back() == '"'))
                ) {
                    current = current.substr(1,current.size()-2);
                }
                parts.push_back(std::move(current));
                current.clear();
            }
            bracket = false;
        }
        else if(!std::isspace(static_cast<unsigned char>(character)) || bracket) {
            current.push_back(character);
        }
    }
    if(!current.empty()) parts.push_back(std::move(current));
    return parts;
}

UIXBinding ChildBinding(const UIXBinding& parent, const std::string& member) {
    if(!parent.Valid()) return {};
    const UIXValue value = parent.Get();

    if(const UIXObject* object = value.AsObject()) {
        const auto found = object->fields.find(member);
        if(found != object->fields.end()) return found->second;
        const std::string normalized = NormalName(member);
        for(const auto& field : object->fields) {
            if(NormalName(field.first) == normalized) return field.second;
        }
        return {};
    }

    if(const UIXArray* array = value.AsArray()) {
        if(NormalName(member) == "length" || NormalName(member) == "size") {
            return UIXBinding::Constant(UIXValue(static_cast<int>(array->items.size())));
        }
        try {
            const std::size_t index = static_cast<std::size_t>(std::stoul(member));
            return index < array->items.size() ? array->items[index] : UIXBinding{};
        }
        catch(...) {
            return {};
        }
    }

    const std::string key = NormalName(member);
    auto component = [parent,key](int index) {
        return UIXBinding(
            [parent,index]() {
                UIXValue current = parent.Get();
                if(const auto* vector = std::get_if<Vector2>(&current.StorageValue())) {
                    return UIXValue(index == 0 ? vector->x : vector->y);
                }
                if(const auto* vector = std::get_if<Vector3>(&current.StorageValue())) {
                    return UIXValue(index == 0 ? vector->x : index == 1 ? vector->y : vector->z);
                }
                if(const auto* vector = std::get_if<Vector4>(&current.StorageValue())) {
                    return UIXValue(index == 0 ? vector->x : index == 1 ? vector->y : index == 2 ? vector->z : vector->w);
                }
                if(const auto* color = std::get_if<Color>(&current.StorageValue())) {
                    const unsigned char values[] = {color->r,color->g,color->b,color->a};
                    return UIXValue(static_cast<int>(values[index]));
                }
                if(const auto* rectangle = std::get_if<Rectangle>(&current.StorageValue())) {
                    const float values[] = {rectangle->x,rectangle->y,rectangle->width,rectangle->height};
                    return UIXValue(values[index]);
                }
                return UIXValue{};
            },
            [parent,index](const UIXValue& incoming) {
                if(!parent.Writable()) return false;
                UIXValue current = parent.Get();
                if(auto* vector = std::get_if<Vector2>(&current.StorageValue())) {
                    (index == 0 ? vector->x : vector->y) = static_cast<float>(incoming.Number());
                }
                else if(auto* vector = std::get_if<Vector3>(&current.StorageValue())) {
                    (index == 0 ? vector->x : index == 1 ? vector->y : vector->z) = static_cast<float>(incoming.Number());
                }
                else if(auto* vector = std::get_if<Vector4>(&current.StorageValue())) {
                    (index == 0 ? vector->x : index == 1 ? vector->y : index == 2 ? vector->z : vector->w) = static_cast<float>(incoming.Number());
                }
                else if(auto* color = std::get_if<Color>(&current.StorageValue())) {
                    unsigned char* values[] = {&color->r,&color->g,&color->b,&color->a};
                    *values[index] = static_cast<unsigned char>(std::clamp(incoming.Number(),0.0,255.0));
                }
                else if(auto* rectangle = std::get_if<Rectangle>(&current.StorageValue())) {
                    float* values[] = {&rectangle->x,&rectangle->y,&rectangle->width,&rectangle->height};
                    *values[index] = static_cast<float>(incoming.Number());
                }
                else {
                    return false;
                }
                return parent.Set(current);
            }
        );
    };

    if(key == "x" || key == "r") return component(0);
    if(key == "y" || key == "g") return component(1);
    if(key == "z" || key == "b" || key == "width") return component(2);
    if(key == "w" || key == "a" || key == "height") return component(3);
    if((key == "length" || key == "size") && std::holds_alternative<std::string>(value.StorageValue())) {
        return UIXBinding::Constant(UIXValue(static_cast<int>(value.String().size())));
    }
    return {};
}

bool EqualValue(const UIXValue& first, const UIXValue& second) {
    if(first.StorageValue().index() != second.StorageValue().index()) {
        const bool numeric_first = std::holds_alternative<int>(first.StorageValue()) ||
            std::holds_alternative<double>(first.StorageValue()) ||
            std::holds_alternative<bool>(first.StorageValue());
        const bool numeric_second = std::holds_alternative<int>(second.StorageValue()) ||
            std::holds_alternative<double>(second.StorageValue()) ||
            std::holds_alternative<bool>(second.StorageValue());
        return numeric_first && numeric_second &&
            std::fabs(first.Number()-second.Number()) <= 0.000001;
    }
    if(first.IsNull()) return true;
    if(const auto* value = std::get_if<bool>(&first.StorageValue())) return *value == std::get<bool>(second.StorageValue());
    if(const auto* value = std::get_if<int>(&first.StorageValue())) return *value == std::get<int>(second.StorageValue());
    if(const auto* value = std::get_if<double>(&first.StorageValue())) return std::fabs(*value-std::get<double>(second.StorageValue())) <= 0.000001;
    if(const auto* value = std::get_if<std::string>(&first.StorageValue())) return *value == std::get<std::string>(second.StorageValue());
    if(const auto* value = std::get_if<Vector2>(&first.StorageValue())) {
        const Vector2 other = std::get<Vector2>(second.StorageValue());
        return value->x == other.x && value->y == other.y;
    }
    if(const auto* value = std::get_if<Vector3>(&first.StorageValue())) {
        const Vector3 other = std::get<Vector3>(second.StorageValue());
        return value->x == other.x && value->y == other.y && value->z == other.z;
    }
    if(const auto* value = std::get_if<Vector4>(&first.StorageValue())) {
        const Vector4 other = std::get<Vector4>(second.StorageValue());
        return value->x == other.x && value->y == other.y && value->z == other.z && value->w == other.w;
    }
    if(const auto* value = std::get_if<Color>(&first.StorageValue())) {
        const Color other = std::get<Color>(second.StorageValue());
        return value->r == other.r && value->g == other.g && value->b == other.b && value->a == other.a;
    }
    return first.String() == second.String();
}

const UIXAttribute* FindAttribute(const UIXNode& node, const std::string& name) {
    const std::string normalized = NormalName(name);
    for(const UIXAttribute& attribute : node.attributes) {
        if(NormalName(attribute.name) == normalized) return &attribute;
    }
    return nullptr;
}

std::string RawAttribute(
    const UIXNode& node,
    const std::string& name,
    std::string fallback = {}
) {
    if(const UIXAttribute* attribute = FindAttribute(node,name)) return attribute->value;
    return fallback;
}

class XMLParser {
public:
    XMLParser(const std::string& source, std::string source_name)
        : source_(source), source_name_(std::move(source_name)) {}

    UIXDocument Parse() {
        UIXDocument document;
        diagnostics_ = &document.diagnostics;
        while(!End()) {
            SkipWhitespace();
            if(End()) break;
            if(Starts("<!--")) {
                SkipUntil("-->","Unterminated XML comment");
                continue;
            }
            if(Starts("<?")) {
                SkipUntil("?>","Unterminated XML processing instruction");
                continue;
            }
            if(Starts("<!")) {
                SkipUntil(">","Unterminated XML declaration");
                continue;
            }
            if(Peek() != '<') {
                UIXNode text = ParseText();
                if(!Trim(text.text).empty()) document.roots.push_back(std::move(text));
                continue;
            }
            auto node = ParseElement();
            if(node.has_value()) document.roots.push_back(std::move(*node));
            else if(!End()) Advance();
        }
        CollectDefinitions(document);
        return document;
    }

private:
    UIXSourceLocation Location() const {
        return {source_name_,line_,column_};
    }

    bool End() const { return position_ >= source_.size(); }
    char Peek(std::size_t offset = 0) const {
        return position_+offset < source_.size() ? source_[position_+offset] : '\0';
    }
    bool Starts(const std::string& text) const {
        return source_.compare(position_,text.size(),text) == 0;
    }
    char Advance() {
        if(End()) return '\0';
        const char character = source_[position_++];
        if(character == '\n') {
            ++line_;
            column_ = 1;
        }
        else {
            ++column_;
        }
        return character;
    }
    void SkipWhitespace() {
        while(!End() && std::isspace(static_cast<unsigned char>(Peek()))) Advance();
    }
    void Error(const UIXSourceLocation& location, std::string message) {
        diagnostics_->push_back({location,std::move(message),true});
    }
    void SkipUntil(const std::string& ending, const std::string& error) {
        const UIXSourceLocation start = Location();
        while(!End() && !Starts(ending)) Advance();
        if(End()) {
            Error(start,error);
            return;
        }
        for(std::size_t index=0; index<ending.size(); ++index) Advance();
    }
    std::string ParseName() {
        std::string name;
        while(!End()) {
            const char character = Peek();
            if(
                std::isalnum(static_cast<unsigned char>(character)) ||
                character == '_' || character == '-' || character == ':' ||
                character == '.' || character == '$'
            ) {
                name.push_back(Advance());
            }
            else {
                break;
            }
        }
        return name;
    }
    UIXNode ParseText() {
        UIXNode node;
        node.type = UIXNodeType::Text;
        node.location = Location();
        while(!End() && Peek() != '<') node.text.push_back(Advance());
        node.text = DecodeEntities(std::move(node.text));
        return node;
    }
    std::string ParseAttributeValue() {
        if(Peek() == '"' || Peek() == '\'') {
            const char quote = Advance();
            std::string value;
            while(!End() && Peek() != quote) value.push_back(Advance());
            if(Peek() == quote) Advance();
            return DecodeEntities(std::move(value));
        }
        if(Peek() == '{') {
            std::string value;
            int depth = 0;
            char quote = '\0';
            while(!End()) {
                const char character = Advance();
                value.push_back(character);
                if(quote != '\0') {
                    if(character == '\\' && !End()) value.push_back(Advance());
                    else if(character == quote) quote = '\0';
                    continue;
                }
                if(character == '"' || character == '\'') quote = character;
                else if(character == '{') ++depth;
                else if(character == '}' && --depth == 0) break;
            }
            return value;
        }
        std::string value;
        while(
            !End() &&
            !std::isspace(static_cast<unsigned char>(Peek())) &&
            Peek() != '>' && !(Peek() == '/' && Peek(1) == '>')
        ) {
            value.push_back(Advance());
        }
        return DecodeEntities(std::move(value));
    }
    std::optional<UIXNode> ParseElement() {
        const UIXSourceLocation start = Location();
        if(Advance() != '<') return std::nullopt;
        if(Peek() == '/') {
            Error(start,"Unexpected closing tag");
            return std::nullopt;
        }
        UIXNode node;
        node.type = UIXNodeType::Element;
        node.location = start;
        node.name = ParseName();
        if(node.name.empty()) {
            Error(start,"Expected an element name after '<'");
            return std::nullopt;
        }

        while(!End()) {
            SkipWhitespace();
            if(Starts("/>")) {
                Advance();
                Advance();
                return node;
            }
            if(Peek() == '>') {
                Advance();
                break;
            }
            UIXAttribute attribute;
            attribute.location = Location();
            attribute.name = ParseName();
            if(attribute.name.empty()) {
                Error(Location(),"Expected an attribute name");
                Advance();
                continue;
            }
            SkipWhitespace();
            if(Peek() == '=') {
                Advance();
                SkipWhitespace();
                attribute.value = ParseAttributeValue();
            }
            else {
                attribute.value = "true";
            }
            node.attributes.push_back(std::move(attribute));
        }

        while(!End()) {
            if(Starts("<!--")) {
                SkipUntil("-->","Unterminated XML comment");
                continue;
            }
            if(Starts("</")) {
                Advance();
                Advance();
                const std::string closing = ParseName();
                SkipWhitespace();
                if(Peek() == '>') Advance();
                if(NormalName(closing) != NormalName(node.name)) {
                    Error(Location(),"Closing tag </"+closing+"> does not match <"+node.name+">");
                }
                return node;
            }
            if(Peek() == '<') {
                auto child = ParseElement();
                if(child.has_value()) node.children.push_back(std::move(*child));
                else if(!End()) Advance();
            }
            else {
                node.children.push_back(ParseText());
            }
        }
        Error(start,"Element <"+node.name+"> was not closed");
        return node;
    }
    static void CollectNodeDefinitions(const UIXNode& node, UIXDocument& document) {
        if(node.type != UIXNodeType::Element) return;
        const std::string type = NormalName(node.name);
        if(type == "function") {
            UIXFunction function;
            function.name = RawAttribute(node,"name");
            function.parameters = SplitWords(RawAttribute(node,"params"));
            function.declaration = node;
            if(!function.name.empty()) {
                document.functions[Lower(function.name)] = std::move(function);
            }
            return;
        }
        if(type == "component") {
            UIXComponent component;
            component.name = RawAttribute(node,"name");
            component.declaration = node;
            if(!component.name.empty()) {
                document.components[Lower(component.name)] = std::move(component);
            }
            return;
        }
        for(const UIXNode& child : node.children) CollectNodeDefinitions(child,document);
    }
    static void CollectDefinitions(UIXDocument& document) {
        for(const UIXNode& node : document.roots) CollectNodeDefinitions(node,document);
    }

    const std::string& source_;
    std::string source_name_;
    std::size_t position_ = 0;
    int line_ = 1;
    int column_ = 1;
    std::vector<UIXDiagnostic>* diagnostics_ = nullptr;
};

} // namespace

bool UIXValue::IsNull() const {
    return std::holds_alternative<std::monostate>(value_);
}

bool UIXValue::IsObject() const {
    return std::holds_alternative<Object>(value_);
}

bool UIXValue::IsArray() const {
    return std::holds_alternative<Array>(value_);
}

bool UIXValue::Truthy() const {
    if(IsNull()) return false;
    if(const auto* value = std::get_if<bool>(&value_)) return *value;
    if(const auto* value = std::get_if<int>(&value_)) return *value != 0;
    if(const auto* value = std::get_if<double>(&value_)) return *value != 0.0 && !std::isnan(*value);
    if(const auto* value = std::get_if<std::string>(&value_)) {
        const std::string normalized = Lower(Trim(*value));
        return !normalized.empty() && normalized != "false" && normalized != "0" && normalized != "null";
    }
    if(const UIXArray* array = AsArray()) return !array->items.empty();
    return true;
}

double UIXValue::Number(double fallback) const {
    if(const auto* value = std::get_if<double>(&value_)) return *value;
    if(const auto* value = std::get_if<int>(&value_)) return static_cast<double>(*value);
    if(const auto* value = std::get_if<bool>(&value_)) return *value ? 1.0 : 0.0;
    if(const auto* value = std::get_if<std::string>(&value_)) {
        try {
            std::size_t consumed = 0;
            const double result = std::stod(Trim(*value),&consumed);
            return consumed == Trim(*value).size() ? result : fallback;
        }
        catch(...) {
            return fallback;
        }
    }
    return fallback;
}

std::string UIXValue::String() const {
    if(IsNull()) return "";
    if(const auto* value = std::get_if<std::string>(&value_)) return *value;
    if(const auto* value = std::get_if<bool>(&value_)) return *value ? "true" : "false";
    if(const auto* value = std::get_if<int>(&value_)) return std::to_string(*value);
    if(const auto* value = std::get_if<double>(&value_)) {
        std::ostringstream stream;
        stream << std::setprecision(8) << *value;
        std::string result = stream.str();
        if(result.find('.') != std::string::npos) {
            while(!result.empty() && result.back() == '0') result.pop_back();
            if(!result.empty() && result.back() == '.') result.pop_back();
        }
        return result;
    }
    if(const auto* value = std::get_if<Vector2>(&value_)) return std::to_string(value->x)+", "+std::to_string(value->y);
    if(const auto* value = std::get_if<Vector3>(&value_)) return std::to_string(value->x)+", "+std::to_string(value->y)+", "+std::to_string(value->z);
    if(const auto* value = std::get_if<Vector4>(&value_)) return std::to_string(value->x)+", "+std::to_string(value->y)+", "+std::to_string(value->z)+", "+std::to_string(value->w);
    if(const auto* value = std::get_if<Color>(&value_)) {
        std::ostringstream stream;
        stream << '#' << std::hex << std::setfill('0') << std::uppercase
               << std::setw(2) << static_cast<int>(value->r)
               << std::setw(2) << static_cast<int>(value->g)
               << std::setw(2) << static_cast<int>(value->b)
               << std::setw(2) << static_cast<int>(value->a);
        return stream.str();
    }
    if(const UIXArray* array = AsArray()) return "["+std::to_string(array->items.size())+" items]";
    if(IsObject()) return "[object]";
    return "[native value]";
}

std::size_t UIXValue::Size() const {
    if(const UIXArray* array = AsArray()) return array->items.size();
    if(const UIXObject* object = AsObject()) return object->fields.size();
    if(const auto* text = std::get_if<std::string>(&value_)) return text->size();
    return 0;
}

const UIXObject* UIXValue::AsObject() const {
    const auto* value = std::get_if<Object>(&value_);
    return value != nullptr && *value ? value->get() : nullptr;
}

UIXObject* UIXValue::AsObject() {
    auto* value = std::get_if<Object>(&value_);
    return value != nullptr && *value ? value->get() : nullptr;
}

const UIXArray* UIXValue::AsArray() const {
    const auto* value = std::get_if<Array>(&value_);
    return value != nullptr && *value ? value->get() : nullptr;
}

UIXArray* UIXValue::AsArray() {
    auto* value = std::get_if<Array>(&value_);
    return value != nullptr && *value ? value->get() : nullptr;
}

struct UIXRegistryState {
    struct BindingEntry {
        std::uint64_t id = 0;
        UIXBinding binding;
    };

    struct FunctionEntry {
        std::uint64_t id = 0;
        UIXNativeFunction function;
    };

    std::unordered_map<std::string,BindingEntry> bindings;
    std::unordered_map<std::string,FunctionEntry> functions;
    std::unordered_set<std::uint64_t> warned_expired;
    UIXRegistry::WarningHandler warning_handler;
    std::uint64_t next_id = 1;
};

namespace {

void WarnExpiredBinding(
    UIXRegistryState& state,
    const std::string& name,
    std::uint64_t id
) {
    if(!state.warned_expired.insert(id).second) return;
    const std::string message =
        "UIX binding '"+name+"' expired because its tracked owner was destroyed before it was explicitly unbound";
    if(state.warning_handler) {
        state.warning_handler(message);
    }
    else {
        TraceLog(LOG_WARNING,"%s",message.c_str());
    }
}

template <typename TMap>
auto FindNamedEntry(TMap& entries, const std::string& name) {
    auto found = entries.find(name);
    if(found != entries.end()) return found;

    const std::string normalized = NormalName(name);
    return std::find_if(
        entries.begin(),entries.end(),
        [&](const auto& entry) {
            return NormalName(entry.first) == normalized;
        }
    );
}

} // namespace

bool UIXBindingHandle::Unbind() const {
    const auto state = state_.lock();
    if(!state || id_ == 0) return false;

    if(kind_ == UIXRegistrationKind::Binding) {
        const auto found = state->bindings.find(name_);
        if(found == state->bindings.end() || found->second.id != id_) {
            return false;
        }
        state->bindings.erase(found);
        state->warned_expired.erase(id_);
        return true;
    }

    const auto found = state->functions.find(name_);
    if(found == state->functions.end() || found->second.id != id_) {
        return false;
    }
    state->functions.erase(found);
    return true;
}

bool UIXBindingHandle::Valid() const {
    const auto state = state_.lock();
    if(!state || id_ == 0) return false;

    if(kind_ == UIXRegistrationKind::Binding) {
        const auto found = state->bindings.find(name_);
        return found != state->bindings.end() &&
            found->second.id == id_ &&
            found->second.binding.Valid();
    }

    const auto found = state->functions.find(name_);
    return found != state->functions.end() && found->second.id == id_;
}

UIXBinding::UIXBinding(
    Getter getter,
    Setter setter,
    AlivePredicate alive
)
    : getter_(std::move(getter)),
      setter_(std::move(setter)),
      alive_(std::move(alive)) {}

UIXValue UIXBinding::Get() const {
    return Valid() ? getter_() : UIXValue{};
}

bool UIXBinding::Set(const UIXValue& value) const {
    return Writable() ? setter_(value) : false;
}

bool UIXBinding::Expired() const {
    return static_cast<bool>(getter_) &&
        static_cast<bool>(alive_) &&
        !alive_();
}

bool UIXBinding::Valid() const {
    return static_cast<bool>(getter_) && !Expired();
}

bool UIXBinding::Writable() const {
    return static_cast<bool>(setter_) && Valid();
}

UIXBinding UIXBinding::Constant(UIXValue value) {
    auto storage = std::make_shared<UIXValue>(std::move(value));
    return UIXBinding([storage]() { return *storage; });
}

UIXBinding UIXBinding::TrackedConstant(
    UIXValue value,
    std::weak_ptr<void> lifetime
) {
    auto storage = std::make_shared<UIXValue>(std::move(value));
    return UIXBinding(
        [storage]() { return *storage; },
        {},
        [lifetime=std::move(lifetime)]() {
            return !lifetime.expired();
        }
    );
}

UIXRegistry::UIXRegistry()
    : state_(std::make_shared<UIXRegistryState>()) {}

UIXRegistry::~UIXRegistry() = default;
UIXRegistry::UIXRegistry(UIXRegistry&&) noexcept = default;
UIXRegistry& UIXRegistry::operator=(UIXRegistry&&) noexcept = default;

UIXBindingHandle UIXRegistry::Bind(
    std::string name,
    UIXBinding binding
) {
    const std::uint64_t id = state_->next_id++;
    const std::string stored_name = std::move(name);
    state_->bindings[stored_name] = {id,std::move(binding)};
    return UIXBindingHandle(
        state_,id,stored_name,UIXRegistrationKind::Binding
    );
}

UIXBindingHandle UIXRegistry::Value(
    std::string name,
    UIXValue value
) {
    return Bind(
        std::move(name),
        UIXBinding::Constant(std::move(value))
    );
}

UIXBindingHandle UIXRegistry::Function(
    std::string name,
    UIXNativeFunction function
) {
    name = Lower(std::move(name));
    const std::uint64_t id = state_->next_id++;
    state_->functions[name] = {id,std::move(function)};
    return UIXBindingHandle(
        state_,id,std::move(name),UIXRegistrationKind::Function
    );
}

UIXBindingHandle UIXRegistry::Function(
    std::string name,
    std::function<void()> function
) {
    return Function(
        std::move(name),
        UIXNativeFunction(
            [function=std::move(function)](const UIXArguments&,UIXEvent&) {
                function();
                return UIXValue{};
            }
        )
    );
}

UIXBindingHandle UIXRegistry::Function(
    std::string name,
    std::function<void(const UIXEvent&)> function
) {
    return Function(
        std::move(name),
        UIXNativeFunction(
            [function=std::move(function)](const UIXArguments&,UIXEvent& event) {
                function(event);
                return UIXValue{};
            }
        )
    );
}

bool UIXRegistry::Unbind(const std::string& name) {
    const auto found = FindNamedEntry(state_->bindings,name);
    if(found == state_->bindings.end()) return false;
    state_->warned_expired.erase(found->second.id);
    state_->bindings.erase(found);
    return true;
}

bool UIXRegistry::Unbind(const UIXBindingHandle& handle) {
    if(handle.Kind() != UIXRegistrationKind::Binding) return false;
    return handle.Unbind();
}

bool UIXRegistry::UnbindFunction(const std::string& name) {
    const auto found = state_->functions.find(Lower(name));
    if(found == state_->functions.end()) return false;
    state_->functions.erase(found);
    return true;
}

bool UIXRegistry::UnbindFunction(const UIXBindingHandle& handle) {
    if(handle.Kind() != UIXRegistrationKind::Function) return false;
    return handle.Unbind();
}

void UIXRegistry::ClearBindings() {
    state_->bindings.clear();
    state_->warned_expired.clear();
}

void UIXRegistry::ClearFunctions() {
    state_->functions.clear();
}

void UIXRegistry::Clear() {
    ClearBindings();
    ClearFunctions();
}

std::size_t UIXRegistry::CleanupExpired() {
    std::size_t removed = 0;
    for(auto iterator = state_->bindings.begin(); iterator != state_->bindings.end();) {
        if(!iterator->second.binding.Expired()) {
            ++iterator;
            continue;
        }

        WarnExpiredBinding(
            *state_,iterator->first,iterator->second.id
        );
        iterator = state_->bindings.erase(iterator);
        ++removed;
    }
    return removed;
}

bool UIXRegistry::HasBinding(const std::string& name) const {
    const auto found = FindNamedEntry(state_->bindings,name);
    return found != state_->bindings.end() && found->second.binding.Valid();
}

bool UIXRegistry::HasFunction(const std::string& name) const {
    return state_->functions.find(Lower(name)) != state_->functions.end();
}

std::size_t UIXRegistry::BindingCount() const {
    return state_->bindings.size();
}

std::size_t UIXRegistry::FunctionCount() const {
    return state_->functions.size();
}

void UIXRegistry::SetWarningHandler(WarningHandler handler) {
    state_->warning_handler = std::move(handler);
}

UIXBinding UIXRegistry::ResolveBinding(const std::string& path) const {
    const std::vector<std::string> parts = PathParts(path);
    if(parts.empty()) return {};

    auto found = FindNamedEntry(state_->bindings,parts.front());
    if(found == state_->bindings.end()) return {};

    if(found->second.binding.Expired()) {
        WarnExpiredBinding(*state_,found->first,found->second.id);
        state_->bindings.erase(found);
        return {};
    }

    UIXBinding result = found->second.binding;
    for(std::size_t index=1; index<parts.size(); ++index) {
        result = ChildBinding(result,parts[index]);
        if(!result.Valid()) break;
    }
    return result;
}

UIXValue UIXRegistry::Resolve(const std::string& path) const {
    return ResolveBinding(path).Get();
}

bool UIXRegistry::Assign(
    const std::string& path,
    const UIXValue& value
) const {
    return ResolveBinding(path).Set(value);
}

UIXValue UIXRegistry::Invoke(
    const std::string& name,
    const UIXArguments& arguments,
    UIXEvent& event
) const {
    const auto found = state_->functions.find(Lower(name));
    return found != state_->functions.end()
        ? found->second.function(arguments,event)
        : UIXValue{};
}

bool UIXDocument::Valid() const {
    return std::none_of(diagnostics.begin(),diagnostics.end(),[](const UIXDiagnostic& diagnostic) {
        return diagnostic.error;
    });
}

UIXDocument ParseUIX(const std::string& source, std::string source_name) {
    return XMLParser(source,std::move(source_name)).Parse();
}

namespace {

void CollectImportNodes(
    const UIXNode& node,
    std::vector<const UIXNode*>& imports
) {
    if(node.type != UIXNodeType::Element) return;
    if(NormalName(node.name) == "import") imports.push_back(&node);
    for(const UIXNode& child : node.children) CollectImportNodes(child,imports);
}

std::filesystem::path NormalizeImportPath(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path normalized = std::filesystem::weakly_canonical(path,error);
    if(!error) return normalized;

    error.clear();
    normalized = std::filesystem::absolute(path,error);
    return (error ? path : normalized).lexically_normal();
}

void MergeImportedDefinitions(
    UIXDocument& destination,
    UIXDocument&& imported
) {
    // emplace deliberately preserves definitions from the importing document.
    for(auto& entry : imported.functions) {
        destination.functions.emplace(entry.first,std::move(entry.second));
    }
    for(auto& entry : imported.components) {
        destination.components.emplace(entry.first,std::move(entry.second));
    }
    destination.diagnostics.insert(
        destination.diagnostics.end(),
        std::make_move_iterator(imported.diagnostics.begin()),
        std::make_move_iterator(imported.diagnostics.end())
    );
}

void ResolveDocumentImports(
    UIXDocument& document,
    const std::filesystem::path& source_path,
    std::set<std::filesystem::path>& loading,
    std::set<std::filesystem::path>& loaded
) {
    std::vector<const UIXNode*> imports;
    for(const UIXNode& root : document.roots) CollectImportNodes(root,imports);

    const std::filesystem::path base_directory =
        source_path.empty() ? std::filesystem::current_path() : source_path.parent_path();

    for(const UIXNode* import : imports) {
        std::string link = RawAttribute(*import,"src");
        if(link.empty()) link = RawAttribute(*import,"href");
        if(link.empty()) link = RawAttribute(*import,"path");
        if(link.empty()) {
            document.diagnostics.push_back({
                import->location,
                "<Import> requires a src attribute",
                true
            });
            continue;
        }

        std::filesystem::path import_path(link);
        if(import_path.is_relative()) import_path = base_directory/import_path;
        import_path = NormalizeImportPath(import_path);

        if(loading.find(import_path) != loading.end()) {
            document.diagnostics.push_back({
                import->location,
                "Circular RML import detected for '"+import_path.string()+"'",
                true
            });
            continue;
        }
        if(loaded.find(import_path) != loaded.end()) continue;

        std::ifstream file(import_path,std::ios::binary);
        if(!file) {
            document.diagnostics.push_back({
                import->location,
                "Unable to open imported RML file '"+import_path.string()+"'",
                true
            });
            continue;
        }

        std::ostringstream source;
        source << file.rdbuf();
        UIXDocument imported = ParseUIX(source.str(),import_path.string());

        loading.insert(import_path);
        ResolveDocumentImports(imported,import_path,loading,loaded);
        loading.erase(import_path);
        loaded.insert(import_path);

        MergeImportedDefinitions(document,std::move(imported));
    }
}

void ResolveDocumentImports(UIXDocument& document, const std::string& source_name) {
    std::filesystem::path source_path;
    if(!source_name.empty() && source_name != "<memory>") {
        source_path = NormalizeImportPath(source_name);
    }

    std::set<std::filesystem::path> loading;
    std::set<std::filesystem::path> loaded;
    if(!source_path.empty()) loading.insert(source_path);
    ResolveDocumentImports(document,source_path,loading,loaded);
}

struct UIXScope {
    std::unordered_map<std::string,UIXBinding> values;
    std::shared_ptr<UIXScope> parent;
    std::string key_suffix;
};

UIXBinding ResolveScopedRoot(
    const UIXRegistry& registry,
    const std::shared_ptr<UIXScope>& scope,
    const std::string& name
) {
    for(auto current = scope; current; current = current->parent) {
        auto found = current->values.find(name);
        if(found != current->values.end()) return found->second;
        const std::string normalized = NormalName(name);
        found = std::find_if(
            current->values.begin(),current->values.end(),
            [&](const auto& entry) { return NormalName(entry.first) == normalized; }
        );
        if(found != current->values.end()) return found->second;
    }
    return registry.ResolveBinding(name);
}

UIXBinding ResolveScopedPath(
    const UIXRegistry& registry,
    const std::shared_ptr<UIXScope>& scope,
    const std::string& path
) {
    const std::vector<std::string> parts = PathParts(path);
    if(parts.empty()) return {};
    UIXBinding binding = ResolveScopedRoot(registry,scope,parts.front());
    for(std::size_t index=1; index<parts.size() && binding.Valid(); ++index) {
        binding = ChildBinding(binding,parts[index]);
    }
    return binding;
}

enum class LinkKind {
    InputValue,
    SelectableSelected,
    ComboIndex,
    RangeLower,
    RangeUpper,
    CollapsibleOpen,
    TreeOpen,
    TreeSelected,
    MenuOpen,
    PopupOpen,
    WindowOpen,
    WindowCollapsed,
    TabIndex
};

struct StateLink {
    std::string key;
    LinkKind kind = LinkKind::InputValue;
    UIXBinding binding;
    UIXValue rendered;
};

std::string LinkCacheKey(const std::string& key, LinkKind kind) {
    return key+"|"+std::to_string(static_cast<int>(kind));
}

struct EvalResult {
    UIXValue value;
    UIXBinding binding;
};

enum class ExprTokenType {
    End,
    Identifier,
    Number,
    String,
    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,
    Comma,
    Dot,
    Question,
    Colon,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Bang,
    EqualEqual,
    BangEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    AndAnd,
    OrOr
};

struct ExprToken {
    ExprTokenType type = ExprTokenType::End;
    std::string text;
    double number = 0.0;
};

class ExprLexer {
public:
    explicit ExprLexer(std::string_view source) : source_(source) {}

    ExprToken Next() {
        while(position_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[position_]))) ++position_;
        if(position_ >= source_.size()) return {};
        const char character = source_[position_++];
        switch(character) {
            case '(': return {ExprTokenType::LeftParen,"("};
            case ')': return {ExprTokenType::RightParen,")"};
            case '[': return {ExprTokenType::LeftBracket,"["};
            case ']': return {ExprTokenType::RightBracket,"]"};
            case ',': return {ExprTokenType::Comma,","};
            case '.':
                if(position_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[position_]))) {
                    --position_;
                    return Number();
                }
                return {ExprTokenType::Dot,"."};
            case '?': return {ExprTokenType::Question,"?"};
            case ':': return {ExprTokenType::Colon,":"};
            case '+': return {ExprTokenType::Plus,"+"};
            case '-': return {ExprTokenType::Minus,"-"};
            case '*': return {ExprTokenType::Star,"*"};
            case '/': return {ExprTokenType::Slash,"/"};
            case '%': return {ExprTokenType::Percent,"%"};
            case '!':
                if(Match('=')) return {ExprTokenType::BangEqual,"!="};
                return {ExprTokenType::Bang,"!"};
            case '=':
                if(Match('=')) return {ExprTokenType::EqualEqual,"=="};
                return {ExprTokenType::EqualEqual,"=="};
            case '<':
                if(Match('=')) return {ExprTokenType::LessEqual,"<="};
                return {ExprTokenType::Less,"<"};
            case '>':
                if(Match('=')) return {ExprTokenType::GreaterEqual,">="};
                return {ExprTokenType::Greater,">"};
            case '&':
                if(Match('&')) return {ExprTokenType::AndAnd,"&&"};
                break;
            case '|':
                if(Match('|')) return {ExprTokenType::OrOr,"||"};
                break;
            case '\'':
            case '"': return String(character);
            default: break;
        }
        if(std::isdigit(static_cast<unsigned char>(character))) {
            --position_;
            return Number();
        }
        if(
            std::isalpha(static_cast<unsigned char>(character)) ||
            character == '_' || character == '$'
        ) {
            std::string identifier(1,character);
            while(position_ < source_.size()) {
                const char next = source_[position_];
                if(
                    std::isalnum(static_cast<unsigned char>(next)) ||
                    next == '_' || next == '$' || next == '-'
                ) {
                    identifier.push_back(next);
                    ++position_;
                }
                else {
                    break;
                }
            }
            return {ExprTokenType::Identifier,std::move(identifier)};
        }
        return Next();
    }

private:
    bool Match(char expected) {
        if(position_ >= source_.size() || source_[position_] != expected) return false;
        ++position_;
        return true;
    }
    ExprToken Number() {
        const std::size_t start = position_;
        bool exponent = false;
        while(position_ < source_.size()) {
            const char character = source_[position_];
            if(std::isdigit(static_cast<unsigned char>(character)) || character == '.') {
                ++position_;
            }
            else if((character == 'e' || character == 'E') && !exponent) {
                exponent = true;
                ++position_;
                if(position_ < source_.size() && (source_[position_] == '+' || source_[position_] == '-')) ++position_;
            }
            else break;
        }
        const std::string text(source_.substr(start,position_-start));
        return {ExprTokenType::Number,text,std::stod(text)};
    }
    ExprToken String(char quote) {
        std::string value;
        while(position_ < source_.size() && source_[position_] != quote) {
            char character = source_[position_++];
            if(character == '\\' && position_ < source_.size()) {
                const char escaped = source_[position_++];
                switch(escaped) {
                    case 'n': character = '\n'; break;
                    case 't': character = '\t'; break;
                    case 'r': character = '\r'; break;
                    default: character = escaped; break;
                }
            }
            value.push_back(character);
        }
        if(position_ < source_.size()) ++position_;
        return {ExprTokenType::String,std::move(value)};
    }
    std::string_view source_;
    std::size_t position_ = 0;
};

std::optional<Color> ParseUIXColor(std::string value) {
    value = Lower(Trim(std::move(value)));
    static const std::unordered_map<std::string,Color> named = {
        {"white",WHITE},{"black",BLACK},{"blank",BLANK},{"transparent",BLANK},
        {"red",RED},{"green",GREEN},{"blue",BLUE},{"yellow",YELLOW},
        {"orange",ORANGE},{"purple",PURPLE},{"pink",PINK},{"gray",GRAY},
        {"grey",GRAY},{"darkgray",DARKGRAY},{"lightgray",LIGHTGRAY}
    };
    if(auto found = named.find(value); found != named.end()) return found->second;
    if(value.size() == 7 || value.size() == 9) {
        if(value.front() != '#') return std::nullopt;
        try {
            const auto byte = [&](std::size_t offset) {
                return static_cast<unsigned char>(std::stoul(value.substr(offset,2),nullptr,16));
            };
            return Color{byte(1),byte(3),byte(5),value.size() == 9 ? byte(7) : static_cast<unsigned char>(255)};
        }
        catch(...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

template <typename T>
T ValueOr(const UIXValue& value, T fallback) {
    T converted{};
    if(value.AssignTo(converted)) return converted;
    return fallback;
}

InputType ParseInputType(std::string value) {
    value = NormalName(std::move(value));
    if(value == "slider") return InputType::Slider;
    if(value == "drag" || value == "dragfloat" || value == "dragint") return InputType::Drag;
    if(value == "trigger" || value == "button") return InputType::Trigger;
    if(value == "checkbox" || value == "toggle") return InputType::Checkbox;
    if(value == "color" || value == "colorpicker") return InputType::ColorPicker;
    return InputType::Instant;
}

UISplitterOrientation ParseSplitterOrientation(std::string value) {
    value = NormalName(std::move(value));
    if(value == "vertical") return UISplitterOrientation::Vertical;
    if(value == "horizontal") return UISplitterOrientation::Horizontal;
    return UISplitterOrientation::Auto;
}

UIImageFit ParseImageFit(std::string value) {
    value = NormalName(std::move(value));
    if(value == "stretch" || value == "fill") return UIImageFit::Stretch;
    if(value == "cover") return UIImageFit::Cover;
    if(value == "center") return UIImageFit::Center;
    if(value == "tile" || value == "repeat") return UIImageFit::Tile;
    return UIImageFit::Contain;
}

UIImageSampling ParseImageSampling(std::string value) {
    value = NormalName(std::move(value));
    return value == "bilinear" || value == "smooth"
        ? UIImageSampling::Bilinear
        : UIImageSampling::Nearest;
}

} // namespace

struct UIXRuntime::Impl {
    UIXRegistry owned_registry;
    UIXRegistry* registry = &owned_registry;
    UIXDocument document;
    std::vector<UIXDiagnostic> diagnostics;
    std::vector<StateLink> links;
    std::unordered_map<std::string,UIXValue> state_cache;
    UIXBuildOptions build_options;
    UIContainer* mounted_root = nullptr;
    std::shared_ptr<void> callback_lifetime = std::make_shared<int>(0);
    UIUndoStack undo_stack;
    int call_depth = 0;

    explicit Impl(UIXRegistry* external = nullptr) {
        if(external != nullptr) registry = external;
    }

    void Warning(const UIXSourceLocation& location, std::string message) {
        diagnostics.push_back({location,std::move(message),false});
    }

    UIXBinding ResolveBinding(
        const std::shared_ptr<UIXScope>& scope,
        const std::string& path
    ) const {
        return ResolveScopedPath(*registry,scope,path);
    }

    EvalResult EvaluateDetailed(
        const std::string& expression,
        const std::shared_ptr<UIXScope>& scope,
        UIXEvent* event
    );

    UIXValue InvokeFunction(
        const std::string& name,
        const UIXArguments& arguments,
        UIXEvent& event,
        const std::shared_ptr<UIXScope>& parent_scope
    );

    bool ExecuteStatements(
        const std::string& statements,
        const std::shared_ptr<UIXScope>& scope,
        UIXEvent& event
    );

    bool ExecuteActions(
        const std::vector<UIXNode>& actions,
        const std::shared_ptr<UIXScope>& scope,
        UIXEvent& event
    );

    void RenderNode(
        UIContainer& parent,
        const UIXNode& node,
        const std::shared_ptr<UIXScope>& scope,
        const UIXNode* slot_source,
        const std::shared_ptr<UIXScope>& slot_scope,
        bool ignore_directives = false,
        int depth = 0
    );

    void RenderChildren(
        UIContainer& parent,
        const std::vector<UIXNode>& children,
        const std::shared_ptr<UIXScope>& scope,
        const UIXNode* slot_source,
        const std::shared_ptr<UIXScope>& slot_scope,
        int depth
    );

    void SyncLinks(UIContainer& root);
};

namespace {

class ExpressionParser {
public:
    ExpressionParser(
        UIXRuntime::Impl& runtime,
        std::string_view expression,
        std::shared_ptr<UIXScope> scope,
        UIXEvent* event
    )
        : runtime_(runtime), lexer_(expression), scope_(std::move(scope)), event_(event) {
        current_ = lexer_.Next();
    }

    EvalResult Parse() { return ParseTernary(); }

private:
    void Advance() { current_ = lexer_.Next(); }
    bool Match(ExprTokenType type) {
        if(current_.type != type) return false;
        Advance();
        return true;
    }
    EvalResult ParseTernary() {
        EvalResult condition = ParseOr();
        if(!Match(ExprTokenType::Question)) return condition;
        EvalResult when_true = ParseTernary();
        Match(ExprTokenType::Colon);
        EvalResult when_false = ParseTernary();
        return condition.value.Truthy() ? when_true : when_false;
    }
    EvalResult ParseOr() {
        EvalResult result = ParseAnd();
        while(Match(ExprTokenType::OrOr)) {
            EvalResult right = ParseAnd();
            result = {UIXValue(result.value.Truthy() || right.value.Truthy()),{}};
        }
        return result;
    }
    EvalResult ParseAnd() {
        EvalResult result = ParseEquality();
        while(Match(ExprTokenType::AndAnd)) {
            EvalResult right = ParseEquality();
            result = {UIXValue(result.value.Truthy() && right.value.Truthy()),{}};
        }
        return result;
    }
    EvalResult ParseEquality() {
        EvalResult result = ParseComparison();
        while(current_.type == ExprTokenType::EqualEqual || current_.type == ExprTokenType::BangEqual) {
            const ExprTokenType operation = current_.type;
            Advance();
            EvalResult right = ParseComparison();
            bool equal = EqualValue(result.value,right.value);
            if(operation == ExprTokenType::BangEqual) equal = !equal;
            result = {UIXValue(equal),{}};
        }
        return result;
    }
    EvalResult ParseComparison() {
        EvalResult result = ParseTerm();
        while(
            current_.type == ExprTokenType::Less ||
            current_.type == ExprTokenType::LessEqual ||
            current_.type == ExprTokenType::Greater ||
            current_.type == ExprTokenType::GreaterEqual
        ) {
            const ExprTokenType operation = current_.type;
            Advance();
            EvalResult right = ParseTerm();
            const double left_number = result.value.Number();
            const double right_number = right.value.Number();
            bool comparison = false;
            if(operation == ExprTokenType::Less) comparison = left_number < right_number;
            if(operation == ExprTokenType::LessEqual) comparison = left_number <= right_number;
            if(operation == ExprTokenType::Greater) comparison = left_number > right_number;
            if(operation == ExprTokenType::GreaterEqual) comparison = left_number >= right_number;
            result = {UIXValue(comparison),{}};
        }
        return result;
    }
    EvalResult ParseTerm() {
        EvalResult result = ParseFactor();
        while(current_.type == ExprTokenType::Plus || current_.type == ExprTokenType::Minus) {
            const ExprTokenType operation = current_.type;
            Advance();
            EvalResult right = ParseFactor();
            if(operation == ExprTokenType::Plus && (
                std::holds_alternative<std::string>(result.value.StorageValue()) ||
                std::holds_alternative<std::string>(right.value.StorageValue())
            )) {
                result = {UIXValue(result.value.String()+right.value.String()),{}};
            }
            else {
                const double number = operation == ExprTokenType::Plus
                    ? result.value.Number()+right.value.Number()
                    : result.value.Number()-right.value.Number();
                result = {UIXValue(number),{}};
            }
        }
        return result;
    }
    EvalResult ParseFactor() {
        EvalResult result = ParseUnary();
        while(
            current_.type == ExprTokenType::Star ||
            current_.type == ExprTokenType::Slash ||
            current_.type == ExprTokenType::Percent
        ) {
            const ExprTokenType operation = current_.type;
            Advance();
            EvalResult right = ParseUnary();
            double value = 0.0;
            if(operation == ExprTokenType::Star) value = result.value.Number()*right.value.Number();
            if(operation == ExprTokenType::Slash) {
                value = right.value.Number() == 0.0 ? 0.0 : result.value.Number()/right.value.Number();
            }
            if(operation == ExprTokenType::Percent) {
                value = right.value.Number() == 0.0 ? 0.0 : std::fmod(result.value.Number(),right.value.Number());
            }
            result = {UIXValue(value),{}};
        }
        return result;
    }
    EvalResult ParseUnary() {
        if(Match(ExprTokenType::Bang)) return {UIXValue(!ParseUnary().value.Truthy()),{}};
        if(Match(ExprTokenType::Minus)) return {UIXValue(-ParseUnary().value.Number()),{}};
        if(Match(ExprTokenType::Plus)) return {UIXValue(ParseUnary().value.Number()),{}};
        return ParsePostfix();
    }
    EvalResult ParsePostfix() {
        EvalResult result = ParsePrimary();
        while(true) {
            if(Match(ExprTokenType::Dot)) {
                if(current_.type != ExprTokenType::Identifier) return result;
                const std::string member = current_.text;
                Advance();
                UIXBinding parent = result.binding.Valid()
                    ? result.binding
                    : UIXBinding::Constant(result.value);
                result.binding = ChildBinding(parent,member);
                result.value = result.binding.Get();
            }
            else if(Match(ExprTokenType::LeftBracket)) {
                EvalResult index = ParseTernary();
                Match(ExprTokenType::RightBracket);
                UIXBinding parent = result.binding.Valid()
                    ? result.binding
                    : UIXBinding::Constant(result.value);
                result.binding = ChildBinding(parent,index.value.String());
                result.value = result.binding.Get();
            }
            else break;
        }
        return result;
    }
    EvalResult ParsePrimary() {
        if(current_.type == ExprTokenType::Number) {
            const double number = current_.number;
            Advance();
            return {UIXValue(number),{}};
        }
        if(current_.type == ExprTokenType::String) {
            std::string value = current_.text;
            Advance();
            return {UIXValue(std::move(value)),{}};
        }
        if(Match(ExprTokenType::LeftParen)) {
            EvalResult result = ParseTernary();
            Match(ExprTokenType::RightParen);
            return result;
        }
        if(Match(ExprTokenType::LeftBracket)) {
            auto array = std::make_shared<UIXArray>();
            if(current_.type != ExprTokenType::RightBracket) {
                do {
                    array->items.push_back(UIXBinding::Constant(ParseTernary().value));
                } while(Match(ExprTokenType::Comma));
            }
            Match(ExprTokenType::RightBracket);
            return {UIXValue(std::move(array)),{}};
        }
        if(current_.type != ExprTokenType::Identifier) return {};
        const std::string identifier = current_.text;
        Advance();
        const std::string normalized = Lower(identifier);
        if(normalized == "true") return {UIXValue(true),{}};
        if(normalized == "false") return {UIXValue(false),{}};
        if(normalized == "null" || normalized == "none") return {};

        if(Match(ExprTokenType::LeftParen)) {
            UIXArguments arguments;
            if(current_.type != ExprTokenType::RightParen) {
                do {
                    arguments.push_back(ParseTernary().value);
                } while(Match(ExprTokenType::Comma));
            }
            Match(ExprTokenType::RightParen);
            UIXEvent local_event;
            UIXEvent& event = event_ != nullptr ? *event_ : local_event;

            if(normalized == "str" || normalized == "string") {
                return {UIXValue(arguments.empty() ? "" : arguments.front().String()),{}};
            }
            if(normalized == "int") {
                return {UIXValue(static_cast<int>(arguments.empty() ? 0.0 : arguments.front().Number())),{}};
            }
            if(normalized == "float" || normalized == "number") {
                return {UIXValue(arguments.empty() ? 0.0 : arguments.front().Number()),{}};
            }
            if(normalized == "bool") {
                return {UIXValue(!arguments.empty() && arguments.front().Truthy()),{}};
            }
            if(normalized == "len" || normalized == "length") {
                return {UIXValue(static_cast<int>(arguments.empty() ? 0 : arguments.front().Size())),{}};
            }
            if(normalized == "min" || normalized == "max") {
                if(arguments.empty()) return {};
                double value = arguments.front().Number();
                for(std::size_t index=1; index<arguments.size(); ++index) {
                    value = normalized == "min"
                        ? std::min(value,arguments[index].Number())
                        : std::max(value,arguments[index].Number());
                }
                return {UIXValue(value),{}};
            }
            if(normalized == "clamp") {
                if(arguments.size() < 3) return {};
                return {UIXValue(std::clamp(arguments[0].Number(),arguments[1].Number(),arguments[2].Number())),{}};
            }
            if(normalized == "vec2") {
                return {UIXValue(Vector2{
                    static_cast<float>(arguments.size() > 0 ? arguments[0].Number() : 0.0),
                    static_cast<float>(arguments.size() > 1 ? arguments[1].Number() : 0.0)
                }),{}};
            }
            if(normalized == "vec3") {
                return {UIXValue(Vector3{
                    static_cast<float>(arguments.size() > 0 ? arguments[0].Number() : 0.0),
                    static_cast<float>(arguments.size() > 1 ? arguments[1].Number() : 0.0),
                    static_cast<float>(arguments.size() > 2 ? arguments[2].Number() : 0.0)
                }),{}};
            }
            if(normalized == "vec4") {
                return {UIXValue(Vector4{
                    static_cast<float>(arguments.size() > 0 ? arguments[0].Number() : 0.0),
                    static_cast<float>(arguments.size() > 1 ? arguments[1].Number() : 0.0),
                    static_cast<float>(arguments.size() > 2 ? arguments[2].Number() : 0.0),
                    static_cast<float>(arguments.size() > 3 ? arguments[3].Number() : 0.0)
                }),{}};
            }
            if(normalized == "rgb" || normalized == "rgba") {
                return {UIXValue(Color{
                    static_cast<unsigned char>(std::clamp(arguments.size() > 0 ? arguments[0].Number() : 0.0,0.0,255.0)),
                    static_cast<unsigned char>(std::clamp(arguments.size() > 1 ? arguments[1].Number() : 0.0,0.0,255.0)),
                    static_cast<unsigned char>(std::clamp(arguments.size() > 2 ? arguments[2].Number() : 0.0,0.0,255.0)),
                    static_cast<unsigned char>(std::clamp(arguments.size() > 3 ? arguments[3].Number() : 255.0,0.0,255.0))
                }),{}};
            }
            if(normalized == "cpp") {
                if(arguments.empty()) return {};
                const std::string native_name = arguments.front().String();
                arguments.erase(arguments.begin());
                return {runtime_.registry->Invoke(native_name,arguments,event),{}};
            }
            return {runtime_.InvokeFunction(identifier,arguments,event,scope_),{}};
        }

        UIXBinding binding = ResolveScopedRoot(*runtime_.registry,scope_,identifier);
        return {binding.Get(),binding};
    }

    UIXRuntime::Impl& runtime_;
    ExprLexer lexer_;
    ExprToken current_;
    std::shared_ptr<UIXScope> scope_;
    UIXEvent* event_ = nullptr;
};

std::string StripExpressionBraces(const std::string& raw) {
    const std::string value = Trim(raw);
    if(value.size() >= 2 && value.front() == '{' && value.back() == '}') {
        return Trim(value.substr(1,value.size()-2));
    }
    return value;
}

bool IsWholeExpression(const std::string& raw) {
    const std::string value = Trim(raw);
    if(value.size() < 2 || value.front() != '{') return false;
    int depth = 0;
    char quote = '\0';
    for(std::size_t index=0; index<value.size(); ++index) {
        const char character = value[index];
        if(quote != '\0') {
            if(character == '\\') ++index;
            else if(character == quote) quote = '\0';
            continue;
        }
        if(character == '"' || character == '\'') quote = character;
        else if(character == '{') ++depth;
        else if(character == '}' && --depth == 0) return index == value.size()-1;
    }
    return false;
}

std::vector<std::string> SplitStatements(const std::string& source) {
    std::vector<std::string> statements;
    std::string current;
    int parentheses = 0;
    int brackets = 0;
    int braces = 0;
    char quote = '\0';
    for(std::size_t index=0; index<source.size(); ++index) {
        const char character = source[index];
        if(quote != '\0') {
            current.push_back(character);
            if(character == '\\' && index+1 < source.size()) current.push_back(source[++index]);
            else if(character == quote) quote = '\0';
            continue;
        }
        if(character == '"' || character == '\'') quote = character;
        else if(character == '(') ++parentheses;
        else if(character == ')') --parentheses;
        else if(character == '[') ++brackets;
        else if(character == ']') --brackets;
        else if(character == '{') ++braces;
        else if(character == '}') --braces;
        if((character == ';' || character == '\n') && parentheses == 0 && brackets == 0 && braces == 0) {
            if(!Trim(current).empty()) statements.push_back(Trim(std::move(current)));
            current.clear();
        }
        else {
            current.push_back(character);
        }
    }
    if(!Trim(current).empty()) statements.push_back(Trim(std::move(current)));
    return statements;
}

struct AssignmentMatch {
    std::string left;
    std::string operation;
    std::string right;
};

std::optional<AssignmentMatch> FindAssignment(const std::string& statement) {
    int parentheses = 0;
    int brackets = 0;
    char quote = '\0';
    for(std::size_t index=0; index<statement.size(); ++index) {
        const char character = statement[index];
        if(quote != '\0') {
            if(character == '\\') ++index;
            else if(character == quote) quote = '\0';
            continue;
        }
        if(character == '"' || character == '\'') { quote = character; continue; }
        if(character == '(') ++parentheses;
        else if(character == ')') --parentheses;
        else if(character == '[') ++brackets;
        else if(character == ']') --brackets;
        if(parentheses != 0 || brackets != 0) continue;
        if(character != '=') continue;
        const char before = index > 0 ? statement[index-1] : '\0';
        const char after = index+1 < statement.size() ? statement[index+1] : '\0';
        if(before == '=' || before == '!' || before == '<' || before == '>' || after == '=') continue;
        std::string operation = "=";
        std::size_t left_end = index;
        if(before == '+' || before == '-' || before == '*' || before == '/' || before == '%') {
            operation = std::string(1,before)+"=";
            --left_end;
        }
        return AssignmentMatch{
            Trim(statement.substr(0,left_end)),
            std::move(operation),
            Trim(statement.substr(index+1))
        };
    }
    return std::nullopt;
}

} // namespace

EvalResult UIXRuntime::Impl::EvaluateDetailed(
    const std::string& expression,
    const std::shared_ptr<UIXScope>& scope,
    UIXEvent* event
) {
    const std::string stripped = StripExpressionBraces(expression);
    if(stripped.empty()) return {};
    return ExpressionParser(*this,stripped,scope,event).Parse();
}

bool UIXRuntime::Impl::ExecuteStatements(
    const std::string& statements,
    const std::shared_ptr<UIXScope>& scope,
    UIXEvent& event
) {
    bool success = true;
    for(std::string statement : SplitStatements(statements)) {
        statement = Trim(std::move(statement));
        if(statement.empty()) continue;
        if(statement.rfind("return ",0) == 0) statement = Trim(statement.substr(7));
        if(statement.size() > 2 && statement.substr(statement.size()-2) == "++") {
            EvalResult target = EvaluateDetailed(statement.substr(0,statement.size()-2),scope,&event);
            success = target.binding.Set(UIXValue(target.value.Number()+1.0)) && success;
            continue;
        }
        if(statement.size() > 2 && statement.substr(statement.size()-2) == "--") {
            EvalResult target = EvaluateDetailed(statement.substr(0,statement.size()-2),scope,&event);
            success = target.binding.Set(UIXValue(target.value.Number()-1.0)) && success;
            continue;
        }
        if(auto assignment = FindAssignment(statement)) {
            EvalResult target = EvaluateDetailed(assignment->left,scope,&event);
            EvalResult incoming = EvaluateDetailed(assignment->right,scope,&event);
            UIXValue value = incoming.value;
            if(assignment->operation == "+=") {
                value = std::holds_alternative<std::string>(target.value.StorageValue()) ||
                    std::holds_alternative<std::string>(incoming.value.StorageValue())
                    ? UIXValue(target.value.String()+incoming.value.String())
                    : UIXValue(target.value.Number()+incoming.value.Number());
            }
            else if(assignment->operation == "-=") value = UIXValue(target.value.Number()-incoming.value.Number());
            else if(assignment->operation == "*=") value = UIXValue(target.value.Number()*incoming.value.Number());
            else if(assignment->operation == "/=") value = UIXValue(incoming.value.Number() == 0.0 ? 0.0 : target.value.Number()/incoming.value.Number());
            else if(assignment->operation == "%=") value = UIXValue(incoming.value.Number() == 0.0 ? 0.0 : std::fmod(target.value.Number(),incoming.value.Number()));
            success = target.binding.Set(value) && success;
        }
        else {
            EvaluateDetailed(statement,scope,&event);
        }
    }
    return success;
}

namespace {

struct EvaluatedAttribute {
    bool present = false;
    UIXValue value;
    UIXBinding binding;
    std::string raw;
};

std::string Interpolate(
    UIXRuntime::Impl& runtime,
    const std::string& raw,
    const std::shared_ptr<UIXScope>& scope,
    UIXEvent* event = nullptr
) {
    std::string output;
    std::size_t position = 0;
    while(position < raw.size()) {
        const std::size_t opening = raw.find('{',position);
        if(opening == std::string::npos) {
            output += raw.substr(position);
            break;
        }
        output += raw.substr(position,opening-position);
        int depth = 1;
        char quote = '\0';
        std::size_t closing = opening+1;
        for(; closing < raw.size(); ++closing) {
            const char character = raw[closing];
            if(quote != '\0') {
                if(character == '\\') ++closing;
                else if(character == quote) quote = '\0';
                continue;
            }
            if(character == '"' || character == '\'') quote = character;
            else if(character == '{') ++depth;
            else if(character == '}' && --depth == 0) break;
        }
        if(closing >= raw.size()) {
            output += raw.substr(opening);
            break;
        }
        output += runtime.EvaluateDetailed(
            raw.substr(opening+1,closing-opening-1),scope,event
        ).value.String();
        position = closing+1;
    }
    return output;
}

EvaluatedAttribute EvaluateAttribute(
    UIXRuntime::Impl& runtime,
    const UIXNode& node,
    const std::string& name,
    const std::shared_ptr<UIXScope>& scope,
    UIXEvent* event = nullptr
) {
    const UIXAttribute* attribute = FindAttribute(node,name);
    if(attribute == nullptr) return {};
    EvaluatedAttribute result;
    result.present = true;
    result.raw = attribute->value;
    if(IsWholeExpression(attribute->value)) {
        EvalResult evaluated = runtime.EvaluateDetailed(attribute->value,scope,event);
        result.value = std::move(evaluated.value);
        result.binding = std::move(evaluated.binding);
    }
    else {
        result.value = UIXValue(Interpolate(runtime,attribute->value,scope,event));
    }
    return result;
}

std::string NodeText(
    UIXRuntime::Impl& runtime,
    const UIXNode& node,
    const std::shared_ptr<UIXScope>& scope
) {
    if(const UIXAttribute* text = FindAttribute(node,"text")) {
        return Interpolate(runtime,text->value,scope);
    }
    std::string result;
    for(const UIXNode& child : node.children) {
        if(child.type == UIXNodeType::Text) result += Interpolate(runtime,child.text,scope);
    }
    return Trim(std::move(result));
}

std::string AttributeString(
    UIXRuntime::Impl& runtime,
    const UIXNode& node,
    const std::string& name,
    const std::shared_ptr<UIXScope>& scope,
    std::string fallback = {}
);

double AttributeNumber(
    UIXRuntime::Impl& runtime,
    const UIXNode& node,
    const std::string& name,
    const std::shared_ptr<UIXScope>& scope,
    double fallback
);

UIXEvent MakeWidgetEvent(
    std::string type,
    const std::string& key,
    UIXValue value,
    UIXValue previous,
    Rectangle bounds,
    void* widget,
    int index = -1
);

void DispatchEvent(
    UIXRuntime::Impl& runtime,
    const std::string& handler,
    const std::shared_ptr<UIXScope>& scope,
    UIXEvent& event
);

template <typename TWidget>
void ApplyCommon(
    UIXRuntime::Impl& runtime,
    TWidget& widget,
    const UIXNode& node,
    const std::shared_ptr<UIXScope>& scope,
    const std::string& generated_key
) {
    const EvaluatedAttribute id = EvaluateAttribute(runtime,node,"id",scope);
    if(id.present && !id.value.String().empty()) widget.style.id = id.value.String();
    const EvaluatedAttribute classes = EvaluateAttribute(runtime,node,"class",scope);
    if(classes.present) {
        for(std::string css_class : SplitWords(classes.value.String())) {
            AddCSSClass(widget.style.classes,std::move(css_class));
        }
    }
    const EvaluatedAttribute key = EvaluateAttribute(runtime,node,"key",scope);
    widget.rect.key = key.present && !key.value.String().empty()
        ? key.value.String()
        : generated_key;
    const EvaluatedAttribute tooltip = EvaluateAttribute(runtime,node,"tooltip",scope);
    if(tooltip.present) widget.rect.tooltip = tooltip.value.String();

    const EvaluatedAttribute states = EvaluateAttribute(runtime,node,"states",scope);
    if(states.present) {
        for(const std::string& state : SplitWords(states.value.String())) {
            widget.rect.pseudo_states.Set(state,true);
        }
    }
    for(const UIXAttribute& attribute : node.attributes) {
        const std::string lower = Lower(attribute.name);
        std::string state_name;
        if(lower.rfind("state:",0) == 0) state_name = attribute.name.substr(6);
        else if(lower.rfind("pseudo:",0) == 0) state_name = attribute.name.substr(7);
        if(state_name.empty()) continue;
        EvaluatedAttribute state = EvaluateAttribute(runtime,node,attribute.name,scope);
        const bool initial = state.value.Truthy();
        if(IsWholeExpression(state.raw)) {
            UIXRuntime::Impl* runtime_pointer = &runtime;
            const std::string expression = state.raw;
            const std::shared_ptr<UIXScope> captured_scope = scope;
            widget.rect.pseudo_states.DefineTracked(
                state_name,
                initial,
                runtime.callback_lifetime,
                [runtime_pointer,expression,captured_scope](bool& active) {
                    active = runtime_pointer->EvaluateDetailed(
                        expression,captured_scope,nullptr
                    ).value.Truthy();
                }
            );
        }
        else {
            widget.rect.pseudo_states.Set(state_name,initial);
        }
    }

    const std::string drag_type = AttributeString(runtime,node,"dragType",scope,"");
    if(!drag_type.empty()) {
        auto source = std::make_shared<UIDragSourceBinding>();
        source->threshold = static_cast<float>(AttributeNumber(runtime,node,"dragThreshold",scope,4.0));
        source->lifetime = runtime.callback_lifetime;
        source->lifetime_tracked = true;
        const std::string drag_value_expression = RawAttribute(node,"dragValue");
        const std::string begin_handler = RawAttribute(node,"onDragBegin",RawAttribute(node,"onDragStart"));
        const std::string end_handler = RawAttribute(node,"onDragEnd");
        UIXRuntime::Impl* runtime_pointer = &runtime;
        const std::shared_ptr<UIXScope> captured_scope = scope;
        const std::string source_key = widget.rect.key;
        source->make_payload = [runtime_pointer,captured_scope,drag_type,drag_value_expression,source_key]() {
            UIDragPayload payload;
            payload.type = drag_type;
            payload.source_key = source_key;
            if(!drag_value_expression.empty()) {
                UIXValue value = runtime_pointer->EvaluateDetailed(
                    drag_value_expression,captured_scope,nullptr
                ).value;
                payload.text = value.String();
                payload.value = std::move(value);
            }
            return payload;
        };
        const std::weak_ptr<void> callback_lifetime = runtime.callback_lifetime;
        source->on_begin = [runtime_pointer,captured_scope,begin_handler,source_key,callback_lifetime]() {
            if(callback_lifetime.expired() || begin_handler.empty()) return;
            UIXEvent event = MakeWidgetEvent("dragbegin",source_key,UIXValue(true),{}, {},nullptr);
            DispatchEvent(*runtime_pointer,begin_handler,captured_scope,event);
        };
        source->on_end = [runtime_pointer,captured_scope,end_handler,source_key,callback_lifetime](bool accepted) {
            if(callback_lifetime.expired() || end_handler.empty()) return;
            UIXEvent event = MakeWidgetEvent("dragend",source_key,UIXValue(accepted),{}, {},nullptr);
            DispatchEvent(*runtime_pointer,end_handler,captured_scope,event);
        };
        widget.rect.drag_source = std::move(source);
    }

    const std::string drop_types = AttributeString(runtime,node,"dropTypes",scope,AttributeString(runtime,node,"acceptDrop",scope,""));
    const std::string drop_handler = RawAttribute(node,"onDrop");
    if(!drop_types.empty() || !drop_handler.empty()) {
        auto target = std::make_shared<UIDropTargetBinding>();
        std::set<std::string> accepted;
        std::string normalized = drop_types;
        std::replace(normalized.begin(),normalized.end(),',',' ');
        for(std::string type : SplitWords(normalized)) accepted.insert(std::move(type));
        target->lifetime = runtime.callback_lifetime;
        target->lifetime_tracked = true;
        target->accepts = [accepted = std::move(accepted)](const UIDragPayload& payload) {
            return accepted.empty() || accepted.count("*") != 0 || accepted.count(payload.type) != 0;
        };
        UIXRuntime::Impl* runtime_pointer = &runtime;
        const std::shared_ptr<UIXScope> captured_scope = scope;
        const std::string target_key = widget.rect.key;
        const std::weak_ptr<void> drop_lifetime = runtime.callback_lifetime;
        target->on_drop = [runtime_pointer,captured_scope,drop_handler,target_key,drop_lifetime](const UIDragPayload& payload) {
            if(drop_lifetime.expired() || drop_handler.empty()) return;
            auto object = std::make_shared<UIXObject>();
            object->fields["type"] = UIXBinding::Constant(payload.type);
            object->fields["source"] = UIXBinding::Constant(payload.source_key);
            object->fields["text"] = UIXBinding::Constant(payload.text);
            if(const UIXValue* value = payload.Get<UIXValue>()) {
                object->fields["value"] = UIXBinding::Constant(*value);
            }
            UIXEvent event = MakeWidgetEvent(
                "drop",target_key,UIXValue(object),{}, {},nullptr
            );
            DispatchEvent(*runtime_pointer,drop_handler,captured_scope,event);
        };
        widget.rect.drop_target = std::move(target);
    }
}

std::string GeneratedKey(
    UIXRuntime::Impl& runtime,
    const UIXNode& node,
    const std::shared_ptr<UIXScope>& scope
) {
    const EvaluatedAttribute key = EvaluateAttribute(runtime,node,"key",scope);
    if(key.present && !key.value.String().empty()) return key.value.String();
    const EvaluatedAttribute id = EvaluateAttribute(runtime,node,"id",scope);
    if(id.present && !id.value.String().empty()) return id.value.String();
    return "__uix:"+node.location.file+":"+
        std::to_string(node.location.line)+":"+
        std::to_string(node.location.column)+
        (scope ? scope->key_suffix : std::string{});
}

bool AttributeBool(
    UIXRuntime::Impl& runtime,
    const UIXNode& node,
    const std::string& name,
    const std::shared_ptr<UIXScope>& scope,
    bool fallback
) {
    EvaluatedAttribute attribute = EvaluateAttribute(runtime,node,name,scope);
    return attribute.present ? attribute.value.Truthy() : fallback;
}

double AttributeNumber(
    UIXRuntime::Impl& runtime,
    const UIXNode& node,
    const std::string& name,
    const std::shared_ptr<UIXScope>& scope,
    double fallback
) {
    EvaluatedAttribute attribute = EvaluateAttribute(runtime,node,name,scope);
    return attribute.present ? attribute.value.Number(fallback) : fallback;
}

std::string AttributeString(
    UIXRuntime::Impl& runtime,
    const UIXNode& node,
    const std::string& name,
    const std::shared_ptr<UIXScope>& scope,
    std::string fallback
) {
    EvaluatedAttribute attribute = EvaluateAttribute(runtime,node,name,scope);
    return attribute.present ? attribute.value.String() : std::move(fallback);
}

Vector2 ToVector2(const UIXValue& value, Vector2 fallback = {}) {
    if(const auto* exact = std::get_if<Vector2>(&value.StorageValue())) return *exact;
    std::string text = value.String();
    std::replace(text.begin(),text.end(),',',' ');
    std::istringstream stream(text);
    Vector2 result = fallback;
    if(stream >> result.x >> result.y) return result;
    return fallback;
}

Vector3 ToVector3(const UIXValue& value, Vector3 fallback = {}) {
    if(const auto* exact = std::get_if<Vector3>(&value.StorageValue())) return *exact;
    std::string text = value.String();
    std::replace(text.begin(),text.end(),',',' ');
    std::istringstream stream(text);
    Vector3 result = fallback;
    if(stream >> result.x >> result.y >> result.z) return result;
    return fallback;
}

Vector4 ToVector4(const UIXValue& value, Vector4 fallback = {}) {
    if(const auto* exact = std::get_if<Vector4>(&value.StorageValue())) return *exact;
    std::string text = value.String();
    std::replace(text.begin(),text.end(),',',' ');
    std::istringstream stream(text);
    Vector4 result = fallback;
    if(stream >> result.x >> result.y >> result.z >> result.w) return result;
    return fallback;
}

Rectangle ToRectangle(const UIXValue& value, Rectangle fallback = {}) {
    if(const auto* exact = std::get_if<Rectangle>(&value.StorageValue())) return *exact;
    std::string text = value.String();
    std::replace(text.begin(),text.end(),',',' ');
    std::istringstream stream(text);
    Rectangle result = fallback;
    if(stream >> result.x >> result.y >> result.width >> result.height) return result;
    return fallback;
}

Color ToColor(const UIXValue& value, Color fallback = WHITE) {
    if(const auto* exact = std::get_if<Color>(&value.StorageValue())) return *exact;
    if(auto color = ParseUIXColor(value.String())) return *color;
    return fallback;
}

void AddEventObject(
    const std::shared_ptr<UIXScope>& scope,
    UIXEvent& event
) {
    auto object = std::make_shared<UIXObject>();
    object->fields["type"] = UIXBinding::Constant(event.type);
    object->fields["id"] = UIXBinding::Constant(event.id);
    object->fields["value"] = UIXBinding::Constant(event.value);
    object->fields["previousValue"] = UIXBinding::Constant(event.previous_value);
    object->fields["index"] = UIXBinding::Constant(event.index);
    object->fields["bounds"] = UIXBinding::Constant(event.bounds);
    scope->values["$event"] = UIXBinding::Constant(UIXValue(std::move(object)));
}

void DispatchEvent(
    UIXRuntime::Impl& runtime,
    const std::string& raw_handler,
    const std::shared_ptr<UIXScope>& scope,
    UIXEvent& event
) {
    const std::string handler = Trim(raw_handler);
    if(handler.empty()) return;
    if(IsWholeExpression(handler)) {
        runtime.ExecuteStatements(StripExpressionBraces(handler),scope,event);
        return;
    }
    const bool bare_name = std::all_of(handler.begin(),handler.end(),[](unsigned char character) {
        return std::isalnum(character) || character == '_' || character == '-' || character == '$';
    });
    if(bare_name) {
        runtime.InvokeFunction(handler,{},event,scope);
    }
    else {
        runtime.ExecuteStatements(handler,scope,event);
    }
}

UIXValue StateValue(
    UIXRuntime::Impl& runtime,
    const std::string& key,
    LinkKind kind,
    const EvaluatedAttribute& attribute,
    UIXValue fallback
) {
    if(attribute.present) fallback = attribute.value;
    if(!attribute.binding.Valid()) {
        auto found = runtime.state_cache.find(LinkCacheKey(key,kind));
        if(found != runtime.state_cache.end()) return found->second;
    }
    return fallback;
}

void AddStateLink(
    UIXRuntime::Impl& runtime,
    const std::string& key,
    LinkKind kind,
    const EvaluatedAttribute& attribute,
    UIXValue rendered
) {
    runtime.links.push_back({key,kind,attribute.binding,std::move(rendered)});
}

} // namespace

UIXValue UIXRuntime::Impl::InvokeFunction(
    const std::string& name,
    const UIXArguments& arguments,
    UIXEvent& event,
    const std::shared_ptr<UIXScope>& parent_scope
) {
    if(call_depth >= 64) {
        Warning({},"Function recursion limit reached while calling '"+name+"'");
        return {};
    }
    const auto markup = document.functions.find(Lower(name));
    if(markup == document.functions.end()) {
        return registry->Invoke(name,arguments,event);
    }

    ++call_depth;
    auto scope = std::make_shared<UIXScope>();
    scope->parent = parent_scope;
    scope->key_suffix = parent_scope ? parent_scope->key_suffix : std::string{};
    for(std::size_t index=0; index<markup->second.parameters.size(); ++index) {
        scope->values[markup->second.parameters[index]] = UIXBinding::Constant(
            index < arguments.size() ? arguments[index] : UIXValue{}
        );
    }
    AddEventObject(scope,event);
    ExecuteActions(markup->second.declaration.children,scope,event);
    --call_depth;
    return {};
}

bool UIXRuntime::Impl::ExecuteActions(
    const std::vector<UIXNode>& actions,
    const std::shared_ptr<UIXScope>& scope,
    UIXEvent& event
) {
    bool success = true;
    for(const UIXNode& action : actions) {
        if(action.type == UIXNodeType::Text) {
            if(!Trim(action.text).empty()) success = ExecuteStatements(action.text,scope,event) && success;
            continue;
        }
        const std::string type = NormalName(action.name);
        if(type == "set") {
            const std::string target = StripExpressionBraces(RawAttribute(action,"target",RawAttribute(action,"path")));
            const EvaluatedAttribute value = EvaluateAttribute(*this,action,"value",scope,&event);
            EvalResult destination = EvaluateDetailed(target,scope,&event);
            success = destination.binding.Set(value.value) && success;
        }
        else if(type == "toggle") {
            const std::string target = StripExpressionBraces(RawAttribute(action,"target",RawAttribute(action,"path")));
            EvalResult destination = EvaluateDetailed(target,scope,&event);
            success = destination.binding.Set(UIXValue(!destination.value.Truthy())) && success;
        }
        else if(type == "call" || type == "emit") {
            const std::string function = AttributeString(*this,action,"name",scope);
            UIXArguments arguments;
            if(const UIXAttribute* raw_arguments = FindAttribute(action,"args")) {
                UIXValue argument_value = EvaluateDetailed(raw_arguments->value,scope,&event).value;
                if(const UIXArray* array = argument_value.AsArray()) {
                    for(const UIXBinding& binding : array->items) arguments.push_back(binding.Get());
                }
                else {
                    arguments.push_back(std::move(argument_value));
                }
            }
            for(const UIXNode& child : action.children) {
                if(child.type == UIXNodeType::Element && NormalName(child.name) == "arg") {
                    arguments.push_back(EvaluateAttribute(*this,child,"value",scope,&event).value);
                }
            }
            InvokeFunction(function,arguments,event,scope);
        }
        else if(type == "setstate" || type == "togglestate" || type == "removestate") {
            const std::string target = AttributeString(
                *this,action,"target",scope,
                AttributeString(*this,action,"id",scope,event.id)
            );
            const std::string state_name = AttributeString(*this,action,"name",scope,AttributeString(*this,action,"state",scope));
            UIElementBase* element = mounted_root != nullptr ? FindUIElementByKey(*mounted_root,target) : nullptr;
            if(element == nullptr || state_name.empty()) {
                Warning(action.location,"Unable to resolve pseudo-state target '"+target+"'");
                success = false;
            }
            else if(type == "togglestate") element->pseudo_states.Toggle(state_name);
            else if(type == "removestate") element->pseudo_states.Remove(state_name);
            else {
                const bool value = AttributeBool(*this,action,"value",scope,true);
                element->pseudo_states.Set(state_name,value);
            }
        }
        else if(
            type == "playanimation" || type == "pauseanimation" ||
            type == "resumeanimation" || type == "stopanimation" ||
            type == "restartanimation" || type == "reverseanimation"
        ) {
            const std::string target = AttributeString(
                *this,action,"target",scope,
                AttributeString(*this,action,"id",scope,event.id)
            );
            const std::string animation = AttributeString(*this,action,"name",scope,AttributeString(*this,action,"animation",scope));
            UIElementBase* element = mounted_root != nullptr ? FindUIElementByKey(*mounted_root,target) : nullptr;
            if(element == nullptr || animation.empty()) {
                Warning(action.location,"Unable to resolve animation target '"+target+"'");
                success = false;
            }
            else if(type == "playanimation") PlayUIAnimation(*element,animation,AttributeBool(*this,action,"restart",scope,true));
            else if(type == "pauseanimation") PauseUIAnimation(*element,animation);
            else if(type == "resumeanimation") ResumeUIAnimation(*element,animation);
            else if(type == "stopanimation") StopUIAnimation(*element,animation);
            else if(type == "restartanimation") RestartUIAnimation(*element,animation);
            else if(type == "reverseanimation") ReverseUIAnimation(*element,animation);
        }
        else if(type == "undo") {
            success = undo_stack.Undo() && success;
        }
        else if(type == "redo") {
            success = undo_stack.Redo() && success;
        }
        else if(type == "canceldrag") {
            GetUIDragDropContext().Cancel();
        }
        else if(type == "if") {
            if(EvaluateAttribute(*this,action,"test",scope,&event).value.Truthy()) {
                std::vector<UIXNode> selected;
                for(const UIXNode& child : action.children) {
                    if(child.type != UIXNodeType::Element || NormalName(child.name) != "else") selected.push_back(child);
                }
                success = ExecuteActions(selected,scope,event) && success;
            }
            else {
                for(const UIXNode& child : action.children) {
                    if(child.type == UIXNodeType::Element && NormalName(child.name) == "else") {
                        success = ExecuteActions(child.children,scope,event) && success;
                    }
                }
            }
        }
        else if(type != "else" && type != "arg") {
            Warning(action.location,"Unknown function action <"+action.name+">");
        }
    }
    return success;
}

namespace {

template <typename T>
T ConvertedValue(const UIXValue& value, T fallback = {}) {
    if constexpr(std::is_same_v<T,Vector2>) return ToVector2(value,fallback);
    else if constexpr(std::is_same_v<T,Vector3>) return ToVector3(value,fallback);
    else if constexpr(std::is_same_v<T,Vector4>) return ToVector4(value,fallback);
    else if constexpr(std::is_same_v<T,Color>) return ToColor(value,fallback);
    else {
        T result{};
        return value.AssignTo(result) ? result : fallback;
    }
}

template <typename TWidget>
void SetDisabled(
    UIXRuntime::Impl& runtime,
    TWidget& widget,
    const UIXNode& node,
    const std::shared_ptr<UIXScope>& scope
) {
    widget.disabled = AttributeBool(runtime,node,"disabled",scope,widget.disabled);
}

UIXEvent MakeWidgetEvent(
    std::string type,
    const std::string& key,
    UIXValue value,
    UIXValue previous,
    Rectangle bounds,
    void* widget,
    int index
) {
    UIXEvent event;
    event.type = std::move(type);
    event.id = key;
    event.value = std::move(value);
    event.previous_value = std::move(previous);
    event.bounds = bounds;
    event.native_widget = widget;
    event.index = index;
    return event;
}

template <typename T>
void ConfigureInput(
    UIXRuntime::Impl& runtime,
    UIContainer& parent,
    const UIXNode& node,
    const std::shared_ptr<UIXScope>& scope
) {
    UIInput<T> input;
    const std::string key = GeneratedKey(runtime,node,scope);
    ApplyCommon(runtime,input,node,scope,key);

    const EvaluatedAttribute value_attribute = EvaluateAttribute(runtime,node,"value",scope);
    T value{};
    if(value_attribute.present) value = ConvertedValue<T>(value_attribute.value,value);
    input.value = value;

    if constexpr(std::is_same_v<T,bool>) {
        input.input_type = InputType::Checkbox;
    }
    else if constexpr(std::is_same_v<T,Color>) {
        input.input_type = InputType::ColorPicker;
    }
    else {
        input.input_type = InputType::Instant;
    }
    const std::string input_type = AttributeString(
        runtime,node,"mode",scope,
        AttributeString(runtime,node,"type",scope,"")
    );
    if(!input_type.empty()) input.input_type = ParseInputType(input_type);

    input.text = NodeText(runtime,node,scope);
    input.placeholder = AttributeString(runtime,node,"placeholder",scope,input.placeholder);
    input.max_characters = static_cast<std::size_t>(std::max(
        0.0,AttributeNumber(runtime,node,"maxCharacters",scope,static_cast<double>(input.max_characters))
    ));
    SetDisabled(runtime,input,node,scope);

    const EvaluatedAttribute minimum = EvaluateAttribute(runtime,node,"min",scope);
    const EvaluatedAttribute maximum = EvaluateAttribute(runtime,node,"max",scope);
    const EvaluatedAttribute step = EvaluateAttribute(runtime,node,"step",scope);
    if(minimum.present) input.min = ConvertedValue<T>(minimum.value,input.min);
    if(maximum.present) input.max = ConvertedValue<T>(maximum.value,input.max);
    if(step.present) input.step = ConvertedValue<T>(step.value,input.step);
    input.show_step_buttons = AttributeBool(runtime,node,"stepButtons",scope,input.show_step_buttons);
    input.show_value_box = AttributeBool(runtime,node,"valueBox",scope,input.show_value_box);
    input.drag_speed = static_cast<float>(AttributeNumber(runtime,node,"dragSpeed",scope,input.drag_speed));
    input.slider_show_value = AttributeBool(runtime,node,"sliderShowValue",scope,input.slider_show_value);
    input.slider_value_width = static_cast<float>(AttributeNumber(runtime,node,"sliderValueWidth",scope,input.slider_value_width));
    input.slider_click_to_set = AttributeBool(runtime,node,"sliderClickToSet",scope,input.slider_click_to_set);
    const std::string step_layout = NormalName(AttributeString(runtime,node,"stepButtonLayout",scope,""));
    if(step_layout == "front") input.step_button_layout = InputStepButtonLayout::Front;
    else if(step_layout == "back") input.step_button_layout = InputStepButtonLayout::Back;
    else if(step_layout == "opposite") input.step_button_layout = InputStepButtonLayout::Opposite;

    const std::string change_handler = RawAttribute(
        node,"onChange",
        input.input_type == InputType::Trigger ? RawAttribute(node,"onClick") : std::string{}
    );
    const std::string submit_handler = RawAttribute(node,"onSubmit");
    const UIXBinding binding = value_attribute.binding;
    const UIXValue previous = UIXValue::From(value);
    UIXRuntime::Impl* runtime_pointer = &runtime;
    input.on_change = [
        runtime_pointer,binding,change_handler,scope,key,previous
    ](UIInput<T>& widget) mutable {
        const UIXValue current = UIXValue::From(widget.value);
        if(binding.Writable()) binding.Set(current);
        UIXEvent event = MakeWidgetEvent(
            widget.input_type == InputType::Trigger ? "click" : "change",
            key,current,previous,widget.rect.transform,&widget
        );
        DispatchEvent(*runtime_pointer,change_handler,scope,event);
    };
    input.on_submit = [
        runtime_pointer,binding,submit_handler,scope,key,previous
    ](UIInput<T>& widget) mutable {
        const UIXValue current = UIXValue::From(widget.value);
        if(binding.Writable()) binding.Set(current);
        UIXEvent event = MakeWidgetEvent(
            "submit",key,current,previous,widget.rect.transform,&widget
        );
        DispatchEvent(*runtime_pointer,submit_handler,scope,event);
    };

    AddStateLink(runtime,key,LinkKind::InputValue,value_attribute,UIXValue::From(value));
    parent.Add(std::move(input));
}

void RenderTextNode(
    UIXRuntime::Impl& runtime,
    UIContainer& parent,
    const UIXNode& node,
    const std::shared_ptr<UIXScope>& scope
) {
    const std::string text = Trim(Interpolate(runtime,node.text,scope));
    if(text.empty()) return;
    UILabel label;
    label.text = text;
    label.rect.key = "__uix:text:"+node.location.file+":"+
        std::to_string(node.location.line)+":"+
        std::to_string(node.location.column)+
        (scope ? scope->key_suffix : std::string{});
    parent.Add(std::move(label));
}

} // namespace

void UIXRuntime::Impl::RenderChildren(
    UIContainer& parent,
    const std::vector<UIXNode>& children,
    const std::shared_ptr<UIXScope>& scope,
    const UIXNode* slot_source,
    const std::shared_ptr<UIXScope>& slot_scope,
    int depth
) {
    for(const UIXNode& child : children) {
        RenderNode(parent,child,scope,slot_source,slot_scope,false,depth+1);
    }
}

void UIXRuntime::Impl::RenderNode(
    UIContainer& parent,
    const UIXNode& node,
    const std::shared_ptr<UIXScope>& scope,
    const UIXNode* slot_source,
    const std::shared_ptr<UIXScope>& slot_scope,
    bool ignore_directives,
    int depth
) {
    if(depth > 96) {
        Warning(node.location,"Component/render recursion limit reached");
        return;
    }
    if(node.type == UIXNodeType::Text) {
        RenderTextNode(*this,parent,node,scope);
        return;
    }

    const std::string tag = NormalName(node.name);
    if(
        tag == "function" || tag == "component" ||
        tag == "functions" || tag == "components" ||
        tag == "import" || tag == "imports"
    ) return;

    if(!ignore_directives) {
        const EvaluatedAttribute condition = EvaluateAttribute(*this,node,"if",scope);
        if(condition.present && !condition.value.Truthy()) return;

        const UIXAttribute* for_attribute = FindAttribute(node,"for");
        if(tag == "for" || for_attribute != nullptr) {
            std::string item_name = RawAttribute(node,"as","item");
            std::string index_name = RawAttribute(node,"index","index");
            std::string collection_expression = RawAttribute(node,"each");
            if(for_attribute != nullptr) {
                const std::string specification = StripExpressionBraces(for_attribute->value);
                const std::size_t in = specification.find(" in ");
                if(in != std::string::npos) {
                    item_name = Trim(specification.substr(0,in));
                    collection_expression = Trim(specification.substr(in+4));
                }
                else {
                    collection_expression = specification;
                }
            }
            const EvalResult collection = EvaluateDetailed(collection_expression,scope,nullptr);
            const UIXArray* array = collection.value.AsArray();
            if(array == nullptr) {
                Warning(node.location,"for/For expected an array expression");
                return;
            }
            for(std::size_t index=0; index<array->items.size(); ++index) {
                auto iteration = std::make_shared<UIXScope>();
                iteration->parent = scope;
                iteration->key_suffix = (scope ? scope->key_suffix : std::string{})+
                    ":"+std::to_string(index);
                iteration->values[item_name] = array->items[index];
                iteration->values[index_name] = UIXBinding::Constant(UIXValue(static_cast<int>(index)));
                if(tag == "for") {
                    RenderChildren(parent,node.children,iteration,slot_source,slot_scope,depth+1);
                }
                else {
                    RenderNode(parent,node,iteration,slot_source,slot_scope,true,depth+1);
                }
            }
            return;
        }
    }

    if(tag == "if") {
        const EvaluatedAttribute condition = EvaluateAttribute(*this,node,"test",scope);
        const bool render_true = condition.value.Truthy();
        for(const UIXNode& child : node.children) {
            const bool is_else = child.type == UIXNodeType::Element && NormalName(child.name) == "else";
            if(render_true && !is_else) RenderNode(parent,child,scope,slot_source,slot_scope,false,depth+1);
            if(!render_true && is_else) RenderChildren(parent,child.children,scope,slot_source,slot_scope,depth+1);
        }
        return;
    }
    if(tag == "else") return;
    if(tag == "slot") {
        if(slot_source != nullptr) {
            RenderChildren(
                parent,slot_source->children,
                slot_scope ? slot_scope : scope,
                nullptr,nullptr,depth+1
            );
        }
        else {
            RenderChildren(parent,node.children,scope,nullptr,nullptr,depth+1);
        }
        return;
    }
    if(tag == "fragment") {
        RenderChildren(parent,node.children,scope,slot_source,slot_scope,depth+1);
        return;
    }

    const auto component = document.components.find(Lower(node.name));
    if(component != document.components.end()) {
        auto component_scope = std::make_shared<UIXScope>();
        component_scope->parent = scope;
        component_scope->key_suffix = (scope ? scope->key_suffix : std::string{})+
            ":component:"+std::to_string(node.location.line)+":"+
            std::to_string(node.location.column);
        auto props = std::make_shared<UIXObject>();
        for(const UIXAttribute& attribute : node.attributes) {
            const std::string attribute_name = NormalName(attribute.name);
            if(attribute_name == "if" || attribute_name == "for" || attribute_name == "key") continue;
            const EvaluatedAttribute evaluated = EvaluateAttribute(*this,node,attribute.name,scope);
            UIXBinding binding = evaluated.binding.Valid()
                ? evaluated.binding
                : UIXBinding::Constant(evaluated.value);
            props->fields[attribute.name] = binding;
            component_scope->values[attribute.name] = binding;
        }
        component_scope->values["props"] = UIXBinding::Constant(UIXValue(std::move(props)));
        RenderChildren(
            parent,component->second.declaration.children,
            component_scope,&node,scope,depth+1
        );
        return;
    }

    if(tag == "ui") {
        const EvaluatedAttribute id = EvaluateAttribute(*this,node,"id",scope);
        if(id.present) parent.style.id = id.value.String();
        const EvaluatedAttribute classes = EvaluateAttribute(*this,node,"class",scope);
        if(classes.present) {
            for(const std::string& css_class : SplitWords(classes.value.String())) AddCSSClass(parent.style.classes,css_class);
        }
        const std::string direction = NormalName(AttributeString(*this,node,"direction",scope,""));
        if(direction == "row") parent.direction = FlexDirection::Row;
        else if(direction == "column") parent.direction = FlexDirection::Column;
        parent.style.flex_direction = parent.direction;
        RenderChildren(parent,node.children,scope,slot_source,slot_scope,depth+1);
        return;
    }

    if(tag == "container" || tag == "row" || tag == "column") {
        UIContainer container;
        container.direction = tag == "row" ? FlexDirection::Row : FlexDirection::Column;
        const std::string direction = NormalName(AttributeString(*this,node,"direction",scope,""));
        if(direction == "row") container.direction = FlexDirection::Row;
        else if(direction == "column") container.direction = FlexDirection::Column;
        container.style.flex_direction = container.direction;
        container.style.display = DisplayType::Flex;
        container.disabled = AttributeBool(*this,node,"disabled",scope,container.disabled);
        container.show_scrollbars = AttributeBool(*this,node,"showScrollbars",scope,container.show_scrollbars);
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,container,node,scope,key);
        RenderChildren(container,node.children,scope,slot_source,slot_scope,depth+1);
        parent.Add(std::move(container));
        return;
    }

    if(tag == "label" || tag == "text") {
        UILabel label;
        label.text = NodeText(*this,node,scope);
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,label,node,scope,key);
        parent.Add(std::move(label));
        return;
    }

    if(tag == "button") {
        UIButton button;
        button.text = NodeText(*this,node,scope);
        button.disabled = AttributeBool(*this,node,"disabled",scope,button.disabled);
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,button,node,scope,key);
        const std::string handler = RawAttribute(node,"onClick");
        UIXRuntime::Impl* runtime_pointer = this;
        button.on_click = [runtime_pointer,handler,scope,key](UIButton& widget) {
            UIXEvent event = MakeWidgetEvent(
                "click",key,UIXValue(true),{},widget.rect.transform,&widget
            );
            DispatchEvent(*runtime_pointer,handler,scope,event);
        };
        parent.Add(std::move(button));
        return;
    }

    if(
        tag == "input" || tag == "floatinput" || tag == "intinput" ||
        tag == "boolinput" || tag == "stringinput" || tag == "textinput" ||
        tag == "vector2input" || tag == "vector3input" || tag == "vector4input" ||
        tag == "colorinput"
    ) {
        std::string value_type = NormalName(AttributeString(*this,node,"valueType",scope,""));
        if(tag != "input") value_type = tag;
        if(value_type.empty()) {
            const EvaluatedAttribute value = EvaluateAttribute(*this,node,"value",scope);
            if(std::holds_alternative<std::string>(value.value.StorageValue())) value_type = "string";
            else if(std::holds_alternative<bool>(value.value.StorageValue())) value_type = "bool";
            else if(std::holds_alternative<Vector2>(value.value.StorageValue())) value_type = "vector2";
            else if(std::holds_alternative<Vector3>(value.value.StorageValue())) value_type = "vector3";
            else if(std::holds_alternative<Vector4>(value.value.StorageValue())) value_type = "vector4";
            else if(std::holds_alternative<Color>(value.value.StorageValue())) value_type = "color";
            else if(std::holds_alternative<int>(value.value.StorageValue())) value_type = "int";
            else value_type = "float";
        }
        if(value_type == "int" || value_type == "integer" || value_type == "intinput") ConfigureInput<int>(*this,parent,node,scope);
        else if(value_type == "bool" || value_type == "boolean" || value_type == "boolinput") ConfigureInput<bool>(*this,parent,node,scope);
        else if(value_type == "string" || value_type == "text" || value_type == "stringinput" || value_type == "textinput") ConfigureInput<std::string>(*this,parent,node,scope);
        else if(value_type == "vector2" || value_type == "vec2" || value_type == "vector2input") ConfigureInput<Vector2>(*this,parent,node,scope);
        else if(value_type == "vector3" || value_type == "vec3" || value_type == "vector3input") ConfigureInput<Vector3>(*this,parent,node,scope);
        else if(value_type == "vector4" || value_type == "vec4" || value_type == "vector4input") ConfigureInput<Vector4>(*this,parent,node,scope);
        else if(value_type == "color" || value_type == "colorinput") ConfigureInput<Color>(*this,parent,node,scope);
        else ConfigureInput<float>(*this,parent,node,scope);
        return;
    }

    if(tag == "selectable") {
        UISelectable selectable;
        selectable.text = NodeText(*this,node,scope);
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,selectable,node,scope,key);
        SetDisabled(*this,selectable,node,scope);
        selectable.toggle_on_click = AttributeBool(*this,node,"toggleOnClick",scope,selectable.toggle_on_click);
        const EvaluatedAttribute selected_attribute = EvaluateAttribute(*this,node,"selected",scope);
        const UIXValue selected_value = StateValue(*this,key,LinkKind::SelectableSelected,selected_attribute,UIXValue(false));
        selectable.selected = selected_value.Truthy();
        const std::string handler = RawAttribute(node,"onClick");
        const UIXBinding binding = selected_attribute.binding;
        UIXRuntime::Impl* runtime_pointer = this;
        selectable.on_click = [runtime_pointer,binding,handler,scope,key](UISelectable& widget) {
            const UIXValue current(widget.selected);
            if(binding.Writable()) binding.Set(current);
            UIXEvent event = MakeWidgetEvent(
                "click",key,current,{},widget.rect.transform,&widget
            );
            DispatchEvent(*runtime_pointer,handler,scope,event);
        };
        AddStateLink(*this,key,LinkKind::SelectableSelected,selected_attribute,UIXValue(selectable.selected));
        parent.Add(std::move(selectable));
        return;
    }

    if(tag == "combobox" || tag == "combo" || tag == "select") {
        UIComboBox combo;
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,combo,node,scope,key);
        SetDisabled(*this,combo,node,scope);
        combo.placeholder = AttributeString(*this,node,"placeholder",scope,combo.placeholder);
        const EvaluatedAttribute items = EvaluateAttribute(*this,node,"items",scope);
        if(const UIXArray* array = items.value.AsArray()) {
            for(const UIXBinding& item : array->items) combo.items.push_back(item.Get().String());
        }
        for(const UIXNode& child : node.children) {
            if(child.type == UIXNodeType::Element && NormalName(child.name) == "option") {
                combo.items.push_back(NodeText(*this,child,scope));
            }
        }
        const EvaluatedAttribute selected_attribute = EvaluateAttribute(*this,node,"selectedIndex",scope);
        const UIXValue selected_value = StateValue(*this,key,LinkKind::ComboIndex,selected_attribute,UIXValue(-1));
        combo.selected_index = static_cast<int>(selected_value.Number(-1.0));
        combo.open = AttributeBool(*this,node,"open",scope,combo.open);
        const UIXBinding binding = selected_attribute.binding;
        const std::string handler = RawAttribute(node,"onChange");
        UIXRuntime::Impl* runtime_pointer = this;
        combo.on_change = [runtime_pointer,binding,handler,scope,key](UIComboBox& widget) {
            const UIXValue current(widget.selected_index);
            if(binding.Writable()) binding.Set(current);
            UIXEvent event = MakeWidgetEvent(
                "change",key,current,{},widget.rect.transform,&widget,widget.selected_index
            );
            DispatchEvent(*runtime_pointer,handler,scope,event);
        };
        AddStateLink(*this,key,LinkKind::ComboIndex,selected_attribute,UIXValue(combo.selected_index));
        parent.Add(std::move(combo));
        return;
    }

    if(tag == "rangeinput" || tag == "floatrange" || tag == "intrange") {
        const bool integer = tag == "intrange" || NormalName(AttributeString(*this,node,"valueType",scope,"float")) == "int";
        const std::string key = GeneratedKey(*this,node,scope);
        const EvaluatedAttribute lower_attribute = EvaluateAttribute(*this,node,"lower",scope);
        const EvaluatedAttribute upper_attribute = EvaluateAttribute(*this,node,"upper",scope);
        const EvaluatedAttribute min_attribute = EvaluateAttribute(*this,node,"min",scope);
        const EvaluatedAttribute max_attribute = EvaluateAttribute(*this,node,"max",scope);
        const std::string handler = RawAttribute(node,"onChange");
        UIXRuntime::Impl* runtime_pointer = this;
        if(integer) {
            UIRangeInput<int> range;
            range.text = NodeText(*this,node,scope);
            range.lower = static_cast<int>(StateValue(*this,key,LinkKind::RangeLower,lower_attribute,0).Number());
            range.upper = static_cast<int>(StateValue(*this,key,LinkKind::RangeUpper,upper_attribute,0).Number());
            range.min = static_cast<int>(min_attribute.value.Number());
            range.max = static_cast<int>(max_attribute.value.Number());
            SetDisabled(*this,range,node,scope);
            ApplyCommon(*this,range,node,scope,key);
            const UIXBinding lower_binding = lower_attribute.binding;
            const UIXBinding upper_binding = upper_attribute.binding;
            range.on_change = [runtime_pointer,lower_binding,upper_binding,handler,scope,key](UIRangeInput<int>& widget) {
                if(lower_binding.Writable()) lower_binding.Set(UIXValue(widget.lower));
                if(upper_binding.Writable()) upper_binding.Set(UIXValue(widget.upper));
                auto values = std::make_shared<UIXArray>();
                values->items.push_back(UIXBinding::Constant(UIXValue(widget.lower)));
                values->items.push_back(UIXBinding::Constant(UIXValue(widget.upper)));
                UIXEvent event = MakeWidgetEvent("change",key,UIXValue(values),{},widget.rect.transform,&widget,widget.active_handle);
                DispatchEvent(*runtime_pointer,handler,scope,event);
            };
            AddStateLink(*this,key,LinkKind::RangeLower,lower_attribute,UIXValue(range.lower));
            AddStateLink(*this,key,LinkKind::RangeUpper,upper_attribute,UIXValue(range.upper));
            parent.Add(std::move(range));
        }
        else {
            UIRangeInput<float> range;
            range.text = NodeText(*this,node,scope);
            range.lower = static_cast<float>(StateValue(*this,key,LinkKind::RangeLower,lower_attribute,0.0).Number());
            range.upper = static_cast<float>(StateValue(*this,key,LinkKind::RangeUpper,upper_attribute,0.0).Number());
            range.min = static_cast<float>(min_attribute.value.Number());
            range.max = static_cast<float>(max_attribute.value.Number());
            SetDisabled(*this,range,node,scope);
            ApplyCommon(*this,range,node,scope,key);
            const UIXBinding lower_binding = lower_attribute.binding;
            const UIXBinding upper_binding = upper_attribute.binding;
            range.on_change = [runtime_pointer,lower_binding,upper_binding,handler,scope,key](UIRangeInput<float>& widget) {
                if(lower_binding.Writable()) lower_binding.Set(UIXValue(widget.lower));
                if(upper_binding.Writable()) upper_binding.Set(UIXValue(widget.upper));
                auto values = std::make_shared<UIXArray>();
                values->items.push_back(UIXBinding::Constant(UIXValue(widget.lower)));
                values->items.push_back(UIXBinding::Constant(UIXValue(widget.upper)));
                UIXEvent event = MakeWidgetEvent("change",key,UIXValue(values),{},widget.rect.transform,&widget,widget.active_handle);
                DispatchEvent(*runtime_pointer,handler,scope,event);
            };
            AddStateLink(*this,key,LinkKind::RangeLower,lower_attribute,UIXValue(range.lower));
            AddStateLink(*this,key,LinkKind::RangeUpper,upper_attribute,UIXValue(range.upper));
            parent.Add(std::move(range));
        }
        return;
    }

    if(tag == "textbox" || tag == "richtext" || tag == "wrappedtext") {
        auto text_box = std::make_shared<UITextBox>();
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,*text_box,node,scope,key);
        const EvaluatedAttribute text_attribute = EvaluateAttribute(*this,node,"value",scope);
        text_box->text = text_attribute.present
            ? text_attribute.value.String()
            : NodeText(*this,node,scope);
        text_box->placeholder = AttributeString(*this,node,"placeholder",scope,"");
        text_box->editable = AttributeBool(*this,node,"editable",scope,tag == "textbox");
        text_box->multiline = AttributeBool(*this,node,"multiline",scope,true);
        text_box->wrap = AttributeBool(*this,node,"wrap",scope,true);
        text_box->dynamic_height = AttributeBool(*this,node,"dynamicHeight",scope,false);
        text_box->disabled = AttributeBool(*this,node,"disabled",scope,false);
        text_box->max_characters = static_cast<std::size_t>(std::max(
            0.0,AttributeNumber(*this,node,"maxCharacters",scope,65536.0)
        ));
        text_box->line_spacing = static_cast<float>(AttributeNumber(*this,node,"lineSpacing",scope,2.0));
        const std::string animation = NormalName(AttributeString(*this,node,"textAnimation",scope,"none"));
        if(animation == "typewriter") text_box->text_animation.mode = UITextAnimationMode::Typewriter;
        else if(animation == "fade" || animation == "fadecharacters") text_box->text_animation.mode = UITextAnimationMode::FadeCharacters;
        else if(animation == "wave") text_box->text_animation.mode = UITextAnimationMode::Wave;
        text_box->text_animation.duration = static_cast<float>(AttributeNumber(*this,node,"animationDuration",scope,0.8));
        text_box->text_animation.stagger = static_cast<float>(AttributeNumber(*this,node,"animationStagger",scope,0.025));
        text_box->text_animation.loop = AttributeBool(*this,node,"animationLoop",scope,false);
        if(text_box->text_animation.mode != UITextAnimationMode::None &&
           AttributeBool(*this,node,"animationPlaying",scope,true)) {
            text_box->PlayTextAnimation(true);
        }
        const UIXBinding text_binding = text_attribute.binding;
        const std::string change_handler = RawAttribute(node,"onChange");
        const std::string submit_handler = RawAttribute(node,"onSubmit");
        UIXRuntime::Impl* runtime_pointer = this;
        const std::weak_ptr<void> lifetime = callback_lifetime;
        text_box->on_change = [runtime_pointer,lifetime,text_binding,change_handler,scope,key](UITextBox& widget) {
            if(lifetime.expired()) return;
            if(text_binding.Writable()) text_binding.Set(UIXValue(widget.text));
            UIXEvent event = MakeWidgetEvent("change",key,UIXValue(widget.text),{},widget.rect.transform,&widget);
            DispatchEvent(*runtime_pointer,change_handler,scope,event);
        };
        text_box->on_submit = [runtime_pointer,lifetime,submit_handler,scope,key](UITextBox& widget) {
            if(lifetime.expired()) return;
            UIXEvent event = MakeWidgetEvent("submit",key,UIXValue(widget.text),{},widget.rect.transform,&widget);
            DispatchEvent(*runtime_pointer,submit_handler,scope,event);
        };
        parent.elements.emplace_back(std::static_pointer_cast<IUICustomElement>(std::move(text_box)));
        parent.layout_dirty = true;
        return;
    }

    if(tag == "virtuallist" || tag == "virtual-list") {
        auto list = std::make_shared<UIVirtualList>();
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,*list,node,scope,key);
        list->item_height = static_cast<float>(AttributeNumber(*this,node,"itemHeight",scope,28.0));
        list->overscan = static_cast<std::size_t>(std::max(0.0,AttributeNumber(*this,node,"overscan",scope,2.0)));
        list->disabled = AttributeBool(*this,node,"disabled",scope,false);
        const EvaluatedAttribute items = EvaluateAttribute(*this,node,"items",scope);
        if(const UIXArray* array = items.value.AsArray()) {
            list->items.reserve(array->items.size());
            for(const UIXBinding& item : array->items) list->items.push_back(item.Get().String());
        }
        else {
            for(const UIXNode& child : node.children) {
                if(child.type == UIXNodeType::Element && NormalName(child.name) == "item") {
                    list->items.push_back(NodeText(*this,child,scope));
                }
            }
        }
        const std::string handler = RawAttribute(node,"onSelect");
        UIXRuntime::Impl* runtime_pointer = this;
        const std::weak_ptr<void> lifetime = callback_lifetime;
        list->on_select = [runtime_pointer,lifetime,handler,scope,key](UIVirtualList& widget,std::size_t index) {
            if(lifetime.expired()) return;
            UIXValue value = index < widget.items.size() ? UIXValue(widget.items[index]) : UIXValue{};
            UIXEvent event = MakeWidgetEvent("select",key,std::move(value),{},widget.rect.transform,&widget,static_cast<int>(index));
            DispatchEvent(*runtime_pointer,handler,scope,event);
        };
        parent.elements.emplace_back(std::static_pointer_cast<IUICustomElement>(std::move(list)));
        parent.layout_dirty = true;
        return;
    }

    if(tag == "table" || tag == "datagrid" || tag == "data-grid") {
        auto table = std::make_shared<UITable>();
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,*table,node,scope,key);
        table->header_height = static_cast<float>(AttributeNumber(*this,node,"headerHeight",scope,30.0));
        table->row_height = static_cast<float>(AttributeNumber(*this,node,"rowHeight",scope,26.0));
        table->overscan = static_cast<std::size_t>(std::max(0.0,AttributeNumber(*this,node,"overscan",scope,2.0)));
        table->multi_select = AttributeBool(*this,node,"multiSelect",scope,false);
        for(const UIXNode& child : node.children) {
            if(child.type != UIXNodeType::Element) continue;
            const std::string child_tag = NormalName(child.name);
            if(child_tag == "column") {
                UITableColumn column;
                column.id = AttributeString(*this,child,"id",scope,AttributeString(*this,child,"key",scope,""));
                column.title = AttributeString(*this,child,"title",scope,NodeText(*this,child,scope));
                column.width = static_cast<float>(AttributeNumber(*this,child,"width",scope,140.0));
                column.min_width = static_cast<float>(AttributeNumber(*this,child,"minWidth",scope,48.0));
                column.max_width = static_cast<float>(AttributeNumber(*this,child,"maxWidth",scope,800.0));
                column.sortable = AttributeBool(*this,child,"sortable",scope,true);
                column.resizable = AttributeBool(*this,child,"resizable",scope,true);
                table->columns.push_back(std::move(column));
            }
            else if(child_tag == "row") {
                UITableRow row;
                row.key = AttributeString(*this,child,"key",scope,std::to_string(table->rows.size()));
                for(const UIXNode& cell : child.children) {
                    if(cell.type == UIXNodeType::Element && NormalName(cell.name) == "cell") {
                        row.cells.push_back(NodeText(*this,cell,scope));
                    }
                }
                if(row.cells.empty()) {
                    const std::string cells = AttributeString(*this,child,"cells",scope,"");
                    std::stringstream stream(cells);
                    std::string value;
                    while(std::getline(stream,value,'|')) row.cells.push_back(Trim(value));
                }
                table->rows.push_back(std::move(row));
            }
        }
        const EvaluatedAttribute rows_attribute = EvaluateAttribute(*this,node,"rows",scope);
        if(const UIXArray* rows = rows_attribute.value.AsArray()) {
            table->rows.clear();
            for(std::size_t row_index=0; row_index<rows->items.size(); ++row_index) {
                const UIXValue row_value = rows->items[row_index].Get();
                UITableRow row;
                row.key = std::to_string(row_index);
                if(const UIXArray* cells = row_value.AsArray()) {
                    for(const UIXBinding& cell : cells->items) row.cells.push_back(cell.Get().String());
                }
                else if(const UIXObject* object = row_value.AsObject()) {
                    auto key_field = object->fields.find("key");
                    if(key_field != object->fields.end()) row.key = key_field->second.Get().String();
                    for(const UITableColumn& column : table->columns) {
                        auto field = object->fields.find(column.id);
                        row.cells.push_back(field != object->fields.end() ? field->second.Get().String() : std::string{});
                    }
                }
                else row.cells.push_back(row_value.String());
                table->rows.push_back(std::move(row));
            }
        }
        const std::string select_handler = RawAttribute(node,"onSelect");
        const std::string sort_handler = RawAttribute(node,"onSort");
        UIXRuntime::Impl* runtime_pointer = this;
        const std::weak_ptr<void> lifetime = callback_lifetime;
        table->on_select = [runtime_pointer,lifetime,select_handler,scope,key](UITable& widget,std::size_t index) {
            if(lifetime.expired()) return;
            UIXValue value = index < widget.rows.size() ? UIXValue(widget.rows[index].key) : UIXValue{};
            UIXEvent event = MakeWidgetEvent("select",key,std::move(value),{},widget.rect.transform,&widget,static_cast<int>(index));
            DispatchEvent(*runtime_pointer,select_handler,scope,event);
        };
        table->on_sort = [runtime_pointer,lifetime,sort_handler,scope,key](UITable& widget,int column,bool ascending) {
            if(lifetime.expired()) return;
            auto object = std::make_shared<UIXObject>();
            object->fields["column"] = UIXBinding::Constant(column);
            object->fields["ascending"] = UIXBinding::Constant(ascending);
            UIXEvent event = MakeWidgetEvent("sort",key,UIXValue(object),{},widget.rect.transform,&widget,column);
            DispatchEvent(*runtime_pointer,sort_handler,scope,event);
        };
        parent.elements.emplace_back(std::static_pointer_cast<IUICustomElement>(std::move(table)));
        parent.layout_dirty = true;
        return;
    }

    if(tag == "dockspace" || tag == "dock-space") {
        auto dock = std::make_shared<UIDockSpace>();
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,*dock,node,scope,key);
        dock->tab_height = static_cast<float>(AttributeNumber(*this,node,"tabHeight",scope,28.0));
        dock->splitter_size = static_cast<float>(AttributeNumber(*this,node,"splitterSize",scope,6.0));
        dock->minimum_pane_size = static_cast<float>(AttributeNumber(*this,node,"minimumPaneSize",scope,80.0));
        for(const UIXNode& child : node.children) {
            if(child.type != UIXNodeType::Element || NormalName(child.name) != "dockpanel") continue;
            UIDockPanel panel;
            panel.id = AttributeString(*this,child,"id",scope,GeneratedKey(*this,child,scope));
            panel.title = AttributeString(*this,child,"title",scope,panel.id);
            panel.open = AttributeBool(*this,child,"open",scope,true);
            panel.closable = AttributeBool(*this,child,"closable",scope,true);
            panel.floating = AttributeBool(*this,child,"floating",scope,false);
            panel.floating_rectangle = {
                static_cast<float>(AttributeNumber(*this,child,"x",scope,80.0)),
                static_cast<float>(AttributeNumber(*this,child,"y",scope,80.0)),
                static_cast<float>(AttributeNumber(*this,child,"width",scope,420.0)),
                static_cast<float>(AttributeNumber(*this,child,"height",scope,320.0))
            };
            RenderChildren(panel.content,child.children,scope,slot_source,slot_scope,depth+1);
            dock->AddPanel(std::move(panel));
        }
        parent.elements.emplace_back(std::static_pointer_cast<IUICustomElement>(std::move(dock)));
        parent.layout_dirty = true;
        return;
    }

    if(tag == "dockpanel" || tag == "column" || tag == "row" || tag == "cell" || tag == "item") {
        // Structural child tags are consumed by their parent widgets.
        return;
    }

    if(tag == "canvas") {
        UICanvas canvas;
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,canvas,node,scope,key);
        SetDisabled(*this,canvas,node,scope);
        canvas.focusable = AttributeBool(*this,node,"focusable",scope,canvas.focusable);
        canvas.double_click_seconds = static_cast<float>(AttributeNumber(*this,node,"doubleClickSeconds",scope,canvas.double_click_seconds));
        canvas.double_click_distance = static_cast<float>(AttributeNumber(*this,node,"doubleClickDistance",scope,canvas.double_click_distance));
        const std::string update_handler = RawAttribute(node,"onUpdate");
        const std::string draw_handler = RawAttribute(node,"onDraw");
        UIXRuntime::Impl* runtime_pointer = this;
        canvas.on_update = [runtime_pointer,update_handler,scope,key](UICanvas& widget) {
            auto state = std::make_shared<UIXObject>();
            state->fields["mouse"] = UIXBinding::Constant(widget.local_mouse);
            state->fields["mouseDelta"] = UIXBinding::Constant(widget.local_mouse_delta);
            state->fields["dragDelta"] = UIXBinding::Constant(widget.drag_delta);
            state->fields["wheel"] = UIXBinding::Constant(widget.wheel);
            state->fields["clicked"] = UIXBinding::Constant(widget.clicked);
            state->fields["dragging"] = UIXBinding::Constant(widget.dragging);
            UIXEvent event = MakeWidgetEvent("update",key,UIXValue(state),{},widget.rect.transform,&widget);
            DispatchEvent(*runtime_pointer,update_handler,scope,event);
        };
        canvas.on_draw = [runtime_pointer,draw_handler,scope,key](const UICanvas& widget, Rectangle bounds) {
            UIXEvent event = MakeWidgetEvent("draw",key,{}, {},bounds,const_cast<UICanvas*>(&widget));
            DispatchEvent(*runtime_pointer,draw_handler,scope,event);
        };
        parent.Add(std::move(canvas));
        return;
    }

    if(tag == "image") {
        UIImage image;
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,image,node,scope,key);
        SetDisabled(*this,image,node,scope);
        const EvaluatedAttribute texture = EvaluateAttribute(*this,node,"texture",scope);
        if(const auto* native_texture = std::get_if<Texture2D>(&texture.value.StorageValue())) image.texture = *native_texture;
        const EvaluatedAttribute source = EvaluateAttribute(*this,node,"source",scope);
        if(source.present) {
            image.source = ToRectangle(source.value);
            image.use_source = true;
        }
        image.tint = ToColor(EvaluateAttribute(*this,node,"tint",scope).value,image.tint);
        image.fit = ParseImageFit(AttributeString(*this,node,"fit",scope,"contain"));
        image.sampling = ParseImageSampling(AttributeString(*this,node,"sampling",scope,"nearest"));
        image.flip_x = AttributeBool(*this,node,"flipX",scope,image.flip_x);
        image.flip_y = AttributeBool(*this,node,"flipY",scope,image.flip_y);
        image.zoom = static_cast<float>(AttributeNumber(*this,node,"zoom",scope,image.zoom));
        const EvaluatedAttribute pan = EvaluateAttribute(*this,node,"pan",scope);
        if(pan.present) image.pan = ToVector2(pan.value,image.pan);
        image.interactive = AttributeBool(*this,node,"interactive",scope,image.interactive);
        const std::string handler = RawAttribute(node,"onClick");
        UIXRuntime::Impl* runtime_pointer = this;
        image.on_click = [runtime_pointer,handler,scope,key](UIImage& widget) {
            UIXEvent event = MakeWidgetEvent("click",key,{}, {},widget.rect.transform,&widget);
            DispatchEvent(*runtime_pointer,handler,scope,event);
        };
        parent.Add(std::move(image));
        return;
    }

    if(tag == "splitter") {
        UISplitter splitter;
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,splitter,node,scope,key);
        SetDisabled(*this,splitter,node,scope);
        splitter.orientation = ParseSplitterOrientation(AttributeString(*this,node,"orientation",scope,"auto"));
        splitter.minimum_before = static_cast<float>(AttributeNumber(*this,node,"minimumBefore",scope,splitter.minimum_before));
        splitter.minimum_after = static_cast<float>(AttributeNumber(*this,node,"minimumAfter",scope,splitter.minimum_after));
        const std::string handler = RawAttribute(node,"onChange");
        UIXRuntime::Impl* runtime_pointer = this;
        splitter.on_change = [runtime_pointer,handler,scope,key](UISplitter& widget) {
            UIXEvent event = MakeWidgetEvent("change",key,UIXValue(widget.delta),{},widget.rect.transform,&widget);
            DispatchEvent(*runtime_pointer,handler,scope,event);
        };
        parent.Add(std::move(splitter));
        return;
    }

    if(tag == "collapsible") {
        UICollapsible collapsible;
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,collapsible,node,scope,key);
        SetDisabled(*this,collapsible,node,scope);
        collapsible.text = NodeText(*this,node,scope);
        const EvaluatedAttribute open_attribute = EvaluateAttribute(*this,node,"open",scope);
        collapsible.open = StateValue(*this,key,LinkKind::CollapsibleOpen,open_attribute,UIXValue(true)).Truthy();
        const UIXBinding binding = open_attribute.binding;
        const std::string handler = RawAttribute(node,"onClick",RawAttribute(node,"onChange"));
        UIXRuntime::Impl* runtime_pointer = this;
        collapsible.on_click = [runtime_pointer,binding,handler,scope,key](UICollapsible& widget) {
            const UIXValue current(widget.open);
            if(binding.Writable()) binding.Set(current);
            UIXEvent event = MakeWidgetEvent("change",key,current,{},widget.rect.transform,&widget);
            DispatchEvent(*runtime_pointer,handler,scope,event);
        };
        RenderChildren(collapsible.content,node.children,scope,slot_source,slot_scope,depth+1);
        AddStateLink(*this,key,LinkKind::CollapsibleOpen,open_attribute,UIXValue(collapsible.open));
        parent.Add(std::move(collapsible));
        return;
    }

    if(tag == "treenode" || tag == "tree") {
        UITreeNode tree;
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,tree,node,scope,key);
        SetDisabled(*this,tree,node,scope);
        tree.text = NodeText(*this,node,scope);
        const EvaluatedAttribute open_attribute = EvaluateAttribute(*this,node,"open",scope);
        const EvaluatedAttribute selected_attribute = EvaluateAttribute(*this,node,"selected",scope);
        tree.open = StateValue(*this,key,LinkKind::TreeOpen,open_attribute,UIXValue(true)).Truthy();
        tree.selected = StateValue(*this,key,LinkKind::TreeSelected,selected_attribute,UIXValue(false)).Truthy();
        const UIXBinding open_binding = open_attribute.binding;
        const UIXBinding selected_binding = selected_attribute.binding;
        const std::string handler = RawAttribute(node,"onClick",RawAttribute(node,"onChange"));
        UIXRuntime::Impl* runtime_pointer = this;
        tree.on_click = [runtime_pointer,open_binding,selected_binding,handler,scope,key](UITreeNode& widget) {
            if(open_binding.Writable()) open_binding.Set(UIXValue(widget.open));
            if(selected_binding.Writable()) selected_binding.Set(UIXValue(widget.selected));
            auto current = std::make_shared<UIXObject>();
            current->fields["open"] = UIXBinding::Constant(UIXValue(widget.open));
            current->fields["selected"] = UIXBinding::Constant(UIXValue(widget.selected));
            UIXEvent event = MakeWidgetEvent("change",key,UIXValue(current),{},widget.rect.transform,&widget);
            DispatchEvent(*runtime_pointer,handler,scope,event);
        };
        RenderChildren(tree.content,node.children,scope,slot_source,slot_scope,depth+1);
        AddStateLink(*this,key,LinkKind::TreeOpen,open_attribute,UIXValue(tree.open));
        AddStateLink(*this,key,LinkKind::TreeSelected,selected_attribute,UIXValue(tree.selected));
        parent.Add(std::move(tree));
        return;
    }

    if(tag == "menu") {
        UIMenu menu;
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,menu,node,scope,key);
        SetDisabled(*this,menu,node,scope);
        menu.text = NodeText(*this,node,scope);
        const EvaluatedAttribute open_attribute = EvaluateAttribute(*this,node,"open",scope);
        menu.open = StateValue(*this,key,LinkKind::MenuOpen,open_attribute,UIXValue(false)).Truthy();
        const UIXBinding binding = open_attribute.binding;
        const std::string handler = RawAttribute(node,"onClick",RawAttribute(node,"onChange"));
        UIXRuntime::Impl* runtime_pointer = this;
        menu.on_click = [runtime_pointer,binding,handler,scope,key](UIMenu& widget) {
            if(binding.Writable()) binding.Set(UIXValue(widget.open));
            UIXEvent event = MakeWidgetEvent("change",key,UIXValue(widget.open),{},widget.rect.transform,&widget);
            DispatchEvent(*runtime_pointer,handler,scope,event);
        };
        RenderChildren(menu.content,node.children,scope,slot_source,slot_scope,depth+1);
        AddStateLink(*this,key,LinkKind::MenuOpen,open_attribute,UIXValue(menu.open));
        parent.Add(std::move(menu));
        return;
    }

    if(tag == "popup" || tag == "modal") {
        UIPopup popup;
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,popup,node,scope,key);
        popup.title = AttributeString(*this,node,"title",scope,NodeText(*this,node,scope));
        const EvaluatedAttribute open_attribute = EvaluateAttribute(*this,node,"open",scope);
        popup.open = StateValue(*this,key,LinkKind::PopupOpen,open_attribute,UIXValue(false)).Truthy();
        popup.modal = tag == "modal" || AttributeBool(*this,node,"modal",scope,popup.modal);
        popup.close_on_escape = AttributeBool(*this,node,"closeOnEscape",scope,popup.close_on_escape);
        popup.close_on_outside_click = AttributeBool(*this,node,"closeOnOutsideClick",scope,popup.close_on_outside_click);
        const UIXBinding binding = open_attribute.binding;
        const std::string open_handler = RawAttribute(node,"onOpen");
        const std::string close_handler = RawAttribute(node,"onClose");
        UIXRuntime::Impl* runtime_pointer = this;
        popup.on_open = [runtime_pointer,binding,open_handler,scope,key](UIPopup& widget) {
            if(binding.Writable()) binding.Set(UIXValue(true));
            UIXEvent event = MakeWidgetEvent("open",key,UIXValue(true),UIXValue(false),widget.rect.transform,&widget);
            DispatchEvent(*runtime_pointer,open_handler,scope,event);
        };
        popup.on_close = [runtime_pointer,binding,close_handler,scope,key](UIPopup& widget) {
            if(binding.Writable()) binding.Set(UIXValue(false));
            UIXEvent event = MakeWidgetEvent("close",key,UIXValue(false),UIXValue(true),widget.rect.transform,&widget);
            DispatchEvent(*runtime_pointer,close_handler,scope,event);
        };
        RenderChildren(popup.content,node.children,scope,slot_source,slot_scope,depth+1);
        AddStateLink(*this,key,LinkKind::PopupOpen,open_attribute,UIXValue(popup.open));
        parent.Add(std::move(popup));
        return;
    }

    if(tag == "window") {
        UIWindow window;
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,window,node,scope,key);
        SetDisabled(*this,window,node,scope);
        window.title = AttributeString(*this,node,"title",scope,NodeText(*this,node,scope));
        const EvaluatedAttribute open_attribute = EvaluateAttribute(*this,node,"open",scope);
        const EvaluatedAttribute collapsed_attribute = EvaluateAttribute(*this,node,"collapsed",scope);
        window.open = StateValue(*this,key,LinkKind::WindowOpen,open_attribute,UIXValue(true)).Truthy();
        window.collapsed = StateValue(*this,key,LinkKind::WindowCollapsed,collapsed_attribute,UIXValue(false)).Truthy();
        const UIXBinding open_binding = open_attribute.binding;
        const UIXBinding collapsed_binding = collapsed_attribute.binding;
        const std::string close_handler = RawAttribute(node,"onClose");
        const std::string collapse_handler = RawAttribute(node,"onCollapse",RawAttribute(node,"onChange"));
        UIXRuntime::Impl* runtime_pointer = this;
        window.on_close = [runtime_pointer,open_binding,close_handler,scope,key](UIWindow& widget) {
            if(open_binding.Writable()) open_binding.Set(UIXValue(false));
            UIXEvent event = MakeWidgetEvent("close",key,UIXValue(false),UIXValue(true),widget.rect.transform,&widget);
            DispatchEvent(*runtime_pointer,close_handler,scope,event);
        };
        window.on_collapse = [runtime_pointer,collapsed_binding,collapse_handler,scope,key](UIWindow& widget) {
            if(collapsed_binding.Writable()) collapsed_binding.Set(UIXValue(widget.collapsed));
            UIXEvent event = MakeWidgetEvent("collapse",key,UIXValue(widget.collapsed),{},widget.rect.transform,&widget);
            DispatchEvent(*runtime_pointer,collapse_handler,scope,event);
        };
        RenderChildren(window.content,node.children,scope,slot_source,slot_scope,depth+1);
        AddStateLink(*this,key,LinkKind::WindowOpen,open_attribute,UIXValue(window.open));
        AddStateLink(*this,key,LinkKind::WindowCollapsed,collapsed_attribute,UIXValue(window.collapsed));
        parent.Add(std::move(window));
        return;
    }

    if(tag == "tabbar" || tag == "tabs") {
        UITabBar tabs;
        const std::string key = GeneratedKey(*this,node,scope);
        ApplyCommon(*this,tabs,node,scope,key);
        SetDisabled(*this,tabs,node,scope);
        const EvaluatedAttribute selected_attribute = EvaluateAttribute(*this,node,"selectedIndex",scope);
        tabs.selected_index = static_cast<int>(StateValue(*this,key,LinkKind::TabIndex,selected_attribute,UIXValue(0)).Number());

        std::function<void(const UIXNode&,const std::shared_ptr<UIXScope>&)> add_tab;
        add_tab = [&](const UIXNode& candidate, const std::shared_ptr<UIXScope>& tab_scope) {
            if(candidate.type != UIXNodeType::Element) return;
            const std::string candidate_tag = NormalName(candidate.name);
            if(candidate_tag == "if") {
                if(EvaluateAttribute(*this,candidate,"test",tab_scope).value.Truthy()) {
                    for(const UIXNode& child : candidate.children) {
                        if(child.type != UIXNodeType::Element || NormalName(child.name) != "else") add_tab(child,tab_scope);
                    }
                }
                else {
                    for(const UIXNode& child : candidate.children) {
                        if(child.type == UIXNodeType::Element && NormalName(child.name) == "else") {
                            for(const UIXNode& nested : child.children) add_tab(nested,tab_scope);
                        }
                    }
                }
                return;
            }
            const UIXAttribute* loop_attribute = FindAttribute(candidate,"for");
            if(loop_attribute != nullptr) {
                const std::string specification = StripExpressionBraces(loop_attribute->value);
                const std::size_t in = specification.find(" in ");
                if(in == std::string::npos) return;
                const std::string item_name = Trim(specification.substr(0,in));
                const EvalResult collection = EvaluateDetailed(specification.substr(in+4),tab_scope,nullptr);
                if(const UIXArray* array = collection.value.AsArray()) {
                    for(std::size_t index=0; index<array->items.size(); ++index) {
                        auto iteration = std::make_shared<UIXScope>();
                        iteration->parent = tab_scope;
                        iteration->key_suffix = tab_scope->key_suffix+":"+std::to_string(index);
                        iteration->values[item_name] = array->items[index];
                        iteration->values[RawAttribute(candidate,"index","index")] = UIXBinding::Constant(UIXValue(static_cast<int>(index)));
                        UIXNode copy = candidate;
                        copy.attributes.erase(
                            std::remove_if(copy.attributes.begin(),copy.attributes.end(),[](const UIXAttribute& attribute) {
                                return NormalName(attribute.name) == "for";
                            }),
                            copy.attributes.end()
                        );
                        add_tab(copy,iteration);
                    }
                }
                return;
            }
            if(candidate_tag != "tab") return;
            UITab tab;
            tab.id = AttributeString(*this,candidate,"id",tab_scope,GeneratedKey(*this,candidate,tab_scope));
            tab.title = AttributeString(*this,candidate,"title",tab_scope,NodeText(*this,candidate,tab_scope));
            tab.disabled = AttributeBool(*this,candidate,"disabled",tab_scope,tab.disabled);
            tab.closable = AttributeBool(*this,candidate,"closable",tab_scope,tab.closable);
            RenderChildren(tab.content,candidate.children,tab_scope,slot_source,slot_scope,depth+1);
            tabs.tabs.push_back(std::move(tab));
        };
        for(const UIXNode& child : node.children) add_tab(child,scope);

        const UIXBinding binding = selected_attribute.binding;
        const std::string handler = RawAttribute(node,"onChange");
        const std::string close_handler = RawAttribute(node,"onClose");
        UIXRuntime::Impl* runtime_pointer = this;
        tabs.on_change = [runtime_pointer,binding,handler,scope,key](UITabBar& widget) {
            const UIXValue current(widget.selected_index);
            if(binding.Writable()) binding.Set(current);
            UIXEvent event = MakeWidgetEvent("change",key,current,{},widget.rect.transform,&widget,widget.selected_index);
            DispatchEvent(*runtime_pointer,handler,scope,event);
        };
        tabs.on_close = [runtime_pointer,close_handler,scope,key](UITabBar& widget, int index) {
            UIXEvent event = MakeWidgetEvent("close",key,UIXValue(index),{},widget.rect.transform,&widget,index);
            DispatchEvent(*runtime_pointer,close_handler,scope,event);
        };
        AddStateLink(*this,key,LinkKind::TabIndex,selected_attribute,UIXValue(tabs.selected_index));
        parent.Add(std::move(tabs));
        return;
    }

    if(build_options.warn_unknown_elements) {
        Warning(node.location,"Unknown UI element <"+node.name+">");
    }
}

namespace {

UIXValue ReadLinkedState(UIElement& element, LinkKind kind) {
    return std::visit([kind](auto& widget) -> UIXValue {
        using T = std::decay_t<decltype(widget)>;
        if constexpr(IsUIInputV<T>) {
            if(kind == LinkKind::InputValue) return UIXValue::From(widget.value);
        }
        else if constexpr(std::is_same_v<T,UISelectable>) {
            if(kind == LinkKind::SelectableSelected) return UIXValue(widget.selected);
        }
        else if constexpr(std::is_same_v<T,UIComboBox>) {
            if(kind == LinkKind::ComboIndex) return UIXValue(widget.selected_index);
        }
        else if constexpr(IsUIRangeInputV<T>) {
            if(kind == LinkKind::RangeLower) return UIXValue::From(widget.lower);
            if(kind == LinkKind::RangeUpper) return UIXValue::From(widget.upper);
        }
        else if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) {
            if(widget && kind == LinkKind::CollapsibleOpen) return UIXValue(widget->open);
        }
        else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) {
            if(widget && kind == LinkKind::TreeOpen) return UIXValue(widget->open);
            if(widget && kind == LinkKind::TreeSelected) return UIXValue(widget->selected);
        }
        else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) {
            if(widget && kind == LinkKind::MenuOpen) return UIXValue(widget->open);
        }
        else if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
            if(widget && kind == LinkKind::PopupOpen) return UIXValue(widget->open);
        }
        else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) {
            if(widget && kind == LinkKind::WindowOpen) return UIXValue(widget->open);
            if(widget && kind == LinkKind::WindowCollapsed) return UIXValue(widget->collapsed);
        }
        else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
            if(widget && kind == LinkKind::TabIndex) return UIXValue(widget->selected_index);
        }
        return {};
    },element);
}

} // namespace

void UIXRuntime::Impl::SyncLinks(UIContainer& root) {
    for(const StateLink& link : links) {
        UIElement* element = FindUIElement(root,link.key);
        if(element == nullptr) continue;
        const UIXValue current = ReadLinkedState(*element,link.kind);
        if(current.IsNull() || EqualValue(current,link.rendered)) continue;
        if(link.binding.Writable()) {
            link.binding.Set(current);
        }
        else {
            state_cache[LinkCacheKey(link.key,link.kind)] = current;
        }
    }
}

UIXRuntime::UIXRuntime()
    : impl_(std::make_unique<Impl>()) {}

UIXRuntime::UIXRuntime(UIXRegistry& registry)
    : impl_(std::make_unique<Impl>(&registry)) {}

UIXRuntime::~UIXRuntime() = default;
UIXRuntime::UIXRuntime(UIXRuntime&&) noexcept = default;
UIXRuntime& UIXRuntime::operator=(UIXRuntime&&) noexcept = default;

UIXRegistry& UIXRuntime::Registry() { return *impl_->registry; }
const UIXRegistry& UIXRuntime::Registry() const { return *impl_->registry; }

bool UIXRuntime::LoadString(
    const std::string& source,
    std::string source_name
) {
    impl_->document = ParseUIX(source,source_name);
    ResolveDocumentImports(impl_->document,source_name);
    impl_->diagnostics = impl_->document.diagnostics;
    impl_->links.clear();
    impl_->state_cache.clear();
    return impl_->document.Valid();
}

bool UIXRuntime::LoadFile(const std::string& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file) {
        impl_->document = {};
        impl_->diagnostics = {{{path,1,1},"Unable to open UI markup file",true}};
        return false;
    }
    std::ostringstream source;
    source << file.rdbuf();
    return LoadString(source.str(),path);
}

bool UIXRuntime::Mount(
    const std::string& path,
    UIContainer& destination,
    const UIXBuildOptions& options
) {
    if(!LoadFile(path)) return false;
    Rebuild(destination,options);
    return Document().Valid();
}

void UIXRuntime::SetDocument(UIXDocument document) {
    impl_->document = std::move(document);
    impl_->diagnostics = impl_->document.diagnostics;
    impl_->links.clear();
    impl_->state_cache.clear();
}

const UIXDocument& UIXRuntime::Document() const { return impl_->document; }

const std::vector<UIXDiagnostic>& UIXRuntime::Diagnostics() const {
    return impl_->diagnostics;
}

void UIXRuntime::ClearRuntimeDiagnostics() {
    impl_->diagnostics = impl_->document.diagnostics;
}

void UIXRuntime::Rebuild(
    UIContainer& destination,
    const UIXBuildOptions& options
) {
    impl_->mounted_root = &destination;
    impl_->SyncLinks(destination);
    impl_->links.clear();
    impl_->build_options = options;

    UIContainer rebuilt;
    rebuilt.direction = destination.direction;
    rebuilt.style = destination.style;
    rebuilt.rect = destination.rect;
    rebuilt.disabled = destination.disabled;
    auto scope = std::make_shared<UIXScope>();
    for(const UIXNode& root : impl_->document.roots) {
        impl_->RenderNode(rebuilt,root,scope,nullptr,nullptr,false,0);
    }

    UIReconcileOptions reconcile;
    reconcile.preserve_values = false;
    reconcile.preserve_selection = false;
    reconcile.preserve_open_state = false;
    reconcile.preserve_runtime_layout = options.preserve_runtime_layout;
    reconcile.match_unkeyed_by_index = true;
    ReconcileUI(destination,std::move(rebuilt),reconcile);
}

void UIXRuntime::Update(UIContainer& destination) {
    impl_->mounted_root = &destination;
    impl_->registry->CleanupExpired();
    impl_->SyncLinks(destination);
}

UIXControl UIXRuntime::Control(const std::string& key) {
    return UIXControl(
        impl_->mounted_root != nullptr
            ? FindUIElementByKey(*impl_->mounted_root,key)
            : nullptr
    );
}

UIXControl UIXRuntime::Control(UIContainer& root, const std::string& key) {
    impl_->mounted_root = &root;
    return UIXControl(FindUIElementByKey(root,key));
}

bool UIXRuntime::SetPseudoState(const std::string& key,const std::string& state,bool active) {
    UIXControl control = Control(key);
    return control.Valid() && control.SetState(state,active);
}
bool UIXRuntime::TogglePseudoState(const std::string& key,const std::string& state) {
    UIXControl control = Control(key);
    return control.Valid() && control.ToggleState(state);
}
bool UIXRuntime::RemovePseudoState(const std::string& key,const std::string& state) {
    UIXControl control = Control(key);
    return control.Valid() && control.RemoveState(state);
}
bool UIXRuntime::HasPseudoState(const std::string& key,const std::string& state) {
    UIXControl control = Control(key);
    return control.Valid() && control.HasState(state);
}
bool UIXRuntime::PlayAnimation(const std::string& key,const std::string& animation,bool restart) {
    UIXControl control = Control(key);
    return control.Valid() && control.Play(animation,restart);
}
bool UIXRuntime::PauseAnimation(const std::string& key,const std::string& animation) {
    UIXControl control = Control(key); return control.Valid() && control.Pause(animation);
}
bool UIXRuntime::ResumeAnimation(const std::string& key,const std::string& animation) {
    UIXControl control = Control(key); return control.Valid() && control.Resume(animation);
}
bool UIXRuntime::StopAnimation(const std::string& key,const std::string& animation) {
    UIXControl control = Control(key); return control.Valid() && control.Stop(animation);
}
bool UIXRuntime::RestartAnimation(const std::string& key,const std::string& animation) {
    UIXControl control = Control(key); return control.Valid() && control.Restart(animation);
}
bool UIXRuntime::ReverseAnimation(const std::string& key,const std::string& animation) {
    UIXControl control = Control(key); return control.Valid() && control.Reverse(animation);
}
bool UIXRuntime::IsAnimationPlaying(const std::string& key,const std::string& animation) {
    UIXControl control = Control(key); return control.Valid() && control.IsPlaying(animation);
}

UIUndoStack& UIXRuntime::UndoStack() { return impl_->undo_stack; }
const UIUndoStack& UIXRuntime::UndoStack() const { return impl_->undo_stack; }
bool UIXRuntime::Undo() { return impl_->undo_stack.Undo(); }
bool UIXRuntime::Redo() { return impl_->undo_stack.Redo(); }

UIXValue UIXRuntime::Evaluate(const std::string& expression) {
    auto scope = std::make_shared<UIXScope>();
    return impl_->EvaluateDetailed(expression,scope,nullptr).value;
}

bool UIXRuntime::Execute(const std::string& statements) {
    auto scope = std::make_shared<UIXScope>();
    UIXEvent event;
    return impl_->ExecuteStatements(statements,scope,event);
}

UIXValue UIXRuntime::Invoke(
    const std::string& function,
    const UIXArguments& arguments,
    UIXEvent event
) {
    auto scope = std::make_shared<UIXScope>();
    AddEventObject(scope,event);
    return impl_->InvokeFunction(function,arguments,event,scope);
}
