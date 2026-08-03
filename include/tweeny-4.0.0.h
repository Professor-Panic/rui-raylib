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

/**
 * @file tweeny.h
 * @brief Builder API for creating tweens.
 *
 * This file provides the fluent builder interface for constructing tween animations.
 * The typical workflow is: from() → to() → via() → during() → build()
 *
 * @code
 * auto tween = tweeny::from(0.0f).to(100.0f).via(easing::linear).during(60U).build();
 * @endcode
 */

#ifndef TWEENY_TWEENY_H
#define TWEENY_TWEENY_H


/**
 * @file tween.h
 * @brief Core tween class for frame-based animation.
 *
 * This file defines the tween class, which represents an animation between keyframes.
 * Tweens interpolate values over time using configurable easing functions and support
 * event listeners for animation lifecycle events.
 *
 * @code
 * auto t = tweeny::from(0).to(100).during(60U).build();
 * t.step(1);              // Advance one frame
 * auto val = t.peek();    // Get current value
 * float p = t.progress(); // Get completion (0.0-1.0)
 * @endcode
 */

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
  /**
   * @struct all_same
   * @brief A type trait that evaluates to `std::true_type`.
   *
   * This specialization of the `all_same` struct is used when no template parameters
   * are provided. It serves as a base case for type pack evaluation and always evaluates
   * to `true_type`, representing that an empty pack is trivially considered to have
   * "all the same" types.
   *
   * This type trait is mainly used as a base case in conjunction with other
   * template specializations that check for type consistency within a parameter pack.
   *
   * @warning This structure is private and shouldn't be used directly
   */
  template<typename...>
  struct all_same : std::true_type {};

  /**
   * @struct all_same
   * @brief A type trait to check if all types in a parameter pack are the same.
   *
   * This struct inherits from `std::conjunction` with the result of comparing all
   * types in the parameter pack `Rest...` using `std::is_same`, relative to the first
   * type `First`. The resulting value will evaluate to `true_type` if all types are the
   * same, otherwise to `false_type`.
   *
   * @tparam First The first type in the parameter pack to compare against.
   * @tparam Rest The remaining types in the parameter pack.
   *
   * @warning This structure is private and shouldn't be used directly
   */
  template<typename First, typename... Rest>
  struct all_same<First, Rest...> : std::conjunction<std::is_same<First, Rest>...> {};

  /**
   * @brief A compile-time boolean that is true if all types in the provided parameter pack are the same, false otherwise.
   *
   * Determines whether all types in the template parameter pack `Ts...` are the same type. This is evaluated at
   * compile-time using the `all_same` trait.
   *
   * Example usage:
   * @code
   * static_assert(all_same_v<int, int, int> == true);
   * static_assert(all_same_v<int, double, int> == false);
   * @endcode
   *
   * @tparam Ts pack of types to be evaluated
   *
   * @warning This value is private and shouldn't be used directly
   */
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
  /**
   * @brief Represents a key frame in a tween.
   *
   * The key_frame class encapsulates the attributes and behaviors
   * of a single frame in an animation sequence, allowing for storage
   * and manipulation of key values at specific points.
   *
   * @warning This structure is private and shouldn't be used directly
   */
  struct key_frame {
    typedef value_container_t<ValueTypes...> values_t;
    typedef std::tuple<std::function<ValueTypes(float, ValueTypes, ValueTypes)>...> value_easing_functions_t;
    typedef std::array<uint32_t, sizeof...(ValueTypes)> value_tween_frame_counts_t;

    /**
     * @brief Constructs a key_frame object initialized with provided values.
     *
     * This constructor initializes the key_frame object by setting its position to 0,
     * populating the values container with the provided values `vs...`, and initializing
     * the easing functions and tween frame counts with default values.
     *
     * @param vs The values to initialize the key_frame, passed as a parameter pack of type `ValueTypes...`.
     *           These values are stored in a container, which can either be a tuple or an array depending
     *           on their types.
     * @return A key_frame object initialized with the provided values.
     */
    explicit key_frame(ValueTypes... vs) : position(0), values{vs...}, easing_functions(), tween_frame_counts() {}

    /**
     * Holds the 'position' of the key frame within the animation sequence.
     *
     * This variable represents the position of the current key frame as an unsigned 32-bit integer.
     * It refers to the frame index at which this key frame is located
     * within the timeline of a tween.
     */
    uint32_t position;

    /**
     * @typedef values_t
     * @brief Represents a container to store values in `key_frame`.
     *
     * The `values_t` type alias is used to hold the data values associated with
     * the `key_frame` structure. The container type is determined at compile-time
     * based on the type consistency of the provided value types. If all value types
     * are the same, the container is implemented as a `std::array`. Otherwise, it
     * is implemented as a `std::tuple`.
     */
    values_t values;

    /**
     * @typedef value_easing_functions_t easing_functions
     * @brief Represents a tuple of easing function objects for each value component in a key frame.
     *
     * This variable contains easing functions used to interpolate or transform
     * individual value components over time in a tweening operation.
     * Each easing function is specified as a callable object (e.g., a lambda or function)
     * following the signature `(float progress, ValueType start, ValueType end) -> ValueType`.
     *
     * It is used within the context of the tween system to determine how intermediate
     * values between this key frame and the next are computed based on the specified easing functions.
     */
    value_easing_functions_t easing_functions;

    /**
     * An array that represents the number of frames allocated for tweening
     * each value in a key frame. The size of the array corresponds to the
     * number of values being tweened, and each entry specifies the frame
     * count for the respective value.
     *
     * This array is populated either by specifying explicit frame counts
     * for each value during a tween's construction or by using a uniform
     * frame count for all values. The highest count is used to calculate
     * the position of key frames and manage the timing of individual
     * value transitions within a tween animation.
     */
    value_tween_frame_counts_t tween_frame_counts;

    /**
     * @brief Retrieves the highest frame count from the tween_frame_counts array.
     *
     * This function searches through the tween_frame_counts array to find
     * and return the maximum frame count value.
     *
     * @return The highest frame count as an unsigned 32-bit integer.
     */
    [[nodiscard]] uint32_t highest_frame_count() const {
      return *std::max_element(tween_frame_counts.begin(), tween_frame_counts.end());
    }
  };
}

#endif //TWEENY_DETAIL_KEY_FRAME_H


#ifndef TWEENY_DETAIL_TWEEN_VALUE_H
#define TWEENY_DETAIL_TWEEN_VALUE_H

#include <type_traits>

#ifndef TWEENY_DETAIL_VALUE_CONTAINER_H
#define TWEENY_DETAIL_VALUE_CONTAINER_H

#include <array>
#include <type_traits>

namespace tweeny::detail {
  /**
   * @struct all_same
   * @brief A type trait that evaluates to `std::true_type`.
   *
   * This specialization of the `all_same` struct is used when no template parameters
   * are provided. It serves as a base case for type pack evaluation and always evaluates
   * to `true_type`, representing that an empty pack is trivially considered to have
   * "all the same" types.
   *
   * This type trait is mainly used as a base case in conjunction with other
   * template specializations that check for type consistency within a parameter pack.
   *
   * @warning This structure is private and shouldn't be used directly
   */
  template<typename...>
  struct all_same : std::true_type {};

  /**
   * @struct all_same
   * @brief A type trait to check if all types in a parameter pack are the same.
   *
   * This struct inherits from `std::conjunction` with the result of comparing all
   * types in the parameter pack `Rest...` using `std::is_same`, relative to the first
   * type `First`. The resulting value will evaluate to `true_type` if all types are the
   * same, otherwise to `false_type`.
   *
   * @tparam First The first type in the parameter pack to compare against.
   * @tparam Rest The remaining types in the parameter pack.
   *
   * @warning This structure is private and shouldn't be used directly
   */
  template<typename First, typename... Rest>
  struct all_same<First, Rest...> : std::conjunction<std::is_same<First, Rest>...> {};

  /**
   * @brief A compile-time boolean that is true if all types in the provided parameter pack are the same, false otherwise.
   *
   * Determines whether all types in the template parameter pack `Ts...` are the same type. This is evaluated at
   * compile-time using the `all_same` trait.
   *
   * Example usage:
   * @code
   * static_assert(all_same_v<int, int, int> == true);
   * static_assert(all_same_v<int, double, int> == false);
   * @endcode
   *
   * @tparam Ts pack of types to be evaluated
   *
   * @warning This value is private and shouldn't be used directly
   */
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

namespace tweeny::detail {

  /**
   * @struct tween_value
   * @brief Metafunction that resolves to a type based on the provided template arguments.
   *
   * This struct determines the appropriate type depending on the number and types
   * of its template parameters. If there is only one parameter (First), it resolves
   * to that type. Otherwise, it resolves to a `value_container_t` type, which is
   * constructed using the provided `First` and additional `Rest` types.
   *
   * @tparam First The first type parameter used for evaluation.
   * @tparam Rest Additional type parameters provided for consideration. These are
   *         used to construct a `value_container_t` if more than one parameter is passed.
   *
   * @see value_container_t
   * @see tween_value_t
   *
   * @warning This structure is private and shouldn't be used directly
   */
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
  /**
   * @brief Tag type for step events.
   * @internal Implementation detail - users should use tweeny::event::step
   */
  struct step_t {};

  /**
   * @brief Tag type for seek events.
   * @internal Implementation detail - users should use tweeny::event::seek
   */
  struct seek_t {};

  /**
   * @brief Tag type for jump events.
   * @internal Implementation detail - users should use tweeny::event::jump
   */
  struct jump_t {};

  /**
   * @brief Tag type for completion events.
   * @internal Implementation detail - users should use tweeny::event::complete
   */
  struct complete_t {};

  /**
   * @brief Tag type for keyframeEnter events.
   * @internal Implementation detail - users should use tweeny::event::keyframeEnter
   */
  struct keyframeEnter_t {};

  /**
   * @brief Tag type for keyframeLeave events.
   * @internal Implementation detail - users should use tweeny::event::keyframeLeave
   */
  struct keyframeLeave_t {};

  /**
   * @brief Tag type for update events.
   * @internal Implementation detail - users should use tweeny::event::update
   */
  struct update_t {};
}

namespace tweeny::event {
  /**
   * @brief Event data passed when entering a new keyframe.
   *
   * This event is triggered when the tween transitions into a new keyframe section,
   * providing the index of the keyframe being entered.
   */
  struct keyframeEnter {
    size_t key_frame;
    explicit keyframeEnter(const size_t key_frame_input) : key_frame(key_frame_input) {}
  };

  /**
   * @brief Event data passed when leaving a keyframe.
   *
   * This event is triggered when the tween transitions out of a keyframe section,
   * providing the index of the keyframe being exited.
   */
  struct keyframeLeave {
    size_t key_frame;
    explicit keyframeLeave(const size_t key_frame_input) : key_frame(key_frame_input) {}
  };

  /**
   * @brief Response codes returned by event callbacks.
   *
   * Controls whether a callback continues receiving events or is automatically removed.
   */
  enum class response {
    /**
     * @brief Continue receiving events.
     *
     * The callback remains registered and will be invoked on future events.
     */
    ok = 0,

    /**
     * @brief Unsubscribe after this callback.
     *
     * The callback is automatically removed after returning and will not
     * receive future events. Useful for one-shot callbacks.
     */
    unsubscribe = 1,
  };
}

#endif // TWEENY_DETAIL_EVENT_H

namespace tweeny {
  /**
   * @brief A tween represents an animation between keyframes.
   *
   * Tweens interpolate values over discrete frames using configurable easing functions.
   * They maintain internal state (current frame and value) and support event listeners
   * for animation events.
   *
   * @tparam FirstValueType Type of the first animated value
   * @tparam RemainingValueTypes Types of additional animated values (for multi-value tweens)
   *
   * @note For single values, tween_value_t is the value type directly.
   *       For multiple values, tween_value_t is std::tuple<FirstValueType, RemainingValueTypes...>
   */
  template<typename FirstValueType, typename... RemainingValueTypes>
  class tween {
    typedef detail::key_frame<FirstValueType, RemainingValueTypes...> key_frame_t;
    typedef std::vector<key_frame_t> key_frames_t;

    public:
      /**
       * @brief The type returned by navigation and query methods.
       *
       * For single-value tweens: the value type itself.
       * For multi-value tweens: std::tuple of all value types.
       */
      using tween_value_t = detail::tween_value_t<FirstValueType, RemainingValueTypes...>;

      /**
       * @brief Constructs a tween from keyframes (use builder API instead).
       * @internal Users should use tweeny::from() to create tweens.
       */
      explicit tween(const key_frames_t & key_frames_input);

      /// @overload
      explicit tween(key_frames_t && key_frames_input);

      /**
       * @brief Seeks to a specific frame in the animation.
       *
       * Jumps directly to the target frame, updating the tween's current position and value.
       * Clamped to [first_frame, last_frame]. Triggers event::seek listeners.
       *
       * @param target_frame Absolute frame number to seek to
       * @return Interpolated value at the target frame
       *
       * @code
       * auto t = tweeny::from(0).to(100).during(100U).build();
       * t.seek(50U);  // Jump to frame 50 (value: 50)
       * t.seek(0U);   // Jump back to start
       * @endcode
       */
      auto seek(uint32_t target_frame) -> tween_value_t;

      /**
       * @brief Jumps to a specific keyframe index.
       *
       * Moves the tween to the exact position of a keyframe. Useful for resetting to
       * known points or implementing discrete state animations. Triggers event::jump listeners.
       *
       * @param target_key_frame Zero-based keyframe index (clamped to valid range)
       * @return Value at the target keyframe
       *
       * @code
       * auto t = tweeny::from(0).to(50).during(30U).to(100).during(30U).build();
       * t.jump(0);  // Jump to first keyframe (value: 0)
       * t.jump(1);  // Jump to second keyframe (value: 50)
       * t.jump(2);  // Jump to third keyframe (value: 100)
       * @endcode
       */
      auto jump(std::size_t target_key_frame) -> tween_value_t;

      /**
       * @brief Advances or rewinds the animation by a frame delta.
       *
       * Moves the tween forward (positive) or backward (negative) by the specified number
       * of frames. Clamped to valid frame range. Triggers event::step listeners.
       *
       * @param frames Number of frames to move (negative values step backward)
       * @return Interpolated value at the new frame
       *
       * @code
       * auto t = tweeny::from(0).to(100).during(100U).build();
       * t.step(10);   // Advance 10 frames (value: 10)
       * t.step(5);    // Advance 5 more frames (value: 15)
       * t.step(-3);   // Rewind 3 frames (value: 12)
       * @endcode
       */
      auto step(int32_t frames) -> tween_value_t;

      /**
       * @brief Returns the current interpolated value without changing state.
       *
       * Non-mutating query of the tween's current value. Does not trigger events
       * or modify the tween's position.
       *
       * @return Current interpolated value
       *
       * @code
       * auto t = tweeny::from(0).to(100).during(100U).build();
       * t.step(25);
       * auto val = t.peek();  // Returns 25, doesn't change state
       * @endcode
       */
      [[nodiscard]] auto peek() const -> tween_value_t;

      /**
       * @brief Queries the interpolated value at any frame without changing state.
       *
       * Previews what the value would be at the target frame without moving the tween.
       * Useful for scrubbing, previewing, or inspecting the animation curve.
       * Does not trigger events.
       *
       * @param target_frame Frame to query (clamped to valid range)
       * @return Interpolated value at the target frame
       *
       * @code
       * auto t = tweeny::from(0).to(100).during(100U).build();
       * auto midpoint = t.peek(50U);  // Preview value at frame 50 (returns 50)
       * auto start = t.peek(0U);      // Preview start value (returns 0)
       * // Tween still at frame 0, not moved
       * @endcode
       */
      [[nodiscard]] auto peek(uint32_t target_frame) const -> tween_value_t;

      /**
       * @brief Returns the animation completion percentage.
       *
       * Calculates progress as (current_frame - first_frame) / total_frames.
       * Returns a value in the range [0.0, 1.0].
       *
       * @return Progress percentage (0.0 = start, 1.0 = complete)
       *
       * @code
       * auto t = tweeny::from(0).to(100).during(100U).build();
       * t.step(25);
       * printf("%.0f%% complete\n", t.progress() * 100.0f);  // "25% complete"
       *
       * // Check if animation is done
       * if (t.progress() >= 1.0f) {
       *   printf("Animation complete!\n");
       * }
       * @endcode
       */
      [[nodiscard]] auto progress() const -> float;

      /**
       * @brief Registers a callback for step() events.
       *
       * The callback is invoked after each step() call. It receives a reference to
       * the tween and must return event::response::ok to continue receiving events,
       * or event::response::unsubscribe to auto-remove.
       *
       * @param cb Callback with signature: event::response(tween&)
       *
       * @code
       * auto t = tweeny::from(0).to(100).during(100U).build();
       * t.on(event::step, [](auto& tween) {
       *   printf("Stepped to: %d\n", tween.peek());
       *   return event::response::ok;
       * });
       * t.step(10);  // Prints: "Stepped to: 10"
       * @endcode
       */
      template <typename Callback> auto on(detail::event::step_t, Callback&& cb) -> void ;

      /**
       * @brief Registers a callback for seek() events.
       *
       * The callback is invoked after each seek() call. Same signature and behavior as step events.
       *
       * @param cb Callback with signature: event::response(tween&)
       *
       * @code
       * t.on(event::seek, [](auto& tween) {
       *   printf("Seeked to: %d\n", tween.peek());
       *   return event::response::ok;
       * });
       * @endcode
       */
      template <typename Callback> auto on(detail::event::seek_t, Callback&& cb) -> void ;

      /**
       * @brief Registers a callback for jump() events.
       *
       * The callback is invoked after each jump() call. Same signature and behavior as step events.
       *
       * @param cb Callback with signature: event::response(tween&)
       *
       * @code
       * t.on(event::jump, [](auto& tween) {
       *   printf("Jumped to keyframe\n");
       *   return event::response::ok;
       * });
       * @endcode
       */
      template <typename Callback> auto on(detail::event::jump_t, Callback&& cb) -> void ;

      /**
       * @brief Registers a callback for animation completion.
       *
       * The callback is invoked when the tween reaches the last frame (progress >= 1.0).
       * Triggered by step(), seek(), or jump() when they result in completion.
       *
       * @param cb Callback with signature: event::response(tween&)
       *
       * @code
       * auto t = tweeny::from(0).to(100).during(100U).build();
       * t.on(event::complete, [](auto& tween) {
       *   printf("Animation complete!\n");
       *   return event::response::ok;
       * });
       * t.seek(100U);  // Triggers complete event
       * @endcode
       */
      template <typename Callback> auto on(detail::event::complete_t, Callback&& cb) -> void ;

      /**
       * @brief Registers a callback for entering a keyframe.
       *
       * The callback is invoked when the tween transitions into a new keyframe section.
       * Receives the tween reference and event data containing the keyframe index.
       *
       * @param cb Callback with signature: event::response(tween&, event::keyframeEnter)
       *
       * @code
       * auto t = tweeny::from(0).to(50).during(30U).to(100).during(30U).build();
       * t.on(event::keyframeEnter, [](auto& tween, auto evt) {
       *   printf("Entered keyframe %zu\n", evt.key_frame);
       *   return event::response::ok;
       * });
       * t.step(31);  // Triggers: "Entered keyframe 1"
       * @endcode
       */
      template <typename Callback> auto on(detail::event::keyframeEnter_t, Callback&& cb) -> void ;

      /**
       * @brief Registers a callback for leaving a keyframe.
       *
       * The callback is invoked when the tween transitions out of a keyframe section.
       * Receives the tween reference and event data containing the keyframe index.
       *
       * @param cb Callback with signature: event::response(tween&, event::keyframeLeave)
       *
       * @code
       * auto t = tweeny::from(0).to(50).during(30U).to(100).during(30U).build();
       * t.on(event::keyframeLeave, [](auto& tween, auto evt) {
       *   printf("Left keyframe %zu\n", evt.key_frame);
       *   return event::response::ok;
       * });
       * t.step(31);  // Triggers: "Left keyframe 0"
       * @endcode
       */
      template <typename Callback> auto on(detail::event::keyframeLeave_t, Callback&& cb) -> void ;

      /**
       * @brief Registers a callback for any position update (step, seek, or jump).
       *
       * The callback is invoked after step(), seek(), or jump() calls. This is a convenience
       * event for monitoring all position changes from a single callback.
       *
       * @param cb Callback with signature: event::response(tween&)
       *
       * @code
       * auto t = tweeny::from(0).to(100).during(60U).build();
       * t.on(event::update, [](auto& tween) {
       *   printf("Tween updated to: %d\n", tween.peek());
       *   return event::response::ok;
       * });
       * t.step(10);  // Triggers update
       * t.seek(50U); // Triggers update
       * @endcode
       */
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


#ifndef TWEENY_DETAIL_KEY_FRAME_H
#define TWEENY_DETAIL_KEY_FRAME_H

#include <algorithm>


#ifndef TWEENY_DETAIL_VALUE_CONTAINER_H
#define TWEENY_DETAIL_VALUE_CONTAINER_H

#include <array>
#include <type_traits>

namespace tweeny::detail {
  /**
   * @struct all_same
   * @brief A type trait that evaluates to `std::true_type`.
   *
   * This specialization of the `all_same` struct is used when no template parameters
   * are provided. It serves as a base case for type pack evaluation and always evaluates
   * to `true_type`, representing that an empty pack is trivially considered to have
   * "all the same" types.
   *
   * This type trait is mainly used as a base case in conjunction with other
   * template specializations that check for type consistency within a parameter pack.
   *
   * @warning This structure is private and shouldn't be used directly
   */
  template<typename...>
  struct all_same : std::true_type {};

  /**
   * @struct all_same
   * @brief A type trait to check if all types in a parameter pack are the same.
   *
   * This struct inherits from `std::conjunction` with the result of comparing all
   * types in the parameter pack `Rest...` using `std::is_same`, relative to the first
   * type `First`. The resulting value will evaluate to `true_type` if all types are the
   * same, otherwise to `false_type`.
   *
   * @tparam First The first type in the parameter pack to compare against.
   * @tparam Rest The remaining types in the parameter pack.
   *
   * @warning This structure is private and shouldn't be used directly
   */
  template<typename First, typename... Rest>
  struct all_same<First, Rest...> : std::conjunction<std::is_same<First, Rest>...> {};

  /**
   * @brief A compile-time boolean that is true if all types in the provided parameter pack are the same, false otherwise.
   *
   * Determines whether all types in the template parameter pack `Ts...` are the same type. This is evaluated at
   * compile-time using the `all_same` trait.
   *
   * Example usage:
   * @code
   * static_assert(all_same_v<int, int, int> == true);
   * static_assert(all_same_v<int, double, int> == false);
   * @endcode
   *
   * @tparam Ts pack of types to be evaluated
   *
   * @warning This value is private and shouldn't be used directly
   */
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
  /**
   * @brief Represents a key frame in a tween.
   *
   * The key_frame class encapsulates the attributes and behaviors
   * of a single frame in an animation sequence, allowing for storage
   * and manipulation of key values at specific points.
   *
   * @warning This structure is private and shouldn't be used directly
   */
  struct key_frame {
    typedef value_container_t<ValueTypes...> values_t;
    typedef std::tuple<std::function<ValueTypes(float, ValueTypes, ValueTypes)>...> value_easing_functions_t;
    typedef std::array<uint32_t, sizeof...(ValueTypes)> value_tween_frame_counts_t;

    /**
     * @brief Constructs a key_frame object initialized with provided values.
     *
     * This constructor initializes the key_frame object by setting its position to 0,
     * populating the values container with the provided values `vs...`, and initializing
     * the easing functions and tween frame counts with default values.
     *
     * @param vs The values to initialize the key_frame, passed as a parameter pack of type `ValueTypes...`.
     *           These values are stored in a container, which can either be a tuple or an array depending
     *           on their types.
     * @return A key_frame object initialized with the provided values.
     */
    explicit key_frame(ValueTypes... vs) : position(0), values{vs...}, easing_functions(), tween_frame_counts() {}

    /**
     * Holds the 'position' of the key frame within the animation sequence.
     *
     * This variable represents the position of the current key frame as an unsigned 32-bit integer.
     * It refers to the frame index at which this key frame is located
     * within the timeline of a tween.
     */
    uint32_t position;

    /**
     * @typedef values_t
     * @brief Represents a container to store values in `key_frame`.
     *
     * The `values_t` type alias is used to hold the data values associated with
     * the `key_frame` structure. The container type is determined at compile-time
     * based on the type consistency of the provided value types. If all value types
     * are the same, the container is implemented as a `std::array`. Otherwise, it
     * is implemented as a `std::tuple`.
     */
    values_t values;

    /**
     * @typedef value_easing_functions_t easing_functions
     * @brief Represents a tuple of easing function objects for each value component in a key frame.
     *
     * This variable contains easing functions used to interpolate or transform
     * individual value components over time in a tweening operation.
     * Each easing function is specified as a callable object (e.g., a lambda or function)
     * following the signature `(float progress, ValueType start, ValueType end) -> ValueType`.
     *
     * It is used within the context of the tween system to determine how intermediate
     * values between this key frame and the next are computed based on the specified easing functions.
     */
    value_easing_functions_t easing_functions;

    /**
     * An array that represents the number of frames allocated for tweening
     * each value in a key frame. The size of the array corresponds to the
     * number of values being tweened, and each entry specifies the frame
     * count for the respective value.
     *
     * This array is populated either by specifying explicit frame counts
     * for each value during a tween's construction or by using a uniform
     * frame count for all values. The highest count is used to calculate
     * the position of key frames and manage the timing of individual
     * value transitions within a tween animation.
     */
    value_tween_frame_counts_t tween_frame_counts;

