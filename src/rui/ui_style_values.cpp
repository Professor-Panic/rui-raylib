#include "css_emulator.h"

#include <cerrno>
#include <cstdlib>
#include <string_view>
#include <unordered_map>

namespace {

UIStyleValueDimension DimensionForUnit(UIStyleUnit unit) {
    switch(unit) {
        case UIStyleUnit::Pixels:
        case UIStyleUnit::Percent:
        case UIStyleUnit::ViewWidth:
        case UIStyleUnit::ViewHeight:
        case UIStyleUnit::Em:
        case UIStyleUnit::RootEm:
            return UIStyleValueDimension::Length;
        case UIStyleUnit::Degrees:
            return UIStyleValueDimension::Angle;
        case UIStyleUnit::Seconds:
        case UIStyleUnit::Milliseconds:
            return UIStyleValueDimension::Time;
        default:
            return UIStyleValueDimension::Number;
    }
}

float ResolveLiteral(float value, UIStyleUnit unit, const UIStyleEvaluationContext& context) {
    switch(unit) {
        case UIStyleUnit::Percent: return context.percent_reference*value/100.0f;
        case UIStyleUnit::ViewWidth: return context.viewport_width*value/100.0f;
        case UIStyleUnit::ViewHeight: return context.viewport_height*value/100.0f;
        case UIStyleUnit::Em: return context.font_size*value;
        case UIStyleUnit::RootEm: return context.root_font_size*value;
        case UIStyleUnit::Milliseconds: return value*0.001f;
        default: return value;
    }
}

struct ParseState {
    std::string_view source;
    std::size_t cursor = 0;
    std::string error;

    void Skip() {
        while(cursor<source.size() && std::isspace(static_cast<unsigned char>(source[cursor]))) ++cursor;
    }
    bool Consume(char c) {
        Skip();
        if(cursor<source.size() && source[cursor]==c) { ++cursor; return true; }
        return false;
    }
    bool AtEnd() { Skip(); return cursor>=source.size(); }
    void Fail(std::string message) { if(error.empty()) error=std::move(message); }

    std::string ParseIdentifier() {
        Skip();
        const std::size_t begin=cursor;
        while(cursor<source.size()) {
            const unsigned char c=static_cast<unsigned char>(source[cursor]);
            if(std::isalnum(c) || source[cursor]=='-' || source[cursor]=='_') ++cursor;
            else break;
        }
        return std::string(source.substr(begin,cursor-begin));
    }

    UIStyleExpression ParseExpression() { return ParseAdditive(); }

    UIStyleExpression MakeBinary(UIStyleExpressionKind kind, UIStyleExpression a, UIStyleExpression b) {
        if(!a || !b) return {};
        auto node=std::make_shared<UIStyleExpressionNode>();
        node->kind=kind;
        node->arguments={std::move(a),std::move(b)};
        return node;
    }

    UIStyleExpression ParseAdditive() {
        auto left=ParseMultiplicative();
        while(left) {
            Skip();
            if(cursor>=source.size() || (source[cursor]!='+' && source[cursor]!='-')) break;
            const char op=source[cursor++];
            auto right=ParseMultiplicative();
            left=MakeBinary(op=='+'?UIStyleExpressionKind::Add:UIStyleExpressionKind::Subtract,std::move(left),std::move(right));
        }
        return left;
    }

    UIStyleExpression ParseMultiplicative() {
        auto left=ParseUnary();
        while(left) {
            Skip();
            if(cursor>=source.size() || (source[cursor]!='*' && source[cursor]!='/')) break;
            const char op=source[cursor++];
            auto right=ParseUnary();
            left=MakeBinary(op=='*'?UIStyleExpressionKind::Multiply:UIStyleExpressionKind::Divide,std::move(left),std::move(right));
        }
        return left;
    }

