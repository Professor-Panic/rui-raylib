/*
This file is part of the Tweeny library.

Copyright (c) 2016-2026 Leonardo Guilherme Lucena de Freitas
Copyright (c) 2016 Guilherme R. Costa

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef TWEENY_TWEENY_H
#define TWEENY_TWEENY_H

#ifndef TWEENY_TWEEN_H
#define TWEENY_TWEEN_H

#include <vector>
#include <cstdint>
#include <cstddef>
#include <functional>

#ifndef TWEENY_DETAIL_KEY_FRAME_H
#define TWEENY_DETAIL_KEY_FRAME_H

#include <algorithm>

#ifndef TWEENY_DETAIL_VALUE_CONTAINER_H
#define TWEENY_DETAIL_VALUE_CONTAINER_H

#include <array>
#include <type_traits>

namespace tweeny::detail {
  template<typename...>
  struct all_same : std::true_type {};

  template<typename First, typename... Rest>
  struct all_same<First, Rest...> : std::conjunction<std::is_same<First, Rest>...> {};

  template<typename... Ts>
  inline constexpr bool all_same_v = all_same<Ts...>::value;

  template<typename... Ts>
  using value_container_t =
      std::conditional_t<
        all_same_v<Ts...>,
        std::array<std::tuple_element_t<0, std::tuple<Ts...>>, sizeof...(Ts)>,
        std::tuple<Ts...>
    >;
}

#endif //TWEENY_DETAIL_VALUE_CONTAINER_H

#include <cstdint>
#include <functional>
#include <tuple>

namespace tweeny::detail {
  template<typename... ValueTypes>
  struct key_frame {
    typedef value_container_t<ValueTypes...> values_t;
    typedef std::tuple<std::function<ValueTypes(float, ValueTypes, ValueTypes)>...> value_easing_functions_t;
    typedef std::array<uint32_t, sizeof...(ValueTypes)> value_tween_frame_counts_t;

    explicit key_frame(ValueTypes... vs) : position(0), values{vs...}, easing_functions(), tween_frame_counts() {}

    uint32_t position;
    values_t values;
    value_easing_functions_t easing_functions;
    value_tween_frame_counts_t tween_frame_counts;

    [[nodiscard]] uint32_t highest_frame_count() const {
      return *std::max_element(tween_frame_counts.begin(), tween_frame_counts.end());
    }
  };
}

#endif //TWEENY_DETAIL_KEY_FRAME_H

#ifndef TWEENY_DETAIL_TWEEN_VALUE_H
#define TWEENY_DETAIL_TWEEN_VALUE_H

#include <type_traits>

namespace tweeny::detail {
  template <typename First, typename... Rest>
  struct tween_value {
    using type = std::conditional_t<
      sizeof...(Rest) == 0,
      First,
      value_container_t<First, Rest...>
    >;
  };

  template <typename First, typename... Rest>
  using tween_value_t = typename tween_value<First, Rest...>::type;
}

#endif // TWEENY_DETAIL_TWEEN_VALUE_H

#ifndef TWEENY_DETAIL_EVENT_H
#define TWEENY_DETAIL_EVENT_H

#include <cstddef>

namespace tweeny::detail::event {
  struct step_t {};
  struct seek_t {};
  struct jump_t {};
  struct complete_t {};
  struct keyframeEnter_t {};
  struct keyframeLeave_t {};
  struct update_t {};
}

namespace tweeny::event {
  struct keyframeEnter {
    size_t key_frame;
    explicit keyframeEnter(const size_t key_frame_input) : key_frame(key_frame_input) {}
  };

  struct keyframeLeave {
    size_t key_frame;
    explicit keyframeLeave(const size_t key_frame_input) : key_frame(key_frame_input) {}
  };

  enum class response {
    ok = 0,
    unsubscribe = 1,
  };
}

#endif // TWEENY_DETAIL_EVENT_H

namespace tweeny {
  template<typename FirstValueType, typename... RemainingValueTypes>
  class tween {
    typedef detail::key_frame<FirstValueType, RemainingValueTypes...> key_frame_t;
    typedef std::vector<key_frame_t> key_frames_t;

    public:
      using tween_value_t = detail::tween_value_t<FirstValueType, RemainingValueTypes...>;

      explicit tween(const key_frames_t & key_frames_input);
      explicit tween(key_frames_t && key_frames_input);

      auto seek(uint32_t target_frame) -> tween_value_t;
      auto jump(std::size_t target_key_frame) -> tween_value_t;
      auto step(int32_t frames) -> tween_value_t;
      [[nodiscard]] auto peek() const -> tween_value_t;
      [[nodiscard]] auto peek(uint32_t target_frame) const -> tween_value_t;
      [[nodiscard]] auto progress() const -> float;

      template <typename Callback> auto on(detail::event::step_t, Callback&& cb) -> void ;
      template <typename Callback> auto on(detail::event::seek_t, Callback&& cb) -> void ;
      template <typename Callback> auto on(detail::event::jump_t, Callback&& cb) -> void ;
      template <typename Callback> auto on(detail::event::complete_t, Callback&& cb) -> void ;
      template <typename Callback> auto on(detail::event::keyframeEnter_t, Callback&& cb) -> void ;
      template <typename Callback> auto on(detail::event::keyframeLeave_t, Callback&& cb) -> void ;
      template <typename Callback> auto on(detail::event::update_t, Callback&& cb) -> void ;

    private:
      using callback_t = std::function<event::response(tween&)>;
      using keyframe_enter_callback_t = std::function<event::response(tween&, struct event::keyframeEnter)>;
      using keyframe_leave_callback_t = std::function<event::response(tween&, struct event::keyframeLeave)>;

      key_frames_t key_frames;
      uint32_t current_frame = 0;
      tween_value_t current_value;
      std::size_t current_keyframe_index = 0;
      std::vector<callback_t> step_listeners;
      std::vector<callback_t> seek_listeners;
      std::vector<callback_t> jump_listeners;
      std::vector<callback_t> complete_listeners;
      std::vector<callback_t> update_listeners;
      std::vector<keyframe_enter_callback_t> keyframe_enter_listeners;
      std::vector<keyframe_leave_callback_t> keyframe_leave_listeners;

      auto invoke_listeners(std::vector<callback_t>& listeners) -> void;
      auto invoke_keyframe_listeners(std::size_t old_keyframe_index, std::size_t new_keyframe_index) -> void;
      auto render(uint32_t target_frame) const -> tween_value_t;
      [[nodiscard]] auto find_key_frame_index(uint32_t frame) const -> std::size_t;
  };
}

#ifndef TWEENY_TWEEN_TCC
#define TWEENY_TWEEN_TCC

#include <algorithm>
#include <cstdint>
#include <tuple>
#include <utility>

#ifndef TWEENY_INTERPOLATE_H
#define TWEENY_INTERPOLATE_H
#include <cstddef>
#include <tuple>
#include <utility>

#ifndef TWEENY_EASING_H
#define TWEENY_EASING_H

#ifndef TWEENY_DETAIL_EASING_BACK_H
#define TWEENY_DETAIL_EASING_BACK_H

namespace tweeny::detail {
  struct backInEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      constexpr float s = 1.70158f;
      float postFix = position;
      return static_cast<T>((end - start) * postFix * position * ((s + 1) * position - s) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct backOutEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      constexpr float s = 1.70158f;
      position -= 1;
      return static_cast<T>((end - start) * ((position) * position * ((s + 1) * position + s) + 1) + start);
    }

    template <typename T>
    T operator()(float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct backInOutEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      float s = 1.70158f;
      float t = position;
      auto b = start;
      auto c = end - start;
      constexpr float d = 1;
      s *= 1.525f;
      if ((t /= d / 2) < 1) return static_cast<T>(c / 2 * (t * t * ((s + 1) * t - s)) + b);
      const float postFix = t -= 2;
      return static_cast<T>(c / 2 * (postFix * t * ((s + 1) * t + s) + 2) + b);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };
}

#endif // TWEENY_DETAIL_EASING_BACK_H

#ifndef TWEENY_DETAIL_EASING_BOUNCE_H
#define TWEENY_DETAIL_EASING_BOUNCE_H

namespace tweeny::detail {
  struct bounceOutEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      T c = end - start;
      if (position < 1 / 2.75f) {
        return static_cast<T>(c * (7.5625f * position * position) + start);
      }
      if (position < 2.0f / 2.75f) {
        const float postFix = position -= 1.5f / 2.75f;
        return static_cast<T>(c * (7.5625f * (postFix) * position + .75f) + start);
      }
      if (position < 2.5f / 2.75f) {
        const float postFix = position -= 2.25f / 2.75f;
        return static_cast<T>(c * (7.5625f * postFix * position + .9375f) + start);
      }
      const float postFix = position -= (2.625f / 2.75f);
      return static_cast<T>(c * (7.5625f * postFix * position + .984375f) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct bounceInEasing {
    template <typename T>
    static T run(const float position, T start, T end) {
      return end - start - bounceOutEasing::run(1 - position, T(), (end - start)) + start;
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct bounceInOutEasing {
    template <typename T>
    static T run(const float position, T start, T end) {
      if (position < 0.5f) return static_cast<T>(bounceInEasing::run(position * 2, T(), end - start) * .5f + start);
      return static_cast<T>(bounceOutEasing::run(position * 2 - 1, T(), end - start) * .5f + (end - start) * .5f +
        start);
    }

    template <typename T>
    T operator()(float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };
}

#endif // TWEENY_DETAIL_EASING_BOUNCE_H

#ifndef TWEENY_DETAIL_EASING_CIRCULAR_H
#define TWEENY_DETAIL_EASING_CIRCULAR_H

#include <cmath>

namespace tweeny::detail {
  struct circularInEasing {
    template <typename T>
    static T run(const float position, T start, T end) {
      return static_cast<T>(-(end - start) * (sqrtf(1 - position * position) - 1) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct circularOutEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      --position;
      return static_cast<T>((end - start) * sqrtf(1 - position * position) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct circularInOutEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      position *= 2;
      if (position < 1) {
        return static_cast<T>(-(end - start) / 2 * (sqrtf(1 - position * position) - 1) + start);
      }

      position -= 2;
      return static_cast<T>((end - start) / 2 * (sqrtf(1 - position * position) + 1) + start);
    }

    template <typename T>
    T operator()(float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };
}

#endif // TWEENY_DETAIL_EASING_CIRCULAR_H

#ifndef TWEENY_DETAIL_EASING_CUBIC_H
#define TWEENY_DETAIL_EASING_CUBIC_H

namespace tweeny::detail {
  struct cubicInEasing {
    template <typename T>
    static T run(const float position, T start, T end) {
      return static_cast<T>((end - start) * position * position * position + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct cubicOutEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      --position;
      return static_cast<T>((end - start) * (position * position * position + 1) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct cubicInOutEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      position *= 2;
      if (position < 1) {
        return static_cast<T>((end - start) / 2 * position * position * position + start);
      }
      position -= 2;
      return static_cast<T>((end - start) / 2 * (position * position * position + 2) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };
}

#endif // TWEENY_DETAIL_EASING_CUBIC_H

#ifndef TWEENY_DETAIL_EASING_DEF_H
#define TWEENY_DETAIL_EASING_DEF_H

#include <type_traits>
#include <cmath>
#include <utility>

namespace tweeny::detail {
  template <class...> struct voidify {
    using type = void;
  };

  template <class... Ts> using void_t = typename voidify<Ts...>::type;

  template <class, class = void>
  struct supports_arithmetic_operations : std::false_type {};

  template <class T>
  struct supports_arithmetic_operations<T, void_t<
                                          decltype(std::declval<T>() + std::declval<T>()),
                                          decltype(std::declval<T>() - std::declval<T>()),
                                          decltype(std::declval<T>() * std::declval<T>()),
                                          decltype(std::declval<T>() * std::declval<float>()),
                                          decltype(std::declval<float>() * std::declval<T>())
                                        >> : std::true_type {};

  struct defaultEasing {
    template <typename T>
    static std::enable_if_t<std::is_integral_v<T>, T> run(float position, T start, T end) {
      return static_cast<T>(roundf((end - start) * position + start));
    }

    template <typename T>
    static typename std::enable_if<supports_arithmetic_operations<T>::value && !std::is_integral<T>::value, T>::type
      run(float position, T start, T end) {
      return static_cast<T>((end - start) * position + start);
    }

    template <typename T>
    static std::enable_if_t<!supports_arithmetic_operations<T>::value, T> run(float /*position*/, T start, T /*end*/) {
      return start;
    }

    template <typename T>
    T operator()(float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };
}