    /**
     * @brief Retrieves the highest frame count from the tween_frame_counts array.
     *
     * This function searches through the tween_frame_counts array to find
     * and return the maximum frame count value.
     *
     * @return The highest frame count as an unsigned 32-bit integer.
     */
    [[nodiscard]] uint32_t highest_frame_count() const {
      return *std::max_element(tween_frame_counts.begin(), tween_frame_counts.end());
    }
  };
}

#endif //TWEENY_DETAIL_KEY_FRAME_H


/**
 * @file easing.h
 * The purpose of this file is to list all bundled easings. Each easing is defined
 * in its own header under include/tweeny/detail/easing/. Users may include individual
 * easing headers or this header to get all easings.
 */

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

/**
 * @namespace tweeny::easing
 * @brief Contains all built-in easing functions for controlling animation curves.
 *
 * Provides 30+ easing functions in In, Out, and InOut variants for creating natural-looking
 * animations.
 */
namespace tweeny::easing {
 /**
  * @brief Easing function that creates anticipation by pulling back before moving forward.
  *
  * The `backIn` easing function begins by moving backwards slightly (overshooting in the
  * negative direction), then reverses and accelerates toward the target value. This creates
  * an anticipation effect similar to pulling back a slingshot or winding up before a throw.
  *
  * The backward motion at the start makes this easing particularly effective for animations
  * that benefit from telegraphing or anticipation, such as:
  * - UI elements that "wind up" before sliding in
  * - Character movements that show preparation before action
  * - Camera movements that pull back before zooming forward
  *
  * Mathematically, this easing uses the formula: `c * t * t * ((s + 1) * t - s) + b`
  * where `s` controls the overshoot amount (typically 1.70158).
  *
  * @note The animation will briefly have values less than the start value before proceeding
  * to the target. Ensure your animation system can handle values outside the expected range.
  *
  * @code
  * // Create a tween with back-in easing for anticipation effect
  * auto tween = tweeny::from(0.0f)
  *   .to(100.0f)
  *   .via(easing::backIn)
  *   .during(60U)
  *   .build();
  *
  * // The animation will dip below 0.0f briefly before accelerating to 100.0f
  * @endcode
  *
  * @see backOut For overshoot at the end instead of the beginning
  * @see backInOut For anticipation at the start and overshoot at the end
  * @see Visualize at https://easings.net/#easeInBack
  */
 inline constexpr detail::backInEasing backIn{};

 /**
  * @brief Easing function that overshoots the target before settling back.
  *
  * The `backOut` easing function accelerates toward and past the target value, then pulls
  * back to settle at the final position. This creates an overshoot or "spring-back" effect
  * that adds liveliness and energy to animations.
  *
  * This easing is particularly effective for:
  * - UI elements that pop into place with energy (buttons, dialogs, notifications)
  * - Object arrivals that should feel bouncy and dynamic
  * - Emphasizing the end state of an animation
  * - Adding personality to mechanical movements
  *
  * The overshoot creates a more natural, less rigid feel compared to standard easing
  * functions. It's widely used in iOS, Android, and Material Design animation patterns
  * to make interactions feel more responsive and playful.
  *
  * Mathematically, this is the inverse of backIn, applied at the end of the transition.
  *
  * @note The animation will briefly exceed the target value before settling. Ensure your
  * rendering or logic can handle values beyond the specified range.
  *
  * @code
  * // Animate a button with overshoot for a lively effect
  * auto scale = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::backOut)
  *   .during(30U)
  *   .build();
  *
  * // Scale will exceed 1.0f (e.g., 1.1f) before settling back to 1.0f
  * @endcode
  *
  * @see backIn For anticipation at the start instead of overshoot at the end
  * @see backInOut For both anticipation and overshoot
  * @see elasticOut For a more pronounced oscillating overshoot effect
  * @see Visualize at https://easings.net/#easeOutBack
  */
 inline constexpr detail::backOutEasing backOut{};

 /**
  * @brief Easing function combining anticipation at the start and overshoot at the end.
  *
  * The `backInOut` easing function creates a dramatic motion curve by pulling back before
  * the start (anticipation), accelerating through the middle, then overshooting past the
  * target before settling (spring-back). This combines the effects of both backIn and
  * backOut for maximum expressiveness.
  *
  * This easing creates highly dynamic animations suitable for:
  * - Attention-grabbing transitions that need maximum visual interest
  * - Character animations requiring wind-up and follow-through
  * - Scene transitions with dramatic flair
  * - Emphasizing both the start and end states of an animation
  *
  * The dual overshoot (negative at start, positive at end) makes this one of the most
  * expressive easings, but it should be used judiciously as it can feel exaggerated if
  * overused. It works best for focal animations rather than ambient motion.
  *
  * @note The animation will have values outside the start-to-target range at both ends.
  * During the first half, values will dip below the start; during the second half, values
  * will exceed the target before settling.
  *
  * @code
  * // Create a dramatic page transition
  * auto position = tweeny::from(-100.0f)
  *   .to(100.0f)
  *   .via(easing::backInOut)
  *   .during(90U)
  *   .build();
  *
  * // Position will go below -100.0f at start and above 100.0f near end
  * @endcode
  *
  * @see backIn For only anticipation without overshoot
  * @see backOut For only overshoot without anticipation
  * @see elasticInOut For a more extreme oscillating version
  * @see Visualize at https://easings.net/#easeInOutBack
  */
 inline constexpr detail::backInOutEasing backInOut{};

 /**
  * @brief Bounce easing simulating a ball bouncing with increasing height at the start.
  *
  * The `bounceIn` easing function creates a bouncing effect before the animation begins,
  * like a ball dropped from above that bounces progressively higher before launching into
  * the main motion. This is the reverse of bounceOut's natural physics.
  *
  * Characteristics:
  * - Multiple discrete "bounces" at the start
  * - Each bounce is higher than the previous
  * - Creates anticipation through impact-based motion
  * - Less commonly used than bounceOut
  *
  * This easing works well for:
  * - **Landing preparation**: Elements about to drop into place
  * - **Impact anticipation**: Building up to a collision
  * - **Reverse playback effects**: Bounced animations played backward
  * - **Stylized entrances**: Cartoon-style wind-up effects
  *
  * BounceIn is less intuitive than bounceOut because it reverses natural physics (balls
  * don't normally bounce higher each time). Consider backIn for more natural anticipation.
  *
  * @code
  * // Element that bounces before sliding in
  * auto position = tweeny::from(0.0f)
  *   .to(200.0f)
  *   .via(easing::bounceIn)
  *   .during(50U)
  *   .build();
  * @endcode
  *
  * @see bounceOut For natural ball-drop bouncing at the end
  * @see bounceInOut For bouncing at both start and end
  * @see backIn For simpler anticipation without discrete impacts
  * @see Visualize at https://easings.net/#easeInBounce
  */
 inline constexpr detail::bounceInEasing bounceIn{};

 /**
  * @brief Bounce easing simulating a ball dropping and bouncing to rest.
  *
  * The `bounceOut` easing function creates the classic ball-drop effect where an object
  * bounces several times with decreasing height before coming to rest. This mimics real-world
  * physics of an inelastic collision and is one of the most recognizable easing patterns.
  *
  * Characteristics:
  * - Multiple discrete bounces with decreasing amplitude
  * - Simulates impact and energy loss
  * - Creates playful, physical motion
  * - Widely recognized and understood by users
  *
  * This easing excels at:
  * - **Object drops**: Items falling into place
  * - **Landing animations**: Characters, UI elements touching down
  * - **Playful interactions**: Buttons, icons, notifications
  * - **Game elements**: Collectibles, power-ups, score displays
  * - **Error/success feedback**: Visual confirmation with personality
  * - **Cartoon physics**: Exaggerated, entertaining motion
  *
  * BounceOut is more popular than elasticOut when you want discrete impacts rather than
  * smooth oscillation. It conveys weight and physicality better than spring-based easings.
  *
  * The bouncing creates natural emphasis on the final position, making it excellent for
  * drawing attention to where something lands.
  *
  * @code
  * // Notification dropping in from above
  * auto y = tweeny::from(-100.0f)
  *   .to(0.0f)
  *   .via(easing::bounceOut)
  *   .during(45U)
  *   .build();
  *
  * // Button that bounces into place
  * auto scale = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::bounceOut)
  *   .during(40U)
  *   .build();
  * @endcode
  *
  * @see bounceIn For inverse bouncing at the start
  * @see bounceInOut For bouncing at both ends
  * @see elasticOut For smooth spring-like alternative
  * @see Visualize at https://easings.net/#easeOutBounce
  */
 inline constexpr detail::bounceOutEasing bounceOut{};

 /**
  * @brief Bounce easing with bouncing at both start and end.
  *
  * The `bounceInOut` easing function combines reverse bouncing at the start with natural
  * bouncing at the end. The motion bounces with increasing height initially, accelerates
  * through the middle, then bounces to rest at the target.
  *
  * Motion profile:
  * - First half: Bounces with increasing amplitude (bounceIn)
  * - Midpoint: Smooth transition
  * - Second half: Bounces with decreasing amplitude (bounceOut)
  * - Creates playful, impact-based motion at both ends
  *
  * This easing is appropriate for:
  * - **Playful transitions**: Fun, energetic scene changes
  * - **Game UI**: High-energy, cartoon-style interfaces
  * - **Children's applications**: Whimsical, entertaining motion
  * - **Attention-grabbing effects**: Elements that need maximum personality
  *
  * BounceInOut is quite dramatic and can feel excessive for business applications,
  * productivity tools, and enterprise software where subtlety is preferred. The dual
  * bouncing works best in contexts where playfulness is a design goal.
  *
  * @code
  * // Playful modal transition
  * auto scale = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::bounceInOut)
  *   .during(60U)
  *   .build();
  * @endcode
  *
  * @see bounceIn For bouncing only at the start
  * @see bounceOut For bouncing only at the end (more commonly useful)
  * @see elasticInOut For spring-like alternative
  * @see Visualize at https://easings.net/#easeInOutBounce
  */
 inline constexpr detail::bounceInOutEasing bounceInOut{};

 /**
  * @brief Circular easing based on quarter-circle arc for smooth acceleration.
  *
  * The `circularIn` easing function accelerates following the curve of a quarter circle,
  * creating smooth, gradual acceleration. The motion follows the equation `1 - sqrt(1 - t²)`,
  * which traces a circular arc.
  *
  * Characteristics:
  * - Smooth, continuous acceleration
  * - More gradual than quadratic, less than cubic
  * - No sharp transitions in velocity
  * - Mathematically elegant curve
  *
  * This easing is ideal for:
  * - **Natural motion**: Movements that need organic feel
  * - **Camera movements**: Smooth pans and zooms
  * - **Scroll animations**: Gentle acceleration for reading comfort
  * - **Subtle UI transitions**: Motion with moderate acceleration
  *
  * Circular easings strike a balance between the gentle quadratic and the more aggressive
  * cubic curves, making them versatile for many contexts.
  *
  * @code
  * // Smooth camera pan
  * auto cameraX = tweeny::from(0.0f)
  *   .to(1000.0f)
  *   .via(easing::circularIn)
  *   .during(90U)
  *   .build();
  * @endcode
  *
  * @see circularOut For circular deceleration
  * @see circularInOut For circular acceleration and deceleration
  * @see quadraticIn For gentler acceleration
  * @see cubicIn For stronger acceleration
  * @see Visualize at https://easings.net/#easeInCirc
  */
 inline constexpr detail::circularInEasing circularIn{};

 /**
  * @brief Circular easing based on quarter-circle arc for smooth deceleration.
  *
  * The `circularOut` easing function decelerates following a circular curve, creating
  * smooth, natural-looking motion that settles gently to rest. Popular for UI animations
  * requiring moderate deceleration without dramatic emphasis.
  *
  * Characteristics:
  * - Smooth, continuous deceleration
  * - Natural feeling without being too soft or aggressive
  * - Balanced between gentle and pronounced
  * - Widely used in production interfaces
  *
  * This easing excels at:
  * - **Modern UI animations**: Cards, panels, menus
  * - **Material Design patterns**: Following Google's motion guidelines
  * - **Content transitions**: Page changes, tab switches
  * - **Professional applications**: Business and productivity software
  * - **General-purpose motion**: Versatile for many animation types
  *
  * CircularOut works well in most contexts. It's less dramatic than cubic but with
  * smoother deceleration than quadratic.
  *
  * @code
  * // Menu panel sliding in
  * auto x = tweeny::from(-300.0f)
  *   .to(0.0f)
  *   .via(easing::circularOut)
  *   .during(35U)
  *   .build();
  * @endcode
  *
  * @see circularIn For circular acceleration
  * @see circularInOut For circular motion at both ends
  * @see quadraticOut For gentler alternative
  * @see cubicOut For more pronounced alternative
  * @see Visualize at https://easings.net/#easeOutCirc
  */
 inline constexpr detail::circularOutEasing circularOut{};

 /**
  * @brief Circular easing with smooth acceleration and deceleration.
  *
  * The `circularInOut` easing function uses circular curves for both acceleration and
  * deceleration, creating balanced motion suitable for general UI work. This is a
  * popular all-purpose easing.
  *
  * Motion profile:
  * - First half: Circular acceleration
  * - Midpoint: Maximum velocity
  * - Second half: Circular deceleration
  * - Creates smooth S-curve
  *
  * This easing is appropriate for:
  * - **All-purpose UI animations**: Buttons, dialogs, drawers
  * - **Material Design**: Recommended in Google's motion guidelines
  * - **Business applications**: Productivity tools, enterprise apps
  * - **Default animation choice**: Safe, versatile motion curve
  *
  * CircularInOut is often recommended as a starting point for animations because it
  * provides polish without being too subtle or too dramatic.
  *
  * @code
  * // Modal dialog appearance
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::circularInOut)
  *   .during(25U)
  *   .build();
  * @endcode
  *
  * @see circularIn For only acceleration
  * @see circularOut For only deceleration
  * @see quadraticInOut For gentler motion
  * @see cubicInOut For more dramatic motion
  * @see Visualize at https://easings.net/#easeInOutCirc
  */
 inline constexpr detail::circularInOutEasing circularInOut{};

 /**
  * @brief Cubic polynomial easing (t³) with moderate acceleration.
  *
  * The `cubicIn` easing function uses a cubic power curve for acceleration, providing
  * smooth, noticeable easing that's more pronounced than quadratic but less extreme
  * than quartic or exponential.
  *
  * Characteristics:
  * - Polynomial acceleration with power of 3
  * - Balanced between gentle and dramatic
  * - Widely used and well-understood
  * - Good default for many animation types
  *
  * This easing is ideal for:
  * - **Standard UI animations**: Menus, panels, overlays
  * - **Easing beginners**: Easy to understand and predict
  * - **General-purpose acceleration**: Versatile for many contexts
  * - **Medium-length animations**: 200-500ms durations
  *
  * CubicIn is a popular choice because it provides clear easing without being subtle
  * (like quadratic) or extreme (like quartic/exponential).
  *
  * Mathematically: `f(t) = t³`
  *
  * @code
  * // Dropdown menu opening
  * auto height = tweeny::from(0.0f)
  *   .to(200.0f)
  *   .via(easing::cubicIn)
  *   .during(30U)
  *   .build();
  * @endcode
  *
  * @see cubicOut For cubic deceleration
  * @see cubicInOut For cubic motion at both ends
  * @see quadraticIn For gentler acceleration
  * @see quarticIn For stronger acceleration
  * @see Visualize at https://easings.net/#easeInCubic
  */
 inline constexpr detail::cubicInEasing cubicIn{};

 /**
  * @brief Cubic polynomial easing (t³) with moderate deceleration.
  *
  * The `cubicOut` easing function provides smooth, balanced deceleration that's popular
  * across many design systems. It's pronounced enough to be noticeable but not dramatic.
  *
  * Characteristics:
  * - Smooth, natural-feeling deceleration
  * - More pronounced than quadratic, gentler than quartic
  * - Industry-standard motion curve
  * - Works well for most animation types
  *
  * This easing excels at:
  * - **Web interfaces**: Following CSS animation best practices
  * - **Mobile UI**: iOS and Android design patterns
  * - **Content animations**: Cards, lists, grids
  * - **Default easing choice**: Safe for most situations
  * - **User-triggered actions**: Button presses, toggles, switches
  *
  * CubicOut is one of the most commonly used easings in production interfaces. It
  * provides clear polish without drawing excessive attention to the motion itself.
  *
  * Mathematically: `f(t) = 1 - (1-t)³`
  *
  * @code
  * // Button press feedback
  * auto scale = tweeny::from(1.0f)
  *   .to(0.95f)
  *   .via(easing::cubicOut)
  *   .during(10U)
  *   .build();
  *
  * // Card appearing
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::cubicOut)
  *   .during(25U)
  *   .build();
  * @endcode
  *
  * @see cubicIn For cubic acceleration
  * @see cubicInOut For cubic motion at both ends
  * @see quadraticOut For gentler deceleration
  * @see quarticOut For stronger deceleration
  * @see Visualize at https://easings.net/#easeOutCubic
  */
 inline constexpr detail::cubicOutEasing cubicOut{};

 /**
  * @brief Cubic polynomial easing with balanced acceleration and deceleration.
  *
  * The `cubicInOut` easing function combines cubic acceleration and deceleration for
  * smooth, professional motion. This is one of the most popular general-purpose easings.
  *
  * Motion profile:
  * - First half: Cubic acceleration (t³)
  * - Midpoint: Maximum velocity
  * - Second half: Cubic deceleration
  * - Creates smooth, balanced S-curve
  *
  * This easing is appropriate for:
  * - **All-purpose animations**: When in doubt, use this
  * - **Web standards**: CSS ease-in-out equivalent
  * - **Design system defaults**: Common in component libraries
  * - **Cross-platform consistency**: Works well everywhere
  * - **Medium animations**: 200-500ms sweet spot
  *
  * CubicInOut is frequently the default easing in design systems and animation libraries
  * because it provides clear, professional motion that works for most scenarios.
  *
  * Mathematically: Combines cubicIn for t < 0.5 and cubicOut for t >= 0.5
  *
  * @code
  * // Page transition
  * auto x = tweeny::from(0.0f)
  *   .to(1920.0f)
  *   .via(easing::cubicInOut)
  *   .during(40U)
  *   .build();
  * @endcode
  *
  * @see cubicIn For only acceleration
  * @see cubicOut For only deceleration
  * @see quadraticInOut For gentler motion
  * @see quarticInOut For more dramatic motion
  * @see Visualize at https://easings.net/#easeInOutCubic
  */
 inline constexpr detail::cubicInOutEasing cubicInOut{};
 /**
  * @brief Default easing function, alias for linear easing.
  *
  * The `def` easing is a convenience alias for `linear`, providing the same constant-velocity
  * interpolation with no acceleration or deceleration. It exists as a semantic indicator that
  * the default easing behavior is being explicitly chosen.
  *
  * Using `def` instead of `linear` can make code intent clearer in contexts where you want to
  * explicitly state "use the default behavior" rather than specifically requesting linear motion.
  * However, both are functionally identical.
  *
  * This is the easing applied when no via() call is made in the tween builder.
  *
  * @code
  * // These three tweens are functionally identical:
  * auto t1 = tweeny::from(0).to(100).during(60U).build();  // Implicit default
  * auto t2 = tweeny::from(0).to(100).via(easing::def).during(60U).build();
  * auto t3 = tweeny::from(0).to(100).via(easing::linear).during(60U).build();
  * @endcode
  *
  * @see linear For the primary documentation of this easing behavior
  */
 inline constexpr detail::defaultEasing def{};

 /**
  * @brief Elastic easing with oscillating spring-like motion at the start.
  *
  * The `elasticIn` easing function creates a spring or elastic band effect that oscillates
  * with increasing amplitude before reaching the starting point of the animation. The motion
  * resembles pulling back an elastic band that vibrates as tension builds.
  *
  * The oscillation characteristics:
  * - Multiple back-and-forth swings before the main motion begins
  * - Amplitude increases as the animation progresses
  * - Creates a "winding up" or "charging" effect
  * - More pronounced than backIn's single overshoot
  *
  * This easing is particularly effective for:
  * - **Magical or fantasy effects**: Spell charging, energy gathering
  * - **Exaggerated cartoon animations**: Extreme anticipation and wind-up
  * - **Attention-grabbing entrances**: Elements that need dramatic introduction
  * - **Game power-ups**: Visual feedback for charging actions
  * - **Playful UI elements**: Whimsical, high-energy interactions
  *
  * The elastic effect is more dramatic than back easing, making it suitable for contexts
  * where strong visual emphasis or entertainment value is desired. It's less appropriate
  * for subtle or professional interfaces.
  *
  * Mathematically, this uses a decaying sine wave with exponential amplitude growth.
  *
  * @warning This easing creates significant overshoot in both directions. Values will
  * oscillate well beyond the start value in both positive and negative directions. Ensure
  * your rendering system can handle these extreme values gracefully.
  *
  * @code
  * // Magical charging effect before an action
  * auto glow = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::elasticIn)
  *   .during(45U)
  *   .build();
  *
  * // The glow will oscillate negative before reaching 1.0f
  * // producing a "charging" visual effect
  * @endcode
  *
  * @see elasticOut For spring-like oscillation at the end
  * @see elasticInOut For oscillation at both start and end
  * @see backIn For a simpler single-overshoot alternative
  * @see Visualize at https://easings.net/#easeInElastic
  */
 inline constexpr detail::elasticInEasing elasticIn{};

 /**
  * @brief Elastic easing with spring-like oscillation at the end.
  *
  * The `elasticOut` easing function creates a natural spring or rubber band effect that
  * overshoots and oscillates around the target value before settling. This is one of the
  * most visually distinctive and playful easings, mimicking real-world elastic physics.
  *
  * The oscillation characteristics:
  * - Overshoots the target value multiple times
  * - Amplitude decreases with each oscillation (damped motion)
  * - Settles naturally at the final value
  * - Creates a bouncy, energetic feel without the hard impacts of bounce easing
  *
  * This easing excels at:
  * - **Playful UI animations**: Buttons, toggles, modal appearances
  * - **Game elements**: Power-up notifications, achievement popups, score displays
  * - **Cartoon-style motion**: Exaggerated, entertaining character movements
  * - **Attention direction**: Drawing eyes to important elements
  * - **Spring simulation**: Rubber bands, diving boards, springy objects
  * - **Joyful interactions**: Adding personality to standard UI patterns
  *
  * ElasticOut is widely used in modern mobile UI design to add energy and delight to
  * interactions. It's more pronounced than backOut but smoother than bounceOut.
  *
  * The spring feel makes animations memorable and adds perceived responsiveness, making
  * interfaces feel "alive" rather than mechanical.
  *
  * Mathematically, this uses a decaying sine wave with exponentially decreasing amplitude.
  *
  * @warning Values will oscillate beyond the target in both directions before settling.
  * For example, animating from 0 to 100 might temporarily reach 110, then 95, then 102,
  * before settling at 100. Ensure clipping or overflow handling is appropriate.
  *
  * @code
  * // Springy button press feedback
  * auto scale = tweeny::from(1.0f)
  *   .to(1.2f)
  *   .via(easing::elasticOut)
  *   .during(40U)
  *   .build();
  *
  * // Scale will overshoot 1.2f (maybe 1.3f) then oscillate
  * // down and up before settling at exactly 1.2f
  *
  * // Modal dialog with playful entrance
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::elasticOut)
  *   .during(50U)
  *   .build();
  * @endcode
  *
  * @see elasticIn For spring anticipation at the start
  * @see elasticInOut For oscillation at both ends
  * @see backOut For a subtler single-overshoot alternative
  * @see bounceOut For a similar but impact-based bouncing effect
  * @see Visualize at https://easings.net/#easeOutElastic
  */
 inline constexpr detail::elasticOutEasing elasticOut{};

 /**
  * @brief Elastic easing combining spring oscillation at both start and end.
  *
  * The `elasticInOut` easing function creates dramatic elastic motion with oscillation
  * during both the initial acceleration and final deceleration phases. The animation
  * winds up with spring-like vibration, accelerates smoothly through the middle, then
  * oscillates around the target before settling.
  *
  * The motion profile:
  * - First half: Oscillates with increasing amplitude (elasticIn behavior)
  * - Middle: Smooth transition through the midpoint
  * - Second half: Oscillates with decreasing amplitude (elasticOut behavior)
  * - Creates the most dramatic elastic effect available
  *
  * This easing is appropriate for:
  * - **Hero animations**: Focal transitions that demand attention
  * - **Game events**: Critical moments, boss appearances, dramatic reveals
  * - **Cartoon physics**: Exaggerated, entertaining motion for stylized visuals
  * - **Transitions with flair**: Scene changes that need maximum personality
  * - **Experimental UI**: Interfaces prioritizing delight over convention
  *
  * ElasticInOut is the most expressive elastic variant but also the most extreme. It works
  * best for animations that are:
  * - Intentionally playful or whimsical
  * - The primary focus of user attention
  * - Part of a stylized, high-energy aesthetic
  * - Not repeated frequently (can become tiresome)
  *
  * Use sparingly; the dual oscillation can feel excessive for everyday interactions.
  * Consider elasticOut or backInOut for a more balanced alternative.
  *
  * @warning This easing produces extreme value overshoots in both directions throughout
  * the animation. During the first half, values oscillate around the start; during the
  * second half, around the target. Plan for values far outside the expected range.
  *
  * @code
  * // Dramatic screen transition
  * auto position = tweeny::from(0.0f)
  *   .to(1920.0f)
  *   .via(easing::elasticInOut)
  *   .during(90U)
  *   .build();
  *
  * // Position will oscillate around 0.0f at start and 1920.0f at end
  *
  * // Game title appearing with maximum impact
  * auto scale = tweeny::from(0.0f)
  *   .to(2.0f)
  *   .via(easing::elasticInOut)
  *   .during(75U)
  *   .build();
  * @endcode
  *
  * @see elasticIn For oscillation only at the start
  * @see elasticOut For oscillation only at the end (more commonly useful)
  * @see backInOut For a less extreme but still expressive alternative
  * @see Visualize at https://easings.net/#easeInOutElastic
  */
 inline constexpr detail::elasticInOutEasing elasticInOut{};

