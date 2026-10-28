#include "css_emulator.h"
#include "css_parser.h"
#include "rlgl.h"

#include <unordered_map>
#include <vector>
#include <cmath>
#include <cstring>

namespace {

struct PooledTarget {
    RenderTexture2D target{};
    int width = 0;
    int height = 0;
    bool in_use = false;
    std::uint64_t last_frame = 0;
};

struct RenderTargetPool {
    // Heap-stable entries matter for nested effects: acquiring a child target
    // must never invalidate the raw handle currently held by a parent pass.
    std::vector<std::unique_ptr<PooledTarget>> targets;
    std::uint64_t frame = 0;

    PooledTarget* Acquire(int width,int height) {
        width=std::max(1,width);
        height=std::max(1,height);
        ++frame;
        for(auto& owned:targets) {
            PooledTarget& entry=*owned;
            if(!entry.in_use && entry.width==width && entry.height==height && entry.target.id!=0) {
                entry.in_use=true;
                entry.last_frame=frame;
                return &entry;
            }
        }
        RenderTexture2D target=LoadRenderTexture(width,height);
        if(target.id==0 || target.texture.id==0) {
            TraceLog(LOG_WARNING,"RUI effect target allocation failed (%dx%d)",width,height);
            if(target.id!=0) UnloadRenderTexture(target);
            return nullptr;
        }
        SetTextureFilter(target.texture,TEXTURE_FILTER_BILINEAR);
        auto entry=std::make_unique<PooledTarget>();
        entry->target=target;
        entry->width=width;
        entry->height=height;
        entry->in_use=true;
        entry->last_frame=frame;
        PooledTarget* result=entry.get();
        targets.push_back(std::move(entry));
        return result;
    }

    void Release(PooledTarget* target) {
        if(target) target->in_use=false;
    }

    void Trim(std::size_t maximum=24) {
        if(targets.size()<=maximum) return;
        // Reordering unique_ptrs does not move the target objects, so active
        // nested layer handles remain valid. Active entries are never trimmed.
        std::stable_sort(targets.begin(),targets.end(),[](const auto& a,const auto& b) {
            if(a->in_use!=b->in_use) return a->in_use>b->in_use;
            return a->last_frame>b->last_frame;
        });
        while(targets.size()>maximum && !targets.back()->in_use) {
            if(targets.back()->target.id!=0) UnloadRenderTexture(targets.back()->target);
            targets.pop_back();
        }
    }

    void Clear() {
        for(auto& entry:targets) if(entry->target.id!=0) UnloadRenderTexture(entry->target);
        targets.clear();
    }
};

struct CachedShader {
    Shader shader{};
    bool attempted=false;
    bool valid=false;
    std::unordered_map<std::string,int> locations;
    std::unordered_map<std::string,std::vector<float>> last_values;
};

RenderTargetPool& TargetPool() {
    static RenderTargetPool pool;
    return pool;
}

std::unordered_map<std::string,CachedShader>& ShaderMap() {
    static std::unordered_map<std::string,CachedShader> map;
    return map;
}

enum class ParsedUniformKind { Invalid, Boolean, Color, Vector, Scalar };
struct ParsedUniformValue {
    ParsedUniformKind kind=ParsedUniformKind::Invalid;
    std::array<float,4> values{};
    std::size_t count=0;
    UIStyleExpression expression;
};

std::unordered_map<std::string,ParsedUniformValue>& ParsedUniformCache() {
    static std::unordered_map<std::string,ParsedUniformValue> cache;
    return cache;
}

struct FramebufferBindingState {
    unsigned int framebuffer=0;
    int width=0;
    int height=0;
};

std::vector<FramebufferBindingState>& FramebufferStack() {
    static std::vector<FramebufferBindingState> stack;
    return stack;
}

void BeginEffectTarget(PooledTarget& target) {
    rlDrawRenderBatchActive();
    FramebufferBindingState previous{
        rlGetActiveFramebuffer(),
        rlGetFramebufferWidth(),
        rlGetFramebufferHeight()
    };
    FramebufferStack().push_back(previous);

    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlMatrixMode(RL_MODELVIEW);
    rlPushMatrix();

    rlEnableFramebuffer(target.target.id);
    rlViewport(0,0,target.width,target.height);
    rlSetFramebufferWidth(target.width);
    rlSetFramebufferHeight(target.height);
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0.0,target.width,target.height,0.0,0.0,1.0);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
}

void EndEffectTarget() {
    rlDrawRenderBatchActive();
    if(FramebufferStack().empty()) {
        rlDisableFramebuffer();
        return;
    }
    const FramebufferBindingState previous=FramebufferStack().back();
    FramebufferStack().pop_back();
    if(previous.framebuffer==0) rlDisableFramebuffer();
    else rlEnableFramebuffer(previous.framebuffer);
    rlViewport(0,0,previous.width,previous.height);
    rlSetFramebufferWidth(previous.width);
    rlSetFramebufferHeight(previous.height);

    rlMatrixMode(RL_MODELVIEW);
    rlPopMatrix();
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);
}

void ClearEffectTarget(PooledTarget& target) {
    BeginEffectTarget(target);
    ClearBackground(BLANK);
    EndEffectTarget();
}