    UIStyleExpression ParseUnary() {
        Skip();
        if(cursor<source.size() && source[cursor]=='-') {
            ++cursor;
            auto child=ParseUnary();
            if(!child) return {};
            auto node=std::make_shared<UIStyleExpressionNode>();
            node->kind=UIStyleExpressionKind::Negate;
            node->arguments.push_back(std::move(child));
            return node;
        }
        if(cursor<source.size() && source[cursor]=='+') { ++cursor; return ParseUnary(); }
        return ParsePrimary();
    }

    UIStyleExpression ParseFunction(std::string name) {
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if(!Consume('(')) { Fail("Expected '(' after "+name); return {}; }
        if(name=="var") {
            Skip();
            const std::size_t begin=cursor;
            while(cursor<source.size() && source[cursor]!=',' && source[cursor]!=')') ++cursor;
            std::string variable(source.substr(begin,cursor-begin));
            auto trim=[](std::string v){
                auto first=v.find_first_not_of(" \t\r\n");
                auto last=v.find_last_not_of(" \t\r\n");
                return first==std::string::npos?std::string{}:v.substr(first,last-first+1);
            };
            variable=trim(std::move(variable));
            if(variable.rfind("--",0)!=0) { Fail("var() expects a --custom-property name"); return {}; }
            std::string fallback;
            if(Consume(',')) {
                const std::size_t fallback_begin=cursor;
                int depth=0;
                while(cursor<source.size()) {
                    const char c=source[cursor];
                    if(c=='(') ++depth;
                    else if(c==')') {
                        if(depth==0) break;
                        --depth;
                    }
                    ++cursor;
                }
                fallback=trim(std::string(source.substr(fallback_begin,cursor-fallback_begin)));
            }
            if(!Consume(')')) { Fail("Unclosed var()"); return {}; }
            auto node=std::make_shared<UIStyleExpressionNode>();
            node->kind=UIStyleExpressionKind::Variable;
            node->variable=std::move(variable);
            node->fallback=std::move(fallback);
            return node;
        }

        std::vector<UIStyleExpression> args;
        if(!Consume(')')) {
            for(;;) {
                auto arg=ParseExpression();
                if(!arg) return {};
                args.push_back(std::move(arg));
                if(Consume(')')) break;
                if(!Consume(',')) { Fail("Expected ',' or ')' in "+name+"()"); return {}; }
            }
        }

        UIStyleExpressionKind kind;
        if(name=="calc") {
            if(args.size()!=1) { Fail("calc() expects one expression"); return {}; }
            return args.front();
        }
        if(name=="min") kind=UIStyleExpressionKind::Minimum;
        else if(name=="max") kind=UIStyleExpressionKind::Maximum;
        else if(name=="clamp") kind=UIStyleExpressionKind::Clamp;
        else { Fail("Unsupported RCSS expression function '"+name+"'"); return {}; }
        if((kind==UIStyleExpressionKind::Clamp && args.size()!=3) ||
           ((kind==UIStyleExpressionKind::Minimum || kind==UIStyleExpressionKind::Maximum) && args.empty())) {
            Fail(name+"() has the wrong argument count"); return {};
        }
        auto node=std::make_shared<UIStyleExpressionNode>();
        node->kind=kind;
        node->arguments=std::move(args);
        return node;
    }