 /**
  * @brief Exponential easing with very slow start and explosive acceleration.
  *
  * The `exponentialIn` easing function starts extremely slowly and builds to a very rapid
  * acceleration toward the end. Based on exponential growth (2^x), this creates one of the
  * most dramatic acceleration curves available, with velocity increasing exponentially over time.
  *
  * Characteristics:
  * - Almost no visible motion for the first portion of the animation
  * - Sudden, explosive acceleration in the final phase
  * - Change rate doubles repeatedly as time progresses
  * - Creates extreme contrast between start and end velocity
  *
  * This easing is ideal for:
  * - **Dramatic reveals**: Elements that burst into view
  * - **Explosive effects**: Particle systems, energy blasts, explosions
  * - **Fade-ins with impact**: Starting invisible and suddenly appearing
  * - **Speed-up effects**: Rockets launching, vehicles accelerating
  * - **Tension building**: Slow build-up to sudden release
  *
  * The extreme acceleration curve makes this feel more dramatic than polynomial easings
  * (quadratic, cubic, etc.). The motion appears to "explode" into existence rather than
  * gradually accelerate.
  *
  * Use exponentialIn when you want maximum contrast between the patient start and the
  * explosive finish. For most UI work, cubic or quartic easings provide sufficient
  * acceleration with less extreme behavior.
  *
  * Mathematically: `f(t) = 2^(10 * (t - 1))`
  *
  * @code
  * // Fade in that suddenly snaps to full visibility
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::exponentialIn)
  *   .during(45U)
  *   .build();
  *
  * // Rocket launch with explosive acceleration
  * auto velocity = tweeny::from(0.0f)
  *   .to(1000.0f)
  *   .via(easing::exponentialIn)
  *   .during(120U)
  *   .build();
  * @endcode
  *
  * @see exponentialOut For explosive deceleration at the end
  * @see exponentialInOut For dramatic acceleration and deceleration
  * @see quarticIn For strong but less extreme acceleration
  * @see Visualize at https://easings.net/#easeInExpo
  */
 inline constexpr detail::exponentialInEasing exponentialIn{};

 /**
  * @brief Exponential easing with explosive start and gradual slow-down.
  *
  * The `exponentialOut` easing function starts with maximum velocity and decelerates
  * exponentially, creating a smooth glide to a stop. The initial burst of speed followed
  * by gradual settling creates a powerful, energetic feel.
  *
  * Characteristics:
  * - Immediate, explosive motion at the start
  * - Exponentially decreasing velocity
  * - Long, smooth deceleration phase
  * - Settles gently to final value
  *
  * This easing excels at:
  * - **Quick UI responses**: Instant feedback that settles smoothly
  * - **Impact effects**: Objects hitting and settling into place
  * - **Momentum-based motion**: Thrown objects, swipe gestures
  * - **Energetic entrances**: Elements that burst in with energy
  * - **Modern UI patterns**: iOS-style animations with quick start
  *
  * ExponentialOut is popular in modern mobile design because it provides immediate
  * visual feedback (the fast start) while ending smoothly. Users perceive the interface
  * as highly responsive due to the instant motion.
  *
  * The long tail of deceleration gives animations a "quality feel" - nothing stops
  * abruptly. This is gentler on the eyes than linear or even cubic deceleration.
  *
  * Mathematically: `f(t) = 1 - 2^(-10 * t)`
  *
  * @code
  * // Responsive drawer that slides out quickly then settles
  * auto position = tweeny::from(-300.0f)
  *   .to(0.0f)
  *   .via(easing::exponentialOut)
  *   .during(35U)
  *   .build();
  *
  * // Notification that pops in with energy
  * auto scale = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::exponentialOut)
  *   .during(25U)
  *   .build();
  * @endcode
  *
  * @see exponentialIn For explosive acceleration at the start
  * @see exponentialInOut For explosive motion at both ends
  * @see quarticOut For strong but less extreme deceleration
  * @see Visualize at https://easings.net/#easeOutExpo
  */
 inline constexpr detail::exponentialOutEasing exponentialOut{};

 /**
  * @brief Exponential easing with extreme acceleration and deceleration.
  *
  * The `exponentialInOut` easing function combines explosive acceleration in the first
  * half with explosive deceleration in the second half. This creates one of the most
  * dramatic, high-energy motion curves available, with rapid changes at both ends and
  * smooth motion through the middle.
  *
  * Motion profile:
  * - First half: Exponential acceleration from near-zero velocity
  * - Midpoint: Maximum velocity
  * - Second half: Exponential deceleration to rest
  * - Creates extreme S-curve shape
  *
  * This easing is appropriate for:
  * - **High-impact transitions**: Scene changes, screen swipes
  * - **Dramatic animations**: Hero elements, focal content
  * - **Fast-paced interfaces**: Games, action-oriented apps
  * - **Attention-grabbing motion**: Elements that need maximum visibility
  * - **Strong transitions**: For elements moving significant distances or longer durations (500ms+)
  *
  * ExponentialInOut creates a powerful sense of momentum and energy. The motion feels
  * purposeful and confident. However, the extreme acceleration can be disorienting if
  * overused or applied to large elements.
  *
  * Best practices:
  * - Use for animations under 1 second duration
  * - Apply to elements that move short to medium distances
  * - Reserve for important, infrequent transitions
  * - Consider quarticInOut or quinticInOut as less extreme alternatives
  *
  * Mathematically: Combines exponentialIn for t < 0.5 and exponentialOut for t >= 0.5
  *
  * @code
  * // Page transition with explosive motion
  * auto position = tweeny::from(0.0f)
  *   .to(1920.0f)
  *   .via(easing::exponentialInOut)
  *   .during(40U)
  *   .build();
  *
  * // Modal dialog with dramatic appearance
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::exponentialInOut)
  *   .during(30U)
  *   .build();
  * @endcode
  *
  * @see exponentialIn For only explosive acceleration
  * @see exponentialOut For only explosive deceleration
  * @see quinticInOut For slightly less extreme alternative
  * @see Visualize at https://easings.net/#easeInOutExpo
  */
 inline constexpr detail::exponentialInOutEasing exponentialInOut{};
 /**
  * @brief Linear easing function with constant velocity throughout the animation.
  *
  * The `linear` easing function produces uniform motion with no acceleration or deceleration.
  * The interpolation progresses at a constant rate from start to finish, creating mechanical,
  * predictable movement.
  *
  * Linear easing is characterized by:
  * - Constant velocity: the rate of change never varies
  * - No ease-in or ease-out: motion starts and stops abruptly
  * - Simple mathematical relationship: output = start + (end - start) * progress
  * - Predictable timing: halfway through time means halfway through distance
  *
  * This easing is appropriate for:
  * - **Mechanical objects**: Conveyor belts, pistons, automated systems
  * - **Progress indicators**: Loading bars, timers, countdowns
  * - **Continuous loops**: Rotating objects, scrolling backgrounds
  * - **Data visualization**: Graph animations where consistency is important
  * - **Debug and testing**: Predictable behavior for verification
  *
  * Linear easing is generally **not recommended** for most UI animations because:
  * - Lacks the natural feel of acceleration/deceleration
  * - Abrupt starts and stops can feel jarring
  * - Missing visual polish that easing provides
  * - Human perception expects objects to ease into and out of motion
  *
  * However, it serves as the foundation for all other easing functions and is essential
  * when mechanical precision is more important than natural motion feel.
  *
  * Mathematically, this implements: `f(t) = t` where t is normalized progress [0, 1].
  *
  * @code
  * // Constant velocity scrolling background
  * auto scroll = tweeny::from(0.0f)
  *   .to(1000.0f)
  *   .via(easing::linear)
  *   .during(600U)
  *   .build();
  *
  * // Progress indicator that matches time exactly
  * auto progress = tweeny::from(0)
  *   .to(100)
  *   .via(easing::linear)
  *   .during(100U)
  *   .build();
  *
  * // At frame 50, progress will be exactly 50
  * @endcode
  *
  * @note Linear is the default easing when no via() is specified, though using def is
  * more explicit in code.
  *
  * @see def Alias for linear easing
  * @see quadraticInOut For a gentle alternative with ease-in and ease-out
  * @see sinusoidalInOut For smooth, natural-feeling motion
  */
 inline constexpr detail::linearEasing linear{};

 /**
  * @brief Quadratic polynomial easing (t²) with gentle acceleration.
  *
  * The `quadraticIn` easing function uses a squared power curve for acceleration,
  * providing the gentlest polynomial easing. It's more subtle than cubic but still
  * provides noticeable easing.
  *
  * Characteristics:
  * - Gentle, smooth acceleration
  * - Polynomial with power of 2
  * - Subtle but perceptible easing
  * - Good for beginners and subtle animations
  *
  * This easing is ideal for:
  * - **Subtle UI motion**: When easing should be felt but not seen
  * - **Quick animations**: Short durations where gentle curves work best
  * - **Minimal designs**: Interfaces prioritizing restraint
  * - **Learning easings**: Easy to understand and predict
  *
  * Mathematically: `f(t) = t²`
  *
  * @code
  * // Gentle fade in
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::quadraticIn)
  *   .during(20U)
  *   .build();
  * @endcode
  *
  * @see quadraticOut For gentle deceleration
  * @see quadraticInOut For gentle motion at both ends
  * @see cubicIn For more pronounced acceleration
  * @see Visualize at https://easings.net/#easeInQuad
  */
 inline constexpr detail::quadraticInEasing quadraticIn{};

 /**
  * @brief Quadratic polynomial easing (t²) with gentle deceleration.
  *
  * The `quadraticOut` easing function provides subtle, smooth deceleration. It's the
  * gentlest polynomial easing, perfect when you want polish without drama.
  *
  * Characteristics:
  * - Gentle, smooth deceleration
  * - Subtle but professional feel
  * - Never feels too slow or too fast
  * - Safe choice for any context
  *
  * This easing excels at:
  * - **Subtle UI polish**: Adding refinement without drawing attention
  * - **Fast animations**: 100-200ms durations
  * - **Minimal interfaces**: Clean, understated design systems
  * - **Accessibility-friendly**: Gentle motion reduces disorientation
  * - **Background animations**: Motion that shouldn't distract
  *
  * QuadraticOut is great when you want animations to stay out of the way. It's less
  * dramatic than cubic but smoother than linear due to gradual deceleration.
  *
  * Mathematically: `f(t) = 1 - (1-t)²`
  *
  * @code
  * // Subtle tooltip appearance
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::quadraticOut)
  *   .during(15U)
  *   .build();
  * @endcode
  *
  * @see quadraticIn For gentle acceleration
  * @see quadraticInOut For gentle motion at both ends
  * @see cubicOut For more pronounced deceleration
  * @see Visualize at https://easings.net/#easeOutQuad
  */
 inline constexpr detail::quadraticOutEasing quadraticOut{};

 /**
  * @brief Quadratic polynomial easing with gentle acceleration and deceleration.
  *
  * The `quadraticInOut` easing function provides the most subtle polynomial S-curve.
  * Perfect for animations that need easing but should remain understated.
  *
  * Motion profile:
  * - First half: Gentle acceleration (t²)
  * - Midpoint: Maximum velocity
  * - Second half: Gentle deceleration
  * - Creates subtle S-curve
  *
  * This easing is appropriate for:
  * - **Minimal design systems**: Understated, refined motion
  * - **Fast animations**: Under 200ms where gentle curves shine
  * - **Accessibility**: Motion-sensitive users
  * - **Background motion**: Animations that shouldn't dominate
  *
  * Mathematically: Combines quadraticIn for t < 0.5 and quadraticOut for t >= 0.5
  *
  * @code
  * // Subtle menu transition
  * auto x = tweeny::from(0.0f)
  *   .to(100.0f)
  *   .via(easing::quadraticInOut)
  *   .during(20U)
  *   .build();
  * @endcode
  *
  * @see quadraticIn For only gentle acceleration
  * @see quadraticOut For only gentle deceleration
  * @see cubicInOut For more pronounced motion
  * @see Visualize at https://easings.net/#easeInOutQuad
  */
 inline constexpr detail::quadraticInOutEasing quadraticInOut{};

 /**
  * @brief Quartic polynomial easing (t⁴) with strong acceleration.
  *
  * The `quarticIn` easing function uses a power-4 curve for acceleration, creating
  * pronounced easing that's stronger than cubic but less extreme than exponential.
  *
  * Characteristics:
  * - Strong acceleration curve
  * - Polynomial with power of 4
  * - Very slow start, rapid finish
  * - More dramatic than cubic
  *
  * This easing is ideal for:
  * - **Dramatic entrances**: Elements that burst into view
  * - **Emphasis**: Drawing attention to motion start
  * - **Game animations**: Action-oriented interfaces
  * - **Long animations**: Over 500ms where strong curves work
  *
  * Mathematically: `f(t) = t⁴`
  *
  * @code
  * // Panel sliding in with emphasis
  * auto x = tweeny::from(-400.0f)
  *   .to(0.0f)
  *   .via(easing::quarticIn)
  *   .during(50U)
  *   .build();
  * @endcode
  *
  * @see quarticOut For strong deceleration
  * @see quarticInOut For strong motion at both ends
  * @see cubicIn For gentler acceleration
  * @see quinticIn For even stronger acceleration
  * @see Visualize at https://easings.net/#easeInQuart
  */
 inline constexpr detail::quarticInEasing quarticIn{};

 /**
  * @brief Quartic polynomial easing (t⁴) with strong deceleration.
  *
  * The `quarticOut` easing function provides pronounced deceleration that's popular
  * for animations needing clear emphasis without being as extreme as exponential.
  *
  * Characteristics:
  * - Strong deceleration curve
  * - Smooth, extended slowdown
  * - Emphasizes final position
  * - Balanced between cubic and exponential
  *
  * This easing excels at:
  * - **Emphasized arrivals**: Elements settling with impact
  * - **Current design patterns**: Strong deceleration suitable for emphasized arrivals
  * - **Medium-length animations**: 300-600ms sweet spot
  * - **Purposeful motion**: Clear acceleration and deceleration
  *
  * QuarticOut provides more "oomph" than cubic while remaining smooth and
  * professional. It's a good choice when cubic feels too subtle.
  *
  * Mathematically: `f(t) = 1 - (1-t)⁴`
  *
  * @code
  * // Card sliding into place with emphasis
  * auto y = tweeny::from(200.0f)
  *   .to(0.0f)
  *   .via(easing::quarticOut)
  *   .during(40U)
  *   .build();
  * @endcode
  *
  * @see quarticIn For strong acceleration
  * @see quarticInOut For strong motion at both ends
  * @see cubicOut For gentler deceleration
  * @see quinticOut For even stronger deceleration
  * @see Visualize at https://easings.net/#easeOutQuart
  */
 inline constexpr detail::quarticOutEasing quarticOut{};

 /**
  * @brief Quartic polynomial easing with strong acceleration and deceleration.
  *
  * The `quarticInOut` easing function combines strong acceleration and deceleration
  * for confident, purposeful motion. More dramatic than cubic, less extreme than quintic.
  *
  * Motion profile:
  * - First half: Strong acceleration (t⁴)
  * - Midpoint: Maximum velocity
  * - Second half: Strong deceleration
  * - Creates pronounced S-curve
  *
  * This easing is appropriate for:
  * - **Emphasized transitions**: Strong acceleration and deceleration
  * - **Current design patterns**: Strong deceleration curves
  * - **Important animations**: Focal transitions
  * - **Medium durations**: 300-600ms range
  *
  * Mathematically: Combines quarticIn for t < 0.5 and quarticOut for t >= 0.5
  *
  * @code
  * // Page transition with impact
  * auto x = tweeny::from(0.0f)
  *   .to(1920.0f)
  *   .via(easing::quarticInOut)
  *   .during(45U)
  *   .build();
  * @endcode
  *
  * @see quarticIn For only strong acceleration
  * @see quarticOut For only strong deceleration
  * @see cubicInOut For gentler motion
  * @see quinticInOut For more dramatic motion
  * @see Visualize at https://easings.net/#easeInOutQuart
  */
 inline constexpr detail::quarticInOutEasing quarticInOut{};

 /**
  * @brief Quintic polynomial easing (t⁵) with very strong acceleration.
  *
  * The `quinticIn` easing function uses a power-5 curve, creating the strongest
  * polynomial acceleration. It's more extreme than quartic but less than exponential.
  *
  * Characteristics:
  * - Very strong acceleration
  * - Polynomial with power of 5
  * - Extremely slow start
  * - Dramatic finish
  *
  * This easing is ideal for:
  * - **Maximum polynomial emphasis**: Strongest polynomial option
  * - **Dramatic effects**: Hero animations, focal transitions
  * - **Long animations**: Over 600ms durations
  * - **Alternative to exponential**: Slightly less extreme
  *
  * Mathematically: `f(t) = t⁵`
  *
  * @code
  * // Dramatic hero section reveal
  * auto scale = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::quinticIn)
  *   .during(60U)
  *   .build();
  * @endcode
  *
  * @see quinticOut For very strong deceleration
  * @see quinticInOut For very strong motion at both ends
  * @see quarticIn For slightly gentler acceleration
  * @see exponentialIn For even more extreme acceleration
  * @see Visualize at https://easings.net/#easeInQuint
  */
 inline constexpr detail::quinticInEasing quinticIn{};

 /**
  * @brief Quintic polynomial easing (t⁵) with very strong deceleration.
  *
  * The `quinticOut` easing function provides the strongest polynomial deceleration,
  * creating smooth but dramatic slowdown. Popular for animations with long durations
  * (500ms+) requiring strong deceleration.
  *
  * Characteristics:
  * - Very strong deceleration
  * - Long, smooth tail
  * - Very strong deceleration with extended slowdown phase
  * - Strongest polynomial option
  *
  * This easing excels at:
  * - **Applications prioritizing strong visual emphasis**: Galleries, portfolios, marketing sites
  * - **Animations with extended durations**: Requiring maximum polynomial deceleration
  * - **Longer animations**: 500ms+ durations
  * - **Emphasized arrivals**: Strong focus on end state
  *
  * QuinticOut provides the strongest polynomial deceleration while maintaining a
  * smooth, continuous curve.
  *
  * Mathematically: `f(t) = 1 - (1-t)⁵`
  *
  * @code
  * // Premium modal entrance
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::quinticOut)
  *   .during(45U)
  *   .build();
  * @endcode
  *
  * @see quinticIn For very strong acceleration
  * @see quinticInOut For very strong motion at both ends
  * @see quarticOut For slightly gentler deceleration
  * @see exponentialOut For even more extreme deceleration
  * @see Visualize at https://easings.net/#easeOutQuint
  */
 inline constexpr detail::quinticOutEasing quinticOut{};

 /**
  * @brief Quintic polynomial easing with very strong acceleration and deceleration.
  *
  * The `quinticInOut` easing function provides the strongest polynomial S-curve,
  * creating powerful motion with maximum polynomial emphasis.
  *
  * Motion profile:
  * - First half: Very strong acceleration (t⁵)
  * - Midpoint: Maximum velocity
  * - Second half: Very strong deceleration
  * - Creates dramatic S-curve
  *
  * This easing is appropriate for:
  * - **Applications prioritizing strong visual emphasis**: Galleries, portfolios, marketing sites
  * - **Animations requiring maximum polynomial emphasis**: At both acceleration and deceleration phases
  * - **Important transitions**: Focal, memorable moments
  * - **Longer animations**: 500ms+ durations
  *
  * Mathematically: Combines quinticIn for t < 0.5 and quinticOut for t >= 0.5
  *
  * @code
  * // Premium screen transition
  * auto x = tweeny::from(0.0f)
  *   .to(1920.0f)
  *   .via(easing::quinticInOut)
  *   .during(50U)
  *   .build();
  * @endcode
  *
  * @see quinticIn For only very strong acceleration
  * @see quinticOut For only very strong deceleration
  * @see quarticInOut For gentler motion
  * @see exponentialInOut For more extreme motion
  * @see Visualize at https://easings.net/#easeInOutQuint
  */
 inline constexpr detail::quinticInOutEasing quinticInOut{};

 /**
  * @brief Sinusoidal easing based on sine wave for smooth acceleration.
  *
  * The `sinusoidalIn` easing function uses a sine curve for acceleration, creating
  * extremely smooth, natural motion. Based on trigonometric functions rather than
  * polynomials.
  *
  * Characteristics:
  * - Very smooth acceleration
  * - Natural, organic feel
  * - No abrupt velocity changes
  * - Gentle but noticeable
  *
  * This easing is ideal for:
  * - **Natural motion**: Organic, flowing animations
  * - **Smooth camera moves**: Pans, zooms, orbits
  * - **Elegant transitions**: Refined, sophisticated feel
  * - **Accessible animations**: Gentle on motion sensitivity
  *
  * Mathematically: `f(t) = 1 - cos(t * π/2)`
  *
  * @code
  * // Smooth camera pan
  * auto x = tweeny::from(0.0f)
  *   .to(1000.0f)
  *   .via(easing::sinusoidalIn)
  *   .during(60U)
  *   .build();
  * @endcode
  *
  * @see sinusoidalOut For smooth deceleration
  * @see sinusoidalInOut For smooth motion at both ends
  * @see quadraticIn For similar gentleness
  * @see Visualize at https://easings.net/#easeInSine
  */
 inline constexpr detail::sinusoidalInEasing sinusoidalIn{};

 /**
  * @brief Sinusoidal easing based on sine wave for smooth deceleration.
  *
  * The `sinusoidalOut` easing function uses a sine curve for deceleration, creating
  * extremely smooth, natural motion that settles gently.
  *
  * Characteristics:
  * - Very smooth deceleration
  * - Natural, flowing motion
  * - Gentle slowdown
  * - Mathematically elegant
  *
  * This easing excels at:
  * - **Natural UI motion**: Smooth, organic feel
  * - **Continuous animations**: Loops, cycles, repeated motion
  * - **Gentle transitions**: Calm, relaxed interfaces
  * - **Accessible design**: Motion-sensitivity friendly
  *
  * SinusoidalOut is excellent when you want smoothness above all else. It's
  * gentler than quadratic while still providing clear easing.
  *
  * Mathematically: `f(t) = sin(t * π/2)`
  *
  * @code
  * // Smooth fade in
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::sinusoidalOut)
  *   .during(30U)
  *   .build();
  * @endcode
  *
  * @see sinusoidalIn For smooth acceleration
  * @see sinusoidalInOut For smooth motion at both ends
  * @see quadraticOut For similar gentleness
  * @see Visualize at https://easings.net/#easeOutSine
  */
 inline constexpr detail::sinusoidalOutEasing sinusoidalOut{};

 /**
  * @brief Sinusoidal easing with smooth acceleration and deceleration.
  *
  * The `sinusoidalInOut` easing function uses sine curves for both acceleration
  * and deceleration, creating the smoothest possible S-curve motion.
  *
  * Motion profile:
  * - First half: Smooth sine-based acceleration
  * - Midpoint: Maximum velocity
  * - Second half: Smooth sine-based deceleration
  * - Creates extremely smooth S-curve
  *
  * This easing is appropriate for:
  * - **Natural, organic motion**: Smoothest easing available
  * - **Continuous loops**: Seamless repeated animations
  * - **Calm interfaces**: Relaxed, gentle design systems
  * - **Accessible animations**: Minimal motion stress
  *
  * SinusoidalInOut is the smoothest InOut easing, making it perfect when
  * fluid, natural motion is the priority.
  *
  * Mathematically: `f(t) = (1 - cos(t * π)) / 2`
  *
  * @code
  * // Ultra-smooth transition
  * auto x = tweeny::from(0.0f)
  *   .to(100.0f)
  *   .via(easing::sinusoidalInOut)
  *   .during(40U)
  *   .build();
  * @endcode
  *
  * @see sinusoidalIn For only smooth acceleration
  * @see sinusoidalOut For only smooth deceleration
  * @see quadraticInOut For similar gentleness
  * @see Visualize at https://easings.net/#easeInOutSine
  */
 inline constexpr detail::sinusoidalInOutEasing sinusoidalInOut{};