const char* FilterFragmentShader() {
    return R"GLSL(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform int uMode;
uniform float uValue;
uniform vec2 uResolution;
out vec4 finalColor;
vec3 hueRotate(vec3 c,float a) {
    const mat3 rgb2yiq=mat3(0.299,0.587,0.114,0.596,-0.275,-0.321,0.212,-0.523,0.311);
    const mat3 yiq2rgb=mat3(1.0,0.956,0.621,1.0,-0.272,-0.647,1.0,-1.106,1.703);
    vec3 yiq=rgb2yiq*c;
    float h=atan(yiq.z,yiq.y)+a;
    float chroma=length(yiq.yz);
    return clamp(yiq2rgb*vec3(yiq.x,chroma*cos(h),chroma*sin(h)),0.0,1.0);
}
void main() {
    vec4 c=texture(texture0,fragTexCoord)*fragColor;
    if(uMode==1) c.rgb*=uValue;
    else if(uMode==2) c.rgb=(c.rgb-0.5)*uValue+0.5;
    else if(uMode==3) { float l=dot(c.rgb,vec3(0.2126,0.7152,0.0722)); c.rgb=mix(vec3(l),c.rgb,uValue); }
    else if(uMode==4) { float l=dot(c.rgb,vec3(0.2126,0.7152,0.0722)); c.rgb=mix(c.rgb,vec3(l),uValue); }
    else if(uMode==5) c.a*=uValue;
    else if(uMode==6) c.rgb=hueRotate(c.rgb,uValue);
    finalColor=c;
})GLSL";
}

const char* BlurFragmentShader() {
    return R"GLSL(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec2 uTexelStep;
uniform float uRadius;
out vec4 finalColor;
void main() {
    vec2 d=uTexelStep*max(0.0,uRadius);
    vec4 c=texture(texture0,fragTexCoord)*0.2270270270;
    c+=texture(texture0,fragTexCoord+d*1.3846153846)*0.3162162162;
    c+=texture(texture0,fragTexCoord-d*1.3846153846)*0.3162162162;
    c+=texture(texture0,fragTexCoord+d*3.2307692308)*0.0702702703;
    c+=texture(texture0,fragTexCoord-d*3.2307692308)*0.0702702703;
    finalColor=c*fragColor;
})GLSL";
}

CachedShader& GetBuiltinShader(const std::string& key,const char* fragment) {
    CachedShader& cached=ShaderMap()[key];
    if(!cached.attempted) {
        cached.attempted=true;
        cached.shader=LoadShaderFromMemory(nullptr,fragment);
        cached.valid=cached.shader.id!=0;
        if(!cached.valid) TraceLog(LOG_WARNING,"RUI built-in effect shader failed: %s",key.c_str());
    }
    return cached;
}

CachedShader& GetCustomShader(const std::string& path) {
    const std::string key="file:"+path;
    CachedShader& cached=ShaderMap()[key];
    if(!cached.attempted) {
        cached.attempted=true;
        cached.shader=LoadShader(nullptr,path.c_str());
        cached.valid=cached.shader.id!=0;
        if(!cached.valid) TraceLog(LOG_WARNING,"RUI shader disabled after load failure: %s",path.c_str());
    }
    return cached;
}

int ShaderLocation(CachedShader& shader,const std::string& name) {
    auto found=shader.locations.find(name);
    if(found!=shader.locations.end()) return found->second;
    const int location=GetShaderLocation(shader.shader,name.c_str());
    shader.locations.emplace(name,location);
    return location;
}

bool SameFloatVector(const std::vector<float>& a,const float* values,std::size_t count) {
    if(a.size()!=count) return false;
    for(std::size_t i=0;i<count;++i) if(std::abs(a[i]-values[i])>0.000001f) return false;
    return true;
}

void UploadFloats(CachedShader& shader,const std::string& name,const float* values,std::size_t count,int type,bool force=false) {
    const int location=ShaderLocation(shader,name);
    if(location<0) return;
    auto& previous=shader.last_values[name];
    if(!force && SameFloatVector(previous,values,count)) return;
    SetShaderValue(shader.shader,location,values,type);
    previous.assign(values,values+count);
}

void UploadInt(CachedShader& shader,const std::string& name,int value,bool force=false) {
    const int location=ShaderLocation(shader,name);
    if(location<0) return;
    float comparison=static_cast<float>(value);
    auto& previous=shader.last_values[name];
    if(!force && SameFloatVector(previous,&comparison,1)) return;
    SetShaderValue(shader.shader,location,&value,SHADER_UNIFORM_INT);
    previous.assign(1,comparison);
}

float EvaluateOperationValue(
    const UIStyleExpression& expression,
    const ComputedStyle& style,
    const Rectangle& bounds,
    float percent_reference
) {
    (void)bounds;
    UIStyleEvaluationContext context;
    context.percent_reference=percent_reference;
    context.viewport_width=CSS_UI_VIEWPORT_WIDTH>0.0f?CSS_UI_VIEWPORT_WIDTH:static_cast<float>(GetScreenWidth());
    context.viewport_height=CSS_UI_VIEWPORT_HEIGHT>0.0f?CSS_UI_VIEWPORT_HEIGHT:static_cast<float>(GetScreenHeight());
    context.font_size=style.font_size;
    context.root_font_size=CSS_UI_ROOT_FONT_SIZE;
    context.variables=style.custom_properties.get();
    auto evaluated=EvaluateUIStyleExpression(expression,context);
    return evaluated ? evaluated->value : 0.0f;
}

void DrawTargetTextureTo(
    const RenderTexture2D& source,
    int source_width,int source_height,
    int destination_width,int destination_height,
    Color tint=WHITE
) {
    DrawTexturePro(
        source.texture,
        {0.0f,0.0f,static_cast<float>(source_width),-static_cast<float>(source_height)},
        {0.0f,0.0f,static_cast<float>(destination_width),static_cast<float>(destination_height)},
        {0.0f,0.0f},0.0f,tint
    );
}

void DrawTargetTexture(const RenderTexture2D& source,int width,int height,Color tint=WHITE) {
    DrawTargetTextureTo(source,width,height,width,height,tint);
}


