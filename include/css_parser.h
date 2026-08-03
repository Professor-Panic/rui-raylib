#pragma once

#include "css_emulator.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

//--------------------------------------------------------------------
// CSS parser
//--------------------------------------------------------------------

std::string TrimCSS(const std::string& text);

std::string LowerCSS(std::string text);

bool EndsWithCSS(
    const std::string& text,
    const std::string& suffix
);

std::optional<float> ParseStrictCSSFloat(
    const std::string& text
);

std::optional<float> ParseCSSNumber(
    std::string value
);

std::optional<CSSLength> ParseCSSLength(
    std::string value
);

std::optional<float> ParseCSSOpacity(
    std::string value
);

std::optional<bool> ParseCSSBoolean(
    std::string value
);

std::vector<std::string> SplitCSSArguments(
    const std::string& arguments
);

std::vector<std::string> SplitCSSWhitespaceValues(
    const std::string& value
);

std::optional<std::array<CSSLength,4>> ParseCSSBoxShorthand(
    const std::string& value
);

std::optional<int> ParsePositiveCSSInteger(
    const std::string& value
);

std::optional<Color> ParseCSSColor(
    std::string value
);

std::optional<GridTrackSize> ParseGridTrackSize(
    std::string value
);

std::optional<GridTrackList> ParseGridTrackList(
    std::string value
);

std::optional<PropertyType> ParsePropertyType(
    const std::string& raw
);

bool IsLengthProperty(
    PropertyType property
);

std::optional<PropertyValueType> ParsePropertyValue(
    PropertyType property,
    const std::string& raw
);

bool AppendGridPlacementDeclarations(
    std::vector<Declaration>& declarations,
    PropertyType property,
    const std::string& raw
);

bool AppendCSSDeclaration(
    std::vector<Declaration>& declarations,
    PropertyType property,
    const std::string& raw
);

SelectorType ParseSelectorType(
    std::string value
);

SelectorState ParseSelectorState(
    std::string value
);

StyleRule ParseRuleSelector(
    std::string selector
);

std::string RemoveCSSComments(
    const std::string& css
);

std::vector<std::string> SplitCSSSelectors(
    const std::string& selectors
);

using CSSVariableMap =
    std::unordered_map<std::string,std::string>;

bool IsCSSCustomProperty(
    const std::string& property_name
);

bool IsCSSRootSelector(
    const std::string& selector
);

std::size_t FindMatchingCSSParenthesis(
    const std::string& text,
    std::size_t opening
);

std::size_t FindTopLevelCSSComma(
    const std::string& text
);

std::optional<std::string> ResolveCSSVariablesInternal(
    const std::string& raw,
    const CSSVariableMap& variables,
    std::unordered_set<std::string>& resolving,
    int recursion_depth
);

std::optional<std::string> ResolveCSSVariables(
    const std::string& raw,
    const CSSVariableMap& variables
);

struct RawCSSDeclaration {
    std::string property_name;
    std::string property_value;
};

std::vector<RawCSSDeclaration> ParseRawCSSDeclarations(
    const std::string& body
);

CSSVariableMap CollectCSSRootVariables(
    const std::string& source
);

StyleSheet ParseStyleSheet(
    const std::string& css
);