 /**
  * @brief Stepped easing that holds the start value until the keyframe completes.
  *
  * The `stepped` easing function returns the starting value throughout the entire
  * duration of the keyframe segment, only jumping to the target value when moving
  * to the next keyframe. This creates instant transitions between keyframes without
  * any interpolation within each segment.
  *
  * Characteristics:
  * - No interpolation within keyframe segments
  * - Holds start value until keyframe ends
  * - Instant jumps between keyframes
  * - Creates discrete, stair-step motion
  *
  * This easing is ideal for:
  * - **Discrete state transitions**: Values that shouldn't interpolate smoothly
  * - **Keyframe-based animations**: Step through distinct poses or states
  * - **Frame-by-frame effects**: Hold each frame without blending
  * - **Boolean-like values**: Properties that need instant changes
  * - **Sprite switching**: Change sprites at keyframe boundaries
  * - **Cut transitions**: Instant changes without fading
  *
  * Stepped is fundamentally different from other easings because it eliminates
  * interpolation entirely within each keyframe segment, creating hard cuts between
  * animation states.
  *
  * @code
  * // Hold value at each keyframe, jump instantly between them
  * auto value = tweeny::from(0)
  *   .to(10).via(easing::stepped).during(100U)
  *   .to(20).via(easing::stepped).during(100U)
  *   .to(30).via(easing::stepped).during(100U)
  *   .build();
  *
  * // During frames 0-99: returns 0
  * // During frames 100-199: returns 10
  * // During frames 200-299: returns 20
  * // At frame 300: returns 30
  *
  * // Useful for sprite animation indices
  * auto spriteIndex = tweeny::from(0)
  *   .to(1).via(easing::stepped).during(10U)
  *   .to(2).via(easing::stepped).during(10U)
  *   .to(3).via(easing::stepped).during(10U)
  *   .build();
  * // Holds each sprite index for 10 frames, then instantly switches
  * @endcode
  *
  * @see linear For smooth constant-velocity interpolation
  */
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


/**
 * @file easing.h
 * The purpose of this file is to list all bundled easings. Each easing is defined
 * in its own header under include/tweeny/detail/easing/. Users may include individual
 * easing headers or this header to get all easings.
 */

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

/**
 * @namespace tweeny::easing
 * @brief Contains all built-in easing functions for controlling animation curves.
 *
 * Provides 30+ easing functions in In, Out, and InOut variants for creating natural-looking
 * animations.
 */
namespace tweeny::easing {
 /**
  * @brief Easing function that creates anticipation by pulling back before moving forward.
  *
  * The `backIn` easing function begins by moving backwards slightly (overshooting in the
  * negative direction), then reverses and accelerates toward the target value. This creates
  * an anticipation effect similar to pulling back a slingshot or winding up before a throw.
  *
  * The backward motion at the start makes this easing particularly effective for animations
  * that benefit from telegraphing or anticipation, such as:
  * - UI elements that "wind up" before sliding in
  * - Character movements that show preparation before action
  * - Camera movements that pull back before zooming forward
  *
  * Mathematically, this easing uses the formula: `c * t * t * ((s + 1) * t - s) + b`
  * where `s` controls the overshoot amount (typically 1.70158).
  *
  * @note The animation will briefly have values less than the start value before proceeding
  * to the target. Ensure your animation system can handle values outside the expected range.
  *
  * @code
  * // Create a tween with back-in easing for anticipation effect
  * auto tween = tweeny::from(0.0f)
  *   .to(100.0f)
  *   .via(easing::backIn)
  *   .during(60U)
  *   .build();
  *
  * // The animation will dip below 0.0f briefly before accelerating to 100.0f
  * @endcode
  *
  * @see backOut For overshoot at the end instead of the beginning
  * @see backInOut For anticipation at the start and overshoot at the end
  * @see Visualize at https://easings.net/#easeInBack
  */
 inline constexpr detail::backInEasing backIn{};

 /**
  * @brief Easing function that overshoots the target before settling back.
  *
  * The `backOut` easing function accelerates toward and past the target value, then pulls
  * back to settle at the final position. This creates an overshoot or "spring-back" effect
  * that adds liveliness and energy to animations.
  *
  * This easing is particularly effective for:
  * - UI elements that pop into place with energy (buttons, dialogs, notifications)
  * - Object arrivals that should feel bouncy and dynamic
  * - Emphasizing the end state of an animation
  * - Adding personality to mechanical movements
  *
  * The overshoot creates a more natural, less rigid feel compared to standard easing
  * functions. It's widely used in iOS, Android, and Material Design animation patterns
  * to make interactions feel more responsive and playful.
  *
  * Mathematically, this is the inverse of backIn, applied at the end of the transition.
  *
  * @note The animation will briefly exceed the target value before settling. Ensure your
  * rendering or logic can handle values beyond the specified range.
  *
  * @code
  * // Animate a button with overshoot for a lively effect
  * auto scale = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::backOut)
  *   .during(30U)
  *   .build();
  *
  * // Scale will exceed 1.0f (e.g., 1.1f) before settling back to 1.0f
  * @endcode
  *
  * @see backIn For anticipation at the start instead of overshoot at the end
  * @see backInOut For both anticipation and overshoot
  * @see elasticOut For a more pronounced oscillating overshoot effect
  * @see Visualize at https://easings.net/#easeOutBack
  */
 inline constexpr detail::backOutEasing backOut{};

 /**
  * @brief Easing function combining anticipation at the start and overshoot at the end.
  *
  * The `backInOut` easing function creates a dramatic motion curve by pulling back before
  * the start (anticipation), accelerating through the middle, then overshooting past the
  * target before settling (spring-back). This combines the effects of both backIn and
  * backOut for maximum expressiveness.
  *
  * This easing creates highly dynamic animations suitable for:
  * - Attention-grabbing transitions that need maximum visual interest
  * - Character animations requiring wind-up and follow-through
  * - Scene transitions with dramatic flair
  * - Emphasizing both the start and end states of an animation
  *
  * The dual overshoot (negative at start, positive at end) makes this one of the most
  * expressive easings, but it should be used judiciously as it can feel exaggerated if
  * overused. It works best for focal animations rather than ambient motion.
  *
  * @note The animation will have values outside the start-to-target range at both ends.
  * During the first half, values will dip below the start; during the second half, values
  * will exceed the target before settling.
  *
  * @code
  * // Create a dramatic page transition
  * auto position = tweeny::from(-100.0f)
  *   .to(100.0f)
  *   .via(easing::backInOut)
  *   .during(90U)
  *   .build();
  *
  * // Position will go below -100.0f at start and above 100.0f near end
  * @endcode
  *
  * @see backIn For only anticipation without overshoot
  * @see backOut For only overshoot without anticipation
  * @see elasticInOut For a more extreme oscillating version
  * @see Visualize at https://easings.net/#easeInOutBack
  */
 inline constexpr detail::backInOutEasing backInOut{};

 /**
  * @brief Bounce easing simulating a ball bouncing with increasing height at the start.
  *
  * The `bounceIn` easing function creates a bouncing effect before the animation begins,
  * like a ball dropped from above that bounces progressively higher before launching into
  * the main motion. This is the reverse of bounceOut's natural physics.
  *
  * Characteristics:
  * - Multiple discrete "bounces" at the start
  * - Each bounce is higher than the previous
  * - Creates anticipation through impact-based motion
  * - Less commonly used than bounceOut
  *
  * This easing works well for:
  * - **Landing preparation**: Elements about to drop into place
  * - **Impact anticipation**: Building up to a collision
  * - **Reverse playback effects**: Bounced animations played backward
  * - **Stylized entrances**: Cartoon-style wind-up effects
  *
  * BounceIn is less intuitive than bounceOut because it reverses natural physics (balls
  * don't normally bounce higher each time). Consider backIn for more natural anticipation.
  *
  * @code
  * // Element that bounces before sliding in
  * auto position = tweeny::from(0.0f)
  *   .to(200.0f)
  *   .via(easing::bounceIn)
  *   .during(50U)
  *   .build();
  * @endcode
  *
  * @see bounceOut For natural ball-drop bouncing at the end
  * @see bounceInOut For bouncing at both start and end
  * @see backIn For simpler anticipation without discrete impacts
  * @see Visualize at https://easings.net/#easeInBounce
  */
 inline constexpr detail::bounceInEasing bounceIn{};

 /**
  * @brief Bounce easing simulating a ball dropping and bouncing to rest.
  *
  * The `bounceOut` easing function creates the classic ball-drop effect where an object
  * bounces several times with decreasing height before coming to rest. This mimics real-world
  * physics of an inelastic collision and is one of the most recognizable easing patterns.
  *
  * Characteristics:
  * - Multiple discrete bounces with decreasing amplitude
  * - Simulates impact and energy loss
  * - Creates playful, physical motion
  * - Widely recognized and understood by users
  *
  * This easing excels at:
  * - **Object drops**: Items falling into place
  * - **Landing animations**: Characters, UI elements touching down
  * - **Playful interactions**: Buttons, icons, notifications
  * - **Game elements**: Collectibles, power-ups, score displays
  * - **Error/success feedback**: Visual confirmation with personality
  * - **Cartoon physics**: Exaggerated, entertaining motion
  *
  * BounceOut is more popular than elasticOut when you want discrete impacts rather than
  * smooth oscillation. It conveys weight and physicality better than spring-based easings.
  *
  * The bouncing creates natural emphasis on the final position, making it excellent for
  * drawing attention to where something lands.
  *
  * @code
  * // Notification dropping in from above
  * auto y = tweeny::from(-100.0f)
  *   .to(0.0f)
  *   .via(easing::bounceOut)
  *   .during(45U)
  *   .build();
  *
  * // Button that bounces into place
  * auto scale = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::bounceOut)
  *   .during(40U)
  *   .build();
  * @endcode
  *
  * @see bounceIn For inverse bouncing at the start
  * @see bounceInOut For bouncing at both ends
  * @see elasticOut For smooth spring-like alternative
  * @see Visualize at https://easings.net/#easeOutBounce
  */
 inline constexpr detail::bounceOutEasing bounceOut{};

 /**
  * @brief Bounce easing with bouncing at both start and end.
  *
  * The `bounceInOut` easing function combines reverse bouncing at the start with natural
  * bouncing at the end. The motion bounces with increasing height initially, accelerates
  * through the middle, then bounces to rest at the target.
  *
  * Motion profile:
  * - First half: Bounces with increasing amplitude (bounceIn)
  * - Midpoint: Smooth transition
  * - Second half: Bounces with decreasing amplitude (bounceOut)
  * - Creates playful, impact-based motion at both ends
  *
  * This easing is appropriate for:
  * - **Playful transitions**: Fun, energetic scene changes
  * - **Game UI**: High-energy, cartoon-style interfaces
  * - **Children's applications**: Whimsical, entertaining motion
  * - **Attention-grabbing effects**: Elements that need maximum personality
  *
  * BounceInOut is quite dramatic and can feel excessive for business applications,
  * productivity tools, and enterprise software where subtlety is preferred. The dual
  * bouncing works best in contexts where playfulness is a design goal.
  *
  * @code
  * // Playful modal transition
  * auto scale = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::bounceInOut)
  *   .during(60U)
  *   .build();
  * @endcode
  *
  * @see bounceIn For bouncing only at the start
  * @see bounceOut For bouncing only at the end (more commonly useful)
  * @see elasticInOut For spring-like alternative
  * @see Visualize at https://easings.net/#easeInOutBounce
  */
 inline constexpr detail::bounceInOutEasing bounceInOut{};

 /**
  * @brief Circular easing based on quarter-circle arc for smooth acceleration.
  *
  * The `circularIn` easing function accelerates following the curve of a quarter circle,
  * creating smooth, gradual acceleration. The motion follows the equation `1 - sqrt(1 - t²)`,
  * which traces a circular arc.
  *
  * Characteristics:
  * - Smooth, continuous acceleration
  * - More gradual than quadratic, less than cubic
  * - No sharp transitions in velocity
  * - Mathematically elegant curve
  *
  * This easing is ideal for:
  * - **Natural motion**: Movements that need organic feel
  * - **Camera movements**: Smooth pans and zooms
  * - **Scroll animations**: Gentle acceleration for reading comfort
  * - **Subtle UI transitions**: Motion with moderate acceleration
  *
  * Circular easings strike a balance between the gentle quadratic and the more aggressive
  * cubic curves, making them versatile for many contexts.
  *
  * @code
  * // Smooth camera pan
  * auto cameraX = tweeny::from(0.0f)
  *   .to(1000.0f)
  *   .via(easing::circularIn)
  *   .during(90U)
  *   .build();
  * @endcode
  *
  * @see circularOut For circular deceleration
  * @see circularInOut For circular acceleration and deceleration
  * @see quadraticIn For gentler acceleration
  * @see cubicIn For stronger acceleration
  * @see Visualize at https://easings.net/#easeInCirc
  */
 inline constexpr detail::circularInEasing circularIn{};

 /**
  * @brief Circular easing based on quarter-circle arc for smooth deceleration.
  *
  * The `circularOut` easing function decelerates following a circular curve, creating
  * smooth, natural-looking motion that settles gently to rest. Popular for UI animations
  * requiring moderate deceleration without dramatic emphasis.
  *
  * Characteristics:
  * - Smooth, continuous deceleration
  * - Natural feeling without being too soft or aggressive
  * - Balanced between gentle and pronounced
  * - Widely used in production interfaces
  *
  * This easing excels at:
  * - **Modern UI animations**: Cards, panels, menus
  * - **Material Design patterns**: Following Google's motion guidelines
  * - **Content transitions**: Page changes, tab switches
  * - **Professional applications**: Business and productivity software
  * - **General-purpose motion**: Versatile for many animation types
  *
  * CircularOut works well in most contexts. It's less dramatic than cubic but with
  * smoother deceleration than quadratic.
  *
  * @code
  * // Menu panel sliding in
  * auto x = tweeny::from(-300.0f)
  *   .to(0.0f)
  *   .via(easing::circularOut)
  *   .during(35U)
  *   .build();
  * @endcode
  *
  * @see circularIn For circular acceleration
  * @see circularInOut For circular motion at both ends
  * @see quadraticOut For gentler alternative
  * @see cubicOut For more pronounced alternative
  * @see Visualize at https://easings.net/#easeOutCirc
  */
 inline constexpr detail::circularOutEasing circularOut{};

 /**
  * @brief Circular easing with smooth acceleration and deceleration.
  *
  * The `circularInOut` easing function uses circular curves for both acceleration and
  * deceleration, creating balanced motion suitable for general UI work. This is a
  * popular all-purpose easing.
  *
  * Motion profile:
  * - First half: Circular acceleration
  * - Midpoint: Maximum velocity
  * - Second half: Circular deceleration
  * - Creates smooth S-curve
  *
  * This easing is appropriate for:
  * - **All-purpose UI animations**: Buttons, dialogs, drawers
  * - **Material Design**: Recommended in Google's motion guidelines
  * - **Business applications**: Productivity tools, enterprise apps
  * - **Default animation choice**: Safe, versatile motion curve
  *
  * CircularInOut is often recommended as a starting point for animations because it
  * provides polish without being too subtle or too dramatic.
  *
  * @code
  * // Modal dialog appearance
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::circularInOut)
  *   .during(25U)
  *   .build();
  * @endcode
  *
  * @see circularIn For only acceleration
  * @see circularOut For only deceleration
  * @see quadraticInOut For gentler motion
  * @see cubicInOut For more dramatic motion
  * @see Visualize at https://easings.net/#easeInOutCirc
  */
 inline constexpr detail::circularInOutEasing circularInOut{};

 /**
  * @brief Cubic polynomial easing (t³) with moderate acceleration.
  *
  * The `cubicIn` easing function uses a cubic power curve for acceleration, providing
  * smooth, noticeable easing that's more pronounced than quadratic but less extreme
  * than quartic or exponential.
  *
  * Characteristics:
  * - Polynomial acceleration with power of 3
  * - Balanced between gentle and dramatic
  * - Widely used and well-understood
  * - Good default for many animation types
  *
  * This easing is ideal for:
  * - **Standard UI animations**: Menus, panels, overlays
  * - **Easing beginners**: Easy to understand and predict
  * - **General-purpose acceleration**: Versatile for many contexts
  * - **Medium-length animations**: 200-500ms durations
  *
  * CubicIn is a popular choice because it provides clear easing without being subtle
  * (like quadratic) or extreme (like quartic/exponential).
  *
  * Mathematically: `f(t) = t³`
  *
  * @code
  * // Dropdown menu opening
  * auto height = tweeny::from(0.0f)
  *   .to(200.0f)
  *   .via(easing::cubicIn)
  *   .during(30U)
  *   .build();
  * @endcode
  *
  * @see cubicOut For cubic deceleration
  * @see cubicInOut For cubic motion at both ends
  * @see quadraticIn For gentler acceleration
  * @see quarticIn For stronger acceleration
  * @see Visualize at https://easings.net/#easeInCubic
  */
 inline constexpr detail::cubicInEasing cubicIn{};

 /**
  * @brief Cubic polynomial easing (t³) with moderate deceleration.
  *
  * The `cubicOut` easing function provides smooth, balanced deceleration that's popular
  * across many design systems. It's pronounced enough to be noticeable but not dramatic.
  *
  * Characteristics:
  * - Smooth, natural-feeling deceleration
  * - More pronounced than quadratic, gentler than quartic
  * - Industry-standard motion curve
  * - Works well for most animation types
  *
  * This easing excels at:
  * - **Web interfaces**: Following CSS animation best practices
  * - **Mobile UI**: iOS and Android design patterns
  * - **Content animations**: Cards, lists, grids
  * - **Default easing choice**: Safe for most situations
  * - **User-triggered actions**: Button presses, toggles, switches
  *
  * CubicOut is one of the most commonly used easings in production interfaces. It
  * provides clear polish without drawing excessive attention to the motion itself.
  *
  * Mathematically: `f(t) = 1 - (1-t)³`
  *
  * @code
  * // Button press feedback
  * auto scale = tweeny::from(1.0f)
  *   .to(0.95f)
  *   .via(easing::cubicOut)
  *   .during(10U)
  *   .build();
  *
  * // Card appearing
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::cubicOut)
  *   .during(25U)
  *   .build();
  * @endcode
  *
  * @see cubicIn For cubic acceleration
  * @see cubicInOut For cubic motion at both ends
  * @see quadraticOut For gentler deceleration
  * @see quarticOut For stronger deceleration
  * @see Visualize at https://easings.net/#easeOutCubic
  */
 inline constexpr detail::cubicOutEasing cubicOut{};

 /**
  * @brief Cubic polynomial easing with balanced acceleration and deceleration.
  *
  * The `cubicInOut` easing function combines cubic acceleration and deceleration for
  * smooth, professional motion. This is one of the most popular general-purpose easings.
  *
  * Motion profile:
  * - First half: Cubic acceleration (t³)
  * - Midpoint: Maximum velocity
  * - Second half: Cubic deceleration
  * - Creates smooth, balanced S-curve
  *
  * This easing is appropriate for:
  * - **All-purpose animations**: When in doubt, use this
  * - **Web standards**: CSS ease-in-out equivalent
  * - **Design system defaults**: Common in component libraries
  * - **Cross-platform consistency**: Works well everywhere
  * - **Medium animations**: 200-500ms sweet spot
  *
  * CubicInOut is frequently the default easing in design systems and animation libraries
  * because it provides clear, professional motion that works for most scenarios.
  *
  * Mathematically: Combines cubicIn for t < 0.5 and cubicOut for t >= 0.5
  *
  * @code
  * // Page transition
  * auto x = tweeny::from(0.0f)
  *   .to(1920.0f)
  *   .via(easing::cubicInOut)
  *   .during(40U)
  *   .build();
  * @endcode
  *
  * @see cubicIn For only acceleration
  * @see cubicOut For only deceleration
  * @see quadraticInOut For gentler motion
  * @see quarticInOut For more dramatic motion
  * @see Visualize at https://easings.net/#easeInOutCubic
  */
 inline constexpr detail::cubicInOutEasing cubicInOut{};
 /**
  * @brief Default easing function, alias for linear easing.
  *
  * The `def` easing is a convenience alias for `linear`, providing the same constant-velocity
  * interpolation with no acceleration or deceleration. It exists as a semantic indicator that
  * the default easing behavior is being explicitly chosen.
  *
  * Using `def` instead of `linear` can make code intent clearer in contexts where you want to
  * explicitly state "use the default behavior" rather than specifically requesting linear motion.
  * However, both are functionally identical.
  *
  * This is the easing applied when no via() call is made in the tween builder.
  *
  * @code
  * // These three tweens are functionally identical:
  * auto t1 = tweeny::from(0).to(100).during(60U).build();  // Implicit default
  * auto t2 = tweeny::from(0).to(100).via(easing::def).during(60U).build();
  * auto t3 = tweeny::from(0).to(100).via(easing::linear).during(60U).build();
  * @endcode
  *
  * @see linear For the primary documentation of this easing behavior
  */
 inline constexpr detail::defaultEasing def{};

 /**
  * @brief Elastic easing with oscillating spring-like motion at the start.
  *
  * The `elasticIn` easing function creates a spring or elastic band effect that oscillates
  * with increasing amplitude before reaching the starting point of the animation. The motion
  * resembles pulling back an elastic band that vibrates as tension builds.
  *
  * The oscillation characteristics:
  * - Multiple back-and-forth swings before the main motion begins
  * - Amplitude increases as the animation progresses
  * - Creates a "winding up" or "charging" effect
  * - More pronounced than backIn's single overshoot
  *
  * This easing is particularly effective for:
  * - **Magical or fantasy effects**: Spell charging, energy gathering
  * - **Exaggerated cartoon animations**: Extreme anticipation and wind-up
  * - **Attention-grabbing entrances**: Elements that need dramatic introduction
  * - **Game power-ups**: Visual feedback for charging actions
  * - **Playful UI elements**: Whimsical, high-energy interactions
  *
  * The elastic effect is more dramatic than back easing, making it suitable for contexts
  * where strong visual emphasis or entertainment value is desired. It's less appropriate
  * for subtle or professional interfaces.
  *
  * Mathematically, this uses a decaying sine wave with exponential amplitude growth.
  *
  * @warning This easing creates significant overshoot in both directions. Values will
  * oscillate well beyond the start value in both positive and negative directions. Ensure
  * your rendering system can handle these extreme values gracefully.
  *
  * @code
  * // Magical charging effect before an action
  * auto glow = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::elasticIn)
  *   .during(45U)
  *   .build();
  *
  * // The glow will oscillate negative before reaching 1.0f
  * // producing a "charging" visual effect
  * @endcode
  *
  * @see elasticOut For spring-like oscillation at the end
  * @see elasticInOut For oscillation at both start and end
  * @see backIn For a simpler single-overshoot alternative
  * @see Visualize at https://easings.net/#easeInElastic
  */
 inline constexpr detail::elasticInEasing elasticIn{};

 /**
  * @brief Elastic easing with spring-like oscillation at the end.
  *
  * The `elasticOut` easing function creates a natural spring or rubber band effect that
  * overshoots and oscillates around the target value before settling. This is one of the
  * most visually distinctive and playful easings, mimicking real-world elastic physics.
  *
  * The oscillation characteristics:
  * - Overshoots the target value multiple times
  * - Amplitude decreases with each oscillation (damped motion)
  * - Settles naturally at the final value
  * - Creates a bouncy, energetic feel without the hard impacts of bounce easing
  *
  * This easing excels at:
  * - **Playful UI animations**: Buttons, toggles, modal appearances
  * - **Game elements**: Power-up notifications, achievement popups, score displays
  * - **Cartoon-style motion**: Exaggerated, entertaining character movements
  * - **Attention direction**: Drawing eyes to important elements
  * - **Spring simulation**: Rubber bands, diving boards, springy objects
  * - **Joyful interactions**: Adding personality to standard UI patterns
  *
  * ElasticOut is widely used in modern mobile UI design to add energy and delight to
  * interactions. It's more pronounced than backOut but smoother than bounceOut.
  *
  * The spring feel makes animations memorable and adds perceived responsiveness, making
  * interfaces feel "alive" rather than mechanical.
  *
  * Mathematically, this uses a decaying sine wave with exponentially decreasing amplitude.
  *
  * @warning Values will oscillate beyond the target in both directions before settling.
  * For example, animating from 0 to 100 might temporarily reach 110, then 95, then 102,
  * before settling at 100. Ensure clipping or overflow handling is appropriate.
  *
  * @code
  * // Springy button press feedback
  * auto scale = tweeny::from(1.0f)
  *   .to(1.2f)
  *   .via(easing::elasticOut)
  *   .during(40U)
  *   .build();
  *
  * // Scale will overshoot 1.2f (maybe 1.3f) then oscillate
  * // down and up before settling at exactly 1.2f
  *
  * // Modal dialog with playful entrance
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::elasticOut)
  *   .during(50U)
  *   .build();
  * @endcode
  *
  * @see elasticIn For spring anticipation at the start
  * @see elasticInOut For oscillation at both ends
  * @see backOut For a subtler single-overshoot alternative
  * @see bounceOut For a similar but impact-based bouncing effect
  * @see Visualize at https://easings.net/#easeOutElastic
  */
 inline constexpr detail::elasticOutEasing elasticOut{};