PooledTarget* CaptureCurrentBackdrop(Rectangle bounds) {
    const int width=std::max(1,static_cast<int>(std::ceil(bounds.width)));
    const int height=std::max(1,static_cast<int>(std::ceil(bounds.height)));
    PooledTarget* output=TargetPool().Acquire(width,height);
    if(!output) return nullptr;

    rlDrawRenderBatchActive();
    const unsigned int source_fbo=rlGetActiveFramebuffer();
    const int framebuffer_width=rlGetFramebufferWidth();
    const int framebuffer_height=rlGetFramebufferHeight();
    const int source_x=static_cast<int>(std::floor(bounds.x-CSS_UI_DRAW_TARGET_ORIGIN.x));
    const int source_top=static_cast<int>(std::floor(bounds.y-CSS_UI_DRAW_TARGET_ORIGIN.y));

    // Effect expansion can push a capture partly outside the current target.
    // Clear the destination first, then blit only the valid intersection so
    // negative blur padding never samples undefined framebuffer pixels.
    rlBindFramebuffer(RL_DRAW_FRAMEBUFFER,output->target.id);
    rlViewport(0,0,width,height);
    rlClearColor(0,0,0,0);
    rlClearScreenBuffers();

    const int clip_left=std::clamp(source_x,0,framebuffer_width);
    const int clip_top=std::clamp(source_top,0,framebuffer_height);
    const int clip_right=std::clamp(source_x+width,0,framebuffer_width);
    const int clip_bottom=std::clamp(source_top+height,0,framebuffer_height);
    if(clip_right>clip_left && clip_bottom>clip_top) {
        const int source_bottom_gl=framebuffer_height-clip_bottom;
        const int source_top_gl=framebuffer_height-clip_top;
        const int destination_left=clip_left-source_x;
        const int destination_right=clip_right-source_x;
        const int destination_bottom=height-(clip_bottom-source_top);
        const int destination_top=height-(clip_top-source_top);

        rlBindFramebuffer(RL_READ_FRAMEBUFFER,source_fbo);
        rlBindFramebuffer(RL_DRAW_FRAMEBUFFER,output->target.id);
        // GL_COLOR_BUFFER_BIT. rlgl exposes blitting but not this GL bit as a
        // public enum, so keep the standard constant local to this operation.
        rlBlitFramebuffer(
            clip_left,source_bottom_gl,clip_right,source_top_gl,
            destination_left,destination_bottom,destination_right,destination_top,
            0x00004000
        );
    }

    rlBindFramebuffer(RL_READ_FRAMEBUFFER,source_fbo);
    rlBindFramebuffer(RL_DRAW_FRAMEBUFFER,source_fbo);
    if(source_fbo==0) rlDisableFramebuffer();
    else rlEnableFramebuffer(source_fbo);
    rlViewport(0,0,framebuffer_width,framebuffer_height);
    return output;
}

PooledTarget* ApplySingleFilter(
    PooledTarget* input,
    const UIFilterOperation& operation,
    const ComputedStyle& style,
    Rectangle bounds,
    std::vector<PooledTarget*>& owned
) {
    if(!input) return nullptr;
    const int width=input->width,height=input->height;
    if(operation.type==UIFilterType::Blur) {
        const float radius=std::max(0.0f,EvaluateOperationValue(
            operation.value,style,bounds,std::min(bounds.width,bounds.height)
        ));
        if(radius<=0.01f) return input;
        CachedShader& shader=GetBuiltinShader("__rui_blur",BlurFragmentShader());
        if(!shader.valid) return input;

        // Large backdrop/filter blurs spend most of their time in texture
        // sampling. Blur at half/quarter resolution and upscale the result;
        // this is deliberately thresholded so small UI blur stays crisp.
        const int downsample = radius>=24.0f ? 4 : radius>=10.0f ? 2 : 1;
        const int work_width=std::max(1,(width+downsample-1)/downsample);
        const int work_height=std::max(1,(height+downsample-1)/downsample);
        PooledTarget* downsampled=nullptr;
        PooledTarget* blur_input=input;
        if(downsample>1) {
            downsampled=TargetPool().Acquire(work_width,work_height);
            if(!downsampled) return input;
            BeginEffectTarget(*downsampled);
            ClearBackground(BLANK);
            DrawTargetTextureTo(input->target,width,height,work_width,work_height);
            EndEffectTarget();
            blur_input=downsampled;
        }

        PooledTarget* horizontal=TargetPool().Acquire(work_width,work_height);
        PooledTarget* vertical=TargetPool().Acquire(work_width,work_height);
        if(!horizontal || !vertical) {
            if(horizontal) TargetPool().Release(horizontal);
            if(vertical) TargetPool().Release(vertical);
            if(downsampled) TargetPool().Release(downsampled);
            return input;
        }
        const float work_radius=radius/static_cast<float>(downsample);
        BeginEffectTarget(*horizontal); ClearBackground(BLANK); BeginShaderMode(shader.shader);
        float step[2]={1.0f/static_cast<float>(work_width),0.0f};
        UploadFloats(shader,"uTexelStep",step,2,SHADER_UNIFORM_VEC2,true);
        UploadFloats(shader,"uRadius",&work_radius,1,SHADER_UNIFORM_FLOAT,true);
        DrawTargetTexture(blur_input->target,work_width,work_height);
        EndShaderMode(); EndEffectTarget();

        BeginEffectTarget(*vertical); ClearBackground(BLANK); BeginShaderMode(shader.shader);
        step[0]=0.0f; step[1]=1.0f/static_cast<float>(work_height);
        UploadFloats(shader,"uTexelStep",step,2,SHADER_UNIFORM_VEC2,true);
        UploadFloats(shader,"uRadius",&work_radius,1,SHADER_UNIFORM_FLOAT,true);
        DrawTargetTexture(horizontal->target,work_width,work_height);
        EndShaderMode(); EndEffectTarget();
        TargetPool().Release(horizontal);
        if(downsampled) TargetPool().Release(downsampled);

        if(downsample==1) {
            owned.push_back(vertical);
            return vertical;
        }

        PooledTarget* upsampled=TargetPool().Acquire(width,height);
        if(!upsampled) {
            // Returning the smaller target is still visually usable because the
            // final composite stretches it to the paint bounds.
            owned.push_back(vertical);
            return vertical;
        }
        BeginEffectTarget(*upsampled);
        ClearBackground(BLANK);
        DrawTargetTextureTo(vertical->target,work_width,work_height,width,height);
        EndEffectTarget();
        TargetPool().Release(vertical);
        owned.push_back(upsampled);
        return upsampled;
    }

    CachedShader& shader=GetBuiltinShader("__rui_color_filter",FilterFragmentShader());
    if(!shader.valid) return input;
    PooledTarget* output=TargetPool().Acquire(width,height);
    if(!output) return input;
    owned.push_back(output);
    int mode=0;
    switch(operation.type) {
        case UIFilterType::Brightness: mode=1; break;
        case UIFilterType::Contrast: mode=2; break;
        case UIFilterType::Saturate: mode=3; break;
        case UIFilterType::Grayscale: mode=4; break;
        case UIFilterType::Opacity: mode=5; break;
        case UIFilterType::HueRotate: mode=6; break;
        case UIFilterType::Blur: break;
    }
    const float scalar_reference = operation.type==UIFilterType::Blur
        ? std::min(bounds.width,bounds.height) : 1.0f;
    float value=EvaluateOperationValue(operation.value,style,bounds,scalar_reference);
    if(operation.type==UIFilterType::HueRotate) value*=DEG2RAD;
    else if(operation.type==UIFilterType::Grayscale || operation.type==UIFilterType::Opacity) {
        value=std::clamp(value,0.0f,1.0f);
    }
    else {
        value=std::max(0.0f,value);
    }
    BeginEffectTarget(*output); ClearBackground(BLANK); BeginShaderMode(shader.shader);
    UploadInt(shader,"uMode",mode,true);
    UploadFloats(shader,"uValue",&value,1,SHADER_UNIFORM_FLOAT,true);
    float resolution[2]={static_cast<float>(width),static_cast<float>(height)};
    UploadFloats(shader,"uResolution",resolution,2,SHADER_UNIFORM_VEC2,true);
    DrawTargetTexture(input->target,width,height); EndShaderMode(); EndEffectTarget();
    return output;
}