    UIStyleExpression ParsePrimary() {
        Skip();
        if(cursor>=source.size()) { Fail("Unexpected end of RCSS expression"); return {}; }
        if(Consume('(')) {
            auto value=ParseExpression();
            if(!Consume(')')) { Fail("Unclosed expression parenthesis"); return {}; }
            return value;
        }
        if(std::isalpha(static_cast<unsigned char>(source[cursor])) || source[cursor]=='_') {
            std::string name=ParseIdentifier();
            return ParseFunction(std::move(name));
        }

        const char* begin=source.data()+cursor;
        char* end=nullptr;
        errno=0;
        const float value=std::strtof(begin,&end);
        if(end==begin || errno==ERANGE) { Fail("Expected a numeric literal"); return {}; }
        cursor=static_cast<std::size_t>(end-source.data());
        const std::size_t unit_begin=cursor;
        while(cursor<source.size()) {
            const unsigned char c=static_cast<unsigned char>(source[cursor]);
            if(std::isalpha(c) || source[cursor]=='%') ++cursor;
            else break;
        }
        std::string unit_text(source.substr(unit_begin,cursor-unit_begin));
        std::transform(unit_text.begin(),unit_text.end(),unit_text.begin(),[](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        UIStyleUnit unit=UIStyleUnit::Number;
        if(unit_text.empty()) unit=UIStyleUnit::Number;
        else if(unit_text=="px") unit=UIStyleUnit::Pixels;
        else if(unit_text=="%") unit=UIStyleUnit::Percent;
        else if(unit_text=="vw") unit=UIStyleUnit::ViewWidth;
        else if(unit_text=="vh") unit=UIStyleUnit::ViewHeight;
        else if(unit_text=="em") unit=UIStyleUnit::Em;
        else if(unit_text=="rem") unit=UIStyleUnit::RootEm;
        else if(unit_text=="deg") unit=UIStyleUnit::Degrees;
        else if(unit_text=="s") unit=UIStyleUnit::Seconds;
        else if(unit_text=="ms") unit=UIStyleUnit::Milliseconds;
        else { Fail("Unsupported RCSS unit '"+unit_text+"'"); return {}; }
        auto node=std::make_shared<UIStyleExpressionNode>();
        node->kind=UIStyleExpressionKind::Literal;
        node->literal=value;
        node->unit=unit;
        return node;
    }
};

std::optional<UIEvaluatedStyleValue> EvaluateNode(
    const UIStyleExpression& expression,
    const UIStyleEvaluationContext& context,
    std::vector<std::string>& resolving,
    int depth,
    std::string* error
) {
    if(!expression) return std::nullopt;
    if(depth>64) { if(error) *error="RCSS expression recursion exceeded 64 levels"; return std::nullopt; }
    const auto fail=[&](const std::string& message)->std::optional<UIEvaluatedStyleValue>{ if(error && error->empty()) *error=message; return std::nullopt; };
    const auto& node=*expression;
    if(node.kind==UIStyleExpressionKind::Literal) {
        return UIEvaluatedStyleValue{ResolveLiteral(node.literal,node.unit,context),DimensionForUnit(node.unit)};
    }
    if(node.kind==UIStyleExpressionKind::Variable) {
        const std::string* source=nullptr;
        if(context.variables) {
            auto found=context.variables->find(node.variable);
            if(found!=context.variables->end()) source=&found->second;
        }
        std::string fallback=node.fallback;
        if(!source && !fallback.empty()) source=&fallback;
        if(!source) return fail("Undefined RCSS variable '"+node.variable+"'");
        if(std::find(resolving.begin(),resolving.end(),node.variable)!=resolving.end()) {
            std::string chain;
            for(const std::string& name:resolving) {
                if(!chain.empty()) chain += " -> ";
                chain += name;
            }
            if(!chain.empty()) chain += " -> ";
            chain += node.variable;
            return fail("Circular RCSS variable reference: "+chain);
        }
        resolving.push_back(node.variable);

        // Variables are stored as raw cascade tokens so they can be inherited and
        // changed by states. Cache their immutable parsed AST by token text rather
        // than reparsing the same --spacing value during every layout pass.
        static std::unordered_map<std::string,UIStyleExpression> expression_cache;
        UIStyleExpression parsed;
        auto cached=expression_cache.find(*source);
        if(cached!=expression_cache.end()) parsed=cached->second;
        else {
            std::string parse_error;
            parsed=ParseUIStyleExpression(*source,&parse_error);
            if(!parsed) {
                resolving.pop_back();
                return fail(parse_error.empty()?"Invalid variable expression":parse_error);
            }
            if(expression_cache.size()>=1024) expression_cache.clear();
            expression_cache.emplace(*source,parsed);
        }
        auto result=EvaluateNode(parsed,context,resolving,depth+1,error);
        resolving.pop_back();
        return result;
    }
    if(node.kind==UIStyleExpressionKind::Negate) {
        auto value=EvaluateNode(node.arguments.at(0),context,resolving,depth+1,error);
        if(!value) return std::nullopt;
        value->value=-value->value;
        return value;
    }
    if(node.kind==UIStyleExpressionKind::Add || node.kind==UIStyleExpressionKind::Subtract ||
       node.kind==UIStyleExpressionKind::Multiply || node.kind==UIStyleExpressionKind::Divide) {
        auto a=EvaluateNode(node.arguments.at(0),context,resolving,depth+1,error);
        auto b=EvaluateNode(node.arguments.at(1),context,resolving,depth+1,error);
        if(!a || !b) return std::nullopt;
        if(node.kind==UIStyleExpressionKind::Add || node.kind==UIStyleExpressionKind::Subtract) {
            if(a->dimension!=b->dimension) return fail("Cannot add/subtract incompatible RCSS value dimensions");
            return UIEvaluatedStyleValue{node.kind==UIStyleExpressionKind::Add?a->value+b->value:a->value-b->value,a->dimension};
        }
        if(node.kind==UIStyleExpressionKind::Multiply) {
            if(a->dimension==UIStyleValueDimension::Number) return UIEvaluatedStyleValue{a->value*b->value,b->dimension};
            if(b->dimension==UIStyleValueDimension::Number) return UIEvaluatedStyleValue{a->value*b->value,a->dimension};
            return fail("RCSS multiplication requires at least one unitless operand");
        }
        if(std::abs(b->value)<0.0000001f) return fail("Division by zero in RCSS expression");
        if(b->dimension!=UIStyleValueDimension::Number) return fail("RCSS division requires a unitless divisor");
        return UIEvaluatedStyleValue{a->value/b->value,a->dimension};
    }

    if(node.arguments.empty()) return fail("RCSS function has no arguments");
    std::vector<UIEvaluatedStyleValue> values;
    values.reserve(node.arguments.size());
    for(const auto& argument:node.arguments) {
        auto value=EvaluateNode(argument,context,resolving,depth+1,error);
        if(!value) return std::nullopt;
        if(!values.empty() && value->dimension!=values.front().dimension) return fail("RCSS function arguments have incompatible dimensions");
        values.push_back(*value);
    }
    if(node.kind==UIStyleExpressionKind::Minimum) {
        auto result=values.front(); for(const auto& v:values) result.value=std::min(result.value,v.value); return result;
    }
    if(node.kind==UIStyleExpressionKind::Maximum) {
        auto result=values.front(); for(const auto& v:values) result.value=std::max(result.value,v.value); return result;
    }
    if(node.kind==UIStyleExpressionKind::Clamp) {
        auto result=values[1]; result.value=std::clamp(result.value,values[0].value,values[2].value); return result;
    }
    return fail("Unsupported RCSS expression node");
}

} // namespace

UIStyleExpression ParseUIStyleExpression(const std::string& source,std::string* error) {
    ParseState state{source};
    auto expression=state.ParseExpression();
    if(expression && !state.AtEnd()) state.Fail("Unexpected trailing tokens in RCSS expression");
    if(!state.error.empty()) {
        if(error) *error=state.error;
        return {};
    }
    if(error) error->clear();
    return expression;
}

std::optional<UIEvaluatedStyleValue> EvaluateUIStyleExpression(
    const UIStyleExpression& expression,
    const UIStyleEvaluationContext& context,
    std::string* error
) {
    if(error) error->clear();
    std::vector<std::string> resolving;
    resolving.reserve(8);
    return EvaluateNode(expression,context,resolving,0,error);
}