 /**
  * @brief Elastic easing combining spring oscillation at both start and end.
  *
  * The `elasticInOut` easing function creates dramatic elastic motion with oscillation
  * during both the initial acceleration and final deceleration phases. The animation
  * winds up with spring-like vibration, accelerates smoothly through the middle, then
  * oscillates around the target before settling.
  *
  * The motion profile:
  * - First half: Oscillates with increasing amplitude (elasticIn behavior)
  * - Middle: Smooth transition through the midpoint
  * - Second half: Oscillates with decreasing amplitude (elasticOut behavior)
  * - Creates the most dramatic elastic effect available
  *
  * This easing is appropriate for:
  * - **Hero animations**: Focal transitions that demand attention
  * - **Game events**: Critical moments, boss appearances, dramatic reveals
  * - **Cartoon physics**: Exaggerated, entertaining motion for stylized visuals
  * - **Transitions with flair**: Scene changes that need maximum personality
  * - **Experimental UI**: Interfaces prioritizing delight over convention
  *
  * ElasticInOut is the most expressive elastic variant but also the most extreme. It works
  * best for animations that are:
  * - Intentionally playful or whimsical
  * - The primary focus of user attention
  * - Part of a stylized, high-energy aesthetic
  * - Not repeated frequently (can become tiresome)
  *
  * Use sparingly; the dual oscillation can feel excessive for everyday interactions.
  * Consider elasticOut or backInOut for a more balanced alternative.
  *
  * @warning This easing produces extreme value overshoots in both directions throughout
  * the animation. During the first half, values oscillate around the start; during the
  * second half, around the target. Plan for values far outside the expected range.
  *
  * @code
  * // Dramatic screen transition
  * auto position = tweeny::from(0.0f)
  *   .to(1920.0f)
  *   .via(easing::elasticInOut)
  *   .during(90U)
  *   .build();
  *
  * // Position will oscillate around 0.0f at start and 1920.0f at end
  *
  * // Game title appearing with maximum impact
  * auto scale = tweeny::from(0.0f)
  *   .to(2.0f)
  *   .via(easing::elasticInOut)
  *   .during(75U)
  *   .build();
  * @endcode
  *
  * @see elasticIn For oscillation only at the start
  * @see elasticOut For oscillation only at the end (more commonly useful)
  * @see backInOut For a less extreme but still expressive alternative
  * @see Visualize at https://easings.net/#easeInOutElastic
  */
 inline constexpr detail::elasticInOutEasing elasticInOut{};

 /**
  * @brief Exponential easing with very slow start and explosive acceleration.
  *
  * The `exponentialIn` easing function starts extremely slowly and builds to a very rapid
  * acceleration toward the end. Based on exponential growth (2^x), this creates one of the
  * most dramatic acceleration curves available, with velocity increasing exponentially over time.
  *
  * Characteristics:
  * - Almost no visible motion for the first portion of the animation
  * - Sudden, explosive acceleration in the final phase
  * - Change rate doubles repeatedly as time progresses
  * - Creates extreme contrast between start and end velocity
  *
  * This easing is ideal for:
  * - **Dramatic reveals**: Elements that burst into view
  * - **Explosive effects**: Particle systems, energy blasts, explosions
  * - **Fade-ins with impact**: Starting invisible and suddenly appearing
  * - **Speed-up effects**: Rockets launching, vehicles accelerating
  * - **Tension building**: Slow build-up to sudden release
  *
  * The extreme acceleration curve makes this feel more dramatic than polynomial easings
  * (quadratic, cubic, etc.). The motion appears to "explode" into existence rather than
  * gradually accelerate.
  *
  * Use exponentialIn when you want maximum contrast between the patient start and the
  * explosive finish. For most UI work, cubic or quartic easings provide sufficient
  * acceleration with less extreme behavior.
  *
  * Mathematically: `f(t) = 2^(10 * (t - 1))`
  *
  * @code
  * // Fade in that suddenly snaps to full visibility
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::exponentialIn)
  *   .during(45U)
  *   .build();
  *
  * // Rocket launch with explosive acceleration
  * auto velocity = tweeny::from(0.0f)
  *   .to(1000.0f)
  *   .via(easing::exponentialIn)
  *   .during(120U)
  *   .build();
  * @endcode
  *
  * @see exponentialOut For explosive deceleration at the end
  * @see exponentialInOut For dramatic acceleration and deceleration
  * @see quarticIn For strong but less extreme acceleration
  * @see Visualize at https://easings.net/#easeInExpo
  */
 inline constexpr detail::exponentialInEasing exponentialIn{};

 /**
  * @brief Exponential easing with explosive start and gradual slow-down.
  *
  * The `exponentialOut` easing function starts with maximum velocity and decelerates
  * exponentially, creating a smooth glide to a stop. The initial burst of speed followed
  * by gradual settling creates a powerful, energetic feel.
  *
  * Characteristics:
  * - Immediate, explosive motion at the start
  * - Exponentially decreasing velocity
  * - Long, smooth deceleration phase
  * - Settles gently to final value
  *
  * This easing excels at:
  * - **Quick UI responses**: Instant feedback that settles smoothly
  * - **Impact effects**: Objects hitting and settling into place
  * - **Momentum-based motion**: Thrown objects, swipe gestures
  * - **Energetic entrances**: Elements that burst in with energy
  * - **Modern UI patterns**: iOS-style animations with quick start
  *
  * ExponentialOut is popular in modern mobile design because it provides immediate
  * visual feedback (the fast start) while ending smoothly. Users perceive the interface
  * as highly responsive due to the instant motion.
  *
  * The long tail of deceleration gives animations a "quality feel" - nothing stops
  * abruptly. This is gentler on the eyes than linear or even cubic deceleration.
  *
  * Mathematically: `f(t) = 1 - 2^(-10 * t)`
  *
  * @code
  * // Responsive drawer that slides out quickly then settles
  * auto position = tweeny::from(-300.0f)
  *   .to(0.0f)
  *   .via(easing::exponentialOut)
  *   .during(35U)
  *   .build();
  *
  * // Notification that pops in with energy
  * auto scale = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::exponentialOut)
  *   .during(25U)
  *   .build();
  * @endcode
  *
  * @see exponentialIn For explosive acceleration at the start
  * @see exponentialInOut For explosive motion at both ends
  * @see quarticOut For strong but less extreme deceleration
  * @see Visualize at https://easings.net/#easeOutExpo
  */
 inline constexpr detail::exponentialOutEasing exponentialOut{};

 /**
  * @brief Exponential easing with extreme acceleration and deceleration.
  *
  * The `exponentialInOut` easing function combines explosive acceleration in the first
  * half with explosive deceleration in the second half. This creates one of the most
  * dramatic, high-energy motion curves available, with rapid changes at both ends and
  * smooth motion through the middle.
  *
  * Motion profile:
  * - First half: Exponential acceleration from near-zero velocity
  * - Midpoint: Maximum velocity
  * - Second half: Exponential deceleration to rest
  * - Creates extreme S-curve shape
  *
  * This easing is appropriate for:
  * - **High-impact transitions**: Scene changes, screen swipes
  * - **Dramatic animations**: Hero elements, focal content
  * - **Fast-paced interfaces**: Games, action-oriented apps
  * - **Attention-grabbing motion**: Elements that need maximum visibility
  * - **Strong transitions**: For elements moving significant distances or longer durations (500ms+)
  *
  * ExponentialInOut creates a powerful sense of momentum and energy. The motion feels
  * purposeful and confident. However, the extreme acceleration can be disorienting if
  * overused or applied to large elements.
  *
  * Best practices:
  * - Use for animations under 1 second duration
  * - Apply to elements that move short to medium distances
  * - Reserve for important, infrequent transitions
  * - Consider quarticInOut or quinticInOut as less extreme alternatives
  *
  * Mathematically: Combines exponentialIn for t < 0.5 and exponentialOut for t >= 0.5
  *
  * @code
  * // Page transition with explosive motion
  * auto position = tweeny::from(0.0f)
  *   .to(1920.0f)
  *   .via(easing::exponentialInOut)
  *   .during(40U)
  *   .build();
  *
  * // Modal dialog with dramatic appearance
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::exponentialInOut)
  *   .during(30U)
  *   .build();
  * @endcode
  *
  * @see exponentialIn For only explosive acceleration
  * @see exponentialOut For only explosive deceleration
  * @see quinticInOut For slightly less extreme alternative
  * @see Visualize at https://easings.net/#easeInOutExpo
  */
 inline constexpr detail::exponentialInOutEasing exponentialInOut{};
 /**
  * @brief Linear easing function with constant velocity throughout the animation.
  *
  * The `linear` easing function produces uniform motion with no acceleration or deceleration.
  * The interpolation progresses at a constant rate from start to finish, creating mechanical,
  * predictable movement.
  *
  * Linear easing is characterized by:
  * - Constant velocity: the rate of change never varies
  * - No ease-in or ease-out: motion starts and stops abruptly
  * - Simple mathematical relationship: output = start + (end - start) * progress
  * - Predictable timing: halfway through time means halfway through distance
  *
  * This easing is appropriate for:
  * - **Mechanical objects**: Conveyor belts, pistons, automated systems
  * - **Progress indicators**: Loading bars, timers, countdowns
  * - **Continuous loops**: Rotating objects, scrolling backgrounds
  * - **Data visualization**: Graph animations where consistency is important
  * - **Debug and testing**: Predictable behavior for verification
  *
  * Linear easing is generally **not recommended** for most UI animations because:
  * - Lacks the natural feel of acceleration/deceleration
  * - Abrupt starts and stops can feel jarring
  * - Missing visual polish that easing provides
  * - Human perception expects objects to ease into and out of motion
  *
  * However, it serves as the foundation for all other easing functions and is essential
  * when mechanical precision is more important than natural motion feel.
  *
  * Mathematically, this implements: `f(t) = t` where t is normalized progress [0, 1].
  *
  * @code
  * // Constant velocity scrolling background
  * auto scroll = tweeny::from(0.0f)
  *   .to(1000.0f)
  *   .via(easing::linear)
  *   .during(600U)
  *   .build();
  *
  * // Progress indicator that matches time exactly
  * auto progress = tweeny::from(0)
  *   .to(100)
  *   .via(easing::linear)
  *   .during(100U)
  *   .build();
  *
  * // At frame 50, progress will be exactly 50
  * @endcode
  *
  * @note Linear is the default easing when no via() is specified, though using def is
  * more explicit in code.
  *
  * @see def Alias for linear easing
  * @see quadraticInOut For a gentle alternative with ease-in and ease-out
  * @see sinusoidalInOut For smooth, natural-feeling motion
  */
 inline constexpr detail::linearEasing linear{};

 /**
  * @brief Quadratic polynomial easing (t²) with gentle acceleration.
  *
  * The `quadraticIn` easing function uses a squared power curve for acceleration,
  * providing the gentlest polynomial easing. It's more subtle than cubic but still
  * provides noticeable easing.
  *
  * Characteristics:
  * - Gentle, smooth acceleration
  * - Polynomial with power of 2
  * - Subtle but perceptible easing
  * - Good for beginners and subtle animations
  *
  * This easing is ideal for:
  * - **Subtle UI motion**: When easing should be felt but not seen
  * - **Quick animations**: Short durations where gentle curves work best
  * - **Minimal designs**: Interfaces prioritizing restraint
  * - **Learning easings**: Easy to understand and predict
  *
  * Mathematically: `f(t) = t²`
  *
  * @code
  * // Gentle fade in
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::quadraticIn)
  *   .during(20U)
  *   .build();
  * @endcode
  *
  * @see quadraticOut For gentle deceleration
  * @see quadraticInOut For gentle motion at both ends
  * @see cubicIn For more pronounced acceleration
  * @see Visualize at https://easings.net/#easeInQuad
  */
 inline constexpr detail::quadraticInEasing quadraticIn{};

 /**
  * @brief Quadratic polynomial easing (t²) with gentle deceleration.
  *
  * The `quadraticOut` easing function provides subtle, smooth deceleration. It's the
  * gentlest polynomial easing, perfect when you want polish without drama.
  *
  * Characteristics:
  * - Gentle, smooth deceleration
  * - Subtle but professional feel
  * - Never feels too slow or too fast
  * - Safe choice for any context
  *
  * This easing excels at:
  * - **Subtle UI polish**: Adding refinement without drawing attention
  * - **Fast animations**: 100-200ms durations
  * - **Minimal interfaces**: Clean, understated design systems
  * - **Accessibility-friendly**: Gentle motion reduces disorientation
  * - **Background animations**: Motion that shouldn't distract
  *
  * QuadraticOut is great when you want animations to stay out of the way. It's less
  * dramatic than cubic but smoother than linear due to gradual deceleration.
  *
  * Mathematically: `f(t) = 1 - (1-t)²`
  *
  * @code
  * // Subtle tooltip appearance
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::quadraticOut)
  *   .during(15U)
  *   .build();
  * @endcode
  *
  * @see quadraticIn For gentle acceleration
  * @see quadraticInOut For gentle motion at both ends
  * @see cubicOut For more pronounced deceleration
  * @see Visualize at https://easings.net/#easeOutQuad
  */
 inline constexpr detail::quadraticOutEasing quadraticOut{};

 /**
  * @brief Quadratic polynomial easing with gentle acceleration and deceleration.
  *
  * The `quadraticInOut` easing function provides the most subtle polynomial S-curve.
  * Perfect for animations that need easing but should remain understated.
  *
  * Motion profile:
  * - First half: Gentle acceleration (t²)
  * - Midpoint: Maximum velocity
  * - Second half: Gentle deceleration
  * - Creates subtle S-curve
  *
  * This easing is appropriate for:
  * - **Minimal design systems**: Understated, refined motion
  * - **Fast animations**: Under 200ms where gentle curves shine
  * - **Accessibility**: Motion-sensitive users
  * - **Background motion**: Animations that shouldn't dominate
  *
  * Mathematically: Combines quadraticIn for t < 0.5 and quadraticOut for t >= 0.5
  *
  * @code
  * // Subtle menu transition
  * auto x = tweeny::from(0.0f)
  *   .to(100.0f)
  *   .via(easing::quadraticInOut)
  *   .during(20U)
  *   .build();
  * @endcode
  *
  * @see quadraticIn For only gentle acceleration
  * @see quadraticOut For only gentle deceleration
  * @see cubicInOut For more pronounced motion
  * @see Visualize at https://easings.net/#easeInOutQuad
  */
 inline constexpr detail::quadraticInOutEasing quadraticInOut{};

 /**
  * @brief Quartic polynomial easing (t⁴) with strong acceleration.
  *
  * The `quarticIn` easing function uses a power-4 curve for acceleration, creating
  * pronounced easing that's stronger than cubic but less extreme than exponential.
  *
  * Characteristics:
  * - Strong acceleration curve
  * - Polynomial with power of 4
  * - Very slow start, rapid finish
  * - More dramatic than cubic
  *
  * This easing is ideal for:
  * - **Dramatic entrances**: Elements that burst into view
  * - **Emphasis**: Drawing attention to motion start
  * - **Game animations**: Action-oriented interfaces
  * - **Long animations**: Over 500ms where strong curves work
  *
  * Mathematically: `f(t) = t⁴`
  *
  * @code
  * // Panel sliding in with emphasis
  * auto x = tweeny::from(-400.0f)
  *   .to(0.0f)
  *   .via(easing::quarticIn)
  *   .during(50U)
  *   .build();
  * @endcode
  *
  * @see quarticOut For strong deceleration
  * @see quarticInOut For strong motion at both ends
  * @see cubicIn For gentler acceleration
  * @see quinticIn For even stronger acceleration
  * @see Visualize at https://easings.net/#easeInQuart
  */
 inline constexpr detail::quarticInEasing quarticIn{};

 /**
  * @brief Quartic polynomial easing (t⁴) with strong deceleration.
  *
  * The `quarticOut` easing function provides pronounced deceleration that's popular
  * for animations needing clear emphasis without being as extreme as exponential.
  *
  * Characteristics:
  * - Strong deceleration curve
  * - Smooth, extended slowdown
  * - Emphasizes final position
  * - Balanced between cubic and exponential
  *
  * This easing excels at:
  * - **Emphasized arrivals**: Elements settling with impact
  * - **Current design patterns**: Strong deceleration suitable for emphasized arrivals
  * - **Medium-length animations**: 300-600ms sweet spot
  * - **Purposeful motion**: Clear acceleration and deceleration
  *
  * QuarticOut provides more "oomph" than cubic while remaining smooth and
  * professional. It's a good choice when cubic feels too subtle.
  *
  * Mathematically: `f(t) = 1 - (1-t)⁴`
  *
  * @code
  * // Card sliding into place with emphasis
  * auto y = tweeny::from(200.0f)
  *   .to(0.0f)
  *   .via(easing::quarticOut)
  *   .during(40U)
  *   .build();
  * @endcode
  *
  * @see quarticIn For strong acceleration
  * @see quarticInOut For strong motion at both ends
  * @see cubicOut For gentler deceleration
  * @see quinticOut For even stronger deceleration
  * @see Visualize at https://easings.net/#easeOutQuart
  */
 inline constexpr detail::quarticOutEasing quarticOut{};

 /**
  * @brief Quartic polynomial easing with strong acceleration and deceleration.
  *
  * The `quarticInOut` easing function combines strong acceleration and deceleration
  * for confident, purposeful motion. More dramatic than cubic, less extreme than quintic.
  *
  * Motion profile:
  * - First half: Strong acceleration (t⁴)
  * - Midpoint: Maximum velocity
  * - Second half: Strong deceleration
  * - Creates pronounced S-curve
  *
  * This easing is appropriate for:
  * - **Emphasized transitions**: Strong acceleration and deceleration
  * - **Current design patterns**: Strong deceleration curves
  * - **Important animations**: Focal transitions
  * - **Medium durations**: 300-600ms range
  *
  * Mathematically: Combines quarticIn for t < 0.5 and quarticOut for t >= 0.5
  *
  * @code
  * // Page transition with impact
  * auto x = tweeny::from(0.0f)
  *   .to(1920.0f)
  *   .via(easing::quarticInOut)
  *   .during(45U)
  *   .build();
  * @endcode
  *
  * @see quarticIn For only strong acceleration
  * @see quarticOut For only strong deceleration
  * @see cubicInOut For gentler motion
  * @see quinticInOut For more dramatic motion
  * @see Visualize at https://easings.net/#easeInOutQuart
  */
 inline constexpr detail::quarticInOutEasing quarticInOut{};

 /**
  * @brief Quintic polynomial easing (t⁵) with very strong acceleration.
  *
  * The `quinticIn` easing function uses a power-5 curve, creating the strongest
  * polynomial acceleration. It's more extreme than quartic but less than exponential.
  *
  * Characteristics:
  * - Very strong acceleration
  * - Polynomial with power of 5
  * - Extremely slow start
  * - Dramatic finish
  *
  * This easing is ideal for:
  * - **Maximum polynomial emphasis**: Strongest polynomial option
  * - **Dramatic effects**: Hero animations, focal transitions
  * - **Long animations**: Over 600ms durations
  * - **Alternative to exponential**: Slightly less extreme
  *
  * Mathematically: `f(t) = t⁵`
  *
  * @code
  * // Dramatic hero section reveal
  * auto scale = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::quinticIn)
  *   .during(60U)
  *   .build();
  * @endcode
  *
  * @see quinticOut For very strong deceleration
  * @see quinticInOut For very strong motion at both ends
  * @see quarticIn For slightly gentler acceleration
  * @see exponentialIn For even more extreme acceleration
  * @see Visualize at https://easings.net/#easeInQuint
  */
 inline constexpr detail::quinticInEasing quinticIn{};

 /**
  * @brief Quintic polynomial easing (t⁵) with very strong deceleration.
  *
  * The `quinticOut` easing function provides the strongest polynomial deceleration,
  * creating smooth but dramatic slowdown. Popular for animations with long durations
  * (500ms+) requiring strong deceleration.
  *
  * Characteristics:
  * - Very strong deceleration
  * - Long, smooth tail
  * - Very strong deceleration with extended slowdown phase
  * - Strongest polynomial option
  *
  * This easing excels at:
  * - **Applications prioritizing strong visual emphasis**: Galleries, portfolios, marketing sites
  * - **Animations with extended durations**: Requiring maximum polynomial deceleration
  * - **Longer animations**: 500ms+ durations
  * - **Emphasized arrivals**: Strong focus on end state
  *
  * QuinticOut provides the strongest polynomial deceleration while maintaining a
  * smooth, continuous curve.
  *
  * Mathematically: `f(t) = 1 - (1-t)⁵`
  *
  * @code
  * // Premium modal entrance
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::quinticOut)
  *   .during(45U)
  *   .build();
  * @endcode
  *
  * @see quinticIn For very strong acceleration
  * @see quinticInOut For very strong motion at both ends
  * @see quarticOut For slightly gentler deceleration
  * @see exponentialOut For even more extreme deceleration
  * @see Visualize at https://easings.net/#easeOutQuint
  */
 inline constexpr detail::quinticOutEasing quinticOut{};

 /**
  * @brief Quintic polynomial easing with very strong acceleration and deceleration.
  *
  * The `quinticInOut` easing function provides the strongest polynomial S-curve,
  * creating powerful motion with maximum polynomial emphasis.
  *
  * Motion profile:
  * - First half: Very strong acceleration (t⁵)
  * - Midpoint: Maximum velocity
  * - Second half: Very strong deceleration
  * - Creates dramatic S-curve
  *
  * This easing is appropriate for:
  * - **Applications prioritizing strong visual emphasis**: Galleries, portfolios, marketing sites
  * - **Animations requiring maximum polynomial emphasis**: At both acceleration and deceleration phases
  * - **Important transitions**: Focal, memorable moments
  * - **Longer animations**: 500ms+ durations
  *
  * Mathematically: Combines quinticIn for t < 0.5 and quinticOut for t >= 0.5
  *
  * @code
  * // Premium screen transition
  * auto x = tweeny::from(0.0f)
  *   .to(1920.0f)
  *   .via(easing::quinticInOut)
  *   .during(50U)
  *   .build();
  * @endcode
  *
  * @see quinticIn For only very strong acceleration
  * @see quinticOut For only very strong deceleration
  * @see quarticInOut For gentler motion
  * @see exponentialInOut For more extreme motion
  * @see Visualize at https://easings.net/#easeInOutQuint
  */
 inline constexpr detail::quinticInOutEasing quinticInOut{};

 /**
  * @brief Sinusoidal easing based on sine wave for smooth acceleration.
  *
  * The `sinusoidalIn` easing function uses a sine curve for acceleration, creating
  * extremely smooth, natural motion. Based on trigonometric functions rather than
  * polynomials.
  *
  * Characteristics:
  * - Very smooth acceleration
  * - Natural, organic feel
  * - No abrupt velocity changes
  * - Gentle but noticeable
  *
  * This easing is ideal for:
  * - **Natural motion**: Organic, flowing animations
  * - **Smooth camera moves**: Pans, zooms, orbits
  * - **Elegant transitions**: Refined, sophisticated feel
  * - **Accessible animations**: Gentle on motion sensitivity
  *
  * Mathematically: `f(t) = 1 - cos(t * π/2)`
  *
  * @code
  * // Smooth camera pan
  * auto x = tweeny::from(0.0f)
  *   .to(1000.0f)
  *   .via(easing::sinusoidalIn)
  *   .during(60U)
  *   .build();
  * @endcode
  *
  * @see sinusoidalOut For smooth deceleration
  * @see sinusoidalInOut For smooth motion at both ends
  * @see quadraticIn For similar gentleness
  * @see Visualize at https://easings.net/#easeInSine
  */
 inline constexpr detail::sinusoidalInEasing sinusoidalIn{};

 /**
  * @brief Sinusoidal easing based on sine wave for smooth deceleration.
  *
  * The `sinusoidalOut` easing function uses a sine curve for deceleration, creating
  * extremely smooth, natural motion that settles gently.
  *
  * Characteristics:
  * - Very smooth deceleration
  * - Natural, flowing motion
  * - Gentle slowdown
  * - Mathematically elegant
  *
  * This easing excels at:
  * - **Natural UI motion**: Smooth, organic feel
  * - **Continuous animations**: Loops, cycles, repeated motion
  * - **Gentle transitions**: Calm, relaxed interfaces
  * - **Accessible design**: Motion-sensitivity friendly
  *
  * SinusoidalOut is excellent when you want smoothness above all else. It's
  * gentler than quadratic while still providing clear easing.
  *
  * Mathematically: `f(t) = sin(t * π/2)`
  *
  * @code
  * // Smooth fade in
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::sinusoidalOut)
  *   .during(30U)
  *   .build();
  * @endcode
  *
  * @see sinusoidalIn For smooth acceleration
  * @see sinusoidalInOut For smooth motion at both ends
  * @see quadraticOut For similar gentleness
  * @see Visualize at https://easings.net/#easeOutSine
  */
 inline constexpr detail::sinusoidalOutEasing sinusoidalOut{};

 /**
  * @brief Sinusoidal easing with smooth acceleration and deceleration.
  *
  * The `sinusoidalInOut` easing function uses sine curves for both acceleration
  * and deceleration, creating the smoothest possible S-curve motion.
  *
  * Motion profile:
  * - First half: Smooth sine-based acceleration
  * - Midpoint: Maximum velocity
  * - Second half: Smooth sine-based deceleration
  * - Creates extremely smooth S-curve
  *
  * This easing is appropriate for:
  * - **Natural, organic motion**: Smoothest easing available
  * - **Continuous loops**: Seamless repeated animations
  * - **Calm interfaces**: Relaxed, gentle design systems
  * - **Accessible animations**: Minimal motion stress
  *
  * SinusoidalInOut is the smoothest InOut easing, making it perfect when
  * fluid, natural motion is the priority.
  *
  * Mathematically: `f(t) = (1 - cos(t * π)) / 2`
  *
  * @code
  * // Ultra-smooth transition
  * auto x = tweeny::from(0.0f)
  *   .to(100.0f)
  *   .via(easing::sinusoidalInOut)
  *   .during(40U)
  *   .build();
  * @endcode
  *
  * @see sinusoidalIn For only smooth acceleration
  * @see sinusoidalOut For only smooth deceleration
  * @see quadraticInOut For similar gentleness
  * @see Visualize at https://easings.net/#easeInOutSine
  */
 inline constexpr detail::sinusoidalInOutEasing sinusoidalInOut{};