bool ParseUniformVector(std::string text,std::vector<float>& values) {
    std::string normalized=LowerCSS(TrimCSS(text));
    for(const char* prefix:{"vec2(","vec3(","vec4("}) {
        const std::string p=prefix;
        if(normalized.rfind(p,0)==0 && normalized.back()==')') {
            text=TrimCSS(text.substr(p.size(),text.size()-p.size()-1));
            break;
        }
    }
    for(char& c:text) if(c==',' || c=='(' || c==')') c=' ';
    std::istringstream stream(text);
    float value=0.0f;
    while(stream>>value) values.push_back(value);
    stream>>std::ws;
    return stream.eof() && values.size()>=2 && values.size()<=4;
}

const ParsedUniformValue& ParseUniformValueCached(const std::string& raw) {
    auto& cache=ParsedUniformCache();
    auto found=cache.find(raw);
    if(found!=cache.end()) return found->second;

    ParsedUniformValue parsed;
    const std::string lowered=LowerCSS(TrimCSS(raw));
    if(lowered=="true" || lowered=="false") {
        parsed.kind=ParsedUniformKind::Boolean;
        parsed.values[0]=lowered=="true"?1.0f:0.0f;
        parsed.count=1;
    }
    else if(auto color=ParseCSSColor(raw)) {
        parsed.kind=ParsedUniformKind::Color;
        parsed.values={
            color->r/255.0f,color->g/255.0f,
            color->b/255.0f,color->a/255.0f
        };
        parsed.count=4;
    }
    else {
        std::vector<float> vector;
        if(ParseUniformVector(raw,vector)) {
            parsed.kind=ParsedUniformKind::Vector;
            parsed.count=vector.size();
            for(std::size_t i=0;i<vector.size();++i) parsed.values[i]=vector[i];
        }
        else {
            parsed.expression=ParseUIStyleExpression(raw);
            if(parsed.expression) {
                parsed.kind=ParsedUniformKind::Scalar;
                parsed.count=1;
            }
        }
    }

    if(cache.size()>=1024) cache.clear();
    return cache.emplace(raw,std::move(parsed)).first->second;
}