#endif // TWEENY_DETAIL_EASING_DEF_H

#ifndef TWEENY_DETAIL_EASING_ELASTIC_H
#define TWEENY_DETAIL_EASING_ELASTIC_H

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace tweeny::detail {
  struct elasticInEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      if (position <= 0.00001f) return start;
      if (position >= 0.999f) return end;
      const float p = .3f;
      auto a = end - start;
      const float s = p / 4;
      const float postFix = a * powf(2, 10 * (position -= 1));
      return static_cast<T>(-(postFix * sinf((position - s) * (2 * static_cast<float>(M_PI)) / p)) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct elasticOutEasing {
    template <typename T>
    static T run(const float position, T start, T end) {
      if (position <= 0.00001f) return start;
      if (position >= 0.999f) return end;
      float p = .3f;
      auto a = end - start;
      float s = p / 4;
      return static_cast<T>(a * powf(2, -10 * position) * sinf(
        (position - s) * (2 * static_cast<float>(M_PI)) / p
      ) + end);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct elasticInOutEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      if (position <= 0.00001f) return start;
      if (position >= 0.999f) return end;
      position *= 2;
      const float p = .3f * 1.5f;
      auto a = end - start;
      const float s = p / 4;
      float postFix;

      if (position < 1) {
        postFix = a * powf(2, 10 * (position -= 1));
        return static_cast<T>(-0.5f * (postFix * sinf((position - s) * (2 * static_cast<float>(M_PI)) / p)) + start);
      }
      postFix = a * powf(2, -10 * (position -= 1));
      return static_cast<T>(postFix * sinf((position - s) * (2 * static_cast<float>(M_PI)) / p) * .5f + end);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };
}

#endif // TWEENY_DETAIL_EASING_ELASTIC_H

#ifndef TWEENY_DETAIL_EASING_EXPONENTIAL_H
#define TWEENY_DETAIL_EASING_EXPONENTIAL_H

#include <cmath>

namespace tweeny::detail {
  struct exponentialInEasing {
    template <typename T>
    static T run(const float position, T start, T end) {
      if (position == 0) return start;
      return static_cast<T>((end - start) * powf(2, 10 * (position - 1)) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct exponentialOutEasing {
    template <typename T>
    static T run(const float position, T start, T end) {
      if (position == 1) return end;
      return static_cast<T>((end - start) * (-powf(2, -10 * position) + 1) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct exponentialInOutEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      if (position == 0) return start;
      if (position == 1) return end;
      position *= 2;
      if (position < 1) {
        return static_cast<T>((end - start) / 2 * powf(2, 10 * (position - 1)) + start);
      }
      --position;
      return static_cast<T>((end - start) / 2 * (-powf(2, -10 * position) + 2) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };
}

#endif // TWEENY_DETAIL_EASING_EXPONENTIAL_H

#ifndef TWEENY_DETAIL_EASING_LINEAR_H
#define TWEENY_DETAIL_EASING_LINEAR_H

#include <type_traits>
#include <cmath>

namespace tweeny::detail {
  struct linearEasing {
    template <typename T>
    static std::enable_if_t<std::is_integral_v<T>, T> run(const float position, T start, T end) {
      return static_cast<T>(roundf((end - start) * position + start));
    }

    template <typename T>
    static std::enable_if_t<!std::is_integral_v<T>, T> run(const float position, T start, T end) {
      return static_cast<T>((end - start) * position + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };
}

#endif // TWEENY_DETAIL_EASING_LINEAR_H

#ifndef TWEENY_DETAIL_EASING_QUADRATIC_H
#define TWEENY_DETAIL_EASING_QUADRATIC_H

namespace tweeny::detail {
  struct quadraticInEasing {
    template <typename T>
    static T run(const float position, T start, T end) {
      return static_cast<T>((end - start) * position * position + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct quadraticOutEasing {
    template <typename T>
    static T run(const float position, T start, T end) {
      return static_cast<T>((-(end - start)) * position * (position - 2) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct quadraticInOutEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      position *= 2;
      if (position < 1) {
        return static_cast<T>((end - start) / 2 * position * position + start);
      }

      --position;
      return static_cast<T>(-(end - start) / 2 * (position * (position - 2) - 1) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };
}

#endif // TWEENY_DETAIL_EASING_QUADRATIC_H

#ifndef TWEENY_DETAIL_EASING_QUARTIC_H
#define TWEENY_DETAIL_EASING_QUARTIC_H

namespace tweeny::detail {
  struct quarticInEasing {
    template <typename T>
    static T run(const float position, T start, T end) {
      return static_cast<T>((end - start) * position * position * position * position + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct quarticOutEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      --position;
      return static_cast<T>(-(end - start) * (position * position * position * position - 1) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct quarticInOutEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      position *= 2;
      if (position < 1) {
        return static_cast<T>((end - start) / 2 * (position * position * position * position) + start);
      }
      position -= 2;
      return static_cast<T>(-(end - start) / 2 * (position * position * position * position - 2) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };
}

#endif // TWEENY_DETAIL_EASING_QUARTIC_H

#ifndef TWEENY_DETAIL_EASING_QUINTIC_H
#define TWEENY_DETAIL_EASING_QUINTIC_H

namespace tweeny::detail {
  struct quinticInEasing {
    template <typename T>
    static T run(const float position, T start, T end) {
      return static_cast<T>((end - start) * position * position * position * position * position + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct quinticOutEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      position--;
      return static_cast<T>((end - start) * (position * position * position * position * position + 1) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct quinticInOutEasing {
    template <typename T>
    static T run(float position, T start, T end) {
      position *= 2;
      if (position < 1) {
        return static_cast<T>((end - start) / 2 * (position * position * position * position * position) + start);
      }
      position -= 2;
      return static_cast<T>((end - start) / 2 * (position * position * position * position * position + 2) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };
}

#endif // TWEENY_DETAIL_EASING_QUINTIC_H

#ifndef TWEENY_DETAIL_EASING_SINUSOIDAL_H
#define TWEENY_DETAIL_EASING_SINUSOIDAL_H

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace tweeny::detail {
  struct sinusoidalInEasing {
    template <typename T>
    static T run(const float position, T start, T end) {
      return static_cast<T>(-(end - start) * cosf(position * static_cast<float>(M_PI) / 2) + (end - start) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct sinusoidalOutEasing {
    template <typename T>
    static T run(const float position, T start, T end) {
      return static_cast<T>((end - start) * sinf(position * static_cast<float>(M_PI) / 2) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };

  struct sinusoidalInOutEasing {
    template <typename T>
    static T run(const float position, T start, T end) {
      return static_cast<T>(-(end - start) / 2 * (cosf(position * static_cast<float>(M_PI)) - 1) + start);
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };
}

#endif // TWEENY_DETAIL_EASING_SINUSOIDAL_H

#ifndef TWEENY_DETAIL_EASING_STEPPED_H
#define TWEENY_DETAIL_EASING_STEPPED_H

namespace tweeny::detail {
  struct steppedEasing {
    template <typename T>
    static T run(float /*position*/, T start, T /*end*/) {
      return start;
    }

    template <typename T>
    T operator()(const float position, T start, T end) const {
      return run<T>(position, start, end);
    }
  };
}

#endif // TWEENY_DETAIL_EASING_STEPPED_H

namespace tweeny::easing {
 inline constexpr detail::backInEasing backIn{};
 inline constexpr detail::backOutEasing backOut{};
 inline constexpr detail::backInOutEasing backInOut{};
 inline constexpr detail::bounceInEasing bounceIn{};
 inline constexpr detail::bounceOutEasing bounceOut{};
 inline constexpr detail::bounceInOutEasing bounceInOut{};
 inline constexpr detail::circularInEasing circularIn{};
 inline constexpr detail::circularOutEasing circularOut{};
 inline constexpr detail::circularInOutEasing circularInOut{};
 inline constexpr detail::cubicInEasing cubicIn{};
 inline constexpr detail::cubicOutEasing cubicOut{};
 inline constexpr detail::cubicInOutEasing cubicInOut{};
 inline constexpr detail::defaultEasing def{};
 inline constexpr detail::elasticInEasing elasticIn{};
 inline constexpr detail::elasticOutEasing elasticOut{};
 inline constexpr detail::elasticInOutEasing elasticInOut{};
 inline constexpr detail::exponentialInEasing exponentialIn{};
 inline constexpr detail::exponentialOutEasing exponentialOut{};
 inline constexpr detail::exponentialInOutEasing exponentialInOut{};
 inline constexpr detail::linearEasing linear{};
 inline constexpr detail::quadraticInEasing quadraticIn{};
 inline constexpr detail::quadraticOutEasing quadraticOut{};
 inline constexpr detail::quadraticInOutEasing quadraticInOut{};
 inline constexpr detail::quarticInEasing quarticIn{};
 inline constexpr detail::quarticOutEasing quarticOut{};
 inline constexpr detail::quarticInOutEasing quarticInOut{};
 inline constexpr detail::quinticInEasing quinticIn{};
 inline constexpr detail::quinticOutEasing quinticOut{};
 inline constexpr detail::quinticInOutEasing quinticInOut{};
 inline constexpr detail::sinusoidalInEasing sinusoidalIn{};
 inline constexpr detail::sinusoidalOutEasing sinusoidalOut{};
 inline constexpr detail::sinusoidalInOutEasing sinusoidalInOut{};
 inline constexpr detail::steppedEasing stepped{};
}

#endif //TWEENY_EASING_H

namespace tweeny::detail {
  template <std::size_t I, typename... ValueTypes>
  static auto interpolate_one(
    float t,
    const key_frame<ValueTypes...> & base,
    const key_frame<ValueTypes...> & next
  ) -> std::remove_reference_t<decltype(std::get<I>(base.values))> {
    const auto & start = std::get<I>(base.values);
    const auto & end = std::get<I>(next.values);
    const auto & func = std::get<I>(base.easing_functions);
    if (func) return func(t, start, end);
    return easing::def(t, start, end);
  }

  template <typename... ValueTypes, std::size_t... I>
  static auto interpolate_values(
    float t,
    const key_frame<ValueTypes...> & base,
    const key_frame<ValueTypes...> & next,
    std::index_sequence<I...>
  ) -> typename key_frame<ValueTypes...>::values_t {
    using values_t = typename key_frame<ValueTypes...>::values_t;
    values_t out{};
    ((std::get<I>(out) = interpolate_one<I>(t, base, next)), ...);
    return out;
  }
}

#endif //TWEENY_INTERPOLATE_H

template <typename FirstValueType, typename... RemainingValueTypes>
tweeny::tween<FirstValueType, RemainingValueTypes...>::tween(const key_frames_t & key_frames_input) : key_frames(key_frames_input), current_value(render(0)) { }

template <typename FirstValueType, typename... RemainingValueTypes>
tweeny::tween<FirstValueType, RemainingValueTypes...>::tween(key_frames_t && key_frames_input) : key_frames(std::move(key_frames_input)), current_value(render(0)) { }

template <typename FirstValueType, typename... RemainingValueTypes>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::find_key_frame_index(uint32_t frame) const -> size_t {
  std::size_t i = 0;
  while (i + 1 < key_frames.size() && frame >= key_frames[i + 1].position) { ++i; }
  return i;
}

template <typename FirstValueType, typename... RemainingValueTypes>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::seek(const uint32_t target_frame) -> tween_value_t {
  const std::size_t old_keyframe_index = current_keyframe_index;
  const std::size_t new_keyframe_index = find_key_frame_index(target_frame);

  current_value = render(target_frame);
  current_frame = target_frame;
  current_keyframe_index = new_keyframe_index;

  invoke_keyframe_listeners(old_keyframe_index, new_keyframe_index);
  invoke_listeners(seek_listeners);
  invoke_listeners(update_listeners);

  if (progress() >= 1.0f) {
    invoke_listeners(complete_listeners);
  }

  return current_value;
}

template <typename FirstValueType, typename... RemainingValueTypes>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::jump(std::size_t target_key_frame) -> tween_value_t {
  const std::size_t old_keyframe_index = current_keyframe_index;
  target_key_frame = std::clamp(target_key_frame, static_cast<size_t>(0), key_frames.size() - 1);

  const auto target_frame = static_cast<uint32_t>(key_frames[target_key_frame].position);
  current_value = render(target_frame);
  current_frame = target_frame;
  current_keyframe_index = target_key_frame;

  invoke_keyframe_listeners(old_keyframe_index, target_key_frame);
  invoke_listeners(jump_listeners);
  invoke_listeners(update_listeners);

  if (progress() >= 1.0f) {
    invoke_listeners(complete_listeners);
  }

  return current_value;
}

template <typename FirstValueType, typename... RemainingValueTypes>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::step(const int32_t frames) -> tween_value_t {
  const std::size_t old_keyframe_index = current_keyframe_index;

  uint32_t target_frame = current_frame;
  if (frames < 0) {
    const auto dec = static_cast<uint32_t>(-frames);
    if (dec > target_frame) target_frame = 0;
    else target_frame -= dec;
  } else {
    target_frame += static_cast<uint32_t>(frames);
  }

  const std::size_t new_keyframe_index = find_key_frame_index(target_frame);

  current_value = render(target_frame);
  current_frame = target_frame;
  current_keyframe_index = new_keyframe_index;

  invoke_keyframe_listeners(old_keyframe_index, new_keyframe_index);
  invoke_listeners(step_listeners);
  invoke_listeners(update_listeners);

  if (progress() >= 1.0f) {
    invoke_listeners(complete_listeners);
  }

  return current_value;
}

template <typename FirstValueType, typename ... RemainingValueTypes>
template <typename Callback>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::on(detail::event::step_t, Callback && cb) -> void {
  using result_t = std::invoke_result_t<Callback, tween&>;
  static_assert(std::is_same_v<result_t, event::response>,
                "step callback must return tweeny::event::response");
  step_listeners.emplace_back(std::forward<Callback>(cb));
}

template <typename FirstValueType, typename ... RemainingValueTypes>
template <typename Callback>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::on(detail::event::seek_t, Callback && cb) -> void {
  using result_t = std::invoke_result_t<Callback, tween&>;
  static_assert(std::is_same_v<result_t, event::response>,
                "seek callback must return tweeny::event::response");
  seek_listeners.emplace_back(std::forward<Callback>(cb));
}

template <typename FirstValueType, typename ... RemainingValueTypes>
template <typename Callback>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::on(detail::event::jump_t, Callback && cb) -> void {
  using result_t = std::invoke_result_t<Callback, tween&>;
  static_assert(std::is_same_v<result_t, event::response>,
                "jump callback must return tweeny::event::response");
  jump_listeners.emplace_back(std::forward<Callback>(cb));
}

template <typename FirstValueType, typename ... RemainingValueTypes>
template <typename Callback>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::on(detail::event::complete_t, Callback && cb) -> void {
  using result_t = std::invoke_result_t<Callback, tween&>;
  static_assert(std::is_same_v<result_t, event::response>,
                "complete callback must return tweeny::event::response");
  complete_listeners.emplace_back(std::forward<Callback>(cb));
}

template <typename FirstValueType, typename ... RemainingValueTypes>
template <typename Callback>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::on(detail::event::keyframeEnter_t, Callback && cb) -> void {
  using result_t = std::invoke_result_t<Callback, tween&, event::keyframeEnter>;
  static_assert(std::is_same_v<result_t, event::response>,
                "keyframeEnter callback must return tweeny::event::response");
  keyframe_enter_listeners.emplace_back(std::forward<Callback>(cb));
}

template <typename FirstValueType, typename ... RemainingValueTypes>
template <typename Callback>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::on(detail::event::keyframeLeave_t, Callback && cb) -> void {
  using result_t = std::invoke_result_t<Callback, tween&, event::keyframeLeave>;
  static_assert(std::is_same_v<result_t, event::response>,
                "keyframeLeave callback must return tweeny::event::response");
  keyframe_leave_listeners.emplace_back(std::forward<Callback>(cb));
}

template <typename FirstValueType, typename ... RemainingValueTypes>
template <typename Callback>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::on(detail::event::update_t, Callback && cb) -> void {
  using result_t = std::invoke_result_t<Callback, tween&>;
  static_assert(std::is_same_v<result_t, event::response>,
                "update callback must return tweeny::event::response");
  update_listeners.emplace_back(std::forward<Callback>(cb));
}

template <typename FirstValueType, typename... RemainingValueTypes>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::invoke_listeners(std::vector<callback_t>& listeners) -> void {
  std::vector<std::size_t> to_remove;
  to_remove.reserve(listeners.size());

  for (std::size_t i = 0; i < listeners.size(); ++i) {
    const auto resp = listeners[i](*this);
    if (resp == event::response::unsubscribe) {
      to_remove.push_back(i);
    }
  }

  if (!to_remove.empty()) {
    using diff_t = typename std::vector<callback_t>::difference_type;
    for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
      listeners.erase(listeners.begin() + static_cast<diff_t>(*it));
    }
  }
}

template <typename FirstValueType, typename... RemainingValueTypes>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::invoke_keyframe_listeners(const std::size_t old_keyframe_index, const std::size_t new_keyframe_index) -> void {
  if (old_keyframe_index == new_keyframe_index) return;

  {
    std::vector<std::size_t> to_remove;
    const struct event::keyframeLeave evt{old_keyframe_index};
    for (std::size_t i = 0; i < keyframe_leave_listeners.size(); ++i) {
      const auto resp = keyframe_leave_listeners[i](*this, evt);
      if (resp == event::response::unsubscribe) {
        to_remove.push_back(i);
      }
    }
    for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
      keyframe_leave_listeners.erase(keyframe_leave_listeners.begin() + static_cast<typename std::vector<keyframe_leave_callback_t>::difference_type>(*it));
    }
  }

  {
    std::vector<std::size_t> to_remove;
    const struct event::keyframeEnter evt{new_keyframe_index};
    for (std::size_t i = 0; i < keyframe_enter_listeners.size(); ++i) {
      const auto resp = keyframe_enter_listeners[i](*this, evt);
      if (resp == event::response::unsubscribe) {
        to_remove.push_back(i);
      }
    }
    for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
      keyframe_enter_listeners.erase(keyframe_enter_listeners.begin() + static_cast<typename std::vector<keyframe_enter_callback_t>::difference_type>(*it));
    }
  }
}
template <typename FirstValueType, typename... RemainingValueTypes>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::render(
    uint32_t target_frame
) const -> tween_value_t {
    constexpr std::size_t ValuesCount =
        sizeof...(RemainingValueTypes) + 1;

    if (key_frames.empty()) {
        if constexpr (ValuesCount == 1) {
            return FirstValueType{};
        } else {
            return typename key_frame_t::values_t{};
        }
    }

    const auto& first_key_frame = key_frames.front();
    const auto& last_key_frame = key_frames.back();

    target_frame = std::clamp(
        target_frame,
        first_key_frame.position,
        last_key_frame.position
    );

    if (target_frame <= first_key_frame.position) {
        if constexpr (ValuesCount == 1) {
            return std::get<0>(first_key_frame.values);
        } else {
            return first_key_frame.values;
        }
    }

    if (target_frame >= last_key_frame.position) {
        if constexpr (ValuesCount == 1) {
            return std::get<0>(last_key_frame.values);
        } else {
            return last_key_frame.values;
        }
    }

    const std::size_t base_key_frame_idx =
        find_key_frame_index(target_frame);

    if (base_key_frame_idx + 1 >= key_frames.size()) {
        if constexpr (ValuesCount == 1) {
            return std::get<0>(last_key_frame.values);
        } else {
            return last_key_frame.values;
        }
    }

    const key_frame_t& base_key_frame =
        key_frames[base_key_frame_idx];

    const key_frame_t& next_key_frame =
        key_frames[base_key_frame_idx + 1];

    const int64_t base_kf_position =
        static_cast<int64_t>(base_key_frame.position);

    const uint32_t target_kf_position =
        next_key_frame.position;

    const int64_t target_frame_i64 =
        static_cast<int64_t>(target_frame);

    float inbetween_progress = 1.0f;

    if (target_kf_position > base_kf_position) {
        const float numerator =
            static_cast<float>(
                target_frame_i64 - base_kf_position
            );

        const float denominator =
            static_cast<float>(
                target_kf_position - base_kf_position
            );

        inbetween_progress = numerator / denominator;
    }

    inbetween_progress =
        std::clamp(inbetween_progress, 0.0f, 1.0f);

    auto values =
        detail::interpolate_values<
            FirstValueType,
            RemainingValueTypes...
        >(
            inbetween_progress,
            base_key_frame,
            next_key_frame,
            std::make_index_sequence<ValuesCount>{}
        );

    if constexpr (ValuesCount == 1) {
        return std::get<0>(values);
    } else {
        return values;
    }
}

template <typename FirstValueType, typename... RemainingValueTypes>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::peek() const -> tween_value_t {
  return current_value;
}

template <typename FirstValueType, typename... RemainingValueTypes>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::peek(uint32_t target_frame) const -> tween_value_t {
  return render(target_frame);
}

template <typename FirstValueType, typename... RemainingValueTypes>
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::progress() const -> float {
  if (key_frames.empty()) return 0.0f;

  const auto & first_key_frame = key_frames.front();
  const auto & last_key_frame = key_frames.back();

  const uint32_t total_frames = last_key_frame.position - first_key_frame.position;
  if (total_frames == 0) return 1.0f;

  const uint32_t current_offset = current_frame - first_key_frame.position;
  return std::clamp(static_cast<float>(current_offset) / static_cast<float>(total_frames), 0.0f, 1.0f);
}

#endif //TWEENY_TWEEN_TCC

#endif //TWEENY_TWEEN_H

#include <tuple>
#include <type_traits>

#ifndef TWEENY_DETAIL_TUPLE_UTILITIES_H
#define TWEENY_DETAIL_TUPLE_UTILITIES_H
#include <cstddef>
#include <tuple>

namespace tweeny::detail {
  template <typename T, std::size_t... I>
  static auto make_repeated_tuple_impl(const T & v, std::index_sequence<I...> sequence) {
    (void) sequence;
    return std::make_tuple(((void) I, v)...);
  }

  template <typename T, std::size_t N>
  static auto make_repeated_tuple(const T & v) {
    return make_repeated_tuple_impl(v, std::make_index_sequence<N>{});
  }
}

#endif //TWEENY_DETAIL_TUPLE_UTILITIES_H

#ifndef TWEENY_EVENT_H
#define TWEENY_EVENT_H

namespace tweeny::event {
  inline constexpr detail::event::step_t step{};
  inline constexpr detail::event::seek_t seek{};
  inline constexpr detail::event::jump_t jump{};
  inline constexpr detail::event::complete_t complete{};
  inline constexpr detail::event::keyframeEnter_t keyframeEnter{};
  inline constexpr detail::event::keyframeLeave_t keyframeLeave{};
  inline constexpr detail::event::update_t update{};
}

#endif //TWEENY_EVENT_H

namespace tweeny {
  template <bool WasToCalled, typename FirstValue, typename... RemainingValues>
  class tweeny_builder;

  template <typename FirstValue, typename... RemainingValues>
  class tweeny_builder<false, FirstValue, RemainingValues...> {
    typedef std::vector<detail::key_frame<FirstValue, RemainingValues...>> key_frames_t;
    typedef tween<FirstValue, RemainingValues...> tween_t;
    static size_t constexpr value_count = 1 + sizeof...(RemainingValues);

    public:
      explicit tweeny_builder(FirstValue firstComponent, RemainingValues... remainingComponents) {
        key_frames.emplace_back(firstComponent, remainingComponents...);
      }

      explicit tweeny_builder(key_frames_t && frames) : key_frames(std::move(frames)) {}
      explicit tweeny_builder(const key_frames_t & frames) : key_frames(frames) {}

      tweeny_builder<true, FirstValue, RemainingValues...> to(const FirstValue & firstComponent, const RemainingValues &... remainingComponents) & {
        key_frames.emplace_back(firstComponent, remainingComponents...);
        return tweeny_builder<true, FirstValue, RemainingValues...>(key_frames);
      }

      tweeny_builder<true, FirstValue, RemainingValues...> to(const FirstValue & firstComponent, const RemainingValues &... remainingComponents) && {
        key_frames.emplace_back(firstComponent, remainingComponents...);
        return tweeny_builder<true, FirstValue, RemainingValues...>(std::move(key_frames));
      }

    private:
      key_frames_t key_frames;
  };

  template <typename FirstValue, typename... RemainingValues>
  class tweeny_builder<true, FirstValue, RemainingValues...> {
    typedef std::vector<detail::key_frame<FirstValue, RemainingValues...>> key_frames_t;
    typedef tween<FirstValue, RemainingValues...> tween_t;
    static size_t constexpr value_count = 1 + sizeof...(RemainingValues);

    public:
      explicit tweeny_builder(FirstValue firstComponent, RemainingValues... remainingComponents) {
        key_frames.emplace_back(firstComponent, remainingComponents...);
      }

      explicit tweeny_builder(key_frames_t && frames) : key_frames(std::move(frames)) {}
      explicit tweeny_builder(const key_frames_t & frames) : key_frames(frames) {}

      tweeny_builder to(const FirstValue & firstComponent, const RemainingValues &... remainingComponents) & {
        key_frames.emplace_back(firstComponent, remainingComponents...);
        return tweeny_builder(key_frames);
      }

      tweeny_builder to(const FirstValue & firstComponent, const RemainingValues &... remainingComponents) && {
        key_frames.emplace_back(firstComponent, remainingComponents...);
        return tweeny_builder(std::move(key_frames));
      }

      // NOTE (MSVC portability): this used to be two overloads -- a variadic
      // template `via(EasingFunctionTypes...)` and a single-argument template
      // `via(EasingFunctionType)` -- disambiguated purely by template partial
      // ordering (a fixed-arity template is preferred over a trailing-pack
      // template when called with exactly one argument). GCC and Clang get
      // this right, but MSVC has a documented history of bugs picking between
      // a parameter-pack overload and a single fixed-parameter overload in
      // exactly this shape. Folding both cases into one template with
      // `if constexpr` removes the ambiguity entirely instead of depending on
      // the compiler's overload resolution/partial ordering being correct.
      template<typename... EasingFunctionTypes>
      tweeny_builder & via(EasingFunctionTypes... easing_functions) {
        static_assert(sizeof...(EasingFunctionTypes) == 1 || sizeof...(EasingFunctionTypes) == value_count,
                      "via() must be called with either a single easing function "
                      "(applied to every tween component) or with exactly one "
                      "easing function per tween component");
        auto & key_frame = key_frames.at(key_frames.size() - 2);
        if constexpr (sizeof...(EasingFunctionTypes) == 1 && value_count != 1) {
          key_frame.easing_functions = std::apply(
            [](auto easing_function) {
              return detail::make_repeated_tuple<decltype(easing_function), value_count>(easing_function);
            },
            std::make_tuple(easing_functions...));
        } else {
          key_frame.easing_functions = std::make_tuple(easing_functions...);
        }
        return *this;
      }

      template<typename... FrameCountsType>
      tweeny_builder & during(FrameCountsType... frame_counts) {
        static_assert(sizeof...(FrameCountsType) == value_count,
                     "during() must have one frame count per tween component");
        static_assert((std::is_same_v<std::decay_t<FrameCountsType>, uint32_t> && ...),
                      "during() parameters must be of type uint32_t");
        auto & key_frame = key_frames.at(key_frames.size() - 2);
        std::array<uint32_t, sizeof...(FrameCountsType)> frame_counts_array = { frame_counts... };
        std::copy(
          std::begin(frame_counts_array),
          std::end(frame_counts_array),
          std::begin(key_frame.tween_frame_counts));
        fix_frame_positions();
        return *this;
      }

      tweeny_builder & during(uint32_t frame_count) {
        auto & key_frame = key_frames.at(key_frames.size() - 2);
        std::fill(
          std::begin(key_frame.tween_frame_counts),
          std::end(key_frame.tween_frame_counts),
          frame_count
        );
        fix_frame_positions();
        return *this;
      }

      // NOTE (MSVC portability): originally `return tween(key_frames);`, which
      // relies on class template argument deduction (CTAD) picking up
      // FirstValue/RemainingValues from the constructor parameter's nested
      // member typedef (tween<...>::key_frames_t) while inside another class
      // template's member function body. This exact pattern -- implicit CTAD
      // deduced from a nested alias, invoked from within another template's
      // member -- is a documented MSVC bug class ("class template argument
      // deduction fails in the body of a ..."), even though GCC and Clang
      // both accept it. Spelling out the template arguments sidesteps
      // deduction altogether, so it's correct and unambiguous everywhere.
      tween_t build() const & { return tween<FirstValue, RemainingValues...>(key_frames); }
      tween_t build() && { return tween<FirstValue, RemainingValues...>(std::move(key_frames)); }

    private:
      key_frames_t key_frames;
      void fix_frame_positions() {
        uint32_t key_frame_position = 0;
        for (auto & key_frame : key_frames) {
          key_frame.position = key_frame_position;
          key_frame_position += key_frame.highest_frame_count();
        }
      }
  };

  template <typename FirstValue, typename... RemainingValues>
  tweeny_builder<false, FirstValue, RemainingValues...> from(FirstValue firstComponent, RemainingValues... remainingComponents) {
    return tweeny_builder<false, FirstValue, RemainingValues...>(firstComponent, remainingComponents...);
  }
}

#endif //TWEENY_TWEENY_H