 /**
  * @brief Stepped easing that holds the start value until the keyframe completes.
  *
  * The `stepped` easing function returns the starting value throughout the entire
  * duration of the keyframe segment, only jumping to the target value when moving
  * to the next keyframe. This creates instant transitions between keyframes without
  * any interpolation within each segment.
  *
  * Characteristics:
  * - No interpolation within keyframe segments
  * - Holds start value until keyframe ends
  * - Instant jumps between keyframes
  * - Creates discrete, stair-step motion
  *
  * This easing is ideal for:
  * - **Discrete state transitions**: Values that shouldn't interpolate smoothly
  * - **Keyframe-based animations**: Step through distinct poses or states
  * - **Frame-by-frame effects**: Hold each frame without blending
  * - **Boolean-like values**: Properties that need instant changes
  * - **Sprite switching**: Change sprites at keyframe boundaries
  * - **Cut transitions**: Instant changes without fading
  *
  * Stepped is fundamentally different from other easings because it eliminates
  * interpolation entirely within each keyframe segment, creating hard cuts between
  * animation states.
  *
  * @code
  * // Hold value at each keyframe, jump instantly between them
  * auto value = tweeny::from(0)
  *   .to(10).via(easing::stepped).during(100U)
  *   .to(20).via(easing::stepped).during(100U)
  *   .to(30).via(easing::stepped).during(100U)
  *   .build();
  *
  * // During frames 0-99: returns 0
  * // During frames 100-199: returns 10
  * // During frames 200-299: returns 20
  * // At frame 300: returns 30
  *
  * // Useful for sprite animation indices
  * auto spriteIndex = tweeny::from(0)
  *   .to(1).via(easing::stepped).during(10U)
  *   .to(2).via(easing::stepped).during(10U)
  *   .to(3).via(easing::stepped).during(10U)
  *   .build();
  * // Holds each sprite index for 10 frames, then instantly switches
  * @endcode
  *
  * @see linear For smooth constant-velocity interpolation
  */
 inline constexpr detail::steppedEasing stepped{};
}

#endif //TWEENY_EASING_H

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

  // Leave listeners
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

  // Enter listeners
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
auto tweeny::tween<FirstValueType, RemainingValueTypes...>::render(uint32_t target_frame) const -> tween_value_t {
  constexpr std::size_t ValuesCount = sizeof...(RemainingValueTypes) + 1;

  const auto as_return_value = [](const auto & val) -> tween_value_t {
    if constexpr (ValuesCount == 1) {
      return std::get<0>(val);
    } else {
      return val;
    }
  };

  if (key_frames.empty()) {
    if constexpr (ValuesCount == 1) {
      return FirstValueType{};
    } else {
      return typename key_frame_t::values_t{};
    }
  }

  const auto & first_key_frame = key_frames.front();
  const auto & last_key_frame = key_frames.back();

  target_frame = std::clamp(
    target_frame,
    first_key_frame.position,
    last_key_frame.position
  );

  if (target_frame <= first_key_frame.position) return as_return_value(first_key_frame.values);
  if (target_frame >= last_key_frame.position) return as_return_value(last_key_frame.values);

  std::size_t base_key_key_frame_idx = find_key_frame_index(target_frame);

  if (base_key_key_frame_idx + 1 >= key_frames.size()) {
    return as_return_value(last_key_frame.values);
  }

  const key_frame_t & base_key_frame = key_frames[base_key_key_frame_idx];
  const key_frame_t & next_key_frame = key_frames[base_key_key_frame_idx + 1];

  const int64_t base_kf_position = base_key_frame.position;
  const uint32_t target_kf_position = next_key_frame.position;
  const int64_t target_frame_i64 = target_frame;

  float inbetween_progress = 1.0f;
  if (target_kf_position > base_kf_position) {
    const auto numerator = static_cast<float>(target_frame_i64 - base_kf_position);
    const auto denominator = static_cast<float>(target_kf_position - base_kf_position);
    inbetween_progress = numerator / denominator;
  }
  inbetween_progress = std::clamp(inbetween_progress, 0.0f, 1.0f);

  auto values = detail::interpolate_values<FirstValueType, RemainingValueTypes...>(
    inbetween_progress,
    base_key_frame,
    next_key_frame,
    std::make_index_sequence<ValuesCount>{}
  );

  return as_return_value(values);
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

  /**
   * Implements the creation of a tuple with repeated elements of the given value.
   *
   * @param v The value to be repeated in the tuple.
   * @param sequence An index sequence used to generate the tuple with the required size.
   * @return A tuple where each element is a copy of the input value.
   *
   * @warning This function is private and shouldn't be used directly
   */
  template <typename T, std::size_t... I>
  static auto make_repeated_tuple_impl(const T & v, std::index_sequence<I...> sequence) {
    (void) sequence;
    return std::make_tuple(((void) I, v)...);
  }

  /**
   * Creates a tuple of size N where each element is a copy of the given value.
   *
   * @param v The value to be repeated in the tuple.
   * @return A tuple of size N with each element being a copy of the input value.
   *
   * @warning This function is private and shouldn't be used directly
   */
  template <typename T, std::size_t N>
  static auto make_repeated_tuple(const T & v) {
    return make_repeated_tuple_impl(v, std::make_index_sequence<N>{});
  }
}

#endif //TWEENY_DETAIL_TUPLE_UTILITIES_H


/**
 * @file event.h
 * @brief Event system for tween animation callbacks.
 *
 * This file defines event types and response codes for the tween event system.
 * Events are triggered during tween operations (step, seek, jump, complete) and
 * allow callbacks to react to animation state changes. Tag types are defined in
 * detail/event.h and instantiated here with documentation.
 *
 * @code
 * auto t = tweeny::from(0).to(100).during(60U).build();
 * t.on(tweeny::event::step, [](auto& tween) {
 *   printf("Value: %d\n", tween.peek());
 *   return tweeny::event::response::ok;
 * });
 * @endcode
 */

#ifndef TWEENY_EVENT_H
#define TWEENY_EVENT_H


#ifndef TWEENY_DETAIL_EVENT_H
#define TWEENY_DETAIL_EVENT_H

#include <cstddef>

namespace tweeny::detail::event {
  /**
   * @brief Tag type for step events.
   * @internal Implementation detail - users should use tweeny::event::step
   */
  struct step_t {};

  /**
   * @brief Tag type for seek events.
   * @internal Implementation detail - users should use tweeny::event::seek
   */
  struct seek_t {};

  /**
   * @brief Tag type for jump events.
   * @internal Implementation detail - users should use tweeny::event::jump
   */
  struct jump_t {};

  /**
   * @brief Tag type for completion events.
   * @internal Implementation detail - users should use tweeny::event::complete
   */
  struct complete_t {};

  /**
   * @brief Tag type for keyframeEnter events.
   * @internal Implementation detail - users should use tweeny::event::keyframeEnter
   */
  struct keyframeEnter_t {};

  /**
   * @brief Tag type for keyframeLeave events.
   * @internal Implementation detail - users should use tweeny::event::keyframeLeave
   */
  struct keyframeLeave_t {};

  /**
   * @brief Tag type for update events.
   * @internal Implementation detail - users should use tweeny::event::update
   */
  struct update_t {};
}

namespace tweeny::event {
  /**
   * @brief Event data passed when entering a new keyframe.
   *
   * This event is triggered when the tween transitions into a new keyframe section,
   * providing the index of the keyframe being entered.
   */
  struct keyframeEnter {
    size_t key_frame;
    explicit keyframeEnter(const size_t key_frame_input) : key_frame(key_frame_input) {}
  };

  /**
   * @brief Event data passed when leaving a keyframe.
   *
   * This event is triggered when the tween transitions out of a keyframe section,
   * providing the index of the keyframe being exited.
   */
  struct keyframeLeave {
    size_t key_frame;
    explicit keyframeLeave(const size_t key_frame_input) : key_frame(key_frame_input) {}
  };

  /**
   * @brief Response codes returned by event callbacks.
   *
   * Controls whether a callback continues receiving events or is automatically removed.
   */
  enum class response {
    /**
     * @brief Continue receiving events.
     *
     * The callback remains registered and will be invoked on future events.
     */
    ok = 0,

    /**
     * @brief Unsubscribe after this callback.
     *
     * The callback is automatically removed after returning and will not
     * receive future events. Useful for one-shot callbacks.
     */
    unsubscribe = 1,
  };
}

#endif // TWEENY_DETAIL_EVENT_H

/**
 * @namespace tweeny::event
 * @brief Event types and response codes for tween callbacks.
 *
 * This namespace contains tag types for registering event listeners and
 * response codes for controlling callback behavior. Use these tags with
 * tween::on() to register callbacks for animation lifecycle events.
 */
namespace tweeny::event {
  /**
   * @brief Event triggered after each step() call.
   *
   * Use this event to react to frame-by-frame progression of an animation.
   * The callback receives a reference to the tween and can query its current
   * state using peek() or progress().
   *
   * Common use cases:
   * - Updating visual properties every frame
   * - Logging animation progress
   * - Synchronizing with other animations
   * - Implementing custom timing logic
   *
   * @code
   * auto tween = tweeny::from(0).to(100).during(60U).build();
   * tween.on(tweeny::event::step, [](auto& t) {
   *   printf("Current value: %d\n", t.peek());
   *   return tweeny::event::response::ok;
   * });
   *
   * tween.step(1);  // Triggers the callback
   * @endcode
   *
   * @see seek For events when jumping to specific frames
   * @see complete For events when animation finishes
   */
  inline constexpr detail::event::step_t step{};

  /**
   * @brief Event triggered after each seek() call.
   *
   * Use this event to react when the tween jumps to a specific frame position.
   * Unlike step events, seek events fire regardless of the direction or distance
   * of the movement.
   *
   * Common use cases:
   * - Scrubbing through animations
   * - Reacting to timeline jumps
   * - Updating state after non-linear navigation
   * - Synchronizing with external timeline controls
   *
   * @code
   * auto tween = tweeny::from(0).to(100).during(100U).build();
   * tween.on(tweeny::event::seek, [](auto& t) {
   *   printf("Seeked to frame with value: %d\n", t.peek());
   *   return tweeny::event::response::ok;
   * });
   *
   * tween.seek(50U);  // Triggers the callback
   * @endcode
   *
   * @see step For frame-by-frame progression events
   * @see jump For keyframe-specific navigation
   */
  inline constexpr detail::event::seek_t seek{};

  /**
   * @brief Event triggered after each jump() call.
   *
   * Use this event to react when the tween jumps to a specific keyframe by index.
   * This is particularly useful for multi-point animations where keyframes represent
   * distinct animation phases.
   *
   * Common use cases:
   * - Transitioning between animation states
   * - Triggering phase-specific logic
   * - Resetting to specific animation checkpoints
   * - Implementing state machines
   *
   * @code
   * auto tween = tweeny::from(0).to(50).during(30U).to(100).during(30U).build();
   * tween.on(tweeny::event::jump, [](auto& t) {
   *   printf("Jumped to keyframe\n");
   *   return tweeny::event::response::ok;
   * });
   *
   * tween.jump(1);  // Jump to keyframe 1, triggers callback
   * @endcode
   *
   * @see keyframeEnter For detecting keyframe transitions during playback
   * @see seek For arbitrary frame jumps
   */
  inline constexpr detail::event::jump_t jump{};

  /**
   * @brief Event triggered when the animation is at completion.
   *
   * Fires whenever progress() is >= 1.0 (100%) after a step(), seek(), or jump() call.
   * This means the callback will fire **every time** the tween is navigated to a completed
   * state, not just the first time.
   *
   * Common use cases:
   * - Cleaning up resources after animation
   * - Chaining animations sequentially
   * - Triggering completion callbacks
   * - Transitioning to next state
   * - Playing sound effects or particle effects at end
   *
   * @note The callback fires every time the tween is at completion (progress >= 1.0).
   * If you step to completion, then back, then forward to completion again, it will
   * fire both times. Use response::unsubscribe if you want one-shot behavior.
   *
   * @code
   * auto tween = tweeny::from(0).to(100).during(60U).build();
   * tween.on(tweeny::event::complete, [](auto& t) {
   *   printf("Animation at completion: %d\n", t.peek());
   *   return tweeny::event::response::unsubscribe;  // One-shot callback
   * });
   *
   * tween.step(60);   // Fires complete callback
   * tween.step(-10);  // Now progress < 1.0
   * tween.step(10);   // Would fire again, but we unsubscribed
   * @endcode
   *
   * @see step For frame-by-frame events during animation
   * @see response::unsubscribe For one-shot completion handlers
   */
  inline constexpr detail::event::complete_t complete{};

  /**
   * @brief Event triggered when entering a new keyframe segment.
   *
   * Fires when the tween transitions into a new keyframe section during playback.
   * The callback receives both the tween reference and a keyframeEnter struct
   * containing the keyframe index.
   *
   * Common use cases:
   * - Triggering phase-specific animations
   * - Playing transition sounds
   * - Updating UI to reflect animation phase
   * - Synchronizing multi-part animations
   * - Implementing animation state machines
   *
   * @note Keyframe indices are 0-based. The first keyframe is index 0, second is 1, etc.
   *
   * @code
   * auto tween = tweeny::from(0)
   *   .to(50).during(30U)
   *   .to(100).during(30U)
   *   .build();
   *
   * tween.on(tweeny::event::keyframeEnter, [](auto& t, tweeny::event::keyframeEnter evt) {
   *   printf("Entering keyframe %zu\n", evt.key_frame);
   *   return tweeny::event::response::ok;
   * });
   *
   * tween.step(31);  // Triggers: "Entering keyframe 1"
   * @endcode
   *
   * @see keyframeLeave For detecting when leaving a keyframe
   * @see keyframeEnter For the event data struct
   * @see jump For manually jumping to keyframes
   */
  inline constexpr detail::event::keyframeEnter_t keyframeEnter{};

  /**
   * @brief Event triggered when leaving a keyframe segment.
   *
   * Fires when the tween transitions out of a keyframe section during playback.
   * The callback receives both the tween reference and a keyframeLeave struct
   * containing the keyframe index being exited.
   *
   * Common use cases:
   * - Cleaning up phase-specific resources
   * - Stopping phase-specific effects
   * - Logging animation progression
   * - Implementing phase exit handlers
   * - Coordinating complex multi-segment animations
   *
   * @note Keyframe indices are 0-based. Leaving keyframe 0 means transitioning
   * from the first to the second segment.
   *
   * @code
   * auto tween = tweeny::from(0)
   *   .to(50).during(30U)
   *   .to(100).during(30U)
   *   .build();
   *
   * tween.on(tweeny::event::keyframeLeave, [](auto& t, tweeny::event::keyframeLeave evt) {
   *   printf("Leaving keyframe %zu\n", evt.key_frame);
   *   return tweeny::event::response::ok;
   * });
   *
   * tween.step(31);  // Triggers: "Leaving keyframe 0"
   * @endcode
   *
   * @see keyframeEnter For detecting when entering a keyframe
   * @see keyframeLeave For the event data struct
   */
  inline constexpr detail::event::keyframeLeave_t keyframeLeave{};

  /**
   * @brief Event triggered whenever the tween position changes.
   *
   * Fires after any step(), seek(), or jump() call. This is a convenience event
   * that consolidates all position update events into a single callback point.
   *
   * Common use cases:
   * - Updating visuals regardless of how the tween changed
   * - Monitoring all tween changes from one place
   * - Logging every position update
   * - Synchronizing with external systems
   * - Implementing universal change handlers
   *
   * @note This event fires **after** the specific event (step, seek, or jump) and
   * **before** the complete event if applicable.
   *
   * @code
   * auto tween = tweeny::from(0).to(100).during(60U).build();
   * tween.on(tweeny::event::update, [](auto& t) {
   *   printf("Tween updated to: %d\n", t.peek());
   *   return tweeny::event::response::ok;
   * });
   *
   * tween.step(10);  // Triggers update
   * tween.seek(50U); // Triggers update
   * tween.jump(1);   // Triggers update
   * @endcode
   *
   * @see step For frame-by-frame updates
   * @see seek For arbitrary frame jumps
   * @see jump For keyframe jumps
   */
  inline constexpr detail::event::update_t update{};
}

#endif //TWEENY_EVENT_H


/**
 * @file easing.h
 * The purpose of this file is to list all bundled easings. Each easing is defined
 * in its own header under include/tweeny/detail/easing/. Users may include individual
 * easing headers or this header to get all easings.
 */

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

/**
 * @namespace tweeny::easing
 * @brief Contains all built-in easing functions for controlling animation curves.
 *
 * Provides 30+ easing functions in In, Out, and InOut variants for creating natural-looking
 * animations.
 */
namespace tweeny::easing {
 /**
  * @brief Easing function that creates anticipation by pulling back before moving forward.
  *
  * The `backIn` easing function begins by moving backwards slightly (overshooting in the
  * negative direction), then reverses and accelerates toward the target value. This creates
  * an anticipation effect similar to pulling back a slingshot or winding up before a throw.
  *
  * The backward motion at the start makes this easing particularly effective for animations
  * that benefit from telegraphing or anticipation, such as:
  * - UI elements that "wind up" before sliding in
  * - Character movements that show preparation before action
  * - Camera movements that pull back before zooming forward
  *
  * Mathematically, this easing uses the formula: `c * t * t * ((s + 1) * t - s) + b`
  * where `s` controls the overshoot amount (typically 1.70158).
  *
  * @note The animation will briefly have values less than the start value before proceeding
  * to the target. Ensure your animation system can handle values outside the expected range.
  *
  * @code
  * // Create a tween with back-in easing for anticipation effect
  * auto tween = tweeny::from(0.0f)
  *   .to(100.0f)
  *   .via(easing::backIn)
  *   .during(60U)
  *   .build();
  *
  * // The animation will dip below 0.0f briefly before accelerating to 100.0f
  * @endcode
  *
  * @see backOut For overshoot at the end instead of the beginning
  * @see backInOut For anticipation at the start and overshoot at the end
  * @see Visualize at https://easings.net/#easeInBack
  */
 inline constexpr detail::backInEasing backIn{};

 /**
  * @brief Easing function that overshoots the target before settling back.
  *
  * The `backOut` easing function accelerates toward and past the target value, then pulls
  * back to settle at the final position. This creates an overshoot or "spring-back" effect
  * that adds liveliness and energy to animations.
  *
  * This easing is particularly effective for:
  * - UI elements that pop into place with energy (buttons, dialogs, notifications)
  * - Object arrivals that should feel bouncy and dynamic
  * - Emphasizing the end state of an animation
  * - Adding personality to mechanical movements
  *
  * The overshoot creates a more natural, less rigid feel compared to standard easing
  * functions. It's widely used in iOS, Android, and Material Design animation patterns
  * to make interactions feel more responsive and playful.
  *
  * Mathematically, this is the inverse of backIn, applied at the end of the transition.
  *
  * @note The animation will briefly exceed the target value before settling. Ensure your
  * rendering or logic can handle values beyond the specified range.
  *
  * @code
  * // Animate a button with overshoot for a lively effect
  * auto scale = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::backOut)
  *   .during(30U)
  *   .build();
  *
  * // Scale will exceed 1.0f (e.g., 1.1f) before settling back to 1.0f
  * @endcode
  *
  * @see backIn For anticipation at the start instead of overshoot at the end
  * @see backInOut For both anticipation and overshoot
  * @see elasticOut For a more pronounced oscillating overshoot effect
  * @see Visualize at https://easings.net/#easeOutBack
  */
 inline constexpr detail::backOutEasing backOut{};

 /**
  * @brief Easing function combining anticipation at the start and overshoot at the end.
  *
  * The `backInOut` easing function creates a dramatic motion curve by pulling back before
  * the start (anticipation), accelerating through the middle, then overshooting past the
  * target before settling (spring-back). This combines the effects of both backIn and
  * backOut for maximum expressiveness.
  *
  * This easing creates highly dynamic animations suitable for:
  * - Attention-grabbing transitions that need maximum visual interest
  * - Character animations requiring wind-up and follow-through
  * - Scene transitions with dramatic flair
  * - Emphasizing both the start and end states of an animation
  *
  * The dual overshoot (negative at start, positive at end) makes this one of the most
  * expressive easings, but it should be used judiciously as it can feel exaggerated if
  * overused. It works best for focal animations rather than ambient motion.
  *
  * @note The animation will have values outside the start-to-target range at both ends.
  * During the first half, values will dip below the start; during the second half, values
  * will exceed the target before settling.
  *
  * @code
  * // Create a dramatic page transition
  * auto position = tweeny::from(-100.0f)
  *   .to(100.0f)
  *   .via(easing::backInOut)
  *   .during(90U)
  *   .build();
  *
  * // Position will go below -100.0f at start and above 100.0f near end
  * @endcode
  *
  * @see backIn For only anticipation without overshoot
  * @see backOut For only overshoot without anticipation
  * @see elasticInOut For a more extreme oscillating version
  * @see Visualize at https://easings.net/#easeInOutBack
  */
 inline constexpr detail::backInOutEasing backInOut{};

 /**
  * @brief Bounce easing simulating a ball bouncing with increasing height at the start.
  *
  * The `bounceIn` easing function creates a bouncing effect before the animation begins,
  * like a ball dropped from above that bounces progressively higher before launching into
  * the main motion. This is the reverse of bounceOut's natural physics.
  *
  * Characteristics:
  * - Multiple discrete "bounces" at the start
  * - Each bounce is higher than the previous
  * - Creates anticipation through impact-based motion
  * - Less commonly used than bounceOut
  *
  * This easing works well for:
  * - **Landing preparation**: Elements about to drop into place
  * - **Impact anticipation**: Building up to a collision
  * - **Reverse playback effects**: Bounced animations played backward
  * - **Stylized entrances**: Cartoon-style wind-up effects
  *
  * BounceIn is less intuitive than bounceOut because it reverses natural physics (balls
  * don't normally bounce higher each time). Consider backIn for more natural anticipation.
  *
  * @code
  * // Element that bounces before sliding in
  * auto position = tweeny::from(0.0f)
  *   .to(200.0f)
  *   .via(easing::bounceIn)
  *   .during(50U)
  *   .build();
  * @endcode
  *
  * @see bounceOut For natural ball-drop bouncing at the end
  * @see bounceInOut For bouncing at both start and end
  * @see backIn For simpler anticipation without discrete impacts
  * @see Visualize at https://easings.net/#easeInBounce
  */
 inline constexpr detail::bounceInEasing bounceIn{};

 /**
  * @brief Bounce easing simulating a ball dropping and bouncing to rest.
  *
  * The `bounceOut` easing function creates the classic ball-drop effect where an object
  * bounces several times with decreasing height before coming to rest. This mimics real-world
  * physics of an inelastic collision and is one of the most recognizable easing patterns.
  *
  * Characteristics:
  * - Multiple discrete bounces with decreasing amplitude
  * - Simulates impact and energy loss
  * - Creates playful, physical motion
  * - Widely recognized and understood by users
  *
  * This easing excels at:
  * - **Object drops**: Items falling into place
  * - **Landing animations**: Characters, UI elements touching down
  * - **Playful interactions**: Buttons, icons, notifications
  * - **Game elements**: Collectibles, power-ups, score displays
  * - **Error/success feedback**: Visual confirmation with personality
  * - **Cartoon physics**: Exaggerated, entertaining motion
  *
  * BounceOut is more popular than elasticOut when you want discrete impacts rather than
  * smooth oscillation. It conveys weight and physicality better than spring-based easings.
  *
  * The bouncing creates natural emphasis on the final position, making it excellent for
  * drawing attention to where something lands.
  *
  * @code
  * // Notification dropping in from above
  * auto y = tweeny::from(-100.0f)
  *   .to(0.0f)
  *   .via(easing::bounceOut)
  *   .during(45U)
  *   .build();
  *
  * // Button that bounces into place
  * auto scale = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::bounceOut)
  *   .during(40U)
  *   .build();
  * @endcode
  *
  * @see bounceIn For inverse bouncing at the start
  * @see bounceInOut For bouncing at both ends
  * @see elasticOut For smooth spring-like alternative
  * @see Visualize at https://easings.net/#easeOutBounce
  */
 inline constexpr detail::bounceOutEasing bounceOut{};

 /**
  * @brief Bounce easing with bouncing at both start and end.
  *
  * The `bounceInOut` easing function combines reverse bouncing at the start with natural
  * bouncing at the end. The motion bounces with increasing height initially, accelerates
  * through the middle, then bounces to rest at the target.
  *
  * Motion profile:
  * - First half: Bounces with increasing amplitude (bounceIn)
  * - Midpoint: Smooth transition
  * - Second half: Bounces with decreasing amplitude (bounceOut)
  * - Creates playful, impact-based motion at both ends
  *
  * This easing is appropriate for:
  * - **Playful transitions**: Fun, energetic scene changes
  * - **Game UI**: High-energy, cartoon-style interfaces
  * - **Children's applications**: Whimsical, entertaining motion
  * - **Attention-grabbing effects**: Elements that need maximum personality
  *
  * BounceInOut is quite dramatic and can feel excessive for business applications,
  * productivity tools, and enterprise software where subtlety is preferred. The dual
  * bouncing works best in contexts where playfulness is a design goal.
  *
  * @code
  * // Playful modal transition
  * auto scale = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::bounceInOut)
  *   .during(60U)
  *   .build();
  * @endcode
  *
  * @see bounceIn For bouncing only at the start
  * @see bounceOut For bouncing only at the end (more commonly useful)
  * @see elasticInOut For spring-like alternative
  * @see Visualize at https://easings.net/#easeInOutBounce
  */
 inline constexpr detail::bounceInOutEasing bounceInOut{};