void UploadCustomUniforms(CachedShader& shader,const ComputedStyle& style,Rectangle bounds,UIElementBase& owner) {
    const float resolution[2]={bounds.width,bounds.height};
    const Rectangle owner_screen=UITransformRectangleToScreen(owner.transform);
    const float element_size[2]={owner_screen.width,owner_screen.height};
    const float screen_resolution[2]={
        CSS_UI_VIEWPORT_WIDTH>0.0f?CSS_UI_VIEWPORT_WIDTH:static_cast<float>(GetScreenWidth()),
        CSS_UI_VIEWPORT_HEIGHT>0.0f?CSS_UI_VIEWPORT_HEIGHT:static_cast<float>(GetScreenHeight())
    };
    const float mouse[2]={GetMousePosition().x,GetMousePosition().y};
    const float element_position[2]={owner_screen.x,owner_screen.y};
    const float time=static_cast<float>(GetTime());
    const float delta=GetFrameTime();
    const float opacity=style.opacity;
    const float hover=owner.pseudo_states.Active("hover")?1.0f:0.0f;
    const float active=owner.pseudo_states.Active("active")?1.0f:0.0f;
    const float focus=owner.pseudo_states.Active("focus")?1.0f:0.0f;
    const float disabled=(owner.inherited_disabled || owner.pseudo_states.Active("disabled"))?1.0f:0.0f;
    UploadFloats(shader,"uResolution",resolution,2,SHADER_UNIFORM_VEC2);
    UploadFloats(shader,"uElementSize",element_size,2,SHADER_UNIFORM_VEC2);
    UploadFloats(shader,"uScreenResolution",screen_resolution,2,SHADER_UNIFORM_VEC2);
    UploadFloats(shader,"uMouse",mouse,2,SHADER_UNIFORM_VEC2);
    UploadFloats(shader,"uElementPosition",element_position,2,SHADER_UNIFORM_VEC2);
    UploadFloats(shader,"uTime",&time,1,SHADER_UNIFORM_FLOAT,true);
    UploadFloats(shader,"uDeltaTime",&delta,1,SHADER_UNIFORM_FLOAT,true);
    UploadFloats(shader,"uOpacity",&opacity,1,SHADER_UNIFORM_FLOAT);
    UploadFloats(shader,"uHover",&hover,1,SHADER_UNIFORM_FLOAT);
    UploadFloats(shader,"uActive",&active,1,SHADER_UNIFORM_FLOAT);
    UploadFloats(shader,"uFocused",&focus,1,SHADER_UNIFORM_FLOAT);
    UploadFloats(shader,"uDisabled",&disabled,1,SHADER_UNIFORM_FLOAT);

    if(!style.shader_uniforms) return;
    static const UIStyleVariableMap empty_variables;
    const UIStyleVariableMap& variables=style.custom_properties?*style.custom_properties:empty_variables;
    for(const UIShaderUniformBinding& binding:*style.shader_uniforms) {
        std::string raw=binding.raw_value;
        // Most uniforms are literal. Avoid running the lexical var resolver on
        // them every frame; dynamic/custom-property bindings still resolve at
        // draw time so animation remains visible to the shader.
        if(raw.find("var(")!=std::string::npos) {
            if(auto resolved=ResolveCSSVariables(raw,variables)) raw=*resolved;
            else continue;
        }
        const ParsedUniformValue& parsed=ParseUniformValueCached(raw);
        switch(parsed.kind) {
            case ParsedUniformKind::Boolean: {
                const int value=parsed.values[0]!=0.0f?1:0;
                UploadInt(shader,binding.name,value);
                break;
            }
            case ParsedUniformKind::Color:
                UploadFloats(shader,binding.name,parsed.values.data(),4,SHADER_UNIFORM_VEC4);
                break;
            case ParsedUniformKind::Vector: {
                const int type=parsed.count==2?SHADER_UNIFORM_VEC2:
                    parsed.count==3?SHADER_UNIFORM_VEC3:SHADER_UNIFORM_VEC4;
                UploadFloats(shader,binding.name,parsed.values.data(),parsed.count,type);
                break;
            }
            case ParsedUniformKind::Scalar: {
                const float value=EvaluateOperationValue(
                    parsed.expression,style,bounds,std::min(bounds.width,bounds.height)
                );
                UploadFloats(shader,binding.name,&value,1,SHADER_UNIFORM_FLOAT);
                break;
            }
            case ParsedUniformKind::Invalid:
                break;
        }
    }
}

PooledTarget* ApplyShaderEffect(
    PooledTarget* input,
    const UIShaderEffect& effect,
    const ComputedStyle& style,
    Rectangle bounds,
    UIElementBase& owner,
    std::vector<PooledTarget*>& owned
) {
    if(!input) return nullptr;
    CachedShader& shader=GetCustomShader(effect.path);
    if(!shader.valid) return input;
    PooledTarget* output=TargetPool().Acquire(input->width,input->height);
    if(!output) return input;
    owned.push_back(output);
    BeginEffectTarget(*output); ClearBackground(BLANK); BeginShaderMode(shader.shader);
    UploadCustomUniforms(shader,style,bounds,owner);
    DrawTargetTexture(input->target,input->width,input->height);
    EndShaderMode(); EndEffectTarget();
    return output;
}