 /**
  * @brief Circular easing based on quarter-circle arc for smooth acceleration.
  *
  * The `circularIn` easing function accelerates following the curve of a quarter circle,
  * creating smooth, gradual acceleration. The motion follows the equation `1 - sqrt(1 - t²)`,
  * which traces a circular arc.
  *
  * Characteristics:
  * - Smooth, continuous acceleration
  * - More gradual than quadratic, less than cubic
  * - No sharp transitions in velocity
  * - Mathematically elegant curve
  *
  * This easing is ideal for:
  * - **Natural motion**: Movements that need organic feel
  * - **Camera movements**: Smooth pans and zooms
  * - **Scroll animations**: Gentle acceleration for reading comfort
  * - **Subtle UI transitions**: Motion with moderate acceleration
  *
  * Circular easings strike a balance between the gentle quadratic and the more aggressive
  * cubic curves, making them versatile for many contexts.
  *
  * @code
  * // Smooth camera pan
  * auto cameraX = tweeny::from(0.0f)
  *   .to(1000.0f)
  *   .via(easing::circularIn)
  *   .during(90U)
  *   .build();
  * @endcode
  *
  * @see circularOut For circular deceleration
  * @see circularInOut For circular acceleration and deceleration
  * @see quadraticIn For gentler acceleration
  * @see cubicIn For stronger acceleration
  * @see Visualize at https://easings.net/#easeInCirc
  */
 inline constexpr detail::circularInEasing circularIn{};

 /**
  * @brief Circular easing based on quarter-circle arc for smooth deceleration.
  *
  * The `circularOut` easing function decelerates following a circular curve, creating
  * smooth, natural-looking motion that settles gently to rest. Popular for UI animations
  * requiring moderate deceleration without dramatic emphasis.
  *
  * Characteristics:
  * - Smooth, continuous deceleration
  * - Natural feeling without being too soft or aggressive
  * - Balanced between gentle and pronounced
  * - Widely used in production interfaces
  *
  * This easing excels at:
  * - **Modern UI animations**: Cards, panels, menus
  * - **Material Design patterns**: Following Google's motion guidelines
  * - **Content transitions**: Page changes, tab switches
  * - **Professional applications**: Business and productivity software
  * - **General-purpose motion**: Versatile for many animation types
  *
  * CircularOut works well in most contexts. It's less dramatic than cubic but with
  * smoother deceleration than quadratic.
  *
  * @code
  * // Menu panel sliding in
  * auto x = tweeny::from(-300.0f)
  *   .to(0.0f)
  *   .via(easing::circularOut)
  *   .during(35U)
  *   .build();
  * @endcode
  *
  * @see circularIn For circular acceleration
  * @see circularInOut For circular motion at both ends
  * @see quadraticOut For gentler alternative
  * @see cubicOut For more pronounced alternative
  * @see Visualize at https://easings.net/#easeOutCirc
  */
 inline constexpr detail::circularOutEasing circularOut{};

 /**
  * @brief Circular easing with smooth acceleration and deceleration.
  *
  * The `circularInOut` easing function uses circular curves for both acceleration and
  * deceleration, creating balanced motion suitable for general UI work. This is a
  * popular all-purpose easing.
  *
  * Motion profile:
  * - First half: Circular acceleration
  * - Midpoint: Maximum velocity
  * - Second half: Circular deceleration
  * - Creates smooth S-curve
  *
  * This easing is appropriate for:
  * - **All-purpose UI animations**: Buttons, dialogs, drawers
  * - **Material Design**: Recommended in Google's motion guidelines
  * - **Business applications**: Productivity tools, enterprise apps
  * - **Default animation choice**: Safe, versatile motion curve
  *
  * CircularInOut is often recommended as a starting point for animations because it
  * provides polish without being too subtle or too dramatic.
  *
  * @code
  * // Modal dialog appearance
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::circularInOut)
  *   .during(25U)
  *   .build();
  * @endcode
  *
  * @see circularIn For only acceleration
  * @see circularOut For only deceleration
  * @see quadraticInOut For gentler motion
  * @see cubicInOut For more dramatic motion
  * @see Visualize at https://easings.net/#easeInOutCirc
  */
 inline constexpr detail::circularInOutEasing circularInOut{};

 /**
  * @brief Cubic polynomial easing (t³) with moderate acceleration.
  *
  * The `cubicIn` easing function uses a cubic power curve for acceleration, providing
  * smooth, noticeable easing that's more pronounced than quadratic but less extreme
  * than quartic or exponential.
  *
  * Characteristics:
  * - Polynomial acceleration with power of 3
  * - Balanced between gentle and dramatic
  * - Widely used and well-understood
  * - Good default for many animation types
  *
  * This easing is ideal for:
  * - **Standard UI animations**: Menus, panels, overlays
  * - **Easing beginners**: Easy to understand and predict
  * - **General-purpose acceleration**: Versatile for many contexts
  * - **Medium-length animations**: 200-500ms durations
  *
  * CubicIn is a popular choice because it provides clear easing without being subtle
  * (like quadratic) or extreme (like quartic/exponential).
  *
  * Mathematically: `f(t) = t³`
  *
  * @code
  * // Dropdown menu opening
  * auto height = tweeny::from(0.0f)
  *   .to(200.0f)
  *   .via(easing::cubicIn)
  *   .during(30U)
  *   .build();
  * @endcode
  *
  * @see cubicOut For cubic deceleration
  * @see cubicInOut For cubic motion at both ends
  * @see quadraticIn For gentler acceleration
  * @see quarticIn For stronger acceleration
  * @see Visualize at https://easings.net/#easeInCubic
  */
 inline constexpr detail::cubicInEasing cubicIn{};

 /**
  * @brief Cubic polynomial easing (t³) with moderate deceleration.
  *
  * The `cubicOut` easing function provides smooth, balanced deceleration that's popular
  * across many design systems. It's pronounced enough to be noticeable but not dramatic.
  *
  * Characteristics:
  * - Smooth, natural-feeling deceleration
  * - More pronounced than quadratic, gentler than quartic
  * - Industry-standard motion curve
  * - Works well for most animation types
  *
  * This easing excels at:
  * - **Web interfaces**: Following CSS animation best practices
  * - **Mobile UI**: iOS and Android design patterns
  * - **Content animations**: Cards, lists, grids
  * - **Default easing choice**: Safe for most situations
  * - **User-triggered actions**: Button presses, toggles, switches
  *
  * CubicOut is one of the most commonly used easings in production interfaces. It
  * provides clear polish without drawing excessive attention to the motion itself.
  *
  * Mathematically: `f(t) = 1 - (1-t)³`
  *
  * @code
  * // Button press feedback
  * auto scale = tweeny::from(1.0f)
  *   .to(0.95f)
  *   .via(easing::cubicOut)
  *   .during(10U)
  *   .build();
  *
  * // Card appearing
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::cubicOut)
  *   .during(25U)
  *   .build();
  * @endcode
  *
  * @see cubicIn For cubic acceleration
  * @see cubicInOut For cubic motion at both ends
  * @see quadraticOut For gentler deceleration
  * @see quarticOut For stronger deceleration
  * @see Visualize at https://easings.net/#easeOutCubic
  */
 inline constexpr detail::cubicOutEasing cubicOut{};

 /**
  * @brief Cubic polynomial easing with balanced acceleration and deceleration.
  *
  * The `cubicInOut` easing function combines cubic acceleration and deceleration for
  * smooth, professional motion. This is one of the most popular general-purpose easings.
  *
  * Motion profile:
  * - First half: Cubic acceleration (t³)
  * - Midpoint: Maximum velocity
  * - Second half: Cubic deceleration
  * - Creates smooth, balanced S-curve
  *
  * This easing is appropriate for:
  * - **All-purpose animations**: When in doubt, use this
  * - **Web standards**: CSS ease-in-out equivalent
  * - **Design system defaults**: Common in component libraries
  * - **Cross-platform consistency**: Works well everywhere
  * - **Medium animations**: 200-500ms sweet spot
  *
  * CubicInOut is frequently the default easing in design systems and animation libraries
  * because it provides clear, professional motion that works for most scenarios.
  *
  * Mathematically: Combines cubicIn for t < 0.5 and cubicOut for t >= 0.5
  *
  * @code
  * // Page transition
  * auto x = tweeny::from(0.0f)
  *   .to(1920.0f)
  *   .via(easing::cubicInOut)
  *   .during(40U)
  *   .build();
  * @endcode
  *
  * @see cubicIn For only acceleration
  * @see cubicOut For only deceleration
  * @see quadraticInOut For gentler motion
  * @see quarticInOut For more dramatic motion
  * @see Visualize at https://easings.net/#easeInOutCubic
  */
 inline constexpr detail::cubicInOutEasing cubicInOut{};
 /**
  * @brief Default easing function, alias for linear easing.
  *
  * The `def` easing is a convenience alias for `linear`, providing the same constant-velocity
  * interpolation with no acceleration or deceleration. It exists as a semantic indicator that
  * the default easing behavior is being explicitly chosen.
  *
  * Using `def` instead of `linear` can make code intent clearer in contexts where you want to
  * explicitly state "use the default behavior" rather than specifically requesting linear motion.
  * However, both are functionally identical.
  *
  * This is the easing applied when no via() call is made in the tween builder.
  *
  * @code
  * // These three tweens are functionally identical:
  * auto t1 = tweeny::from(0).to(100).during(60U).build();  // Implicit default
  * auto t2 = tweeny::from(0).to(100).via(easing::def).during(60U).build();
  * auto t3 = tweeny::from(0).to(100).via(easing::linear).during(60U).build();
  * @endcode
  *
  * @see linear For the primary documentation of this easing behavior
  */
 inline constexpr detail::defaultEasing def{};

 /**
  * @brief Elastic easing with oscillating spring-like motion at the start.
  *
  * The `elasticIn` easing function creates a spring or elastic band effect that oscillates
  * with increasing amplitude before reaching the starting point of the animation. The motion
  * resembles pulling back an elastic band that vibrates as tension builds.
  *
  * The oscillation characteristics:
  * - Multiple back-and-forth swings before the main motion begins
  * - Amplitude increases as the animation progresses
  * - Creates a "winding up" or "charging" effect
  * - More pronounced than backIn's single overshoot
  *
  * This easing is particularly effective for:
  * - **Magical or fantasy effects**: Spell charging, energy gathering
  * - **Exaggerated cartoon animations**: Extreme anticipation and wind-up
  * - **Attention-grabbing entrances**: Elements that need dramatic introduction
  * - **Game power-ups**: Visual feedback for charging actions
  * - **Playful UI elements**: Whimsical, high-energy interactions
  *
  * The elastic effect is more dramatic than back easing, making it suitable for contexts
  * where strong visual emphasis or entertainment value is desired. It's less appropriate
  * for subtle or professional interfaces.
  *
  * Mathematically, this uses a decaying sine wave with exponential amplitude growth.
  *
  * @warning This easing creates significant overshoot in both directions. Values will
  * oscillate well beyond the start value in both positive and negative directions. Ensure
  * your rendering system can handle these extreme values gracefully.
  *
  * @code
  * // Magical charging effect before an action
  * auto glow = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::elasticIn)
  *   .during(45U)
  *   .build();
  *
  * // The glow will oscillate negative before reaching 1.0f
  * // producing a "charging" visual effect
  * @endcode
  *
  * @see elasticOut For spring-like oscillation at the end
  * @see elasticInOut For oscillation at both start and end
  * @see backIn For a simpler single-overshoot alternative
  * @see Visualize at https://easings.net/#easeInElastic
  */
 inline constexpr detail::elasticInEasing elasticIn{};

 /**
  * @brief Elastic easing with spring-like oscillation at the end.
  *
  * The `elasticOut` easing function creates a natural spring or rubber band effect that
  * overshoots and oscillates around the target value before settling. This is one of the
  * most visually distinctive and playful easings, mimicking real-world elastic physics.
  *
  * The oscillation characteristics:
  * - Overshoots the target value multiple times
  * - Amplitude decreases with each oscillation (damped motion)
  * - Settles naturally at the final value
  * - Creates a bouncy, energetic feel without the hard impacts of bounce easing
  *
  * This easing excels at:
  * - **Playful UI animations**: Buttons, toggles, modal appearances
  * - **Game elements**: Power-up notifications, achievement popups, score displays
  * - **Cartoon-style motion**: Exaggerated, entertaining character movements
  * - **Attention direction**: Drawing eyes to important elements
  * - **Spring simulation**: Rubber bands, diving boards, springy objects
  * - **Joyful interactions**: Adding personality to standard UI patterns
  *
  * ElasticOut is widely used in modern mobile UI design to add energy and delight to
  * interactions. It's more pronounced than backOut but smoother than bounceOut.
  *
  * The spring feel makes animations memorable and adds perceived responsiveness, making
  * interfaces feel "alive" rather than mechanical.
  *
  * Mathematically, this uses a decaying sine wave with exponentially decreasing amplitude.
  *
  * @warning Values will oscillate beyond the target in both directions before settling.
  * For example, animating from 0 to 100 might temporarily reach 110, then 95, then 102,
  * before settling at 100. Ensure clipping or overflow handling is appropriate.
  *
  * @code
  * // Springy button press feedback
  * auto scale = tweeny::from(1.0f)
  *   .to(1.2f)
  *   .via(easing::elasticOut)
  *   .during(40U)
  *   .build();
  *
  * // Scale will overshoot 1.2f (maybe 1.3f) then oscillate
  * // down and up before settling at exactly 1.2f
  *
  * // Modal dialog with playful entrance
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::elasticOut)
  *   .during(50U)
  *   .build();
  * @endcode
  *
  * @see elasticIn For spring anticipation at the start
  * @see elasticInOut For oscillation at both ends
  * @see backOut For a subtler single-overshoot alternative
  * @see bounceOut For a similar but impact-based bouncing effect
  * @see Visualize at https://easings.net/#easeOutElastic
  */
 inline constexpr detail::elasticOutEasing elasticOut{};

 /**
  * @brief Elastic easing combining spring oscillation at both start and end.
  *
  * The `elasticInOut` easing function creates dramatic elastic motion with oscillation
  * during both the initial acceleration and final deceleration phases. The animation
  * winds up with spring-like vibration, accelerates smoothly through the middle, then
  * oscillates around the target before settling.
  *
  * The motion profile:
  * - First half: Oscillates with increasing amplitude (elasticIn behavior)
  * - Middle: Smooth transition through the midpoint
  * - Second half: Oscillates with decreasing amplitude (elasticOut behavior)
  * - Creates the most dramatic elastic effect available
  *
  * This easing is appropriate for:
  * - **Hero animations**: Focal transitions that demand attention
  * - **Game events**: Critical moments, boss appearances, dramatic reveals
  * - **Cartoon physics**: Exaggerated, entertaining motion for stylized visuals
  * - **Transitions with flair**: Scene changes that need maximum personality
  * - **Experimental UI**: Interfaces prioritizing delight over convention
  *
  * ElasticInOut is the most expressive elastic variant but also the most extreme. It works
  * best for animations that are:
  * - Intentionally playful or whimsical
  * - The primary focus of user attention
  * - Part of a stylized, high-energy aesthetic
  * - Not repeated frequently (can become tiresome)
  *
  * Use sparingly; the dual oscillation can feel excessive for everyday interactions.
  * Consider elasticOut or backInOut for a more balanced alternative.
  *
  * @warning This easing produces extreme value overshoots in both directions throughout
  * the animation. During the first half, values oscillate around the start; during the
  * second half, around the target. Plan for values far outside the expected range.
  *
  * @code
  * // Dramatic screen transition
  * auto position = tweeny::from(0.0f)
  *   .to(1920.0f)
  *   .via(easing::elasticInOut)
  *   .during(90U)
  *   .build();
  *
  * // Position will oscillate around 0.0f at start and 1920.0f at end
  *
  * // Game title appearing with maximum impact
  * auto scale = tweeny::from(0.0f)
  *   .to(2.0f)
  *   .via(easing::elasticInOut)
  *   .during(75U)
  *   .build();
  * @endcode
  *
  * @see elasticIn For oscillation only at the start
  * @see elasticOut For oscillation only at the end (more commonly useful)
  * @see backInOut For a less extreme but still expressive alternative
  * @see Visualize at https://easings.net/#easeInOutElastic
  */
 inline constexpr detail::elasticInOutEasing elasticInOut{};

 /**
  * @brief Exponential easing with very slow start and explosive acceleration.
  *
  * The `exponentialIn` easing function starts extremely slowly and builds to a very rapid
  * acceleration toward the end. Based on exponential growth (2^x), this creates one of the
  * most dramatic acceleration curves available, with velocity increasing exponentially over time.
  *
  * Characteristics:
  * - Almost no visible motion for the first portion of the animation
  * - Sudden, explosive acceleration in the final phase
  * - Change rate doubles repeatedly as time progresses
  * - Creates extreme contrast between start and end velocity
  *
  * This easing is ideal for:
  * - **Dramatic reveals**: Elements that burst into view
  * - **Explosive effects**: Particle systems, energy blasts, explosions
  * - **Fade-ins with impact**: Starting invisible and suddenly appearing
  * - **Speed-up effects**: Rockets launching, vehicles accelerating
  * - **Tension building**: Slow build-up to sudden release
  *
  * The extreme acceleration curve makes this feel more dramatic than polynomial easings
  * (quadratic, cubic, etc.). The motion appears to "explode" into existence rather than
  * gradually accelerate.
  *
  * Use exponentialIn when you want maximum contrast between the patient start and the
  * explosive finish. For most UI work, cubic or quartic easings provide sufficient
  * acceleration with less extreme behavior.
  *
  * Mathematically: `f(t) = 2^(10 * (t - 1))`
  *
  * @code
  * // Fade in that suddenly snaps to full visibility
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::exponentialIn)
  *   .during(45U)
  *   .build();
  *
  * // Rocket launch with explosive acceleration
  * auto velocity = tweeny::from(0.0f)
  *   .to(1000.0f)
  *   .via(easing::exponentialIn)
  *   .during(120U)
  *   .build();
  * @endcode
  *
  * @see exponentialOut For explosive deceleration at the end
  * @see exponentialInOut For dramatic acceleration and deceleration
  * @see quarticIn For strong but less extreme acceleration
  * @see Visualize at https://easings.net/#easeInExpo
  */
 inline constexpr detail::exponentialInEasing exponentialIn{};

 /**
  * @brief Exponential easing with explosive start and gradual slow-down.
  *
  * The `exponentialOut` easing function starts with maximum velocity and decelerates
  * exponentially, creating a smooth glide to a stop. The initial burst of speed followed
  * by gradual settling creates a powerful, energetic feel.
  *
  * Characteristics:
  * - Immediate, explosive motion at the start
  * - Exponentially decreasing velocity
  * - Long, smooth deceleration phase
  * - Settles gently to final value
  *
  * This easing excels at:
  * - **Quick UI responses**: Instant feedback that settles smoothly
  * - **Impact effects**: Objects hitting and settling into place
  * - **Momentum-based motion**: Thrown objects, swipe gestures
  * - **Energetic entrances**: Elements that burst in with energy
  * - **Modern UI patterns**: iOS-style animations with quick start
  *
  * ExponentialOut is popular in modern mobile design because it provides immediate
  * visual feedback (the fast start) while ending smoothly. Users perceive the interface
  * as highly responsive due to the instant motion.
  *
  * The long tail of deceleration gives animations a "quality feel" - nothing stops
  * abruptly. This is gentler on the eyes than linear or even cubic deceleration.
  *
  * Mathematically: `f(t) = 1 - 2^(-10 * t)`
  *
  * @code
  * // Responsive drawer that slides out quickly then settles
  * auto position = tweeny::from(-300.0f)
  *   .to(0.0f)
  *   .via(easing::exponentialOut)
  *   .during(35U)
  *   .build();
  *
  * // Notification that pops in with energy
  * auto scale = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::exponentialOut)
  *   .during(25U)
  *   .build();
  * @endcode
  *
  * @see exponentialIn For explosive acceleration at the start
  * @see exponentialInOut For explosive motion at both ends
  * @see quarticOut For strong but less extreme deceleration
  * @see Visualize at https://easings.net/#easeOutExpo
  */
 inline constexpr detail::exponentialOutEasing exponentialOut{};

 /**
  * @brief Exponential easing with extreme acceleration and deceleration.
  *
  * The `exponentialInOut` easing function combines explosive acceleration in the first
  * half with explosive deceleration in the second half. This creates one of the most
  * dramatic, high-energy motion curves available, with rapid changes at both ends and
  * smooth motion through the middle.
  *
  * Motion profile:
  * - First half: Exponential acceleration from near-zero velocity
  * - Midpoint: Maximum velocity
  * - Second half: Exponential deceleration to rest
  * - Creates extreme S-curve shape
  *
  * This easing is appropriate for:
  * - **High-impact transitions**: Scene changes, screen swipes
  * - **Dramatic animations**: Hero elements, focal content
  * - **Fast-paced interfaces**: Games, action-oriented apps
  * - **Attention-grabbing motion**: Elements that need maximum visibility
  * - **Strong transitions**: For elements moving significant distances or longer durations (500ms+)
  *
  * ExponentialInOut creates a powerful sense of momentum and energy. The motion feels
  * purposeful and confident. However, the extreme acceleration can be disorienting if
  * overused or applied to large elements.
  *
  * Best practices:
  * - Use for animations under 1 second duration
  * - Apply to elements that move short to medium distances
  * - Reserve for important, infrequent transitions
  * - Consider quarticInOut or quinticInOut as less extreme alternatives
  *
  * Mathematically: Combines exponentialIn for t < 0.5 and exponentialOut for t >= 0.5
  *
  * @code
  * // Page transition with explosive motion
  * auto position = tweeny::from(0.0f)
  *   .to(1920.0f)
  *   .via(easing::exponentialInOut)
  *   .during(40U)
  *   .build();
  *
  * // Modal dialog with dramatic appearance
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::exponentialInOut)
  *   .during(30U)
  *   .build();
  * @endcode
  *
  * @see exponentialIn For only explosive acceleration
  * @see exponentialOut For only explosive deceleration
  * @see quinticInOut For slightly less extreme alternative
  * @see Visualize at https://easings.net/#easeInOutExpo
  */
 inline constexpr detail::exponentialInOutEasing exponentialInOut{};
 /**
  * @brief Linear easing function with constant velocity throughout the animation.
  *
  * The `linear` easing function produces uniform motion with no acceleration or deceleration.
  * The interpolation progresses at a constant rate from start to finish, creating mechanical,
  * predictable movement.
  *
  * Linear easing is characterized by:
  * - Constant velocity: the rate of change never varies
  * - No ease-in or ease-out: motion starts and stops abruptly
  * - Simple mathematical relationship: output = start + (end - start) * progress
  * - Predictable timing: halfway through time means halfway through distance
  *
  * This easing is appropriate for:
  * - **Mechanical objects**: Conveyor belts, pistons, automated systems
  * - **Progress indicators**: Loading bars, timers, countdowns
  * - **Continuous loops**: Rotating objects, scrolling backgrounds
  * - **Data visualization**: Graph animations where consistency is important
  * - **Debug and testing**: Predictable behavior for verification
  *
  * Linear easing is generally **not recommended** for most UI animations because:
  * - Lacks the natural feel of acceleration/deceleration
  * - Abrupt starts and stops can feel jarring
  * - Missing visual polish that easing provides
  * - Human perception expects objects to ease into and out of motion
  *
  * However, it serves as the foundation for all other easing functions and is essential
  * when mechanical precision is more important than natural motion feel.
  *
  * Mathematically, this implements: `f(t) = t` where t is normalized progress [0, 1].
  *
  * @code
  * // Constant velocity scrolling background
  * auto scroll = tweeny::from(0.0f)
  *   .to(1000.0f)
  *   .via(easing::linear)
  *   .during(600U)
  *   .build();
  *
  * // Progress indicator that matches time exactly
  * auto progress = tweeny::from(0)
  *   .to(100)
  *   .via(easing::linear)
  *   .during(100U)
  *   .build();
  *
  * // At frame 50, progress will be exactly 50
  * @endcode
  *
  * @note Linear is the default easing when no via() is specified, though using def is
  * more explicit in code.
  *
  * @see def Alias for linear easing
  * @see quadraticInOut For a gentle alternative with ease-in and ease-out
  * @see sinusoidalInOut For smooth, natural-feeling motion
  */
 inline constexpr detail::linearEasing linear{};

 /**
  * @brief Quadratic polynomial easing (t²) with gentle acceleration.
  *
  * The `quadraticIn` easing function uses a squared power curve for acceleration,
  * providing the gentlest polynomial easing. It's more subtle than cubic but still
  * provides noticeable easing.
  *
  * Characteristics:
  * - Gentle, smooth acceleration
  * - Polynomial with power of 2
  * - Subtle but perceptible easing
  * - Good for beginners and subtle animations
  *
  * This easing is ideal for:
  * - **Subtle UI motion**: When easing should be felt but not seen
  * - **Quick animations**: Short durations where gentle curves work best
  * - **Minimal designs**: Interfaces prioritizing restraint
  * - **Learning easings**: Easy to understand and predict
  *
  * Mathematically: `f(t) = t²`
  *
  * @code
  * // Gentle fade in
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::quadraticIn)
  *   .during(20U)
  *   .build();
  * @endcode
  *
  * @see quadraticOut For gentle deceleration
  * @see quadraticInOut For gentle motion at both ends
  * @see cubicIn For more pronounced acceleration
  * @see Visualize at https://easings.net/#easeInQuad
  */
 inline constexpr detail::quadraticInEasing quadraticIn{};