void EvaluateTransform(
    const ComputedStyle& style,Rectangle bounds,
    Rectangle& destination,Vector2& origin,float& rotation,
    float& skew_x_degrees,float& skew_y_degrees
) {
    destination=bounds;
    origin={0.0f,0.0f};
    rotation=0.0f;
    skew_x_degrees=0.0f;
    skew_y_degrees=0.0f;

    // Origin defaults to 50%/50% (element center) — same as before this
    // property existed. font_size is used for both the font_size and
    // root_font_size arguments here since ComputedStyle only tracks its own
    // resolved font size, not a separate document-root value; this only
    // affects the rare case of an `rem`-unit transform-origin.
    const CSSLength default_origin{50.0f,LengthUnit::Percent};
    const CSSLength& origin_x_length =
        style.transform_origin_x_length.has_value() ? *style.transform_origin_x_length : default_origin;
    const CSSLength& origin_y_length =
        style.transform_origin_y_length.has_value() ? *style.transform_origin_y_length : default_origin;
    const float origin_x = ResolveCSSLength(origin_x_length,bounds.width,style.font_size,style.font_size,nullptr);
    const float origin_y = ResolveCSSLength(origin_y_length,bounds.height,style.font_size,style.font_size,nullptr);

    if(!style.transform_operations) {
        origin={origin_x,origin_y};
        return;
    }
    float translate_x=0.0f,translate_y=0.0f,scale_x=1.0f,scale_y=1.0f;
    for(const UITransformOperation& operation:style.transform_operations->operations) {
        const float reference=(operation.type==UITransformOperationType::TranslateY)?bounds.height:bounds.width;
        const float value=EvaluateOperationValue(operation.value,style,bounds,reference);
        switch(operation.type) {
            case UITransformOperationType::TranslateX: translate_x+=value; break;
            case UITransformOperationType::TranslateY: translate_y+=value; break;
            case UITransformOperationType::ScaleX: scale_x*=value; break;
            case UITransformOperationType::ScaleY: scale_y*=value; break;
            case UITransformOperationType::Rotate: rotation+=value; break;
            case UITransformOperationType::SkewX: skew_x_degrees+=value; break;
            case UITransformOperationType::SkewY: skew_y_degrees+=value; break;
        }
    }
    destination.width*=scale_x;
    destination.height*=scale_y;
    destination.x=bounds.x+bounds.width*0.5f+translate_x;
    destination.y=bounds.y+bounds.height*0.5f+translate_y;
    // origin is expressed in DrawTexturePro's own convention: an offset from
    // destination's top-left to the pivot, in destination's (already-scaled)
    // space — so the configurable origin scales along with scale_x/scale_y,
    // same as the old hardcoded destination.width*0.5f/height*0.5f did.
    origin={(origin_x/std::max(1.0f,bounds.width))*destination.width,
            (origin_y/std::max(1.0f,bounds.height))*destination.height};
}

int RaylibBlend(UIMixBlendMode mode) {
    switch(mode) {
        case UIMixBlendMode::Additive: return BLEND_ADDITIVE;
        case UIMixBlendMode::Multiply: return BLEND_MULTIPLIED;
        case UIMixBlendMode::AddColors: return BLEND_ADD_COLORS;
        case UIMixBlendMode::SubtractColors: return BLEND_SUBTRACT_COLORS;
        case UIMixBlendMode::AlphaPremultiply: return BLEND_ALPHA_PREMULTIPLY;
        case UIMixBlendMode::Normal:
        case UIMixBlendMode::Alpha: return BLEND_ALPHA;
    }
    return BLEND_ALPHA;
}

} // namespace

bool RequiresUICompositing(const ComputedStyle& style) {
    return
        (style.filters && !style.filters->operations.empty()) ||
        (style.backdrop_filters && !style.backdrop_filters->operations.empty()) ||
        (style.shader_effects && !style.shader_effects->effects.empty()) ||
        style.mix_blend_mode != UIMixBlendMode::Normal ||
        (style.transform_operations && !style.transform_operations->operations.empty()) ||
        style.perspective.has_value();   // <-- add this
}

float UIEffectExpansion(const ComputedStyle& style,const Rectangle& bounds) {
    float expansion=0.0f;
    auto inspect=[&](const UIFilterListRef& filters) {
        if(!filters) return;
        for(const UIFilterOperation& operation:filters->operations) {
            if(operation.type!=UIFilterType::Blur) continue;
            expansion=std::max(expansion,
                std::max(0.0f,EvaluateOperationValue(operation.value,style,bounds,std::min(bounds.width,bounds.height)))*2.0f);
        }
    };
    inspect(style.filters);
    inspect(style.backdrop_filters);
    // Border strokes (DrawRectangleRoundedLinesEx) are centered on the box
    // edge, so up to half the border width paints OUTSIDE `bounds`. Without
    // this, that outward half gets cut off at the offscreen capture
    // texture's edge for any composited (transformed/filtered/blended)
    // element, making borders look clipped or vanish entirely.
    const EdgeValues border_widths=EffectiveBorderWidths(style);
    const float max_border_width=std::max(
        {border_widths.left,border_widths.right,border_widths.top,border_widths.bottom}
    );
    expansion=std::max(expansion,max_border_width*0.5f);
    return expansion;
}

Rectangle ExpandUIPaintBounds(const Rectangle& bounds,const ComputedStyle& style) {
    const float expansion=UIEffectExpansion(style,bounds);
    return {
        bounds.x-expansion,bounds.y-expansion,
        std::max(1.0f,bounds.width+expansion*2.0f),
        std::max(1.0f,bounds.height+expansion*2.0f)
    };
}
bool DrawUICompositedLayer(
    UIElementBase& owner,
    const ComputedStyle& style,
    Rectangle paint_bounds,
    const std::function<void()>& draw_subtree
) {
    if(!RequiresUICompositing(style)) return false;

    const Rectangle logical_paint_bounds =
        ExpandUIPaintBounds(paint_bounds,style);

    const UITransformContext transform =
        GetCurrentUITransform();

    const Rectangle screen_paint_bounds =
        UITransformRectangleToScreen(logical_paint_bounds);

    // ------------------------------------------------------------
    // Backdrop filters
    // ------------------------------------------------------------

    if(style.backdrop_filters &&
       !style.backdrop_filters->operations.empty()) {

        std::vector<PooledTarget*> backdrop_owned;

        if(PooledTarget* backdrop =
            CaptureCurrentBackdrop(screen_paint_bounds)) {

            backdrop_owned.push_back(backdrop);

            PooledTarget* filtered = backdrop;

            for(const UIFilterOperation& filter :
                style.backdrop_filters->operations) {

                PooledTarget* next = ApplySingleFilter(
                    filtered,
                    filter,
                    style,
                    logical_paint_bounds,
                    backdrop_owned
                );

                if(next != filtered)
                    TargetPool().Release(filtered);

                filtered = next;
            }

            DrawTexturePro(
                filtered->target.texture,
                {
                    0.0f,
                    0.0f,
                    static_cast<float>(filtered->width),
                    -static_cast<float>(filtered->height)
                },
                logical_paint_bounds,
                {0.0f,0.0f},
                0.0f,
                WHITE
            );

            for(PooledTarget* target : backdrop_owned)
                TargetPool().Release(target);
        }
    }

    // ------------------------------------------------------------
    // Render source subtree into an intermediate target
    // ------------------------------------------------------------

    const int width =
        std::max(
            1,
            static_cast<int>(
                std::ceil(screen_paint_bounds.width)
            )
        );

    const int height =
        std::max(
            1,
            static_cast<int>(
                std::ceil(screen_paint_bounds.height)
            )
        );

    PooledTarget* source =
        TargetPool().Acquire(width,height);

    if(!source)
        return false;

    std::vector<PooledTarget*> owned;
    owned.push_back(source);

    const Vector2 previous_origin =
        CSS_UI_DRAW_TARGET_ORIGIN;

    CSS_UI_DRAW_TARGET_ORIGIN = {
        screen_paint_bounds.x,
        screen_paint_bounds.y
    };

    BeginEffectTarget(*source);
    ClearBackground(BLANK);

    rlTranslatef(
        transform.offset.x - screen_paint_bounds.x,
        transform.offset.y - screen_paint_bounds.y,
        0.0f
    );

    rlScalef(
        transform.scale,
        transform.scale,
        1.0f
    );

    draw_subtree();

    EndEffectTarget();

    CSS_UI_DRAW_TARGET_ORIGIN = previous_origin;

    // ------------------------------------------------------------
    // Filters
    // ------------------------------------------------------------

    PooledTarget* current = source;

    if(style.filters) {
        for(const UIFilterOperation& filter :
            style.filters->operations) {

            PooledTarget* next = ApplySingleFilter(
                current,
                filter,
                style,
                logical_paint_bounds,
                owned
            );

            if(next != current)
                TargetPool().Release(current);

            current = next;
        }
    }

    // ------------------------------------------------------------
    // Shader effects
    // ------------------------------------------------------------

    if(style.shader_effects) {
        for(const UIShaderEffect& effect :
            style.shader_effects->effects) {

            PooledTarget* next = ApplyShaderEffect(
                current,
                effect,
                style,
                screen_paint_bounds,
                owner,
                owned
            );

            if(next != current)
                TargetPool().Release(current);

            current = next;
        }
    }

    // ------------------------------------------------------------
    // Final compositing
    // ------------------------------------------------------------
    const int blend = RaylibBlend(style.mix_blend_mode);

    BeginBlendMode(blend);

    // CSS perspective, done with raylib's own tested camera math
    // (MatrixPerspective/MatrixLookAt — the same functions BeginMode3D uses
    // internally) instead of a hand-rolled W-perturbation term folded into
    // the local transform. That approach never rendered correctly across
    // several attempts; this one leans on proven code for the actual
    // projection/view construction and keeps this function's job to just
    // positioning the quad and applying the element's own (non-perspective)
    // transform on top, same as it always did.
    //
    // Not using BeginMode3D()/EndMode3D() directly: those derive aspect
    // ratio from the current framebuffer's dimensions, which is wrong here
    // — a non-square element would get stretched to match the framebuffer's
    // aspect instead of its own. Building the matrices directly allows using
    // the element's own width/height for aspect instead.
    const bool has_perspective = style.perspective.has_value();
    float perspective_distance = 0.0f;
    if (has_perspective) {
        perspective_distance = ResolveCSSLength(
            *style.perspective,
            std::min(logical_paint_bounds.width,logical_paint_bounds.height),
            style.font_size,CSS_UI_ROOT_FONT_SIZE,style.custom_properties.get()
        );
    }

    if (has_perspective && perspective_distance > 0.0f) {
        rlDrawRenderBatchActive();

        const float center_x = logical_paint_bounds.x + logical_paint_bounds.width*0.5f;
        const float center_y = logical_paint_bounds.y + logical_paint_bounds.height*0.5f;
        const float d = perspective_distance;

        // Camera sits `d` pixels back (CSS perspective's own distance value)
        // from the element's own center and looks straight at it. up={0,-1,0}
        // because this coordinate space is Y-down (screen convention) while
        // raylib's 3D world is conventionally Y-up.
        const Vector3 eye    = {center_x,center_y,d};
        const Vector3 target = {center_x,center_y,0.0f};
        const Vector3 up     = {0.0f,-1.0f,0.0f};

        // fovy sized so the element's own box maps 1:1 (world units = pixels)
        // at the Z=0 target plane — matching every other coordinate this
        // function works in. Aspect uses the element's own box, not the
        // framebuffer's, for the reason noted above.
        const float half_height = std::max(1.0f,logical_paint_bounds.height*0.5f);
        const float fovy_radians = 2.0f*atanf(half_height/d);
        const float aspect = logical_paint_bounds.width/std::max(1.0f,logical_paint_bounds.height);
        const float far_plane = d*4.0f + 4000.0f; // generous headroom past the camera distance

        const Matrix projection = MatrixPerspective(fovy_radians,aspect,1.0f,far_plane);
        const Matrix view = MatrixLookAt(eye,target,up);

        rlMatrixMode(RL_PROJECTION);
        rlPushMatrix();
        rlLoadIdentity();
        rlMultMatrixf(MatrixToFloat(projection));

        rlMatrixMode(RL_MODELVIEW);
        rlPushMatrix();
        rlLoadIdentity();
        rlMultMatrixf(MatrixToFloat(view));

        rlTranslatef(logical_paint_bounds.x,logical_paint_bounds.y,0.0f);
        rlMultMatrixf(MatrixToFloat(style.transform_matrix));

        rlSetTexture(current->target.texture.id);
        rlBegin(RL_QUADS);
        rlColor4ub(255,255,255,255);
        {
            const float w = static_cast<float>(current->width);
            const float h = static_cast<float>(current->height);
            // Render textures are stored bottom-up in OpenGL — each vertex
            // samples from the opposite V than its screen-space position
            // would naively suggest, same reasoning as the non-perspective
            // path below.
            rlTexCoord2f(0.0f,1.0f); rlVertex3f(0.0f,0.0f,0.0f);
            rlTexCoord2f(1.0f,1.0f); rlVertex3f(w,0.0f,0.0f);
            rlTexCoord2f(1.0f,0.0f); rlVertex3f(w,h,0.0f);
            rlTexCoord2f(0.0f,0.0f); rlVertex3f(0.0f,h,0.0f);
        }
        rlEnd();
        rlSetTexture(0);

        rlDrawRenderBatchActive();
        rlMatrixMode(RL_MODELVIEW);
        rlPopMatrix();
        rlMatrixMode(RL_PROJECTION);
        rlPopMatrix();
        rlMatrixMode(RL_MODELVIEW);
    } else {
        // No perspective set: orthographic. Unchanged from before — this
        // path already handles plain 2D transforms and 3D rotation without
        // perspective correctly, so it's left exactly as it was, apart from
        // the rlDrawRenderBatchActive() call added below: the perspective
        // branch above has one right before it starts poking the matrix
        // stacks, this branch didn't — an inconsistency between the two,
        // not something either had before they were split into an if/else.
        rlDrawRenderBatchActive();

        const float viewport_width =
            CSS_UI_VIEWPORT_WIDTH>0.0f ? CSS_UI_VIEWPORT_WIDTH : static_cast<float>(GetScreenWidth());
        const float viewport_height =
            CSS_UI_VIEWPORT_HEIGHT>0.0f ? CSS_UI_VIEWPORT_HEIGHT : static_cast<float>(GetScreenHeight());

        rlMatrixMode(RL_PROJECTION);
        rlPushMatrix();
        rlLoadIdentity();
        rlOrtho(0.0,static_cast<double>(viewport_width),static_cast<double>(viewport_height),0.0,-10000.0,10000.0);

        rlMatrixMode(RL_MODELVIEW);
        rlPushMatrix();
        rlTranslatef(logical_paint_bounds.x,logical_paint_bounds.y,0.0f);
        rlMultMatrixf(MatrixToFloat(style.transform_matrix));

        rlSetTexture(current->target.texture.id);
        rlBegin(RL_QUADS);
        rlColor4ub(255,255,255,255);
        {
            const float w = static_cast<float>(current->width);
            const float h = static_cast<float>(current->height);
            rlTexCoord2f(0.0f,1.0f); rlVertex3f(0.0f,0.0f,0.0f);
            rlTexCoord2f(1.0f,1.0f); rlVertex3f(w,0.0f,0.0f);
            rlTexCoord2f(1.0f,0.0f); rlVertex3f(w,h,0.0f);
            rlTexCoord2f(0.0f,0.0f); rlVertex3f(0.0f,h,0.0f);
        }
        rlEnd();
        rlSetTexture(0);

        rlDrawRenderBatchActive();
        rlPopMatrix();
        rlMatrixMode(RL_PROJECTION);
        rlPopMatrix();
        rlMatrixMode(RL_MODELVIEW);
    }

    EndBlendMode();

    if (CSS_UI_DEBUG_COMPOSITING) {
        // Temporary, opt-in diagnostics: static analysis alone hasn't been
        // enough to pin down this bug precisely, so this prints the actual
        // runtime state instead of guessing again. Set
        // CSS_UI_DEBUG_COMPOSITING = true near startup, reproduce the
        // disappearing case, and send back the console output — it'll show
        // exactly which branch ran and what the real matrix values were,
        // which is what's actually needed to fix this with confidence
        // rather than another guess. Safe to delete once this is resolved.
        TraceLog(LOG_INFO,
            "[RUI composite] perspective=%s d=%.2f branch=%s bounds=(%.1f,%.1f %.1fx%.1f) "
            "matrix_row0=(%.3f,%.3f,%.3f,%.3f) matrix_row3=(%.3f,%.3f,%.3f,%.3f)",
            has_perspective?"yes":"no",
            static_cast<double>(perspective_distance),
            (has_perspective && perspective_distance>0.0f)?"perspective-camera":"orthographic",
            static_cast<double>(logical_paint_bounds.x),static_cast<double>(logical_paint_bounds.y),
            static_cast<double>(logical_paint_bounds.width),static_cast<double>(logical_paint_bounds.height),
            static_cast<double>(style.transform_matrix.m0),static_cast<double>(style.transform_matrix.m4),
            static_cast<double>(style.transform_matrix.m8),static_cast<double>(style.transform_matrix.m12),
            static_cast<double>(style.transform_matrix.m3),static_cast<double>(style.transform_matrix.m7),
            static_cast<double>(style.transform_matrix.m11),static_cast<double>(style.transform_matrix.m15)
        );
    }

    // ------------------------------------------------------------
    // Cleanup
    // ------------------------------------------------------------

    for(PooledTarget* target : owned)
        TargetPool().Release(target);

    TargetPool().Trim();

    owner.dirty_flags =
        static_cast<UIDirtyFlags>(
            static_cast<std::uint32_t>(owner.dirty_flags) &
            ~static_cast<std::uint32_t>(UIDirtyFlags::Composite)
        );

    return true;
}

void ClearUIEffectResources() {
    TargetPool().Clear();
    for(auto& entry:ShaderMap()) {
        if(entry.second.valid && entry.second.shader.id!=0) UnloadShader(entry.second.shader);
    }
    ShaderMap().clear();
    ParsedUniformCache().clear();
}