 /**
  * @brief Quadratic polynomial easing (t²) with gentle deceleration.
  *
  * The `quadraticOut` easing function provides subtle, smooth deceleration. It's the
  * gentlest polynomial easing, perfect when you want polish without drama.
  *
  * Characteristics:
  * - Gentle, smooth deceleration
  * - Subtle but professional feel
  * - Never feels too slow or too fast
  * - Safe choice for any context
  *
  * This easing excels at:
  * - **Subtle UI polish**: Adding refinement without drawing attention
  * - **Fast animations**: 100-200ms durations
  * - **Minimal interfaces**: Clean, understated design systems
  * - **Accessibility-friendly**: Gentle motion reduces disorientation
  * - **Background animations**: Motion that shouldn't distract
  *
  * QuadraticOut is great when you want animations to stay out of the way. It's less
  * dramatic than cubic but smoother than linear due to gradual deceleration.
  *
  * Mathematically: `f(t) = 1 - (1-t)²`
  *
  * @code
  * // Subtle tooltip appearance
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::quadraticOut)
  *   .during(15U)
  *   .build();
  * @endcode
  *
  * @see quadraticIn For gentle acceleration
  * @see quadraticInOut For gentle motion at both ends
  * @see cubicOut For more pronounced deceleration
  * @see Visualize at https://easings.net/#easeOutQuad
  */
 inline constexpr detail::quadraticOutEasing quadraticOut{};

 /**
  * @brief Quadratic polynomial easing with gentle acceleration and deceleration.
  *
  * The `quadraticInOut` easing function provides the most subtle polynomial S-curve.
  * Perfect for animations that need easing but should remain understated.
  *
  * Motion profile:
  * - First half: Gentle acceleration (t²)
  * - Midpoint: Maximum velocity
  * - Second half: Gentle deceleration
  * - Creates subtle S-curve
  *
  * This easing is appropriate for:
  * - **Minimal design systems**: Understated, refined motion
  * - **Fast animations**: Under 200ms where gentle curves shine
  * - **Accessibility**: Motion-sensitive users
  * - **Background motion**: Animations that shouldn't dominate
  *
  * Mathematically: Combines quadraticIn for t < 0.5 and quadraticOut for t >= 0.5
  *
  * @code
  * // Subtle menu transition
  * auto x = tweeny::from(0.0f)
  *   .to(100.0f)
  *   .via(easing::quadraticInOut)
  *   .during(20U)
  *   .build();
  * @endcode
  *
  * @see quadraticIn For only gentle acceleration
  * @see quadraticOut For only gentle deceleration
  * @see cubicInOut For more pronounced motion
  * @see Visualize at https://easings.net/#easeInOutQuad
  */
 inline constexpr detail::quadraticInOutEasing quadraticInOut{};

 /**
  * @brief Quartic polynomial easing (t⁴) with strong acceleration.
  *
  * The `quarticIn` easing function uses a power-4 curve for acceleration, creating
  * pronounced easing that's stronger than cubic but less extreme than exponential.
  *
  * Characteristics:
  * - Strong acceleration curve
  * - Polynomial with power of 4
  * - Very slow start, rapid finish
  * - More dramatic than cubic
  *
  * This easing is ideal for:
  * - **Dramatic entrances**: Elements that burst into view
  * - **Emphasis**: Drawing attention to motion start
  * - **Game animations**: Action-oriented interfaces
  * - **Long animations**: Over 500ms where strong curves work
  *
  * Mathematically: `f(t) = t⁴`
  *
  * @code
  * // Panel sliding in with emphasis
  * auto x = tweeny::from(-400.0f)
  *   .to(0.0f)
  *   .via(easing::quarticIn)
  *   .during(50U)
  *   .build();
  * @endcode
  *
  * @see quarticOut For strong deceleration
  * @see quarticInOut For strong motion at both ends
  * @see cubicIn For gentler acceleration
  * @see quinticIn For even stronger acceleration
  * @see Visualize at https://easings.net/#easeInQuart
  */
 inline constexpr detail::quarticInEasing quarticIn{};

 /**
  * @brief Quartic polynomial easing (t⁴) with strong deceleration.
  *
  * The `quarticOut` easing function provides pronounced deceleration that's popular
  * for animations needing clear emphasis without being as extreme as exponential.
  *
  * Characteristics:
  * - Strong deceleration curve
  * - Smooth, extended slowdown
  * - Emphasizes final position
  * - Balanced between cubic and exponential
  *
  * This easing excels at:
  * - **Emphasized arrivals**: Elements settling with impact
  * - **Current design patterns**: Strong deceleration suitable for emphasized arrivals
  * - **Medium-length animations**: 300-600ms sweet spot
  * - **Purposeful motion**: Clear acceleration and deceleration
  *
  * QuarticOut provides more "oomph" than cubic while remaining smooth and
  * professional. It's a good choice when cubic feels too subtle.
  *
  * Mathematically: `f(t) = 1 - (1-t)⁴`
  *
  * @code
  * // Card sliding into place with emphasis
  * auto y = tweeny::from(200.0f)
  *   .to(0.0f)
  *   .via(easing::quarticOut)
  *   .during(40U)
  *   .build();
  * @endcode
  *
  * @see quarticIn For strong acceleration
  * @see quarticInOut For strong motion at both ends
  * @see cubicOut For gentler deceleration
  * @see quinticOut For even stronger deceleration
  * @see Visualize at https://easings.net/#easeOutQuart
  */
 inline constexpr detail::quarticOutEasing quarticOut{};

 /**
  * @brief Quartic polynomial easing with strong acceleration and deceleration.
  *
  * The `quarticInOut` easing function combines strong acceleration and deceleration
  * for confident, purposeful motion. More dramatic than cubic, less extreme than quintic.
  *
  * Motion profile:
  * - First half: Strong acceleration (t⁴)
  * - Midpoint: Maximum velocity
  * - Second half: Strong deceleration
  * - Creates pronounced S-curve
  *
  * This easing is appropriate for:
  * - **Emphasized transitions**: Strong acceleration and deceleration
  * - **Current design patterns**: Strong deceleration curves
  * - **Important animations**: Focal transitions
  * - **Medium durations**: 300-600ms range
  *
  * Mathematically: Combines quarticIn for t < 0.5 and quarticOut for t >= 0.5
  *
  * @code
  * // Page transition with impact
  * auto x = tweeny::from(0.0f)
  *   .to(1920.0f)
  *   .via(easing::quarticInOut)
  *   .during(45U)
  *   .build();
  * @endcode
  *
  * @see quarticIn For only strong acceleration
  * @see quarticOut For only strong deceleration
  * @see cubicInOut For gentler motion
  * @see quinticInOut For more dramatic motion
  * @see Visualize at https://easings.net/#easeInOutQuart
  */
 inline constexpr detail::quarticInOutEasing quarticInOut{};

 /**
  * @brief Quintic polynomial easing (t⁵) with very strong acceleration.
  *
  * The `quinticIn` easing function uses a power-5 curve, creating the strongest
  * polynomial acceleration. It's more extreme than quartic but less than exponential.
  *
  * Characteristics:
  * - Very strong acceleration
  * - Polynomial with power of 5
  * - Extremely slow start
  * - Dramatic finish
  *
  * This easing is ideal for:
  * - **Maximum polynomial emphasis**: Strongest polynomial option
  * - **Dramatic effects**: Hero animations, focal transitions
  * - **Long animations**: Over 600ms durations
  * - **Alternative to exponential**: Slightly less extreme
  *
  * Mathematically: `f(t) = t⁵`
  *
  * @code
  * // Dramatic hero section reveal
  * auto scale = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::quinticIn)
  *   .during(60U)
  *   .build();
  * @endcode
  *
  * @see quinticOut For very strong deceleration
  * @see quinticInOut For very strong motion at both ends
  * @see quarticIn For slightly gentler acceleration
  * @see exponentialIn For even more extreme acceleration
  * @see Visualize at https://easings.net/#easeInQuint
  */
 inline constexpr detail::quinticInEasing quinticIn{};

 /**
  * @brief Quintic polynomial easing (t⁵) with very strong deceleration.
  *
  * The `quinticOut` easing function provides the strongest polynomial deceleration,
  * creating smooth but dramatic slowdown. Popular for animations with long durations
  * (500ms+) requiring strong deceleration.
  *
  * Characteristics:
  * - Very strong deceleration
  * - Long, smooth tail
  * - Very strong deceleration with extended slowdown phase
  * - Strongest polynomial option
  *
  * This easing excels at:
  * - **Applications prioritizing strong visual emphasis**: Galleries, portfolios, marketing sites
  * - **Animations with extended durations**: Requiring maximum polynomial deceleration
  * - **Longer animations**: 500ms+ durations
  * - **Emphasized arrivals**: Strong focus on end state
  *
  * QuinticOut provides the strongest polynomial deceleration while maintaining a
  * smooth, continuous curve.
  *
  * Mathematically: `f(t) = 1 - (1-t)⁵`
  *
  * @code
  * // Premium modal entrance
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::quinticOut)
  *   .during(45U)
  *   .build();
  * @endcode
  *
  * @see quinticIn For very strong acceleration
  * @see quinticInOut For very strong motion at both ends
  * @see quarticOut For slightly gentler deceleration
  * @see exponentialOut For even more extreme deceleration
  * @see Visualize at https://easings.net/#easeOutQuint
  */
 inline constexpr detail::quinticOutEasing quinticOut{};

 /**
  * @brief Quintic polynomial easing with very strong acceleration and deceleration.
  *
  * The `quinticInOut` easing function provides the strongest polynomial S-curve,
  * creating powerful motion with maximum polynomial emphasis.
  *
  * Motion profile:
  * - First half: Very strong acceleration (t⁵)
  * - Midpoint: Maximum velocity
  * - Second half: Very strong deceleration
  * - Creates dramatic S-curve
  *
  * This easing is appropriate for:
  * - **Applications prioritizing strong visual emphasis**: Galleries, portfolios, marketing sites
  * - **Animations requiring maximum polynomial emphasis**: At both acceleration and deceleration phases
  * - **Important transitions**: Focal, memorable moments
  * - **Longer animations**: 500ms+ durations
  *
  * Mathematically: Combines quinticIn for t < 0.5 and quinticOut for t >= 0.5
  *
  * @code
  * // Premium screen transition
  * auto x = tweeny::from(0.0f)
  *   .to(1920.0f)
  *   .via(easing::quinticInOut)
  *   .during(50U)
  *   .build();
  * @endcode
  *
  * @see quinticIn For only very strong acceleration
  * @see quinticOut For only very strong deceleration
  * @see quarticInOut For gentler motion
  * @see exponentialInOut For more extreme motion
  * @see Visualize at https://easings.net/#easeInOutQuint
  */
 inline constexpr detail::quinticInOutEasing quinticInOut{};

 /**
  * @brief Sinusoidal easing based on sine wave for smooth acceleration.
  *
  * The `sinusoidalIn` easing function uses a sine curve for acceleration, creating
  * extremely smooth, natural motion. Based on trigonometric functions rather than
  * polynomials.
  *
  * Characteristics:
  * - Very smooth acceleration
  * - Natural, organic feel
  * - No abrupt velocity changes
  * - Gentle but noticeable
  *
  * This easing is ideal for:
  * - **Natural motion**: Organic, flowing animations
  * - **Smooth camera moves**: Pans, zooms, orbits
  * - **Elegant transitions**: Refined, sophisticated feel
  * - **Accessible animations**: Gentle on motion sensitivity
  *
  * Mathematically: `f(t) = 1 - cos(t * π/2)`
  *
  * @code
  * // Smooth camera pan
  * auto x = tweeny::from(0.0f)
  *   .to(1000.0f)
  *   .via(easing::sinusoidalIn)
  *   .during(60U)
  *   .build();
  * @endcode
  *
  * @see sinusoidalOut For smooth deceleration
  * @see sinusoidalInOut For smooth motion at both ends
  * @see quadraticIn For similar gentleness
  * @see Visualize at https://easings.net/#easeInSine
  */
 inline constexpr detail::sinusoidalInEasing sinusoidalIn{};

 /**
  * @brief Sinusoidal easing based on sine wave for smooth deceleration.
  *
  * The `sinusoidalOut` easing function uses a sine curve for deceleration, creating
  * extremely smooth, natural motion that settles gently.
  *
  * Characteristics:
  * - Very smooth deceleration
  * - Natural, flowing motion
  * - Gentle slowdown
  * - Mathematically elegant
  *
  * This easing excels at:
  * - **Natural UI motion**: Smooth, organic feel
  * - **Continuous animations**: Loops, cycles, repeated motion
  * - **Gentle transitions**: Calm, relaxed interfaces
  * - **Accessible design**: Motion-sensitivity friendly
  *
  * SinusoidalOut is excellent when you want smoothness above all else. It's
  * gentler than quadratic while still providing clear easing.
  *
  * Mathematically: `f(t) = sin(t * π/2)`
  *
  * @code
  * // Smooth fade in
  * auto opacity = tweeny::from(0.0f)
  *   .to(1.0f)
  *   .via(easing::sinusoidalOut)
  *   .during(30U)
  *   .build();
  * @endcode
  *
  * @see sinusoidalIn For smooth acceleration
  * @see sinusoidalInOut For smooth motion at both ends
  * @see quadraticOut For similar gentleness
  * @see Visualize at https://easings.net/#easeOutSine
  */
 inline constexpr detail::sinusoidalOutEasing sinusoidalOut{};

 /**
  * @brief Sinusoidal easing with smooth acceleration and deceleration.
  *
  * The `sinusoidalInOut` easing function uses sine curves for both acceleration
  * and deceleration, creating the smoothest possible S-curve motion.
  *
  * Motion profile:
  * - First half: Smooth sine-based acceleration
  * - Midpoint: Maximum velocity
  * - Second half: Smooth sine-based deceleration
  * - Creates extremely smooth S-curve
  *
  * This easing is appropriate for:
  * - **Natural, organic motion**: Smoothest easing available
  * - **Continuous loops**: Seamless repeated animations
  * - **Calm interfaces**: Relaxed, gentle design systems
  * - **Accessible animations**: Minimal motion stress
  *
  * SinusoidalInOut is the smoothest InOut easing, making it perfect when
  * fluid, natural motion is the priority.
  *
  * Mathematically: `f(t) = (1 - cos(t * π)) / 2`
  *
  * @code
  * // Ultra-smooth transition
  * auto x = tweeny::from(0.0f)
  *   .to(100.0f)
  *   .via(easing::sinusoidalInOut)
  *   .during(40U)
  *   .build();
  * @endcode
  *
  * @see sinusoidalIn For only smooth acceleration
  * @see sinusoidalOut For only smooth deceleration
  * @see quadraticInOut For similar gentleness
  * @see Visualize at https://easings.net/#easeInOutSine
  */
 inline constexpr detail::sinusoidalInOutEasing sinusoidalInOut{};

 /**
  * @brief Stepped easing that holds the start value until the keyframe completes.
  *
  * The `stepped` easing function returns the starting value throughout the entire
  * duration of the keyframe segment, only jumping to the target value when moving
  * to the next keyframe. This creates instant transitions between keyframes without
  * any interpolation within each segment.
  *
  * Characteristics:
  * - No interpolation within keyframe segments
  * - Holds start value until keyframe ends
  * - Instant jumps between keyframes
  * - Creates discrete, stair-step motion
  *
  * This easing is ideal for:
  * - **Discrete state transitions**: Values that shouldn't interpolate smoothly
  * - **Keyframe-based animations**: Step through distinct poses or states
  * - **Frame-by-frame effects**: Hold each frame without blending
  * - **Boolean-like values**: Properties that need instant changes
  * - **Sprite switching**: Change sprites at keyframe boundaries
  * - **Cut transitions**: Instant changes without fading
  *
  * Stepped is fundamentally different from other easings because it eliminates
  * interpolation entirely within each keyframe segment, creating hard cuts between
  * animation states.
  *
  * @code
  * // Hold value at each keyframe, jump instantly between them
  * auto value = tweeny::from(0)
  *   .to(10).via(easing::stepped).during(100U)
  *   .to(20).via(easing::stepped).during(100U)
  *   .to(30).via(easing::stepped).during(100U)
  *   .build();
  *
  * // During frames 0-99: returns 0
  * // During frames 100-199: returns 10
  * // During frames 200-299: returns 20
  * // At frame 300: returns 30
  *
  * // Useful for sprite animation indices
  * auto spriteIndex = tweeny::from(0)
  *   .to(1).via(easing::stepped).during(10U)
  *   .to(2).via(easing::stepped).during(10U)
  *   .to(3).via(easing::stepped).during(10U)
  *   .build();
  * // Holds each sprite index for 10 frames, then instantly switches
  * @endcode
  *
  * @see linear For smooth constant-velocity interpolation
  */
 inline constexpr detail::steppedEasing stepped{};
}

#endif //TWEENY_EASING_H

/**
 * @namespace tweeny
 * @brief Contains all public API types and functions for creating and managing tweens.
 *
 * This namespace provides the builder pattern API (from(), tweeny_builder), the main
 * tween class template, easing functions, and event types. All user-facing functionality
 * is contained within this namespace to avoid naming conflicts.
 */
namespace tweeny {
  /**
   * @brief Primary template for the tween builder type.
   *
   * Models a fluent builder used to construct tween instances for one or more parts.
   * The boolean template parameter encodes the builder state: when false, no to() call
   * has been made yet; when true, easing functions and durations can be configured.
   *
   * @tparam WasToCalled Compile-time flag indicating whether at least one to() call has been made.
   * @tparam FirstValue Type of the first tweened component.
   * @tparam RemainingValues Types of the remaining tweened components.
   */
  template <bool WasToCalled, typename FirstValue, typename... RemainingValues>
  class tweeny_builder;

  template <typename FirstValue, typename... RemainingValues>
  /**
   * @brief Builder specialization for the stage before the first to() call.
   *
   * Holds the initial key-frame values and allows adding the next key-frame via to().
   * Calling `to()` transitions the builder to the WasToCalled=true specialization; build()
   * can be used to create a tween with the currently collected key-frames.
   *
   * @tparam FirstValue Type of the first tweened component.
   * @tparam RemainingValues Types of the remaining tweened components.
   */
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

      /**
       * @brief Adds a target keyframe to the tween.
       *
       * Specifies the destination value(s) for the animation. After calling this,
       * you can configure easing and duration with via() and during().
       *
       * @param firstComponent Target value for the first component
       * @param remainingComponents Target values for remaining components (if multi-value tween)
       * @return Builder in a configurable state (can call via(), during(), to(), or build())
       *
       * @code
       * auto t = tweeny::from(0).to(100).during(60U).build();
       * @endcode
       */
      tweeny_builder<true, FirstValue, RemainingValues...> to(const FirstValue & firstComponent, const RemainingValues &... remainingComponents) & {
        key_frames.emplace_back(firstComponent, remainingComponents...);
        return tweeny_builder<true, FirstValue, RemainingValues...>(key_frames);
      }

      /// @overload
      tweeny_builder<true, FirstValue, RemainingValues...> to(const FirstValue & firstComponent, const RemainingValues &... remainingComponents) && {
        key_frames.emplace_back(firstComponent, remainingComponents...);
        return tweeny_builder<true, FirstValue, RemainingValues...>(std::move(key_frames));
      }

    private:
      key_frames_t key_frames;
  };

  /**
 * @brief Builder specialization for the stage after at least one to() call.
 *
 * In this stage, additional key-frames can be appended with to(), easing functions can be
 * specified using via(), and per-component or uniform frame counts can be set with during().
 * Finally, build() materializes the configured tween.
 *
 * @tparam FirstValue Type of the first tweened component.
 * @tparam RemainingValues Types of the remaining tweened components.
 */
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

      /**
       * @brief Adds another keyframe to create multipoint animations.
       *
       * Call `to()` multiple times to create complex animations with multiple segments,
       * each with its own easing and duration.
       *
       * @param firstComponent Target value for the first component
       * @param remainingComponents Target values for remaining components
       * @return Reference to this builder for method chaining
       *
       * @code
       * // Three-point animation: 0 → 50 → 100
       * auto t = tweeny::from(0)
       *   .to(50).via(easing::quadraticOut).during(30U)
       *   .to(100).via(easing::bounceOut).during(30U)
       *   .build();
       * @endcode
       *
       * @anchor builder_to
       */
      tweeny_builder to(const FirstValue & firstComponent, const RemainingValues &... remainingComponents) & {
        key_frames.emplace_back(firstComponent, remainingComponents...);
        return tweeny_builder(key_frames);
      }

      /// @overload
      tweeny_builder to(const FirstValue & firstComponent, const RemainingValues &... remainingComponents) && {
        key_frames.emplace_back(firstComponent, remainingComponents...);
        return tweeny_builder(std::move(key_frames));
      }

      /**
       * @brief Specifies per-component easing functions for the last keyframe segment.
       *
       * Each tween component gets its own easing function. The number of easing
       * functions must match the number of tween components (compile-time checked).
       *
       * @param easing_functions One easing function per tween component
       * @return Reference to this builder for method chaining
       *
       * @code
       * // Two components with different easings
       * auto t = tweeny::from(0, 0.0f)
       *   .to(100, 50.0f)
       *   .via(easing::linear, easing::bounceOut)
       *   .during(60U)
       *   .build();
       * @endcode
       */
      template<typename... EasingFunctionTypes>
      tweeny_builder & via(EasingFunctionTypes... easing_functions) {
        static_assert(sizeof...(EasingFunctionTypes) == value_count,
                      "via() must have one easing function per tween component");
        auto & key_frame = key_frames.at(key_frames.size() - 2);
        key_frame.easing_functions = std::make_tuple(easing_functions...);
        return *this;
      }

      /**
       * @brief Specifies a single easing function for all components.
       *
       * Applies the same easing to all tween components. This is the most common usage.
       *
       * @param easing_function Easing function to apply to all components
       * @return Reference to this builder for method chaining
       *
       * @code
       * auto t = tweeny::from(0, 0.0f).to(100, 100.0f).via(easing::quadraticInOut).during(60U).build();
       * @endcode
       *
       * @anchor builder_via
       */
      template<typename EasingFunctionType>
      tweeny_builder & via(EasingFunctionType easing_function) {
        auto & key_frame = key_frames.at(key_frames.size() - 2);
        key_frame.easing_functions = detail::make_repeated_tuple<EasingFunctionType, value_count>(easing_function);
        return *this;
      }

      /**
       * @brief Specifies per-component frame durations for the last keyframe segment.
       *
       * Each component can have its own animation duration in frames. The number of
       * durations must match the number of components (compile-time checked).
       * All parameters must be uint32_t.
       *
       * @param frame_counts Duration in frames for each component
       * @return Reference to this builder for method chaining
       *
       * @code
       * // X animates over 60 frames, Y over 120 frames
       * auto t = tweeny::from(0, 0).to(100, 100).during(60U, 120U).build();
       * @endcode
       *
       * @anchor builder_during
       */
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

      /**
       * @brief Specifies a uniform frame duration for all components.
       *
       * All tween components will animate over the same number of frames.
       * This is the most common usage.
       *
       * @param frame_count Duration in frames for the animation segment
       * @return Reference to this builder for method chaining
       *
       * @code
       * // Animate from 0 to 100 over 60 frames
       * auto t = tweeny::from(0, 0).to(100, 100).during(60U).build();
       * @endcode
       */
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

      /**
       * @brief Constructs a tween object from the configured keyframes.
       *
       * Creates a tween with all configured keyframes, easings, and durations.
       * The builder can be reused to create multiple tween instances with the same
       * configuration or modified further to create variations.
       *
       * @return A tween object ready for animation
       *
       * @code
       * // Direct use (builder discarded after build)
       * auto t1 = tweeny::from(0).to(100).via(easing::linear).during(60U).build();
       *
       * // Reusable builder
       * auto builder = tweeny::from(0).to(100).via(easing::linear).during(60U);
       * auto t2 = builder.build();  // First tween
       * auto t3 = builder.build();  // Second tween with same config
       *
       * // Create variations
       * auto t4 = builder.to(200).during(120U).build();  // Extended animation
       * @endcode
       *
       * @anchor builder_build
       */
      tween_t build() const & { return tween(key_frames); }

      /// @overload
      tween_t build() && { return tween(std::move(key_frames)); }

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

  /**
   * @brief Creates a new tween builder starting from the specified value(s).
   *
   * This is the entry point for creating all tweens. It deduces types automatically
   * and supports single values, multiple values, and heterogeneous types.
   *
   * @param firstComponent Initial value for the first component
   * @param remainingComponents Initial values for additional components (optional)
   * @return A builder in the initial state (must call to() next)
   *
   * @code
   * // Single value
   * auto t1 = tweeny::from(0).to(100).during(60U).build();
   *
   * // Multiple homogeneous values
   * auto t2 = tweeny::from(0, 0).to(100, 100).during(60U).build();
   *
   * // Heterogeneous types
   * auto t3 = tweeny::from(0, 0.0f, 0u).to(10, 5.0f, 100u).during(60U).build();
   * @endcode
   */
  template <typename FirstValue, typename... RemainingValues>
  tweeny_builder<false, FirstValue, RemainingValues...> from(FirstValue firstComponent, RemainingValues... remainingComponents) {
    return tweeny_builder<false, FirstValue, RemainingValues...>(firstComponent, remainingComponents...);
  }
}

#endif //TWEENY_TWEENY_H
